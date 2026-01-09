#include "../../include/utils/magic_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

static magic_config_t g_config;

/* 设置默认配置 */
static void set_default_config(void)
{
    int i;
    
    /* 链路模拟器默认IP */
    strcpy(g_config.links.simulator_ip, "127.0.0.1");
    
    /* 链路默认端口 */
    g_config.links.port[MAGIC_LINK_ETHERNET - 1] = 10001;
    g_config.links.port[MAGIC_LINK_WIFI - 1] = 10002;
    g_config.links.port[MAGIC_LINK_CELLULAR - 1] = 10003;
    g_config.links.port[MAGIC_LINK_SATELLITE - 1] = 10004;
    
    /* 链路默认带宽 (Mbps) */
    g_config.links.bandwidth[MAGIC_LINK_ETHERNET - 1] = 100;
    g_config.links.bandwidth[MAGIC_LINK_WIFI - 1] = 54;
    g_config.links.bandwidth[MAGIC_LINK_CELLULAR - 1] = 10;
    g_config.links.bandwidth[MAGIC_LINK_SATELLITE - 1] = 2;
    
    /* 链路默认延迟 (ms) */
    g_config.links.latency[MAGIC_LINK_ETHERNET - 1] = 1;
    g_config.links.latency[MAGIC_LINK_WIFI - 1] = 5;
    g_config.links.latency[MAGIC_LINK_CELLULAR - 1] = 50;
    g_config.links.latency[MAGIC_LINK_SATELLITE - 1] = 500;
    
    /* 链路默认可靠性 (0-100) */
    g_config.links.reliability[MAGIC_LINK_ETHERNET - 1] = 99;
    g_config.links.reliability[MAGIC_LINK_WIFI - 1] = 90;
    g_config.links.reliability[MAGIC_LINK_CELLULAR - 1] = 80;
    g_config.links.reliability[MAGIC_LINK_SATELLITE - 1] = 70;
    
    /* 链路默认信号强度 (0-100) */
    g_config.links.signal_strength[MAGIC_LINK_ETHERNET - 1] = 100;
    g_config.links.signal_strength[MAGIC_LINK_WIFI - 1] = 80;
    g_config.links.signal_strength[MAGIC_LINK_CELLULAR - 1] = 70;
    g_config.links.signal_strength[MAGIC_LINK_SATELLITE - 1] = 60;
    
    /* 默认策略权重 */
    g_config.policy.bandwidth_weight = 40;
    g_config.policy.latency_weight = 30;
    g_config.policy.reliability_weight = 20;
    g_config.policy.signal_strength_weight = 10;
    
    /* 全局默认配置 */
    g_config.global.max_clients = 100;
    g_config.global.total_bandwidth = 200; /* Mbps */
    g_config.global.client_timeout = 300;  /* 秒 */
    g_config.global.link_monitor_interval = 5; /* 秒 */
}

/* 初始化配置 */
int magic_config_init(const char *config_file)
{
    config_t cfg;
    const char *str;
    int i;
    
    /* 设置默认配置 */
    set_default_config();
    
    if (!config_file) {
        MAGIC_LOG(FD_LOG_NOTICE, "使用默认配置");
        return MAGIC_OK;
    }
    
    /* 初始化libconfig */
    config_init(&cfg);
    
    /* 读取配置文件 */
    if (!config_read_file(&cfg, config_file)) {
        MAGIC_ERROR("配置文件读取错误: %s 行 %d - %s",
                   config_file, config_error_line(&cfg),
                   config_error_text(&cfg));
        config_destroy(&cfg);
        return MAGIC_ERROR_CONFIG;
    }
    
    /* 读取链路模拟器IP */
    if (config_lookup_string(&cfg, "links.simulator_ip", &str)) {
        strncpy(g_config.links.simulator_ip, str, sizeof(g_config.links.simulator_ip) - 1);
    }
    
    /* 读取链路端口 */
    config_lookup_int(&cfg, "links.ethernet_port", &g_config.links.port[MAGIC_LINK_ETHERNET - 1]);
    config_lookup_int(&cfg, "links.wifi_port", &g_config.links.port[MAGIC_LINK_WIFI - 1]);
    config_lookup_int(&cfg, "links.cellular_port", &g_config.links.port[MAGIC_LINK_CELLULAR - 1]);
    config_lookup_int(&cfg, "links.satellite_port", &g_config.links.port[MAGIC_LINK_SATELLITE - 1]);
    
    /* 读取链路带宽 */
    config_lookup_int(&cfg, "links.ethernet_bandwidth", &g_config.links.bandwidth[MAGIC_LINK_ETHERNET - 1]);
    config_lookup_int(&cfg, "links.wifi_bandwidth", &g_config.links.bandwidth[MAGIC_LINK_WIFI - 1]);
    config_lookup_int(&cfg, "links.cellular_bandwidth", &g_config.links.bandwidth[MAGIC_LINK_CELLULAR - 1]);
    config_lookup_int(&cfg, "links.satellite_bandwidth", &g_config.links.bandwidth[MAGIC_LINK_SATELLITE - 1]);
    
    /* 读取链路延迟 */
    config_lookup_int(&cfg, "links.ethernet_latency", &g_config.links.latency[MAGIC_LINK_ETHERNET - 1]);
    config_lookup_int(&cfg, "links.wifi_latency", &g_config.links.latency[MAGIC_LINK_WIFI - 1]);
    config_lookup_int(&cfg, "links.cellular_latency", &g_config.links.latency[MAGIC_LINK_CELLULAR - 1]);
    config_lookup_int(&cfg, "links.satellite_latency", &g_config.links.latency[MAGIC_LINK_SATELLITE - 1]);
    
    /* 读取链路可靠性 */
    config_lookup_int(&cfg, "links.ethernet_reliability", &g_config.links.reliability[MAGIC_LINK_ETHERNET - 1]);
    config_lookup_int(&cfg, "links.wifi_reliability", &g_config.links.reliability[MAGIC_LINK_WIFI - 1]);
    config_lookup_int(&cfg, "links.cellular_reliability", &g_config.links.reliability[MAGIC_LINK_CELLULAR - 1]);
    config_lookup_int(&cfg, "links.satellite_reliability", &g_config.links.reliability[MAGIC_LINK_SATELLITE - 1]);
    
    /* 读取链路信号强度 */
    config_lookup_int(&cfg, "links.ethernet_signal", &g_config.links.signal_strength[MAGIC_LINK_ETHERNET - 1]);
    config_lookup_int(&cfg, "links.wifi_signal", &g_config.links.signal_strength[MAGIC_LINK_WIFI - 1]);
    config_lookup_int(&cfg, "links.cellular_signal", &g_config.links.signal_strength[MAGIC_LINK_CELLULAR - 1]);
    config_lookup_int(&cfg, "links.satellite_signal", &g_config.links.signal_strength[MAGIC_LINK_SATELLITE - 1]);
    
    /* 读取策略权重 */
    config_lookup_int(&cfg, "policy.bandwidth_weight", &g_config.policy.bandwidth_weight);
    config_lookup_int(&cfg, "policy.latency_weight", &g_config.policy.latency_weight);
    config_lookup_int(&cfg, "policy.reliability_weight", &g_config.policy.reliability_weight);
    config_lookup_int(&cfg, "policy.signal_strength_weight", &g_config.policy.signal_strength_weight);
    
    /* 读取全局配置 */
    config_lookup_int(&cfg, "global.max_clients", &g_config.global.max_clients);
    config_lookup_int(&cfg, "global.total_bandwidth", &g_config.global.total_bandwidth);
    config_lookup_int(&cfg, "global.client_timeout", &g_config.global.client_timeout);
    config_lookup_int(&cfg, "global.link_monitor_interval", &g_config.global.link_monitor_interval);
    
    config_destroy(&cfg);
    
    MAGIC_LOG(FD_LOG_NOTICE, "配置文件 %s 加载成功", config_file);
    return MAGIC_OK;
}

/* 清理配置 */
void magic_config_cleanup(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "清理配置");
    
    /* 无需特殊清理 */
}

/* 获取配置 */
magic_config_t *config_get(void)
{
    return &g_config;
}