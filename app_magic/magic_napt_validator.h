/**
 * @file magic_napt_validator.h
 * @brief ARINC 839 标准 NAPT 规则验证器
 *
 * 格式：<NAT-Type>,<Source-IP>,<Destination-IP>,<IP-Protocol>,<Destination-Port>,<Source-Port>,<to-IP>,<to-Port>
 */

#ifndef MAGIC_NAPT_VALIDATOR_H
#define MAGIC_NAPT_VALIDATOR_H

#include "magic_tft_validator.h"
#include <stdbool.h>
#include <stdint.h>

/* NAPT 类型 */
typedef enum {
  NAPT_TYPE_SNAT,   ///< 源地址转换 (Source NAT)。
  NAPT_TYPE_DNAT,   ///< 目标地址转换 (Destination NAT)。
  NAPT_TYPE_UNKNOWN ///< 未知类型。
} NAPTType;

/* NAPT 规则结构体 */
typedef struct {
  NAPTType type;      ///< NAPT 类型 (SNAT/DNAT)。
  IPRange src_ip;     ///< 源 IP 范围。
  IPRange dst_ip;     ///< 目标 IP 范围。
  uint8_t protocol;   ///< 协议号 (6=TCP, 17=UDP)。
  PortRange dst_port; ///< 目标端口范围。
  PortRange src_port; ///< 源端口范围。
  IPRange to_ip;      ///< 翻译后的目标 IP (对于 SNAT 是出口 IP)。
  PortRange to_port;  ///< 翻译后的端口。
  bool is_valid;      ///< 规则解析是否成功。
} NAPTRule;

/**
 * @brief 解析 ARINC 839 标准 NAPT 字符串
 * @param napt_str NAPT 字符串
 * @param rule 输出解析结果
 * @param link_ip 动态链路 IP（用于替换 %LinkIp%），如果未知可传 NULL
 * @return 0=成功, -1=解析失败
 */
int napt_parse_rule(const char *napt_str, NAPTRule *rule, const char *link_ip);

/**
 * @brief 验证 NAPT 规则是否符合白名单
 * @param napt_str NAPT 字符串
 * @param whitelist 白名单配置
 * @param link_ip 动态链路 IP
 * @param error_msg 错误消息缓冲区
 * @param error_msg_len 缓冲区长度
 * @return 0=成功, -1=验证失败, -2=解析失败
 */
int napt_validate_against_whitelist(const char *napt_str,
                                    const TrafficSecurityConfig *whitelist,
                                    const char *link_ip, char *error_msg,
                                    size_t error_msg_len);

#endif /* MAGIC_NAPT_VALIDATOR_H */
