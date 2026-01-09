/*
 * udp_test.h
 * UDP 业务测试模块头文件
 * 
 * 用于测试 MCCR 分配的链路是否正常工作
 */

#ifndef UDP_TEST_H
#define UDP_TEST_H

#include <stdint.h>
#include <stdbool.h>

/* UDP 测试默认参数 */
#define UDP_TEST_DEFAULT_PORT   5000    /* 默认目标端口 */
#define UDP_TEST_DEFAULT_COUNT  5       /* 默认发送次数 */
#define UDP_TEST_DEFAULT_SIZE   64      /* 默认数据大小 */
#define UDP_TEST_INTERVAL_MS    1000    /* 发送间隔（毫秒）*/

/**
 * @brief UDP 测试结果结构
 */
typedef struct {
    uint32_t packets_sent;      /* 发送的包数 */
    uint32_t packets_recv;      /* 收到回复的包数 */
    uint32_t bytes_sent;        /* 发送的总字节数 */
    double avg_rtt_ms;          /* 平均往返时间 (ms) */
    double min_rtt_ms;          /* 最小往返时间 */
    double max_rtt_ms;          /* 最大往返时间 */
} udp_test_result_t;

/**
 * @brief 发送 UDP 测试数据到指定地址
 * 
 * @param dest_ip 目标 IP 地址
 * @param dest_port 目标端口
 * @param message 要发送的消息内容
 * @param count 发送次数
 * @param result 输出：测试结果
 * @return 0=成功, -1=失败
 */
int udp_test_send(const char* dest_ip, uint16_t dest_port, 
                  const char* message, int count,
                  udp_test_result_t* result);

/**
 * @brief 发送 UDP Echo 测试（发送并等待回复）
 * 
 * @param dest_ip 目标 IP 地址
 * @param dest_port 目标端口（通常是 echo 服务端口 7 或自定义）
 * @param count 发送次数
 * @param size 每个包的数据大小
 * @param result 输出：测试结果
 * @return 0=成功, -1=失败
 */
int udp_test_echo(const char* dest_ip, uint16_t dest_port,
                  int count, int size, udp_test_result_t* result);

/**
 * @brief 绑定到指定源 IP 和源端口发送 UDP 数据
 * 
 * @param src_ip 源 IP 地址（本机接口 IP）
 * @param src_port 源端口（0=系统自动分配）
 * @param dest_ip 目标 IP 地址
 * @param dest_port 目标端口
 * @param message 要发送的消息内容
 * @param count 发送次数
 * @return 0=成功, -1=失败
 */
int udp_test_send_from(const char* src_ip, uint16_t src_port,
                       const char* dest_ip, uint16_t dest_port,
                       const char* message, int count);

/**
 * @brief 打印 UDP 测试结果
 * 
 * @param result 测试结果
 */
void udp_test_print_result(const udp_test_result_t* result);

#endif /* UDP_TEST_H */
