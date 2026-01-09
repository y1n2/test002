/**
 * @file dlm_config_parser.h
 * @brief DLM INI 配置文件解析器
 * @date 2025-11-27
 */

#ifndef DLM_CONFIG_PARSER_H
#define DLM_CONFIG_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define DLM_CFG_MAX_LINE      256
#define DLM_CFG_MAX_SECTION   64
#define DLM_CFG_MAX_KEY       64
#define DLM_CFG_MAX_VALUE     128

/*===========================================================================
 * 配置数据结构 (与 dlm_common.h 中的 dlm_config_manager_t 对应)
 *===========================================================================*/

typedef struct {
    /* [general] */
    uint8_t     link_id;
    char        link_type[16];
    char        link_name[32];
    
    /* [interface] */
    char        interface_name[16];
    char        ip_address[20];
    char        netmask[20];
    
    /* [bandwidth] */
    uint32_t    max_bandwidth_fl;
    uint32_t    max_bandwidth_rl;
    
    /* [latency] */
    uint32_t    delay_ms;
    uint32_t    jitter_ms;
    
    /* [signal] */
    int32_t     rssi_threshold_dbm;
    int32_t     rssi_min_dbm;
    int32_t     rssi_max_dbm;
    int32_t     initial_rssi_dbm;
    
    /* [cost] */
    float       cost_factor;
    uint32_t    cost_per_mb_cents;
    
    /* [network] */
    uint8_t     security_level;
    uint16_t    mtu;
    bool        is_asymmetric;
    bool        ground_only;
    
    /* [timing] */
    uint32_t    reporting_interval_sec;
    uint32_t    heartbeat_interval_sec;
    uint32_t    going_down_lead_time_ms;
    
    /* [socket] */
    char        mihf_socket_path[128];
    char        dlm_socket_path[128];
    
} dlm_parsed_config_t;

/*===========================================================================
 * 辅助函数：去除字符串首尾空白
 *===========================================================================*/

static inline char* dlm_cfg_trim(char* str)
{
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/*===========================================================================
 * 解析布尔值
 *===========================================================================*/

static inline bool dlm_cfg_parse_bool(const char* value)
{
    if (strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 ||
        strcasecmp(value, "1") == 0) {
        return true;
    }
    return false;
}

/*===========================================================================
 * 解析十六进制或十进制整数
 *===========================================================================*/

static inline uint32_t dlm_cfg_parse_hex_or_dec(const char* value)
{
    if (strncasecmp(value, "0x", 2) == 0) {
        return (uint32_t)strtoul(value, NULL, 16);
    }
    return (uint32_t)strtoul(value, NULL, 10);
}

/*===========================================================================
 * 主解析函数
 *===========================================================================*/

static inline int dlm_config_parse(const char* filepath, dlm_parsed_config_t* cfg)
{
    FILE* fp;
    char line[DLM_CFG_MAX_LINE];
    char current_section[DLM_CFG_MAX_SECTION] = "";
    char key[DLM_CFG_MAX_KEY];
    char value[DLM_CFG_MAX_VALUE];
    
    if (!filepath || !cfg) {
        return -1;
    }
    
    /* 初始化默认值 */
    memset(cfg, 0, sizeof(dlm_parsed_config_t));
    cfg->mtu = 1500;
    cfg->security_level = 3;
    cfg->reporting_interval_sec = 5;
    cfg->heartbeat_interval_sec = 10;
    cfg->going_down_lead_time_ms = 3000;
    strncpy(cfg->mihf_socket_path, "/tmp/mihf.sock", sizeof(cfg->mihf_socket_path) - 1);
    strncpy(cfg->dlm_socket_path, "/tmp/dlm.sock", sizeof(cfg->dlm_socket_path) - 1);
    
    fp = fopen(filepath, "r");
    if (!fp) {
        perror("[DLM-CFG] 无法打开配置文件");
        return -1;
    }
    
    printf("[DLM-CFG] 正在解析配置文件: %s\n", filepath);
    
    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = dlm_cfg_trim(line);
        
        /* 跳过空行和注释 */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            continue;
        }
        
        /* 检测 section */
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
            }
            continue;
        }
        
        /* 解析 key = value */
        char* eq = strchr(trimmed, '=');
        if (!eq) continue;
        
        *eq = '\0';
        strncpy(key, dlm_cfg_trim(trimmed), sizeof(key) - 1);
        strncpy(value, dlm_cfg_trim(eq + 1), sizeof(value) - 1);
        
        /* 根据 section 和 key 设置配置 */
        if (strcmp(current_section, "general") == 0) {
            if (strcmp(key, "link_id") == 0) {
                cfg->link_id = (uint8_t)dlm_cfg_parse_hex_or_dec(value);
            } else if (strcmp(key, "link_type") == 0) {
                strncpy(cfg->link_type, value, sizeof(cfg->link_type) - 1);
            } else if (strcmp(key, "link_name") == 0) {
                strncpy(cfg->link_name, value, sizeof(cfg->link_name) - 1);
            }
        } else if (strcmp(current_section, "interface") == 0) {
            if (strcmp(key, "name") == 0) {
                strncpy(cfg->interface_name, value, sizeof(cfg->interface_name) - 1);
            } else if (strcmp(key, "ip_address") == 0) {
                strncpy(cfg->ip_address, value, sizeof(cfg->ip_address) - 1);
            } else if (strcmp(key, "netmask") == 0) {
                strncpy(cfg->netmask, value, sizeof(cfg->netmask) - 1);
            }
        } else if (strcmp(current_section, "bandwidth") == 0) {
            if (strcmp(key, "max_forward_link_kbps") == 0) {
                cfg->max_bandwidth_fl = (uint32_t)atoi(value);
            } else if (strcmp(key, "max_return_link_kbps") == 0) {
                cfg->max_bandwidth_rl = (uint32_t)atoi(value);
            }
        } else if (strcmp(current_section, "latency") == 0) {
            if (strcmp(key, "delay_ms") == 0) {
                cfg->delay_ms = (uint32_t)atoi(value);
            } else if (strcmp(key, "jitter_ms") == 0) {
                cfg->jitter_ms = (uint32_t)atoi(value);
            }
        } else if (strcmp(current_section, "signal") == 0) {
            if (strcmp(key, "rssi_threshold_dbm") == 0) {
                cfg->rssi_threshold_dbm = atoi(value);
            } else if (strcmp(key, "rssi_min_dbm") == 0) {
                cfg->rssi_min_dbm = atoi(value);
            } else if (strcmp(key, "rssi_max_dbm") == 0) {
                cfg->rssi_max_dbm = atoi(value);
            } else if (strcmp(key, "initial_rssi_dbm") == 0) {
                cfg->initial_rssi_dbm = atoi(value);
            }
        } else if (strcmp(current_section, "cost") == 0) {
            if (strcmp(key, "factor") == 0) {
                cfg->cost_factor = (float)atof(value);
            } else if (strcmp(key, "per_mb_cents") == 0) {
                cfg->cost_per_mb_cents = (uint32_t)atoi(value);
            }
        } else if (strcmp(current_section, "network") == 0) {
            if (strcmp(key, "security_level") == 0) {
                cfg->security_level = (uint8_t)atoi(value);
            } else if (strcmp(key, "mtu") == 0) {
                cfg->mtu = (uint16_t)atoi(value);
            } else if (strcmp(key, "is_asymmetric") == 0) {
                cfg->is_asymmetric = dlm_cfg_parse_bool(value);
            } else if (strcmp(key, "ground_only") == 0) {
                cfg->ground_only = dlm_cfg_parse_bool(value);
            }
        } else if (strcmp(current_section, "timing") == 0) {
            if (strcmp(key, "reporting_interval_sec") == 0) {
                cfg->reporting_interval_sec = (uint32_t)atoi(value);
            } else if (strcmp(key, "heartbeat_interval_sec") == 0) {
                cfg->heartbeat_interval_sec = (uint32_t)atoi(value);
            } else if (strcmp(key, "going_down_lead_time_ms") == 0) {
                cfg->going_down_lead_time_ms = (uint32_t)atoi(value);
            }
        } else if (strcmp(current_section, "socket") == 0) {
            if (strcmp(key, "mihf_path") == 0) {
                strncpy(cfg->mihf_socket_path, value, sizeof(cfg->mihf_socket_path) - 1);
            } else if (strcmp(key, "dlm_path") == 0) {
                strncpy(cfg->dlm_socket_path, value, sizeof(cfg->dlm_socket_path) - 1);
            }
        }
    }
    
    fclose(fp);
    
    printf("[DLM-CFG] 配置加载完成:\n");
    printf("  - Link: %s (ID=0x%02X, Type=%s)\n", cfg->link_name, cfg->link_id, cfg->link_type);
    printf("  - Interface: %s, IP: %s/%s\n", cfg->interface_name, cfg->ip_address, cfg->netmask);
    printf("  - Bandwidth FL/RL: %u/%u kbps\n", cfg->max_bandwidth_fl, cfg->max_bandwidth_rl);
    printf("  - Delay: %u ms (±%u ms)\n", cfg->delay_ms, cfg->jitter_ms);
    printf("  - RSSI Threshold: %d dBm\n", cfg->rssi_threshold_dbm);
    
    return 0;
}

/*===========================================================================
 * 打印配置摘要
 *===========================================================================*/

static inline void dlm_config_print(const dlm_parsed_config_t* cfg)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║            DLM Configuration - %-32s ║\n", cfg->link_name);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [General]                                                        ║\n");
    printf("║   Link ID: 0x%02X    Type: %-10s                              ║\n", cfg->link_id, cfg->link_type);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Interface]                                                      ║\n");
    printf("║   Name: %-10s  IP: %-15s  Netmask: %-15s  ║\n", 
           cfg->interface_name, cfg->ip_address, cfg->netmask);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Bandwidth]                                                      ║\n");
    printf("║   Forward Link: %6u kbps    Return Link: %6u kbps            ║\n",
           cfg->max_bandwidth_fl, cfg->max_bandwidth_rl);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Latency]                                                        ║\n");
    printf("║   Delay: %4u ms    Jitter: ±%3u ms                              ║\n",
           cfg->delay_ms, cfg->jitter_ms);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Signal]                                                         ║\n");
    printf("║   Threshold: %4d dBm   Range: [%4d, %4d] dBm   Initial: %4d  ║\n",
           cfg->rssi_threshold_dbm, cfg->rssi_min_dbm, cfg->rssi_max_dbm, cfg->initial_rssi_dbm);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Cost]                                                           ║\n");
    printf("║   Factor: %.2f    Per MB: $%.2f                                   ║\n",
           cfg->cost_factor, cfg->cost_per_mb_cents / 100.0f);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Network]                                                        ║\n");
    printf("║   Security: %u    MTU: %4u    Asymmetric: %-3s   Ground-Only: %-3s║\n",
           cfg->security_level, cfg->mtu, 
           cfg->is_asymmetric ? "Yes" : "No",
           cfg->ground_only ? "Yes" : "No");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Timing]                                                         ║\n");
    printf("║   Report: %3us    Heartbeat: %3us    GoingDown Lead: %5ums     ║\n",
           cfg->reporting_interval_sec, cfg->heartbeat_interval_sec, cfg->going_down_lead_time_ms);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Socket]                                                         ║\n");
    printf("║   MIHF: %-58s ║\n", cfg->mihf_socket_path);
    printf("║   DLM:  %-58s ║\n", cfg->dlm_socket_path);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
}

#endif /* DLM_CONFIG_PARSER_H */
