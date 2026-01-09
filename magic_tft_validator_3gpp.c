/**
 * @file magic_tft_validator_3gpp.c
 * @brief TFT 白名单验证器 - 3GPP TS 23.060 格式实现
 * @description 按照 ARINC 839 §1.2.2.1-1.2.2.2 要求实现 TFT 解析和验证
 *
 * TFT 格式（ARINC 839 第92页）：
 * _iTFT=[<cid>,[<packet filter id>,<eval precedence>[,<source addr and mask>
 *        [,<dest addr and mask>[,<protocol number>[,<dest port range>
 *        [,<source port range>[,<ipsec spi>[,<tos and mask>
 *        [,<flow label>]]]]]]]]]]
 *
 * 示例：
 * _iTFT=,192.168.0.0.255.255.255.0,10.16.0.0.255.255.0.0,6,0.1023,0.65535,,,
 * 表示: 源IP=192.168.0.0/24, 目标IP=10.16.0.0/24, 协议=TCP(6), 
 *      目标端口=0-1023, 源端口=0-65535
 *
 * @author MAGIC System Development Team
 * @version 2.0
 * @date 2025-12-25
 * @copyright ARINC 839-2014 Compliant
 */

#include "magic_tft_validator.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

/*===========================================================================
 * IP/端口解析辅助函数
 *===========================================================================*/

/**
 * @brief 解析点分十进制IP和掩码为IPRange
 * @param ip_str IP地址字符串（格式：a.b.c.d）
 * @param mask_str 掩码字符串（格式：m1.m2.m3.m4）
 * @param range 输出的IP范围
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   ip_str="192.168.0.0", mask_str="255.255.255.0"
 *   -> start_ip=192.168.0.0, end_ip=192.168.0.255
 */
static int parse_ip_with_mask(const char *ip_str, const char *mask_str, IPRange *range) {
    if (!ip_str || !mask_str || !range) {
        return -1;
    }
    
    /* 解析IP地址 */
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, ip_str, &ip_addr) != 1) {
        fd_log_debug("[tft_validator] Invalid IP: %s", ip_str);
        return -1;
    }
    
    /* 解析掩码 */
    struct in_addr mask_addr;
    if (inet_pton(AF_INET, mask_str, &mask_addr) != 1) {
        fd_log_debug("[tft_validator] Invalid mask: %s", mask_str);
        return -1;
    }
    
    uint32_t ip = ntohl(ip_addr.s_addr);
    uint32_t mask = ntohl(mask_addr.s_addr);
    
    /* 计算网络范围 */
    range->start_ip = ip & mask;              /* 网络地址 */
    range->end_ip = ip | (~mask & 0xFFFFFFFF);  /* 广播地址 */
    range->is_valid = true;
    
    return 0;
}

/**
 * @brief 解析端口范围字符串（3GPP格式：startPort.endPort）
 * @param port_str 端口范围字符串（格式：start.end）
 * @param range 输出的端口范围
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   "0.1023" -> start_port=0, end_port=1023
 *   "80.80" -> start_port=80, end_port=80
 *   "0.65535" -> start_port=0, end_port=65535（任意端口）
 */
static int parse_port_range_3gpp(const char *port_str, PortRange *range) {
    if (!port_str || !range) {
        return -1;
    }
    
    /* 格式：start.end */
    int start, end;
    if (sscanf(port_str, "%d.%d", &start, &end) != 2) {
        fd_log_debug("[tft_validator] Invalid port range: %s", port_str);
        return -1;
    }
    
    if (start < 0 || start > 65535 || end < 0 || end > 65535 || start > end) {
        fd_log_debug("[tft_validator] Port range out of bounds: %s", port_str);
        return -1;
    }
    
    range->start_port = (uint16_t)start;
    range->end_port = (uint16_t)end;
    range->is_valid = true;
    
    return 0;
}

/**
 * @brief 检查IP是否在范围内（内部使用）
 */
static bool ip_range_contains(const IPRange *ip, const IPRange *whitelist) {
    if (!ip->is_valid || !whitelist->is_valid) {
        return false;
    }
    
    /* 检查请求的IP范围是否完全在白名单范围内 */
    return (ip->start_ip >= whitelist->start_ip && ip->end_ip <= whitelist->end_ip);
}

/**
 * @brief 检查端口是否在范围内（内部使用）
 */
static bool port_range_contains(const PortRange *port, const PortRange *whitelist) {
    if (!port->is_valid || !whitelist->is_valid) {
        return false;
    }
    
    /* 检查请求的端口范围是否完全在白名单范围内 */
    return (port->start_port >= whitelist->start_port && port->end_port <= whitelist->end_port);
}

/*===========================================================================
 * TFT 解析实现（3GPP TS 23.060格式）
 *===========================================================================*/

/**
 * @brief 解析3GPP格式的TFT字符串
 * @param tft_string TFT字符串
 * @param rule 输出的解析结果
 * @return 0=成功, -1=失败
 *
 * 格式：_iTFT=[cid,[pf_id,precedence[,src_ip.src_mask[,dst_ip.dst_mask
 *             [,protocol[,dst_port_range[,src_port_range[,...]]]]]]]
 *
 * 关键字段（从左到右）：
 * 字段0: CID（可选，通常为空）
 * 字段1: Packet Filter ID（可选）
 * 字段2: Evaluation Precedence（可选）
 * 字段3: Source Address.Mask（格式：a.b.c.d.m1.m2.m3.m4）
 * 字段4: Dest Address.Mask（格式：a.b.c.d.m1.m2.m3.m4）
 * 字段5: Protocol Number（6=TCP, 17=UDP）
 * 字段6: Dest Port Range（格式：start.end）
 * 字段7: Source Port Range（格式：start.end）
 */
int tft_parse_rule(const char *tft_string, TFTRule *rule) {
    if (!tft_string || !rule) {
        return -1;
    }
    
    memset(rule, 0, sizeof(*rule));
    rule->is_valid = false;
    
    char buffer[MAX_TFT_LEN];
    strncpy(buffer, tft_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* 去除前缀 "_iTFT=" 或 "+CGTFT="（如果存在）*/
    char *start = buffer;
    if (strncmp(start, "_iTFT=", 6) == 0) {
        start += 6;
        rule->is_outbound = true;  /* Aircraft-to-Ground */
    } else if (strncmp(start, "+CGTFT=", 7) == 0) {
        start += 7;
        rule->is_outbound = false;  /* Ground-to-Aircraft */
    }
    
    /* ★★★ 健壮的字段分割逻辑 (处理空字段) ★★★ */
    char *fields[15];  /* 最多15个字段 */
    int field_count = 0;
    char *p = start;
    
    /* 初始化字段数组 */
    for (int i = 0; i < 15; i++) fields[i] = "";
    
    if (*p != '\0') {
        fields[field_count++] = p;
        while (*p && field_count < 15) {
            if (*p == ',') {
                *p = '\0';
                fields[field_count++] = p + 1;
            }
            p++;
        }
    }
    
    fd_log_debug("[tft_validator] Parsed %d fields from TFT", field_count);
    
    /* 
     * 字段索引说明 (基于 strtok 修复后的索引):
     * fields[0]: CID (通常为空)
     * fields[1]: Packet Filter ID
     * fields[2]: Precedence
     * fields[3]: Source IP.Mask
     * fields[4]: Dest IP.Mask
     * fields[5]: Protocol
     * fields[6]: Dest Port Range
     * fields[7]: Source Port Range
     */

    /* 字段3: 源地址和掩码（格式：a.b.c.d.m1.m2.m3.m4）*/
    if (field_count > 3 && fields[3][0] != '\0') {
        char octets[8][16];
        int octet_count = 0;
        char *ip_p = fields[3];
        char *dot;
        
        while ((dot = strchr(ip_p, '.')) != NULL && octet_count < 8) {
            size_t len = dot - ip_p;
            if (len < 16) {
                strncpy(octets[octet_count], ip_p, len);
                octets[octet_count][len] = '\0';
                octet_count++;
                ip_p = dot + 1;
            } else break;
        }
        if (octet_count < 8 && strlen(ip_p) < 16) {
            strcpy(octets[octet_count++], ip_p);
        }
        
        if (octet_count == 8) {
            char ip_str[64], mask_str[64];
            snprintf(ip_str, sizeof(ip_str), "%s.%s.%s.%s", octets[0], octets[1], octets[2], octets[3]);
            snprintf(mask_str, sizeof(mask_str), "%s.%s.%s.%s", octets[4], octets[5], octets[6], octets[7]);
            
            if (parse_ip_with_mask(ip_str, mask_str, &rule->src_ip) != 0) {
                fd_log_debug("[tft_validator] Failed to parse source IP: %s", fields[3]);
            }
        }
    }
    
    /* 字段4: 目标地址和掩码 */
    if (field_count > 4 && fields[4][0] != '\0') {
        char octets[8][16];
        int octet_count = 0;
        char *ip_p = fields[4];
        char *dot;
        
        while ((dot = strchr(ip_p, '.')) != NULL && octet_count < 8) {
            size_t len = dot - ip_p;
            if (len < 16) {
                strncpy(octets[octet_count], ip_p, len);
                octets[octet_count][len] = '\0';
                octet_count++;
                ip_p = dot + 1;
            } else break;
        }
        if (octet_count < 8 && strlen(ip_p) < 16) {
            strcpy(octets[octet_count++], ip_p);
        }
        
        if (octet_count == 8) {
            char ip_str[64], mask_str[64];
            snprintf(ip_str, sizeof(ip_str), "%s.%s.%s.%s", octets[0], octets[1], octets[2], octets[3]);
            snprintf(mask_str, sizeof(mask_str), "%s.%s.%s.%s", octets[4], octets[5], octets[6], octets[7]);
            
            if (parse_ip_with_mask(ip_str, mask_str, &rule->dst_ip) != 0) {
                fd_log_debug("[tft_validator] Failed to parse dest IP: %s", fields[4]);
            }
        } else if (strcmp(fields[4], "0.0.0.0.0.0.0.0") == 0) {
            rule->dst_ip.start_ip = 0;
            rule->dst_ip.end_ip = 0xFFFFFFFF;
            rule->dst_ip.is_valid = true;
        }
    }
    
    /* 字段5: 协议号 */
    if (field_count > 5 && fields[5][0] != '\0') {
        rule->protocol = (uint8_t)atoi(fields[5]);
        rule->has_protocol = true;
    }
    
    /* 字段6: 目标端口范围 */
    if (field_count > 6 && fields[6][0] != '\0') {
        if (parse_port_range_3gpp(fields[6], &rule->dst_port) != 0) {
            rule->dst_port.start_port = 0;
            rule->dst_port.end_port = 65535;
            rule->dst_port.is_valid = true;
        }
    } else {
        rule->dst_port.start_port = 0;
        rule->dst_port.end_port = 65535;
        rule->dst_port.is_valid = true;
    }
    
    /* 字段7: 源端口范围 */
    if (field_count > 7 && fields[7][0] != '\0') {
        if (parse_port_range_3gpp(fields[7], &rule->src_port) != 0) {
            rule->src_port.start_port = 0;
            rule->src_port.end_port = 65535;
            rule->src_port.is_valid = true;
        }
    } else {
        rule->src_port.start_port = 0;
        rule->src_port.end_port = 65535;
        rule->src_port.is_valid = true;
    }
    
    rule->is_valid = true;
    return 0;
}

/*===========================================================================
 * 白名单验证核心逻辑
 *===========================================================================*/

/**
 * @brief 解析白名单配置中的IP范围
 * @param ip_range_str IP范围字符串（格式：10.2.2.0/24 或 10.2.2.0-10.2.2.255）
 * @param range 输出的IP范围
 * @return 0=成功, -1=失败
 */
static int parse_whitelist_ip_range(const char *ip_range_str, IPRange *range) {
    if (!ip_range_str || !range || ip_range_str[0] == '\0') {
        return -1;
    }
    
    char buffer[128];
    strncpy(buffer, ip_range_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* 检查CIDR格式（10.2.2.0/24）*/
    char *slash = strchr(buffer, '/');
    if (slash) {
        *slash = '\0';
        int prefix_len = atoi(slash + 1);
        
        struct in_addr ip_addr;
        if (inet_pton(AF_INET, buffer, &ip_addr) != 1) {
            return -1;
        }
        
        uint32_t ip = ntohl(ip_addr.s_addr);
        uint32_t mask = (0xFFFFFFFF << (32 - prefix_len)) & 0xFFFFFFFF;
        
        range->start_ip = ip & mask;
        range->end_ip = ip | (~mask & 0xFFFFFFFF);
        range->is_valid = true;
        return 0;
    }
    
    /* 检查范围格式（10.2.2.0-10.2.2.255）*/
    char *dash = strchr(buffer, '-');
    if (dash) {
        *dash = '\0';
        
        struct in_addr start_addr, end_addr;
        if (inet_pton(AF_INET, buffer, &start_addr) != 1 ||
            inet_pton(AF_INET, dash + 1, &end_addr) != 1) {
            return -1;
        }
        
        range->start_ip = ntohl(start_addr.s_addr);
        range->end_ip = ntohl(end_addr.s_addr);
        range->is_valid = true;
        return 0;
    }
    
    /* 单个IP地址 */
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, buffer, &ip_addr) == 1) {
        range->start_ip = ntohl(ip_addr.s_addr);
        range->end_ip = range->start_ip;
        range->is_valid = true;
        return 0;
    }
    
    return -1;
}

/**
 * @brief 解析白名单配置中的端口范围
 * @param port_range_str 端口范围字符串（格式：80,443,5000-6000）
 * @param range 输出的端口范围（使用最宽松的范围）
 * @return 0=成功, -1=失败
 */
static int parse_whitelist_port_range(const char *port_range_str, PortRange *range) {
    if (!port_range_str || !range || port_range_str[0] == '\0') {
        /* 默认：任意端口 */
        range->start_port = 0;
        range->end_port = 65535;
        range->is_valid = true;
        return 0;
    }
    
    /* 简化实现：查找最小和最大端口 */
    uint16_t min_port = 65535;
    uint16_t max_port = 0;
    
    char buffer[MAX_PORTLIST_LEN];
    strncpy(buffer, port_range_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char *token = strtok(buffer, ",");
    while (token) {
        /* 去除空格 */
        while (*token && isspace(*token)) token++;
        
        /* 检查范围格式（5000-6000）*/
        char *dash = strchr(token, '-');
        if (dash) {
            int start = atoi(token);
            int end = atoi(dash + 1);
            if (start < min_port) min_port = start;
            if (end > max_port) max_port = end;
        } else {
            /* 单个端口 */
            int port = atoi(token);
            if (port < min_port) min_port = port;
            if (port > max_port) max_port = port;
        }
        
        token = strtok(NULL, ",");
    }
    
    range->start_port = min_port;
    range->end_port = max_port;
    range->is_valid = true;
    return 0;
}

/**
 * @brief 验证TFT规则是否在白名单中（精确匹配）
 * 
 * 新的验证逻辑：检查客户端请求的 TFT 是否存在于服务器配置的白名单中
 * 采用精确字符串匹配，而非范围检查
 * 
 * @param tft_string 客户端请求的 TFT 规则字符串
 * @param whitelist 服务器配置的白名单（包含允许的 TFT 列表）
 * @param error_msg 错误信息输出缓冲区
 * @param error_msg_len 错误信息缓冲区长度
 * @return 0=通过验证, -1=不在白名单中, -2=参数错误
 */
int tft_validate_against_whitelist(
    const char *tft_string,
    const TrafficSecurityConfig *whitelist,
    char *error_msg,
    size_t error_msg_len)
{
    if (!tft_string || !whitelist || !error_msg) {
        return -2;
    }
    
    error_msg[0] = '\0';
    
    /* 去除输入 TFT 字符串的首尾空白 */
    char normalized_tft[512];
    const char* start = tft_string;
    while (*start && isspace(*start)) start++;
    
    strncpy(normalized_tft, start, sizeof(normalized_tft) - 1);
    normalized_tft[sizeof(normalized_tft) - 1] = '\0';
    
    char* end = normalized_tft + strlen(normalized_tft) - 1;
    while (end > normalized_tft && isspace(*end)) {
        *end = '\0';
        end--;
    }
    
    /* 检查白名单是否为空 */
    if (whitelist->num_allowed_tfts == 0) {
        snprintf(error_msg, error_msg_len, 
                 "No TFT whitelist configured for this client");
        fd_log_error("[tft_validator] %s", error_msg);
        return -1;
    }
    
    /* ★★★ 解析客户端请求的TFT规则 ★★★ */
    TFTRule request_rule;
    if (tft_parse_rule(normalized_tft, &request_rule) != 0 || !request_rule.is_valid) {
        snprintf(error_msg, error_msg_len, "Failed to parse requested TFT rule");
        fd_log_error("[tft_validator] %s: %s", error_msg, normalized_tft);
        return -2;
    }
    
    fd_log_debug("[tft_validator] Request TFT: src_port=%u-%u, dst_port=%u-%u, proto=%u",
                 request_rule.src_port.start_port, request_rule.src_port.end_port,
                 request_rule.dst_port.start_port, request_rule.dst_port.end_port,
                 request_rule.protocol);
    
    /* ★★★ 遍历白名单，进行语义范围验证 ★★★ */
    for (uint32_t i = 0; i < whitelist->num_allowed_tfts; i++) {
        TFTRule whitelist_rule;
        if (tft_parse_rule(whitelist->allowed_tfts[i], &whitelist_rule) != 0) {
            fd_log_debug("[tft_validator] Skipping invalid whitelist entry [%u]: %s", 
                        i+1, whitelist->allowed_tfts[i]);
            continue;
        }
        
        fd_log_debug("[tft_validator] Checking against whitelist[%u]: src_port=%u-%u, dst_port=%u-%u",
                     i+1, whitelist_rule.src_port.start_port, whitelist_rule.src_port.end_port,
                     whitelist_rule.dst_port.start_port, whitelist_rule.dst_port.end_port);
        
        /* 检查协议是否匹配 */
        if (whitelist_rule.has_protocol && whitelist_rule.protocol != 0) {
            if (request_rule.protocol != whitelist_rule.protocol) {
                fd_log_debug("[tft_validator]   ✗ Protocol mismatch: req=%u, wl=%u",
                            request_rule.protocol, whitelist_rule.protocol);
                continue;
            }
        }
        
        /* 检查源IP是否在白名单范围内 */
        if (whitelist_rule.src_ip.is_valid && request_rule.src_ip.is_valid) {
            if (request_rule.src_ip.start_ip < whitelist_rule.src_ip.start_ip ||
                request_rule.src_ip.end_ip > whitelist_rule.src_ip.end_ip) {
                fd_log_debug("[tft_validator]   ✗ Source IP out of range");
                continue;
            }
        }
        
        /* 检查目标IP是否在白名单范围内 */
        if (whitelist_rule.dst_ip.is_valid && request_rule.dst_ip.is_valid) {
            if (request_rule.dst_ip.start_ip < whitelist_rule.dst_ip.start_ip ||
                request_rule.dst_ip.end_ip > whitelist_rule.dst_ip.end_ip) {
                fd_log_debug("[tft_validator]   ✗ Destination IP out of range");
                continue;
            }
        }
        
        /* ★★★ 检查目标端口是否在白名单范围内 ★★★ */
        if (whitelist_rule.dst_port.is_valid && request_rule.dst_port.is_valid) {
            /* 跳过 0.65535 表示任意端口的白名单规则 */
            if (!(whitelist_rule.dst_port.start_port == 0 && whitelist_rule.dst_port.end_port == 65535)) {
                if (request_rule.dst_port.start_port < whitelist_rule.dst_port.start_port ||
                    request_rule.dst_port.end_port > whitelist_rule.dst_port.end_port) {
                    fd_log_debug("[tft_validator]   ✗ Dest port out of range: req=%u-%u, wl=%u-%u",
                                request_rule.dst_port.start_port, request_rule.dst_port.end_port,
                                whitelist_rule.dst_port.start_port, whitelist_rule.dst_port.end_port);
                    continue;
                }
            }
        }
        
        /* ★★★ 检查源端口是否在白名单范围内 ★★★ */
        if (whitelist_rule.src_port.is_valid && request_rule.src_port.is_valid) {
            /* 跳过 0.65535 表示任意端口的白名单规则 */
            if (!(whitelist_rule.src_port.start_port == 0 && whitelist_rule.src_port.end_port == 65535)) {
                if (request_rule.src_port.start_port < whitelist_rule.src_port.start_port ||
                    request_rule.src_port.end_port > whitelist_rule.src_port.end_port) {
                    fd_log_debug("[tft_validator]   ✗ Source port out of range: req=%u-%u, wl=%u-%u",
                                request_rule.src_port.start_port, request_rule.src_port.end_port,
                                whitelist_rule.src_port.start_port, whitelist_rule.src_port.end_port);
                    continue;
                }
            }
        }
        
        /* 所有条件通过 */
        fd_log_notice("[tft_validator] ✓ TFT validated against whitelist entry [%u/%u]",
                     i + 1, whitelist->num_allowed_tfts);
        fd_log_notice("[tft_validator]   Allowed: src_port=%u-%u, dst_port=%u-%u",
                     whitelist_rule.src_port.start_port, whitelist_rule.src_port.end_port,
                     whitelist_rule.dst_port.start_port, whitelist_rule.dst_port.end_port);
        return 0;  /* 验证通过 */
    }
    
    /* 未找到匹配的规则 */
    snprintf(error_msg, error_msg_len, 
             "TFT not in whitelist: src_port=%u-%u, dst_port=%u-%u (checked %u entries)", 
             request_rule.src_port.start_port, request_rule.src_port.end_port,
             request_rule.dst_port.start_port, request_rule.dst_port.end_port,
             whitelist->num_allowed_tfts);
    fd_log_error("[tft_validator] ✗ REJECTED: %s", error_msg);
    fd_log_error("[tft_validator]   Requested TFT: %s", normalized_tft);
    fd_log_error("[tft_validator]   Allowed TFTs:");
    for (uint32_t i = 0; i < whitelist->num_allowed_tfts && i < 5; i++) {
        fd_log_error("[tft_validator]     [%u] %s", i + 1, whitelist->allowed_tfts[i]);
    }
    if (whitelist->num_allowed_tfts > 5) {
        fd_log_error("[tft_validator]     ... and %u more", whitelist->num_allowed_tfts - 5);
    }
    
    return -1;
}

/**
 * @brief 验证NAPT规则（暂未实现，返回成功）
 */
#if 0
int napt_validate_against_whitelist(
    const char *napt_rule,
    const TrafficSecurityConfig *whitelist,
    const char *link_ip,
    char *error_msg,
    size_t error_msg_len)
{
    /* NAPT验证暂不实现，默认通过 */
    fd_log_debug("[tft_validator] NAPT validation not implemented, skipping");
    return 0;
}
#endif
