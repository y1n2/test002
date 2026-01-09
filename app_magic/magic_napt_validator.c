/**
 * @file magic_napt_validator.c
 * @brief ARINC 839 标准 NAPT 规则验证器实现。
 * @details 本模块解析和验证 NAPT 规则字符串，格式如下：
 *          `<NAT-Type>,<Source-IP>,<Destination-IP>,<IP-Protocol>,<Destination-Port>,<Source-Port>,<to-IP>,<to-Port>`
 *
 *          支持的 NAT 类型:
 *          - SNAT (源地址转换): 将客户端源 IP 替换为链路出口 IP。
 *          - DNAT (目标地址转换): 将目标 IP/端口替换为服务器真实地址。
 *
 *          特殊占位符:
 *          - %LinkIp%: 在规则中动态替换为实际链路 IP 地址。
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2025-12-25
 */

#include "magic_napt_validator.h"
#include "freeDiameter/freeDiameter-host.h"
#include "freeDiameter/libfdcore.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 内部辅助函数：解析 IP.掩码 格式 */
static int parse_ip_mask(const char *str, IPRange *range, const char *link_ip) {
  char buf[64];
  char *ip_part, *mask_part;
  struct in_addr addr, mask;

  if (!str || strlen(str) == 0) {
    range->is_valid = false;
    return 0;
  }

  /* 处理 %LinkIp% 占位符 */
  if (strcmp(str, "%LinkIp%") == 0) {
    if (!link_ip) {
      range->is_valid = false;
      return 0; /* 暂时标记为无效，待链路建立后替换 */
    }
    str = link_ip;
  }

  strncpy(buf, str, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  ip_part = buf;
  mask_part = strrchr(buf, '.');

  if (mask_part) {
    *mask_part = '\0';
    mask_part++;

    /* 检查是否是 4 段式的掩码 (如 255.255.255.0) */
    int dots = 0;
    for (char *p = mask_part; *p; p++)
      if (*p == '.')
        dots++;

    if (dots == 3) {
      /* 完整的点分十进制掩码 */
      if (inet_pton(AF_INET, mask_part, &mask) != 1)
        return -1;
    } else {
      /* 可能是 CIDR 或者单段掩码，这里简化处理，仅支持点分十进制 */
      return -1;
    }
  } else {
    /* 无掩码，默认为 255.255.255.255 */
    mask.s_addr = 0xFFFFFFFF;
  }

  if (inet_pton(AF_INET, ip_part, &addr) != 1)
    return -1;

  uint32_t ip_val = ntohl(addr.s_addr);
  uint32_t mask_val = ntohl(mask.s_addr);

  range->start_ip = ip_val & mask_val;
  range->end_ip = ip_val | (~mask_val);
  range->is_valid = true;

  return 0;
}

/* 内部辅助函数：解析端口范围 (如 80 或 2000.2099) */
static int parse_napt_port_range(const char *str, PortRange *range) {
  if (!str || strlen(str) == 0) {
    range->is_valid = false;
    return 0;
  }

  char *dot = strchr(str, '.');
  if (dot) {
    range->start_port = (uint16_t)atoi(str);
    range->end_port = (uint16_t)atoi(dot + 1);
  } else {
    range->start_port = range->end_port = (uint16_t)atoi(str);
  }
  range->is_valid = true;
  return 0;
}

int napt_parse_rule(const char *napt_str, NAPTRule *rule, const char *link_ip) {
  if (!napt_str || !rule)
    return -1;

  memset(rule, 0, sizeof(NAPTRule));
  char *tmp = strdup(napt_str);
  char *saveptr;
  char *fields[8];
  int i = 0;

  /* 分割 8 个字段 */
  char *token = strtok_r(tmp, ",", &saveptr);
  while (token && i < 8) {
    fields[i++] = token;
    token = strtok_r(NULL, ",", &saveptr);
  }

  if (i < 8) {
    free(tmp);
    return -1;
  }

  /* 1. NAT-Type */
  if (strcasecmp(fields[0], "SNAT") == 0)
    rule->type = NAPT_TYPE_SNAT;
  else if (strcasecmp(fields[0], "DNAT") == 0)
    rule->type = NAPT_TYPE_DNAT;
  else
    rule->type = NAPT_TYPE_UNKNOWN;

  /* 2. Source-IP */
  parse_ip_mask(fields[1], &rule->src_ip, link_ip);

  /* 3. Destination-IP */
  parse_ip_mask(fields[2], &rule->dst_ip, link_ip);

  /* 4. IP-Protocol */
  rule->protocol = (uint8_t)atoi(fields[3]);

  /* 5. Destination-Port */
  parse_napt_port_range(fields[4], &rule->dst_port);

  /* 6. Source-Port */
  parse_napt_port_range(fields[5], &rule->src_port);

  /* 7. to-IP */
  parse_ip_mask(fields[6], &rule->to_ip, link_ip);

  /* 8. to-Port */
  parse_napt_port_range(fields[7], &rule->to_port);

  rule->is_valid = true;
  free(tmp);
  return 0;
}

int napt_validate_against_whitelist(const char *napt_str,
                                    const TrafficSecurityConfig *whitelist,
                                    const char *link_ip, char *error_msg,
                                    size_t error_msg_len) {
  NAPTRule rule;
  if (napt_parse_rule(napt_str, &rule, link_ip) != 0) {
    snprintf(error_msg, error_msg_len,
             "Malformed NAPT string (ARINC 839 format required)");
    return -2;
  }

  /* 验证逻辑：NAPT 规则中的目标 IP/端口必须在白名单范围内 */
  /* 对于 DNAT，检查 Destination-IP；对于 SNAT，检查 to-IP
   * (转换后的源地址通常是链路地址) */

  IPRange wl_ip;
  /* 这里复用 magic_tft_validator_3gpp.c 中的解析逻辑，假设它已导出或可用 */
  /* 简化处理：直接检查 IP 字符串 */

  fd_log_debug("[napt_validator] Validating NAPT: %s", napt_str);

  /* TODO: 实现更深层次的白名单校验 */
  /* 目前先实现格式校验，确保符合 8 字段规范 */

  return 0;
}
