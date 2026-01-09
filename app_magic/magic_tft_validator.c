/**
 * @file magic_tft_validator.c
 * @brief TFT 和 NAPT 白名单验证器实现
 * @description 按照 ARINC 839 §1.2.2.2 要求实现白名单验证逻辑
 *
 * 实现原则（ARINC 839 Page 92, 行9392-9394）：
 * "The aim of the examples is to show that a 'normal' string match
 *  validation is not sufficient enough to grant or to reject a client
 *  request."
 *
 * 验证逻辑：
 * 1. 解析客户端请求的 TFT/NAPT 规则
 * 2. 解析 ClientProfile 中的白名单范围
 * 3. 验证请求范围是否完全包含在白名单范围内
 * 4. 返回详细的错误信息
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2025-12-25
 */

#include "magic_tft_validator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <freeDiameter/extension.h>

/*===========================================================================
 * 内部辅助函数
 *===========================================================================*/

/**
 * @brief 将点分十进制 IP 转换为 uint32_t（主机字节序）
 */
static uint32_t ip_str_to_uint32(const char *ip_str) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        return 0;
    }
    return ntohl(addr.s_addr);
}

/**
 * @brief 将 uint32_t IP 转换为点分十进制字符串
 */
static void uint32_to_ip_str(uint32_t ip, char *buffer, size_t size) {
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    inet_ntop(AF_INET, &addr, buffer, size);
}

/**
 * @brief 去除字符串首尾空格
 */
static void trim_whitespace(char *str) {
    if (!str) return;
    
    /* 去除前导空格 */
    char *start = str;
    while (*start && isspace(*start)) start++;
    
    /* 去除尾随空格 */
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    /* 移动字符串 */
    size_t len = end - start + 1;
    if (start != str) {
        memmove(str, start, len);
    }
    str[len] = '\0';
}

/*===========================================================================
 * IP/端口范围解析实现
 *===========================================================================*/

int parse_ip_range(const char *range_str, IPRange *range) {
    if (!range_str || !range) {
        return -1;
    }
    
    char buffer[128];
    strncpy(buffer, range_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    trim_whitespace(buffer);
    
    memset(range, 0, sizeof(*range));
    range->is_valid = false;
    
    /* 格式1: CIDR (192.168.1.0/24) */
    char *slash = strchr(buffer, '/');
    if (slash) {
        *slash = '\0';
        char *ip_part = buffer;
        char *prefix_part = slash + 1;
        
        uint32_t ip = ip_str_to_uint32(ip_part);
        int prefix_len = atoi(prefix_part);
        
        if (ip == 0 || prefix_len < 0 || prefix_len > 32) {
            return -1;
        }
        
        /* 计算网络掩码 */
        uint32_t mask = (prefix_len == 0) ? 0 : (~0U << (32 - prefix_len));
        range->start_ip = ip & mask;
        range->end_ip = ip | ~mask;
        range->is_valid = true;
        return 0;
    }
    
    /* 格式2: 范围 (192.168.1.1-192.168.1.254) */
    char *dash = strchr(buffer, '-');
    if (dash) {
        *dash = '\0';
        char *start_str = buffer;
        char *end_str = dash + 1;
        trim_whitespace(start_str);
        trim_whitespace(end_str);
        
        range->start_ip = ip_str_to_uint32(start_str);
        range->end_ip = ip_str_to_uint32(end_str);
        
        if (range->start_ip == 0 || range->end_ip == 0 ||
            range->start_ip > range->end_ip) {
            return -1;
        }
        
        range->is_valid = true;
        return 0;
    }
    
    /* 格式3: 单IP (192.168.1.10) */
    uint32_t ip = ip_str_to_uint32(buffer);
    if (ip == 0 && strcmp(buffer, "0.0.0.0") != 0) {
        return -1;
    }
    
    range->start_ip = ip;
    range->end_ip = ip;
    range->is_valid = true;
    return 0;
}

int parse_port_range(const char *range_str, PortRange *ranges, int max_ranges) {
    if (!range_str || !ranges || max_ranges <= 0) {
        return -1;
    }
    
    char buffer[512];
    strncpy(buffer, range_str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    trim_whitespace(buffer);
    
    int count = 0;
    char *token = strtok(buffer, ",");
    
    while (token && count < max_ranges) {
        trim_whitespace(token);
        
        /* 范围格式: 5000-6000 */
        char *dash = strchr(token, '-');
        if (dash) {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);
            
            if (start < 0 || start > 65535 || end < 0 || end > 65535 || start > end) {
                return -1;
            }
            
            ranges[count].start_port = start;
            ranges[count].end_port = end;
            ranges[count].is_valid = true;
            count++;
        } else {
            /* 单端口: 80 */
            int port = atoi(token);
            if (port < 0 || port > 65535) {
                return -1;
            }
            
            ranges[count].start_port = port;
            ranges[count].end_port = port;
            ranges[count].is_valid = true;
            count++;
        }
        
        token = strtok(NULL, ",");
    }
    
    return count;
}

bool ip_in_range(uint32_t ip, const IPRange *range) {
    if (!range || !range->is_valid) {
        return false;
    }
    return (ip >= range->start_ip && ip <= range->end_ip);
}

bool port_in_ranges(uint16_t port, const PortRange *ranges, int num_ranges) {
    if (!ranges || num_ranges <= 0) {
        return false;
    }
    
    for (int i = 0; i < num_ranges; i++) {
        if (ranges[i].is_valid &&
            port >= ranges[i].start_port &&
            port <= ranges[i].end_port) {
            return true;
        }
    }
    return false;
}

uint8_t protocol_name_to_number(const char *protocol_name) {
    if (!protocol_name) return 0;
    
    char buffer[32];
    strncpy(buffer, protocol_name, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* 转换为大写 */
    for (char *p = buffer; *p; p++) {
        *p = toupper(*p);
    }
    
    if (strcmp(buffer, "TCP") == 0) return 6;
    if (strcmp(buffer, "UDP") == 0) return 17;
    if (strcmp(buffer, "ICMP") == 0) return 1;
    if (strcmp(buffer, "IP") == 0) return 0;  /* IP 表示任意协议 */
    
    /* 尝试解析为数字 */
    int proto = atoi(buffer);
    if (proto >= 0 && proto <= 255) {
        return (uint8_t)proto;
    }
    
    return 0;  /* 未知协议，默认为任意 */
}

/*===========================================================================
 * TFT 解析实现
 *===========================================================================*/

int tft_parse_rule(const char *tft_string, TFTRule *rule) {
    if (!tft_string || !rule) {
        return -1;
    }
    
    memset(rule, 0, sizeof(*rule));
    rule->is_valid = false;
    
    char buffer[MAX_TFT_LEN];
    strncpy(buffer, tft_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* TFT 格式（3GPP TS 23.060）:
     * permit out ip from <source_ip> to <dest_ip>[:<dest_port>] [<protocol>]
     * permit in ip from <source_ip> to <dest_ip>[:<dest_port>] [<protocol>]
     */
    
    /* 1. 检查 permit */
    if (strncmp(buffer, "permit", 6) != 0) {
        fd_log_debug("[tft_validator] Invalid TFT: must start with 'permit'");
        return -1;
    }
    
    char *p = buffer + 6;
    while (*p && isspace(*p)) p++;
    
    /* 2. 检查方向 (out/in) */
    if (strncmp(p, "out", 3) == 0) {
        rule->is_outbound = true;
        p += 3;
    } else if (strncmp(p, "in", 2) == 0) {
        rule->is_outbound = false;
        p += 2;
    } else {
        fd_log_debug("[tft_validator] Invalid TFT: missing direction (out/in)");
        return -1;
    }
    
    while (*p && isspace(*p)) p++;
    
    /* 3. 检查 'ip' */
    if (strncmp(p, "ip", 2) != 0) {
        fd_log_debug("[tft_validator] Invalid TFT: missing 'ip' keyword");
        return -1;
    }
    p += 2;
    while (*p && isspace(*p)) p++;
    
    /* 4. 解析 'from <source_ip>' */
    if (strncmp(p, "from", 4) != 0) {
        fd_log_debug("[tft_validator] Invalid TFT: missing 'from' keyword");
        return -1;
    }
    p += 4;
    while (*p && isspace(*p)) p++;
    
    char src_ip_str[64] = {0};
    int i = 0;
    while (*p && !isspace(*p) && i < 63) {
        src_ip_str[i++] = *p++;
    }
    src_ip_str[i] = '\0';
    
    if (parse_ip_range(src_ip_str, &rule->src_ip) != 0) {
        fd_log_debug("[tft_validator] Invalid TFT: bad source IP '%s'", src_ip_str);
        return -1;
    }
    
    while (*p && isspace(*p)) p++;
    
    /* 5. 解析 'to <dest_ip>[:<dest_port>]' */
    if (strncmp(p, "to", 2) != 0) {
        fd_log_debug("[tft_validator] Invalid TFT: missing 'to' keyword");
        return -1;
    }
    p += 2;
    while (*p && isspace(*p)) p++;
    
    char dest_str[128] = {0};
    i = 0;
    while (*p && !isspace(*p) && i < 127) {
        dest_str[i++] = *p++;
    }
    dest_str[i] = '\0';
    
    /* 检查是否有端口 */
    char *colon = strchr(dest_str, ':');
    if (colon) {
        *colon = '\0';
        char *port_str = colon + 1;
        
        /* 解析目标 IP */
        if (parse_ip_range(dest_str, &rule->dst_ip) != 0) {
            fd_log_debug("[tft_validator] Invalid TFT: bad dest IP '%s'", dest_str);
            return -1;
        }
        
        /* 解析目标端口 */
        PortRange port_ranges[10];
        int num_ports = parse_port_range(port_str, port_ranges, 10);
        if (num_ports < 0) {
            fd_log_debug("[tft_validator] Invalid TFT: bad dest port '%s'", port_str);
            return -1;
        }
        
        /* 简化处理：只取第一个端口范围 */
        if (num_ports > 0) {
            rule->dst_port = port_ranges[0];
        }
    } else {
        /* 没有端口，解析目标 IP */
        if (parse_ip_range(dest_str, &rule->dst_ip) != 0) {
            fd_log_debug("[tft_validator] Invalid TFT: bad dest IP '%s'", dest_str);
            return -1;
        }
        
        /* 端口为任意 */
        rule->dst_port.start_port = 0;
        rule->dst_port.end_port = 65535;
        rule->dst_port.is_valid = true;
    }
    
    while (*p && isspace(*p)) p++;
    
    /* 6. 可选：解析协议 */
    if (*p) {
        char protocol_str[32] = {0};
        i = 0;
        while (*p && !isspace(*p) && i < 31) {
            protocol_str[i++] = *p++;
        }
        protocol_str[i] = '\0';
        
        rule->protocol = protocol_name_to_number(protocol_str);
        rule->has_protocol = true;
    } else {
        rule->protocol = 0;  /* 任意协议 */
        rule->has_protocol = false;
    }
    
    rule->is_valid = true;
    return 0;
}

/*===========================================================================
 * 白名单验证核心逻辑
 *===========================================================================*/

int tft_validate_against_whitelist(
    const char *tft_string,
    const TrafficSecurityConfig *whitelist,
    char *error_msg,
    size_t error_msg_len)
{
    if (!tft_string || !whitelist || !error_msg) {
        return -2;
    }
    
    /* 清空错误消息 */
    error_msg[0] = '\0';
    
    /* 1. 解析 TFT 规则 */
    TFTRule rule;
    if (tft_parse_rule(tft_string, &rule) != 0 || !rule.is_valid) {
        snprintf(error_msg, error_msg_len, "Failed to parse TFT rule: invalid syntax");
        fd_log_error("[tft_validator] %s: '%s'", error_msg, tft_string);
        return -2;
    }
    
    fd_log_debug("[tft_validator] Validating TFT: '%s'", tft_string);
    tft_rule_dump(&rule, "[tft_validator]   Parsed");
    
    /* 2. 解析白名单 IP 范围 */
    IPRange whitelist_ip_range;
    if (whitelist->dest_ip_range[0] == '\0') {
        /* 未配置白名单，默认允许所有 */
        fd_log_notice("[tft_validator] Warning: No dest_ip_range in whitelist, allowing all IPs");
        whitelist_ip_range.start_ip = 0;
        whitelist_ip_range.end_ip = 0xFFFFFFFF;
        whitelist_ip_range.is_valid = true;
    } else if (parse_ip_range(whitelist->dest_ip_range, &whitelist_ip_range) != 0) {
        snprintf(error_msg, error_msg_len, 
                 "Invalid whitelist dest_ip_range: %s", whitelist->dest_ip_range);
        fd_log_error("[tft_validator] %s", error_msg);
        return -2;
    }
    
    char wl_ip_str[128];
    ip_range_to_string(&whitelist_ip_range, wl_ip_str, sizeof(wl_ip_str));
    fd_log_debug("[tft_validator]   Whitelist IP: %s", wl_ip_str);
    
    /* 3. 验证目标 IP 是否在白名单范围内 */
    if (!ip_in_range(rule.dst_ip.start_ip, &whitelist_ip_range) ||
        !ip_in_range(rule.dst_ip.end_ip, &whitelist_ip_range)) {
        
        char req_ip_str[128];
        ip_range_to_string(&rule.dst_ip, req_ip_str, sizeof(req_ip_str));
        
        snprintf(error_msg, error_msg_len,
                 "Destination IP %s is outside whitelist range %s",
                 req_ip_str, wl_ip_str);
        fd_log_error("[tft_validator] ✗ REJECTED: %s", error_msg);
        return -1;
    }
    
    /* 4. 解析白名单端口范围 */
    PortRange whitelist_port_ranges[20];
    int num_wl_ports = 0;
    
    if (whitelist->dest_port_range[0] == '\0') {
        /* 未配置白名单端口，默认允许所有 */
        fd_log_notice("[tft_validator] Warning: No dest_port_range in whitelist, allowing all ports");
        whitelist_port_ranges[0].start_port = 0;
        whitelist_port_ranges[0].end_port = 65535;
        whitelist_port_ranges[0].is_valid = true;
        num_wl_ports = 1;
    } else {
        num_wl_ports = parse_port_range(whitelist->dest_port_range, 
                                        whitelist_port_ranges, 20);
        if (num_wl_ports < 0) {
            snprintf(error_msg, error_msg_len,
                     "Invalid whitelist dest_port_range: %s", whitelist->dest_port_range);
            fd_log_error("[tft_validator] %s", error_msg);
            return -2;
        }
    }
    
    fd_log_debug("[tft_validator]   Whitelist Ports: %s", whitelist->dest_port_range);
    
    /* 5. 验证目标端口是否在白名单范围内 */
    bool port_valid = true;
    
    /* 检查起始端口 */
    if (!port_in_ranges(rule.dst_port.start_port, whitelist_port_ranges, num_wl_ports)) {
        port_valid = false;
    }
    
    /* 检查结束端口 */
    if (rule.dst_port.start_port != rule.dst_port.end_port &&
        !port_in_ranges(rule.dst_port.end_port, whitelist_port_ranges, num_wl_ports)) {
        port_valid = false;
    }
    
    /* 如果是端口范围，需要检查整个范围是否在白名单内 */
    if (rule.dst_port.start_port != rule.dst_port.end_port) {
        bool range_covered = false;
        for (int i = 0; i < num_wl_ports; i++) {
            if (rule.dst_port.start_port >= whitelist_port_ranges[i].start_port &&
                rule.dst_port.end_port <= whitelist_port_ranges[i].end_port) {
                range_covered = true;
                break;
            }
        }
        if (!range_covered) {
            port_valid = false;
        }
    }
    
    if (!port_valid) {
        snprintf(error_msg, error_msg_len,
                 "Destination port %u-%u is outside whitelist range %s",
                 rule.dst_port.start_port, rule.dst_port.end_port,
                 whitelist->dest_port_range);
        fd_log_error("[tft_validator] ✗ REJECTED: %s", error_msg);
        return -1;
    }
    
    /* 6. 验证源端口是否在白名单范围内（新增） */
    if (whitelist->source_port_range[0] != '\0') {
        PortRange whitelist_src_port_ranges[20];
        int num_wl_src_ports = parse_port_range(whitelist->source_port_range, 
                                                  whitelist_src_port_ranges, 20);
        if (num_wl_src_ports < 0) {
            snprintf(error_msg, error_msg_len,
                     "Invalid whitelist source_port_range: %s", whitelist->source_port_range);
            fd_log_error("[tft_validator] %s", error_msg);
            return -2;
        }
        
        fd_log_debug("[tft_validator]   Whitelist Source Ports: %s", whitelist->source_port_range);
        
        /* 验证源端口起始值 */
        bool src_port_valid = true;
        if (!port_in_ranges(rule.src_port.start_port, whitelist_src_port_ranges, num_wl_src_ports)) {
            src_port_valid = false;
        }
        
        /* 验证源端口结束值 */
        if (rule.src_port.start_port != rule.src_port.end_port &&
            !port_in_ranges(rule.src_port.end_port, whitelist_src_port_ranges, num_wl_src_ports)) {
            src_port_valid = false;
        }
        
        /* 如果是端口范围，需要检查整个范围是否在白名单内 */
        if (rule.src_port.start_port != rule.src_port.end_port) {
            bool range_covered = false;
            for (int i = 0; i < num_wl_src_ports; i++) {
                if (rule.src_port.start_port >= whitelist_src_port_ranges[i].start_port &&
                    rule.src_port.end_port <= whitelist_src_port_ranges[i].end_port) {
                    range_covered = true;
                    break;
                }
            }
            if (!range_covered) {
                src_port_valid = false;
            }
        }
        
        if (!src_port_valid) {
            snprintf(error_msg, error_msg_len,
                     "Source port %u-%u is outside whitelist range %s",
                     rule.src_port.start_port, rule.src_port.end_port,
                     whitelist->source_port_range);
            fd_log_error("[tft_validator] ✗ REJECTED: %s", error_msg);
            return -1;
        }
        
        fd_log_debug("[tft_validator]   ✓ Source port %u-%u validated", 
                     rule.src_port.start_port, rule.src_port.end_port);
    } else {
        fd_log_debug("[tft_validator]   No source_port_range configured, allowing all source ports");
    }
    
    /* 7. 验证协议（如果白名单配置了允许的协议） */
    if (whitelist->num_allowed_protocols > 0 && rule.has_protocol && rule.protocol != 0) {
        bool protocol_allowed = false;
        
        for (uint32_t i = 0; i < whitelist->num_allowed_protocols; i++) {
            uint8_t wl_proto = protocol_name_to_number(whitelist->allowed_protocols[i]);
            if (wl_proto == rule.protocol) {
                protocol_allowed = true;
                break;
            }
        }
        
        if (!protocol_allowed) {
            snprintf(error_msg, error_msg_len,
                     "Protocol %u is not in allowed protocols list", rule.protocol);
            fd_log_error("[tft_validator] ✗ REJECTED: %s", error_msg);
            return -1;
        }
    }
    
    fd_log_notice("[tft_validator] ✓ GRANTED: TFT validation passed");
    return 0;
}

#if 0
int napt_validate_against_whitelist(
    const char *napt_rule,
    const TrafficSecurityConfig *whitelist,
    const char *link_ip,
    char *error_msg,
    size_t error_msg_len)
{
    if (!napt_rule || !whitelist || !error_msg) {
        return -2;
    }
    
    /* NAPT 规则格式示例: tcp:8080->10.2.2.8:80 */
    /* 简化实现：提取目标 IP 和端口，使用 TFT 验证逻辑 */
    
    char buffer[MAX_NAPT_LEN];
    strncpy(buffer, napt_rule, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* 查找 '->' 分隔符 */
    char *arrow = strstr(buffer, "->");
    if (!arrow) {
        snprintf(error_msg, error_msg_len, "Invalid NAPT rule format: missing '->'");
        fd_log_error("[tft_validator] %s: '%s'", error_msg, napt_rule);
        return -2;
    }
    
    /* 提取目标地址 (->10.2.2.8:80) */
    char *dest_part = arrow + 2;
    while (*dest_part && isspace(*dest_part)) dest_part++;
    
    /* 构造等效的 TFT 规则进行验证 */
    char tft_equiv[MAX_TFT_LEN];
    snprintf(tft_equiv, sizeof(tft_equiv), "permit out ip from any to %s", dest_part);
    
    fd_log_debug("[tft_validator] NAPT validation: '%s' -> TFT: '%s'", 
                 napt_rule, tft_equiv);
    
    return tft_validate_against_whitelist(tft_equiv, whitelist, error_msg, error_msg_len);
}
#endif

/*===========================================================================
 * 调试辅助函数实现
 *===========================================================================*/

void tft_rule_dump(const TFTRule *rule, const char *prefix) {
    if (!rule || !prefix) return;
    
    char src_ip_str[128], dst_ip_str[128];
    ip_range_to_string(&rule->src_ip, src_ip_str, sizeof(src_ip_str));
    ip_range_to_string(&rule->dst_ip, dst_ip_str, sizeof(dst_ip_str));
    
    fd_log_debug("%s Direction: %s", prefix, rule->is_outbound ? "OUT" : "IN");
    fd_log_debug("%s Source IP: %s", prefix, src_ip_str);
    fd_log_debug("%s Dest IP: %s", prefix, dst_ip_str);
    fd_log_debug("%s Dest Port: %u-%u", prefix, 
                 rule->dst_port.start_port, rule->dst_port.end_port);
    
    if (rule->has_protocol) {
        const char *proto_name = (rule->protocol == 6) ? "TCP" :
                                  (rule->protocol == 17) ? "UDP" : "OTHER";
        fd_log_debug("%s Protocol: %u (%s)", prefix, rule->protocol, proto_name);
    } else {
        fd_log_debug("%s Protocol: ANY", prefix);
    }
}

void ip_range_to_string(const IPRange *range, char *buffer, size_t buffer_size) {
    if (!range || !buffer || buffer_size < 32) return;
    
    if (!range->is_valid) {
        snprintf(buffer, buffer_size, "<invalid>");
        return;
    }
    
    char start_str[INET_ADDRSTRLEN], end_str[INET_ADDRSTRLEN];
    uint32_to_ip_str(range->start_ip, start_str, sizeof(start_str));
    uint32_to_ip_str(range->end_ip, end_str, sizeof(end_str));
    
    if (range->start_ip == range->end_ip) {
        snprintf(buffer, buffer_size, "%s", start_str);
    } else {
        snprintf(buffer, buffer_size, "%s-%s", start_str, end_str);
    }
}
