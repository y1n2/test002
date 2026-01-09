/**
 * @file magic_tft_validator.h
 * @brief TFT 和 NAPT 白名单验证器头文件
 * @description 按照 ARINC 839 §1.2.2.2 要求实现白名单验证
 *
 * 规范要求（第92页）：
 * "MAGIC shall compare TFT strings provided by a client via the Client
 *  Interface Handler with predefined white lists on a content basis by
 *  interpreting each value against the according profile entries."
 *
 * "MAGIC shall reject the request with an error message in its answer
 *  if one of the values provided in the TFTs is outside the predefined
 *  white list range."
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2025-12-25
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MAGIC_TFT_VALIDATOR_H
#define MAGIC_TFT_VALIDATOR_H

#include "magic_config.h"
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/
#define MAX_TFT_LEN 512       /* TFT 字符串最大长度 */
#define MAX_NAPT_LEN 256      /* NAPT 规则字符串最大长度 */
#define MAX_ERROR_MSG_LEN 512 /* 错误消息最大长度 */

/*===========================================================================
 * IP/端口范围结构体
 *===========================================================================*/

/**
 * @brief IP 地址范围结构体
 * 支持 CIDR 格式（192.168.1.0/24）和范围格式（192.168.1.1-192.168.1.254）
 */
typedef struct {
  uint32_t start_ip; ///< 起始 IP（网络字节序）。
  uint32_t end_ip;   ///< 结束 IP（网络字节序）。
  bool is_valid;     ///< 范围是否有效。
} IPRange;

/**
 * @brief 端口范围结构体
 * 支持单端口（80）、范围（5000-6000）、列表（80,443,8080）
 */
typedef struct {
  uint16_t start_port; ///< 起始端口。
  uint16_t end_port;   ///< 结束端口。
  bool is_valid;       ///< 范围是否有效。
} PortRange;

/*===========================================================================
 * TFT 解析结构体
 *===========================================================================*/

/**
 * @brief TFT 规则解析结果
 * 基于 3GPP TS 23.060 格式：
 * permit out ip from <source_ip> to <dest_ip> <protocol> <dest_port>
 */
typedef struct {
  /* 源地址 */
  IPRange src_ip; ///< 源 IP 范围。

  /* 目标地址 */
  IPRange dst_ip; ///< 目标 IP 范围。

  /* 源端口 (ARINC 839 扩展) */
  PortRange src_port; ///< 源端口范围。

  /* 目标端口 */
  PortRange dst_port; ///< 目标端口范围。

  /* 协议 */
  uint8_t protocol;  ///< 协议号: 6=TCP, 17=UDP, 0=any。
  bool has_protocol; ///< 是否指定了协议。

  /* 方向 */
  bool is_outbound; ///< true=out(toGround), false=in(toAircraft)。

  /* 解析状态 */
  bool is_valid; ///< 解析是否成功。
} TFTRule;

/*===========================================================================
 * 核心验证 API
 *===========================================================================*/

/**
 * @brief 验证 TFT 字符串是否在白名单范围内
 *
 * @param tft_string TFT 字符串（3GPP TS 23.060 格式）
 * @param whitelist 白名单配置（来自 ClientProfile.traffic）
 * @param error_msg 错误消息输出缓冲区（失败时填充）
 * @param error_msg_len 错误消息缓冲区大小
 * @return 0=验证通过, -1=验证失败（超出白名单）, -2=解析错误
 *
 * @note ARINC 839 §1.2.2.2:
 *       "The aim of the examples is to show that a 'normal' string match
 *        validation is not sufficient enough to grant or to reject a
 *        client request."
 */
int tft_validate_against_whitelist(const char *tft_string,
                                   const TrafficSecurityConfig *whitelist,
                                   char *error_msg, size_t error_msg_len);

/*===========================================================================
 * 辅助解析函数
 *===========================================================================*/

/**
 * @brief 解析 TFT 字符串为结构化数据
 *
 * @param tft_string TFT 字符串
 * @param rule 输出的解析结果
 * @return 0=成功, -1=失败
 *
 * @example
 *   输入: "permit out ip from 192.168.0.3 to 10.16.0.5:80"
 *   输出: rule.src_ip = 192.168.0.3/32
 *         rule.dst_ip = 10.16.0.5/32
 *         rule.dst_port = 80-80
 *         rule.protocol = 0 (any)
 */
int tft_parse_rule(const char *tft_string, TFTRule *rule);

/**
 * @brief 解析 IP 范围字符串
 *
 * @param range_str IP 范围字符串（支持 CIDR 和范围格式）
 * @param range 输出的 IP 范围结构体
 * @return 0=成功, -1=失败
 *
 * @example
 *   支持格式:
 *   - CIDR: "192.168.1.0/24"
 *   - 范围: "192.168.1.1-192.168.1.254"
 *   - 单IP: "192.168.1.10"
 */
int parse_ip_range(const char *range_str, IPRange *range);

/**
 * @brief 解析端口范围字符串
 *
 * @param range_str 端口范围字符串
 * @param ranges 输出的端口范围数组（支持多段）
 * @param max_ranges 数组最大容量
 * @return 实际解析的范围数量, -1=失败
 *
 * @example
 *   支持格式:
 *   - 单端口: "80"
 *   - 范围: "5000-6000"
 *   - 列表: "80,443,8080"
 *   - 混合: "80,443,5000-6000"
 */
int parse_port_range(const char *range_str, PortRange *ranges, int max_ranges);

/**
 * @brief 检查 IP 是否在范围内
 *
 * @param ip 要检查的 IP（网络字节序）
 * @param range IP 范围
 * @return true=在范围内, false=不在范围内
 */
bool ip_in_range(uint32_t ip, const IPRange *range);

/**
 * @brief 检查端口是否在范围数组内
 *
 * @param port 要检查的端口
 * @param ranges 端口范围数组
 * @param num_ranges 范围数量
 * @return true=在范围内, false=不在范围内
 */
bool port_in_ranges(uint16_t port, const PortRange *ranges, int num_ranges);

/**
 * @brief 将协议名转换为协议号
 *
 * @param protocol_name 协议名称（"TCP", "UDP", "ICMP"等）
 * @return 协议号（6=TCP, 17=UDP, 0=未知）
 */
uint8_t protocol_name_to_number(const char *protocol_name);

/*===========================================================================
 * 调试辅助函数
 *===========================================================================*/

/**
 * @brief 打印 TFT 规则的详细信息（用于调试）
 *
 * @param rule TFT 规则
 * @param prefix 日志前缀
 */
void tft_rule_dump(const TFTRule *rule, const char *prefix);

/**
 * @brief 打印 IP 范围（点分十进制格式）
 *
 * @param range IP 范围
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 */
void ip_range_to_string(const IPRange *range, char *buffer, size_t buffer_size);

#endif /* MAGIC_TFT_VALIDATOR_H */
