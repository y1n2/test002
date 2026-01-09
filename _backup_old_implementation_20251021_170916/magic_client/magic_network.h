#ifndef MAGIC_NETWORK_H
#define MAGIC_NETWORK_H

#include "magic_client.h"

// 网络接口类型
typedef enum {
    INTERFACE_TYPE_ETHERNET = 1,
    INTERFACE_TYPE_WIFI = 2,
    INTERFACE_TYPE_VIRTUAL = 3
} interface_type_t;

// 网络接口信息
typedef struct {
    char name[32];                    // 接口名称 (如 eth0, wlan0)
    interface_type_t type;            // 接口类型
    char mac_address[18];             // MAC地址
    char current_ip[MAX_IP_ADDR_LEN]; // 当前IP地址
    char current_netmask[MAX_IP_ADDR_LEN]; // 当前子网掩码
    char current_gateway[MAX_IP_ADDR_LEN]; // 当前网关
    bool is_up;                       // 接口是否启用
    bool is_managed;                  // 是否由MAGIC管理
} network_interface_t;

// 路由表项
typedef struct {
    char destination[MAX_IP_ADDR_LEN]; // 目标网络
    char netmask[MAX_IP_ADDR_LEN];     // 子网掩码
    char gateway[MAX_IP_ADDR_LEN];     // 网关
    char interface[32];                // 出接口
    int metric;                        // 路由优先级
} route_entry_t;

// 网络备份配置
typedef struct {
    network_interface_t original_interface; // 原始接口配置
    route_entry_t *original_routes;         // 原始路由表
    int route_count;                        // 路由条目数
    char original_dns[2][MAX_IP_ADDR_LEN];  // 原始DNS配置
    bool backup_valid;                      // 备份是否有效
} network_backup_t;

// 网络监控信息
typedef struct {
    unsigned long rx_bytes;           // 接收字节数
    unsigned long tx_bytes;           // 发送字节数
    unsigned long rx_packets;         // 接收包数
    unsigned long tx_packets;         // 发送包数
    unsigned long rx_errors;          // 接收错误数
    unsigned long tx_errors;          // 发送错误数
    time_t last_update;               // 最后更新时间
} network_stats_t;

// 网络配置管理器
typedef struct {
    network_interface_t *interfaces;  // 网络接口列表
    int interface_count;              // 接口数量
    network_backup_t backup;          // 网络配置备份
    network_stats_t stats;            // 网络统计信息
    char managed_interface[32];       // MAGIC管理的接口
    pthread_mutex_t config_mutex;     // 配置互斥锁
    bool monitoring_enabled;          // 是否启用监控
} network_manager_t;

// 网络配置函数声明
int magic_network_init(network_manager_t *manager);
void magic_network_cleanup(network_manager_t *manager);

// 接口管理
int magic_network_discover_interfaces(network_manager_t *manager);
int magic_network_select_interface(network_manager_t *manager, const char *preferred_name);
int magic_network_get_interface_info(const char *interface_name, network_interface_t *info);

// 网络配置应用
int magic_network_backup_current_config(network_manager_t *manager);
int magic_network_apply_config(network_manager_t *manager, const network_config_t *config);
int magic_network_restore_config(network_manager_t *manager);

// IP地址配置
int magic_network_set_ip_address(const char *interface, const char *ip, const char *netmask);
int magic_network_set_gateway(const char *gateway);
int magic_network_set_dns_servers(const char *primary, const char *secondary);

// 路由管理
int magic_network_add_route(const char *destination, const char *netmask, 
                           const char *gateway, const char *interface);
int magic_network_delete_route(const char *destination, const char *netmask);
int magic_network_get_default_route(route_entry_t *route);
int magic_network_set_default_route(const char *gateway, const char *interface);

// 接口控制
int magic_network_interface_up(const char *interface);
int magic_network_interface_down(const char *interface);
int magic_network_flush_interface(const char *interface);

// 网络监控和测试
int magic_network_start_monitoring(network_manager_t *manager);
void magic_network_stop_monitoring(network_manager_t *manager);
int magic_network_ping_test(const char *target_ip, int timeout_ms);
int magic_network_dns_test(const char *hostname);
int magic_network_connectivity_test(const network_config_t *config);
int magic_network_get_stats(network_manager_t *manager, network_stats_t *stats);
void magic_network_reset_stats(network_manager_t *manager);

// 带宽控制
int magic_network_set_bandwidth_limit(const char *interface, int limit_kbps);
int magic_network_remove_bandwidth_limit(const char *interface);
int magic_network_get_bandwidth_usage(const char *interface, unsigned long *rx_rate, unsigned long *tx_rate);

// 防火墙规则
int magic_network_add_firewall_rule(const char *rule);
int magic_network_remove_firewall_rule(const char *rule);
int magic_network_flush_firewall_rules(void);

// 网络诊断
int magic_network_diagnose_connectivity(network_manager_t *manager, char *diagnosis, size_t diag_len);
int magic_network_check_interface_status(const char *interface);
int magic_network_validate_config(const network_config_t *config);

// 工具函数
bool magic_network_is_valid_ip(const char *ip);
bool magic_network_is_valid_netmask(const char *netmask);
int magic_network_ip_to_int(const char *ip);
void magic_network_int_to_ip(int ip_int, char *ip_str);
int magic_network_calculate_network(const char *ip, const char *netmask, char *network);

// 系统命令执行
int magic_network_execute_command(const char *command, char *output, size_t output_len);
int magic_network_execute_command_silent(const char *command);

// 错误处理和日志
void magic_network_log_error(const char *function, int error_code, const char *message);
const char* magic_network_get_error_string(int error_code);

// 网络配置文件操作
int magic_network_save_config_to_file(const network_config_t *config, const char *filename);
int magic_network_load_config_from_file(network_config_t *config, const char *filename);

#endif // MAGIC_NETWORK_H