/**
 * @file mih_transport.h
 * @brief MIH Transport Layer for Unix Domain Socket Communication - 用于Unix域套接字通信的MIH传输层
 * @description Provides IPC mechanisms for CM Core <-> DLM communication - 为CM核心<->DLM通信提供IPC机制
 * @date 2025-11-27
 * 
 * 支持两种传输模式:
 * 1. 流式模式 (SOCK_STREAM): 使用完整 mih_transport_header_t (12 字节)
 * 2. 数据报模式 (SOCK_DGRAM): 使用简化 2 字节类型码前缀
 */

#ifndef MIH_TRANSPORT_H  // 防止头文件被重复包含的保护宏开始
#define MIH_TRANSPORT_H  // 定义头文件保护宏

#include <stdint.h>      // 包含标准整数类型定义头文件，提供uint16_t、uint32_t等类型
#include <sys/types.h>   // 包含系统类型定义头文件，提供size_t等类型
#include <sys/socket.h>  // 包含套接字定义头文件，提供 struct sockaddr, socklen_t 等

/*===========================================================================
 * Transport Configuration - 传输配置
 *===========================================================================*/

#define MIH_SOCKET_PATH         "/tmp/magic_core.sock"  // MIH套接字路径，Unix域套接字文件路径 (流式)
#define MIH_DGRAM_SOCKET_PATH   "/tmp/mihf.sock"        // MIH数据报套接字路径 (用于 DLM 原型)
#define MIH_MAX_MESSAGE_SIZE    4096                    // MIH最大消息大小，单位字节
#define MIH_SOCKET_BACKLOG      10                      // MIH套接字监听队列长度

/* 简化消息头大小 (仅 2 字节类型码) - 用于数据报模式 */
#define MIH_DGRAM_HEADER_SIZE   2

/*===========================================================================
 * Transport Header - 传输头
 * @description Wraps all MIH primitives for socket transmission - 为套接字传输包装所有MIH原语
 *===========================================================================*/

/**
 * @brief 完整传输头 (用于流式 SOCK_STREAM 模式)
 */
typedef struct {
    uint16_t        primitive_type;     /* MIH primitive code (see mih_protocol.h and mih_extensions.h) - MIH原语代码（参见mih_protocol.h和mih_extensions.h） */
    uint16_t        message_length;     /* Total length including this header - 包括此头的总长度 */
    uint32_t        transaction_id;     /* For request/confirm pairing - 用于请求/确认配对 */
    uint32_t        timestamp;          /* Unix timestamp (seconds) - Unix时间戳（秒） */
} __attribute__((packed)) mih_transport_header_t;  // MIH传输头结构体，使用紧凑打包

/**
 * @brief 简化传输头 (用于数据报 SOCK_DGRAM 模式)
 * DLM 原型使用此格式：2 字节类型码 + 原始结构体
 */
typedef struct {
    uint16_t        primitive_type;     /* MIH 原语代码 (2 字节) */
} __attribute__((packed)) mih_dgram_header_t;

/*===========================================================================
 * Transport Functions - 传输函数
 *===========================================================================*/

/**
 * @brief Initialize MIH transport header - 初始化MIH传输头
 * @param header Pointer to header structure - 指向头结构体的指针
 * @param primitive_type MIH primitive type code - MIH原语类型代码
 * @param payload_length Size of payload (without header) - 有效载荷大小（不包括头）
 * @param transaction_id Transaction ID (0 = auto-generate) - 事务ID（0 = 自动生成）
 */
void mih_transport_init_header(mih_transport_header_t *header,   // 初始化MIH传输头函数，设置头结构体字段
                               uint16_t primitive_type,           // MIH原语类型参数
                               uint16_t payload_length,           // 有效载荷长度参数
                               uint32_t transaction_id);          // 事务ID参数

/**
 * @brief Send MIH message over socket - 通过套接字发送MIH消息
 * @param sockfd Socket file descriptor - 套接字文件描述符
 * @param primitive_type MIH primitive type - MIH原语类型
 * @param payload Pointer to MIH primitive payload - 指向MIH原语有效载荷的指针
 * @param payload_len Size of payload - 有效载荷大小
 * @return 0 on success, -1 on failure - 成功返回0，失败返回-1
 */
int mih_transport_send(int sockfd,                              // 通过套接字发送MIH消息函数
                      uint16_t primitive_type,                  // MIH原语类型参数
                      const void *payload,                      // 有效载荷指针参数
                      size_t payload_len);                      // 有效载荷长度参数

/**
 * @brief Receive MIH message from socket - 从套接字接收MIH消息
 * @param sockfd Socket file descriptor - 套接字文件描述符
 * @param header Pointer to receive header - 指向接收头的指针
 * @param payload_buf Buffer for payload - 有效载荷缓冲区
 * @param buf_size Size of payload buffer - 有效载荷缓冲区大小
 * @return Bytes received (payload only), -1 on failure - 接收到的字节数（仅有效载荷），失败返回-1
 * 
 * @note This function reads header first, validates it, then reads payload - 此函数首先读取头，验证它，然后读取有效载荷
 */
int mih_transport_recv(int sockfd,                             // 从套接字接收MIH消息函数
                      mih_transport_header_t *header,          // 接收头指针参数
                      void *payload_buf,                       // 有效载荷缓冲区参数
                      size_t buf_size);                        // 缓冲区大小参数

/**
 * @brief Connect to MIH server (for DLM clients) - 连接到MIH服务器（用于DLM客户端）
 * @param socket_path Path to Unix domain socket - Unix域套接字路径
 * @param retry_count Number of connection retries - 连接重试次数
 * @param retry_delay_sec Delay between retries - 重试之间的延迟
 * @return Socket FD on success, -1 on failure - 成功返回套接字FD，失败返回-1
 */
int mih_transport_connect(const char *socket_path,             // 连接到MIH服务器函数
                         int retry_count,                      // 重试次数参数
                         int retry_delay_sec);                 // 重试延迟参数

/**
 * @brief Create and bind MIH server socket (for CM Core) - 创建并绑定MIH服务器套接字（用于CM核心）
 * @param socket_path Path to Unix domain socket - Unix域套接字路径
 * @return Socket FD on success, -1 on failure - 成功返回套接字FD，失败返回-1
 */
int mih_transport_create_server(const char *socket_path);     // 创建MIH服务器套接字函数

/**
 * @brief Get next transaction ID (auto-increment) - 获取下一个事务ID（自动递增）
 * @return New transaction ID - 新事务ID
 */
uint32_t mih_transport_next_transaction_id(void);              // 获取下一个事务ID函数

/*===========================================================================
 * 数据报模式函数 (SOCK_DGRAM) - 用于 DLM 原型
 * 使用简化的 2 字节类型码前缀
 *===========================================================================*/

/**
 * @brief 创建数据报模式 MIHF 服务器套接字
 * @param socket_path Unix 域套接字路径
 * @return 成功返回套接字 FD，失败返回 -1
 */
int mih_transport_create_dgram_server(const char *socket_path);

/**
 * @brief 通过数据报套接字发送 MIH 消息 (带 2 字节类型码)
 * @param sockfd 套接字文件描述符
 * @param dest_addr 目标地址
 * @param addr_len 地址长度
 * @param primitive_type MIH 原语类型
 * @param payload 有效载荷
 * @param payload_len 有效载荷长度
 * @return 成功返回 0，失败返回 -1
 */
int mih_transport_sendto(int sockfd,
                        const struct sockaddr *dest_addr,
                        socklen_t addr_len,
                        uint16_t primitive_type,
                        const void *payload,
                        size_t payload_len);

/**
 * @brief 从数据报套接字接收 MIH 消息 (解析 2 字节类型码)
 * @param sockfd 套接字文件描述符
 * @param from_addr 源地址 (输出)
 * @param addr_len 地址长度 (输入/输出)
 * @param primitive_type 解析出的原语类型 (输出)
 * @param payload_buf 有效载荷缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回有效载荷长度，失败返回 -1
 */
int mih_transport_recvfrom(int sockfd,
                          struct sockaddr *from_addr,
                          socklen_t *addr_len,
                          uint16_t *primitive_type,
                          void *payload_buf,
                          size_t buf_size);

#endif /* MIH_TRANSPORT_H */  // 结束头文件保护宏
