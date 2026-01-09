/*
 * udp_test.c
 * UDP 业务测试模块实现
 +
 * 说明 (如何用它做端到端数据面测试):
 * - 目标：从客户端发送 UDP 数据包，确保服务端（MAGIC）数据平面
 *   将该流量按会话分配的链路（eg. ens37/ens33/ens38）转发出去。
 *
 * 基本流程（手动测试）:
 * 1) 在服务器上确认 dataplane 已为会话添加 ip rule（或先用 MCCR 建立会话）：
 *      sudo ip rule list | grep <client_ip>
 *
 * 2) 在被分配的链路接口上观察外发包（在服务器或链路端口所在主机）：
 *      sudo tcpdump -n -i <link-iface> host <client_ip> and udp -A
 *
 * 3) 在客户端运行本工具发送 UDP 包（必须绑定源 IP 为会话中登记的 client_ip）：
 *    - 指定源 IP:
 *        udp_test -s 192.168.126.5 192.168.126.1 5000 "test-message" 3
 *      这里 192.168.126.5 是客户端源 IP（必须是会话配置的 SourceIP），
 *      192.168.126.1 是服务器 ingress IP，5000 是目标 UDP 端口。
 *
 *    - 若不需要绑定源 IP（不常见），可直接发送：
 *        udp_test 192.168.126.1 5000 "hello" 3
 *
 * 4) 如果绑定源 IP 时报 "Cannot assign requested address"，请先在客户端临时添加该 IP 到接口：
 *      sudo ip addr add 192.168.126.5/32 dev <client-iface>
 *    发送完成后移除：
 *      sudo ip addr del 192.168.126.5/32 dev <client-iface>
 *
 * 5) 验证：在链路接口上应看到来自 client_ip 的 UDP 包并且 payload 包含工具打印的内容。
 *
 * 常用示例：
 *  - 发送 3 个包，绑定源 IP:
 *      udp_test -s 192.168.126.5 192.168.126.1 5000 "MAGIC-UDP-TEST" 3
 *  - 使用默认端口(5000)发送一次:
 *      udp_test 192.168.126.1 "ping-from-client"
 *  - Echo 测试（等待回应，测 RTT）:
 *      udp_test echo 192.168.126.1 5000 3 64
 *
 * 注意事项：
 * - 一定要绑定到正确的 source IP（和会话/配置一致），否则服务器的 ip rule 不会匹配，
 *   流量可能被默认 DROP 或走其他路由表。
 * - 本文件不改变网络配置，仅发送 UDP 包；若需自动化完整 MCCR/STR 流程，请使用上层测试脚本。
 */

#include "udp_test.h"
#include "cli_interface.h"
#include "magic_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

/* 获取当前时间（毫秒） */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * 检查字符串是否是有效的端口号 (1-65535)
 */
static int is_valid_port(const char* str) {
    if (!str || !*str) return 0;
    char* endptr;
    long val = strtol(str, &endptr, 10);
    /* 必须完全是数字，且在有效端口范围内 */
    return (*endptr == '\0' && val > 0 && val <= 65535);
}

/**
 * UDP 测试命令处理函数
 * 用法: udp_test [ip] [port] [message] [count]
 *       udp_test [ip] [message] [count]        -- 如果第二参数不是数字，视为消息
 *       udp_test -s [源IP] [目标IP] [port] [message] [count]  -- 指定源IP
 *       udp_test echo [ip] [port] [count] [size]
 */
int cmd_udp_test(int argc, char **argv)
{
    if (argc < 2) {
        cli_info("用法: udp_test <目标IP> [目标端口] [消息] [次数]");
        cli_info("      udp_test <目标IP> <消息> [次数]  (使用默认端口5000)");
        cli_info("      udp_test -s <源IP>:<源端口> <目标IP>:<目标端口> [消息] [次数]");
        cli_info("      udp_test echo <目标IP> [端口] [次数] [大小]");
        cli_info("");
        cli_info("示例:");
        cli_info("  udp_test 192.168.1.100           - 发送到默认端口5000");
        cli_info("  udp_test 192.168.1.100 hello     - 发送消息到默认端口");
        cli_info("  udp_test 192.168.1.100 8080 \"Hello World\" 10");
        cli_info("  udp_test -s 192.168.126.5:80 10.2.2.8:5880 \"test\" 3");
        cli_info("  udp_test -s 192.168.126.5 10.2.2.8:5000 \"test\" 5  (源端口随机)");
        cli_info("  udp_test echo 192.168.1.100 7 10 128");
        return 0;
    }
    
    /* 检查是否指定源 IP (-s 参数) */
    if (strcmp(argv[1], "-s") == 0) {
        if (argc < 4) {
            cli_error("用法: udp_test -s <源IP[:源端口]> <目标IP[:目标端口]> [消息] [次数]");
            return -1;
        }
        
        /* 解析源地址 IP:PORT */
        char src_ip[64] = {0};
        uint16_t src_port = 0;  /* 0=随机 */
        char* colon = strchr(argv[2], ':');
        if (colon) {
            size_t ip_len = colon - argv[2];
            strncpy(src_ip, argv[2], ip_len < 63 ? ip_len : 63);
            src_port = (uint16_t)atoi(colon + 1);
        } else {
            strncpy(src_ip, argv[2], 63);
        }
        
        /* 解析目标地址 IP:PORT */
        char dest_ip[64] = {0};
        uint16_t dest_port = UDP_TEST_DEFAULT_PORT;
        colon = strchr(argv[3], ':');
        if (colon) {
            size_t ip_len = colon - argv[3];
            strncpy(dest_ip, argv[3], ip_len < 63 ? ip_len : 63);
            dest_port = (uint16_t)atoi(colon + 1);
        } else {
            strncpy(dest_ip, argv[3], 63);
        }
        
        const char* message = "MAGIC-UDP-TEST";
        int count = UDP_TEST_DEFAULT_COUNT;
        
        if (argc > 4) {
            message = argv[4];
            count = (argc > 5) ? atoi(argv[5]) : UDP_TEST_DEFAULT_COUNT;
        }
        
        return udp_test_send_from(src_ip, src_port, dest_ip, dest_port, message, count);
    }
    
    /* 检查是否是 echo 模式 */
    if (strcmp(argv[1], "echo") == 0) {
        if (argc < 3) {
            cli_error("echo 模式需要指定目标 IP");
            return -1;
        }
        
        const char* dest_ip = argv[2];
        uint16_t port = (argc > 3) ? (uint16_t)atoi(argv[3]) : 7;  /* 默认 echo 端口 */
        int count = (argc > 4) ? atoi(argv[4]) : UDP_TEST_DEFAULT_COUNT;
        int size = (argc > 5) ? atoi(argv[5]) : UDP_TEST_DEFAULT_SIZE;
        
        udp_test_result_t result;
        return udp_test_echo(dest_ip, port, count, size, &result);
    }
    
    /* 普通发送模式 - 智能解析参数 */
    const char* dest_ip = argv[1];
    uint16_t port = UDP_TEST_DEFAULT_PORT;
    const char* message = "MAGIC-UDP-TEST";
    int count = UDP_TEST_DEFAULT_COUNT;
    
    if (argc > 2) {
        if (is_valid_port(argv[2])) {
            /* argv[2] 是端口号 */
            port = (uint16_t)atoi(argv[2]);
            message = (argc > 3) ? argv[3] : "MAGIC-UDP-TEST";
            count = (argc > 4) ? atoi(argv[4]) : UDP_TEST_DEFAULT_COUNT;
        } else {
            /* argv[2] 不是端口，视为消息内容 */
            message = argv[2];
            count = (argc > 3) ? atoi(argv[3]) : UDP_TEST_DEFAULT_COUNT;
        }
    }
    
    udp_test_result_t result;
    return udp_test_send(dest_ip, port, message, count, &result);
}

int udp_test_send(const char* dest_ip, uint16_t dest_port, 
                  const char* message, int count,
                  udp_test_result_t* result)
{
    int sock;
    struct sockaddr_in dest_addr;
    
    if (!dest_ip || !message) {
        cli_error("参数无效");
        return -1;
    }
    
    /* 创建 UDP 套接字 */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cli_error("创建 UDP 套接字失败: %s", strerror(errno));
        return -1;
    }
    
    /* 配置目标地址 */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) {
        cli_error("无效的目标 IP 地址: %s", dest_ip);
        close(sock);
        return -1;
    }
    
    /* 初始化结果 */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    size_t msg_len = strlen(message);
    
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    cli_info("UDP 测试: %s:%u", dest_ip, dest_port);
    cli_info("消息内容: \"%s\" (%zu 字节)", message, msg_len);
    cli_info("发送次数: %d", count);
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    for (int i = 0; i < count; i++) {
        /* 构造带序号的消息 */
        char buf[1024];
        snprintf(buf, sizeof(buf), "[%d] %s", i + 1, message);
        
        ssize_t sent = sendto(sock, buf, strlen(buf), 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent < 0) {
            cli_error("  [%d] 发送失败: %s", i + 1, strerror(errno));
        } else {
            cli_success("  [%d] 发送成功: %zd 字节 → %s:%u", 
                       i + 1, sent, dest_ip, dest_port);
            if (result) {
                result->packets_sent++;
                result->bytes_sent += sent;
            }
        }
        
        /* 发送间隔 */
        if (i < count - 1) {
            usleep(UDP_TEST_INTERVAL_MS * 1000);
        }
    }
    
    close(sock);
    
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    if (result) {
        cli_info("统计: 发送 %u 包, %u 字节", 
                result->packets_sent, result->bytes_sent);
    }
    
    return 0;
}

int udp_test_echo(const char* dest_ip, uint16_t dest_port,
                  int count, int size, udp_test_result_t* result)
{
    int sock;
    struct sockaddr_in dest_addr;
    char* send_buf = NULL;
    char recv_buf[2048];
    
    if (!dest_ip) {
        cli_error("目标 IP 无效");
        return -1;
    }
    
    /* 创建 UDP 套接字 */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cli_error("创建 UDP 套接字失败: %s", strerror(errno));
        return -1;
    }
    
    /* 设置接收超时 */
    struct timeval tv;
    tv.tv_sec = 2;  /* 2 秒超时 */
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* 配置目标地址 */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) {
        cli_error("无效的目标 IP 地址: %s", dest_ip);
        close(sock);
        return -1;
    }
    
    /* 分配发送缓冲区 */
    send_buf = malloc(size + 32);
    if (!send_buf) {
        cli_error("内存分配失败");
        close(sock);
        return -1;
    }
    
    /* 初始化结果 */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->min_rtt_ms = 999999.0;
    }
    
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    cli_info("UDP Echo 测试: %s:%u", dest_ip, dest_port);
    cli_info("数据大小: %d 字节, 发送次数: %d", size, count);
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    double total_rtt = 0;
    
    for (int i = 0; i < count; i++) {
        /* 构造测试数据 */
        snprintf(send_buf, size + 32, "MAGIC-ECHO-SEQ=%d-", i + 1);
        size_t prefix_len = strlen(send_buf);
        memset(send_buf + prefix_len, 'X', size - prefix_len);
        send_buf[size] = '\0';
        
        double start_time = get_time_ms();
        
        ssize_t sent = sendto(sock, send_buf, size, 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent < 0) {
            cli_error("  [%d] 发送失败: %s", i + 1, strerror(errno));
            continue;
        }
        
        if (result) {
            result->packets_sent++;
            result->bytes_sent += sent;
        }
        
        /* 等待回复 */
        socklen_t addr_len = sizeof(dest_addr);
        ssize_t recvd = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0,
                                  (struct sockaddr*)&dest_addr, &addr_len);
        
        double end_time = get_time_ms();
        double rtt = end_time - start_time;
        
        if (recvd > 0) {
            recv_buf[recvd] = '\0';
            cli_success("  [%d] 回复: %zd 字节, RTT=%.2f ms", i + 1, recvd, rtt);
            
            if (result) {
                result->packets_recv++;
                total_rtt += rtt;
                if (rtt < result->min_rtt_ms) result->min_rtt_ms = rtt;
                if (rtt > result->max_rtt_ms) result->max_rtt_ms = rtt;
            }
        } else if (recvd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cli_warn("  [%d] 超时 (>2000ms)", i + 1);
            } else {
                cli_error("  [%d] 接收失败: %s", i + 1, strerror(errno));
            }
        }
        
        /* 发送间隔 */
        if (i < count - 1) {
            usleep(UDP_TEST_INTERVAL_MS * 1000);
        }
    }
    
    free(send_buf);
    close(sock);
    
    /* 计算统计 */
    if (result && result->packets_recv > 0) {
        result->avg_rtt_ms = total_rtt / result->packets_recv;
    }
    
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    if (result) {
        udp_test_print_result(result);
    }
    
    return 0;
}

int udp_test_send_from(const char* src_ip, uint16_t src_port,
                       const char* dest_ip, uint16_t dest_port,
                       const char* message, int count)
{
    int sock;
    struct sockaddr_in src_addr, dest_addr;
    
    if (!src_ip || !dest_ip || !message) {
        cli_error("参数无效");
        return -1;
    }
    
    /* 创建 UDP 套接字 */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cli_error("创建 UDP 套接字失败: %s", strerror(errno));
        return -1;
    }
    
    /* 绑定到指定源 IP 和源端口 */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(src_port);  /* 0=随机端口 */
    if (inet_pton(AF_INET, src_ip, &src_addr.sin_addr) <= 0) {
        cli_error("无效的源 IP 地址: %s", src_ip);
        close(sock);
        return -1;
    }
    
    if (bind(sock, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        cli_error("绑定源地址失败 %s:%u: %s", src_ip, src_port, strerror(errno));
        close(sock);
        return -1;
    }
    
    /* 配置目标地址 */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr) <= 0) {
        cli_error("无效的目标 IP 地址: %s", dest_ip);
        close(sock);
        return -1;
    }
    
    /* 获取实际绑定的源端口 */
    struct sockaddr_in actual_addr;
    socklen_t addr_len = sizeof(actual_addr);
    getsockname(sock, (struct sockaddr*)&actual_addr, &addr_len);
    uint16_t actual_src_port = ntohs(actual_addr.sin_port);
    
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    cli_info("UDP 测试 (绑定源地址)");
    cli_info("  源:   %s:%u %s", src_ip, actual_src_port, 
             src_port == 0 ? "(系统分配)" : "");
    cli_info("  目标: %s:%u", dest_ip, dest_port);
    cli_info("  消息: \"%s\"", message);
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    for (int i = 0; i < count; i++) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "[%d] %s", i + 1, message);
        
        ssize_t sent = sendto(sock, buf, strlen(buf), 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent < 0) {
            cli_error("  [%d] 发送失败: %s", i + 1, strerror(errno));
        } else {
            cli_success("  [%d] 发送成功: %zd 字节 (%s:%u → %s:%u)", 
                       i + 1, sent, src_ip, actual_src_port, dest_ip, dest_port);
        }
        
        if (i < count - 1) {
            usleep(UDP_TEST_INTERVAL_MS * 1000);
        }
    }
    
    close(sock);
    cli_info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    return 0;
}

void udp_test_print_result(const udp_test_result_t* result)
{
    if (!result) return;
    
    cli_info("测试统计:");
    cli_info("  发送包数: %u", result->packets_sent);
    cli_info("  接收包数: %u", result->packets_recv);
    cli_info("  发送字节: %u", result->bytes_sent);
    
    if (result->packets_recv > 0) {
        cli_info("  丢包率:   %.1f%%", 
                100.0 * (result->packets_sent - result->packets_recv) / result->packets_sent);
        cli_info("  RTT 最小: %.2f ms", result->min_rtt_ms);
        cli_info("  RTT 平均: %.2f ms", result->avg_rtt_ms);
        cli_info("  RTT 最大: %.2f ms", result->max_rtt_ms);
    } else {
        cli_warn("  丢包率: 100%% (无回复)");
    }
}
