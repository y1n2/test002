/**
 * @file mih_transport.c
 * @brief MIH 传输层实现
 *
 * 实现 MIH (Media Independent Handover) 协议的传输层
 * 提供基于 Unix Domain Socket 的可靠消息传输
 * 支持 MIH 原语的发送和接收
 */

#include "mih_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

/*===========================================================================
 * 全局变量
 *===========================================================================*/

/**
 * @brief 全局事务 ID 计数器
 *
 * 用于生成唯一的 MIH 消息事务标识符
 * 线程不安全，需要在多线程环境中保护
 */
static uint32_t g_transaction_id = 0;

/*===========================================================================
 * 消息头操作函数
 *===========================================================================*/

/**
 * @brief 初始化 MIH 传输层消息头
 *
 * 为 MIH 消息构造传输层头部信息
 * 自动生成事务 ID 和时间戳
 *
 * @param header 指向消息头的指针
 * @param primitive_type MIH 原语类型
 * @param payload_length 有效载荷长度
 * @param transaction_id 事务 ID (0 表示自动生成)
 */
void mih_transport_init_header(mih_transport_header_t *header,
                               uint16_t primitive_type,
                               uint16_t payload_length,
                               uint32_t transaction_id)
{
    /* 参数验证 */
    if (!header) return;

    /* 设置原语类型 */
    header->primitive_type = primitive_type;

    /* 计算并设置消息总长度 */
    header->message_length = sizeof(mih_transport_header_t) + payload_length;

    /* 设置事务 ID */
    header->transaction_id = (transaction_id == 0) ? mih_transport_next_transaction_id() : transaction_id;

    /* 设置时间戳 */
    header->timestamp = (uint32_t)time(NULL);
}

/*===========================================================================
 * 消息发送和接收函数
 *===========================================================================*/

/**
 * @brief 发送 MIH 消息
 *
 * 通过 Socket 发送完整的 MIH 消息（头部 + 有效载荷）
 * 使用同步发送，确保消息完整传输
 *
 * @param sockfd Socket 文件描述符
 * @param primitive_type MIH 原语类型
 * @param payload 消息有效载荷数据
 * @param payload_len 有效载荷长度
 * @return 成功返回 0, 失败返回 -1
 */
int mih_transport_send(int sockfd,
                      uint16_t primitive_type,
                      const void *payload,
                      size_t payload_len)
{
    /* 参数验证 */
    if (sockfd < 0 || !payload || payload_len == 0) {
        return -1;
    }

    /* 构造消息头 */
    mih_transport_header_t header;
    mih_transport_init_header(&header, primitive_type, payload_len, 0);

    /* 发送消息头 */
    ssize_t sent = send(sockfd, &header, sizeof(header), 0);
    if (sent != sizeof(header)) {
        /* 头部发送失败 */
        return -1;
    }

    /* 发送有效载荷 */
    sent = send(sockfd, payload, payload_len, 0);
    if ((size_t)sent != payload_len) {
        /* 有效载荷发送失败 */
        return -1;
    }

    /* 发送成功 */
    return 0;
}

/**
 * @brief 接收 MIH 消息
 *
 * 从 Socket 接收完整的 MIH 消息
 * 先接收头部，再根据头部信息接收有效载荷
 *
 * @param sockfd Socket 文件描述符
 * @param header 接收到的消息头（输出参数）
 * @param payload_buf 有效载荷缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回接收到的有效载荷长度, 失败返回 -1
 */
int mih_transport_recv(int sockfd,
                      mih_transport_header_t *header,
                      void *payload_buf,
                      size_t buf_size)
{
    /* 参数验证 */
    if (sockfd < 0 || !header || !payload_buf) {
        return -1;
    }

    /* 接收消息头 */
    ssize_t n = recv(sockfd, header, sizeof(mih_transport_header_t), MSG_WAITALL);
    if (n != sizeof(mih_transport_header_t)) {
        /* 头部接收失败 */
        return -1;
    }

    /* 计算有效载荷长度 */
    uint16_t payload_len = header->message_length - sizeof(mih_transport_header_t);

    /* 验证有效载荷长度 */
    if (payload_len > buf_size) {
        /* 缓冲区不足 */
        fprintf(stderr, "[MIH Transport] Payload too large: %u > %zu\n",
                payload_len, buf_size);
        return -1;
    }

    /* 接收有效载荷 */
    if (payload_len > 0) {
        n = recv(sockfd, payload_buf, payload_len, MSG_WAITALL);
        if (n != payload_len) {
            /* 有效载荷接收失败 */
            return -1;
        }
    }

    /* 返回接收到的有效载荷长度 */
    return (int)payload_len;
}

/*===========================================================================
 * 连接管理函数
 *===========================================================================*/

/**
 * @brief 连接到 MIH 服务器
 *
 * 建立到 MIH 服务器的 Unix Domain Socket 连接
 * 支持重试机制，提高连接成功率
 *
 * @param socket_path Socket 文件路径
 * @param retry_count 最大重试次数
 * @param retry_delay_sec 重试间隔（秒）
 * @return 成功返回 Socket 文件描述符, 失败返回 -1
 */
int mih_transport_connect(const char *socket_path,
                         int retry_count,
                         int retry_delay_sec)
{
    /* 参数验证 */
    if (!socket_path) {
        return -1;
    }

    /* 创建 Socket */
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[MIH Transport] socket");
        return -1;
    }

    /* 设置服务器地址 */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /* 重试连接 */
    for (int i = 0; i < retry_count; i++) {
        /* 尝试连接 */
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            /* 连接成功 */
            return sockfd;
        }

        /* 连接失败，判断是否需要重试 */
        if (i < retry_count - 1) {
            printf("[MIH Transport] Connection failed, retrying in %d seconds...\n",
                   retry_delay_sec);
            sleep(retry_delay_sec);
        }
    }

    /* 所有重试都失败 */
    perror("[MIH Transport] connect");
    close(sockfd);
    return -1;
}

/**
 * @brief 创建 MIH 服务器 Socket
 *
 * 创建 Unix Domain Socket 服务器
 * 绑定到指定路径并开始监听连接请求
 *
 * @param socket_path Socket 文件路径
 * @return 成功返回服务器 Socket 文件描述符, 失败返回 -1
 */
int mih_transport_create_server(const char *socket_path)
{
    /* 参数验证 */
    if (!socket_path) {
        return -1;
    }

    /* 创建服务器 Socket */
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[MIH Transport] socket");
        return -1;
    }

    /* 删除可能存在的旧 Socket 文件 */
    unlink(socket_path);

    /* 设置服务器地址 */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /* 绑定 Socket */
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[MIH Transport] bind");
        close(sockfd);
        return -1;
    }

    /* 开始监听 */
    if (listen(sockfd, MIH_SOCKET_BACKLOG) < 0) {
        perror("[MIH Transport] listen");
        close(sockfd);
        unlink(socket_path);
        return -1;
    }

    /* 创建成功 */
    return sockfd;
}

/*===========================================================================
 * 事务 ID 管理函数
 *===========================================================================*/

/**
 * @brief 获取下一个事务 ID
 *
 * 生成唯一的 MIH 消息事务标识符
 * 使用全局计数器，自动递增
 *
 * @return 新的唯一事务 ID
 */
uint32_t mih_transport_next_transaction_id(void)
{
    /* 返回递增后的事务 ID */
    return ++g_transaction_id;
}

/*===========================================================================
 * 数据报模式函数实现 (SOCK_DGRAM)
 * 
 * 用于与 DLM 原型通信，使用简化的 2 字节类型码前缀
 * 消息格式: [2字节类型码] + [原始结构体数据]
 *===========================================================================*/

/**
 * @brief 创建数据报模式 MIHF 服务器套接字
 *
 * 创建 Unix Domain Datagram Socket 服务器
 * 用于接收 DLM 原型发送的 MIH 消息
 *
 * @param socket_path Socket 文件路径
 * @return 成功返回服务器 Socket 文件描述符, 失败返回 -1
 */
int mih_transport_create_dgram_server(const char *socket_path)
{
    /* 参数验证 */
    if (!socket_path) {
        return -1;
    }

    /* 创建数据报 Socket */
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[MIH Transport] socket (DGRAM)");
        return -1;
    }

    /* 删除可能存在的旧 Socket 文件 */
    unlink(socket_path);

    /* 设置服务器地址 */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /* 绑定 Socket */
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[MIH Transport] bind (DGRAM)");
        close(sockfd);
        return -1;
    }

    /* 创建成功 */
    return sockfd;
}

/**
 * @brief 通过数据报套接字发送 MIH 消息
 *
 * 发送格式: [2字节类型码] + [有效载荷]
 * 与 DLM 原型的 dlm_send_to_mihf() 格式一致
 *
 * @param sockfd Socket 文件描述符
 * @param dest_addr 目标地址
 * @param addr_len 地址长度
 * @param primitive_type MIH 原语类型
 * @param payload 消息有效载荷数据
 * @param payload_len 有效载荷长度
 * @return 成功返回 0, 失败返回 -1
 */
int mih_transport_sendto(int sockfd,
                        const struct sockaddr *dest_addr,
                        socklen_t addr_len,
                        uint16_t primitive_type,
                        const void *payload,
                        size_t payload_len)
{
    /* 参数验证 */
    if (sockfd < 0 || !dest_addr) {
        return -1;
    }

    /* 构造消息缓冲区: [2字节类型码] + [有效载荷] */
    uint8_t buffer[MIH_MAX_MESSAGE_SIZE];
    size_t total_len = MIH_DGRAM_HEADER_SIZE + payload_len;

    if (total_len > sizeof(buffer)) {
        fprintf(stderr, "[MIH Transport] Message too large: %zu > %zu\n",
                total_len, sizeof(buffer));
        return -1;
    }

    /* 写入 2 字节类型码 */
    *(uint16_t *)buffer = primitive_type;

    /* 写入有效载荷 */
    if (payload && payload_len > 0) {
        memcpy(buffer + MIH_DGRAM_HEADER_SIZE, payload, payload_len);
    }

    /* 发送消息 */
    ssize_t sent = sendto(sockfd, buffer, total_len, 0, dest_addr, addr_len);
    if (sent < 0) {
        perror("[MIH Transport] sendto");
        return -1;
    }

    if ((size_t)sent != total_len) {
        fprintf(stderr, "[MIH Transport] Partial send: %zd / %zu\n", sent, total_len);
        return -1;
    }

    return 0;
}

/**
 * @brief 从数据报套接字接收 MIH 消息
 *
 * 接收格式: [2字节类型码] + [有效载荷]
 * 解析 DLM 原型发送的消息格式
 *
 * @param sockfd Socket 文件描述符
 * @param from_addr 源地址 (输出参数)
 * @param addr_len 地址长度 (输入/输出参数)
 * @param primitive_type 解析出的原语类型 (输出参数)
 * @param payload_buf 有效载荷缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回有效载荷长度, 失败返回 -1
 */
int mih_transport_recvfrom(int sockfd,
                          struct sockaddr *from_addr,
                          socklen_t *addr_len,
                          uint16_t *primitive_type,
                          void *payload_buf,
                          size_t buf_size)
{
    /* 参数验证 */
    if (sockfd < 0 || !primitive_type || !payload_buf) {
        return -1;
    }

    /* 接收缓冲区 */
    uint8_t buffer[MIH_MAX_MESSAGE_SIZE];

    /* 接收消息 */
    ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                 from_addr, addr_len);
    if (recv_len < 0) {
        perror("[MIH Transport] recvfrom");
        return -1;
    }

    /* 验证最小长度 (至少需要 2 字节类型码) */
    if (recv_len < MIH_DGRAM_HEADER_SIZE) {
        fprintf(stderr, "[MIH Transport] Message too short: %zd bytes\n", recv_len);
        return -1;
    }

    /* 解析 2 字节类型码 */
    *primitive_type = *(uint16_t *)buffer;

    /* 计算有效载荷长度 */
    size_t payload_len = recv_len - MIH_DGRAM_HEADER_SIZE;

    /* 验证缓冲区大小 */
    if (payload_len > buf_size) {
        fprintf(stderr, "[MIH Transport] Payload too large: %zu > %zu\n",
                payload_len, buf_size);
        return -1;
    }

    /* 复制有效载荷 */
    if (payload_len > 0) {
        memcpy(payload_buf, buffer + MIH_DGRAM_HEADER_SIZE, payload_len);
    }

    return (int)payload_len;
}
