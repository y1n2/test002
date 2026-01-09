#ifndef _MAGIC_CONFIG_H
#define _MAGIC_CONFIG_H

#include "../magic_common.h"

/* 配置结构体 */
typedef struct {
    /* 链路配置 */
    struct {
        char simulator_ip[64];
        int port[MAGIC_LINK_MAX];
        int bandwidth[MAGIC_LINK_MAX];
        int latency[MAGIC_LINK_MAX];
        int reliability[MAGIC_LINK_MAX];
        int signal_strength[MAGIC_LINK_MAX];
    } links;
    
    /* 策略配置 */
    struct {
        int bandwidth_weight;
        int latency_weight;
        int reliability_weight;
        int signal_strength_weight;
    } policy;
    
    /* 全局配置 */
    struct {
        int max_clients;
        int total_bandwidth;
        int client_timeout;
        int link_monitor_interval;
    } global;
} magic_config_t;

/* 配置API */
int magic_config_init(const char *config_file);
void magic_config_cleanup(void);
magic_config_t *config_get(void);

#endif /* _MAGIC_CONFIG_H */