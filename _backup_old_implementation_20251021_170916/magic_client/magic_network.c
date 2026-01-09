#include "magic_network.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/route.h>
#include <resolv.h>

// 内部函数声明
static int parse_interface_info(const char *interface_name, network_interface_t *info);
static int backup_dns_config(char dns[][MAX_IP_ADDR_LEN]);
static int restore_dns_config(char dns[][MAX_IP_ADDR_LEN]);
static void* network_monitor_thread(void *arg);

/**
 * 初始化网络管理器
 */
int magic_network_init(network_manager_t *manager) {
    if (!manager) {
        return -1;
    }
    
    memset(manager, 0, sizeof(network_manager_t));
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->config_mutex, NULL) != 0) {
        return -1;
    }
    
    // 发现网络接口
    if (magic_network_discover_interfaces(manager) != 0) {
        pthread_mutex_destroy(&manager->config_mutex);
        return -1;
    }
    
    manager->monitoring_enabled = false;
    
    return 0;
}

/**
 * 清理网络管理器
 */
void magic_network_cleanup(network_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 停止监控
    magic_network_stop_monitoring(manager);
    
    // 释放接口列表
    if (manager->interfaces) {
        free(manager->interfaces);
        manager->interfaces = NULL;
    }
    
    // 释放路由备份
    if (manager->backup.original_routes) {
        free(manager->backup.original_routes);
        manager->backup.original_routes = NULL;
    }
    
    // 销毁互斥锁
    pthread_mutex_destroy(&manager->config_mutex);
}

/**
 * 发现网络接口
 */
int magic_network_discover_interfaces(network_manager_t *manager) {
    if (!manager) {
        return -1;
    }
    
    struct ifaddrs *ifaddrs_ptr, *ifa;
    int interface_count = 0;
    
    // 获取接口列表
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return -1;
    }
    
    // 计算接口数量
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            interface_count++;
        }
    }
    
    // 分配接口数组
    manager->interfaces = malloc(interface_count * sizeof(network_interface_t));
    if (!manager->interfaces) {
        freeifaddrs(ifaddrs_ptr);
        return -1;
    }
    
    // 填充接口信息
    manager->interface_count = 0;
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            network_interface_t *iface = &manager->interfaces[manager->interface_count];
            
            strncpy(iface->name, ifa->ifa_name, sizeof(iface->name) - 1);
            iface->name[sizeof(iface->name) - 1] = '\0';
            
            // 解析接口详细信息
            if (parse_interface_info(iface->name, iface) == 0) {
                manager->interface_count++;
            }
        }
    }
    
    freeifaddrs(ifaddrs_ptr);
    return 0;
}

/**
 * 选择网络接口
 */
int magic_network_select_interface(network_manager_t *manager, const char *preferred_name) {
    if (!manager) {
        return -1;
    }
    
    // 如果指定了首选接口名称，尝试使用它
    if (preferred_name) {
        for (int i = 0; i < manager->interface_count; i++) {
            if (strcmp(manager->interfaces[i].name, preferred_name) == 0 &&
                manager->interfaces[i].is_up) {
                strncpy(manager->managed_interface, preferred_name, 
                       sizeof(manager->managed_interface) - 1);
                return 0;
            }
        }
    }
    
    // 自动选择最佳接口
    for (int i = 0; i < manager->interface_count; i++) {
        network_interface_t *iface = &manager->interfaces[i];
        
        // 跳过回环接口
        if (strcmp(iface->name, "lo") == 0) {
            continue;
        }
        
        // 选择第一个启用的接口
        if (iface->is_up) {
            strncpy(manager->managed_interface, iface->name, 
                   sizeof(manager->managed_interface) - 1);
            return 0;
        }
    }
    
    return -1;
}

/**
 * 获取接口信息
 */
int magic_network_get_interface_info(const char *interface_name, network_interface_t *info) {
    if (!interface_name || !info) {
        return -1;
    }
    
    return parse_interface_info(interface_name, info);
}

/**
 * 备份当前网络配置
 */
int magic_network_backup_current_config(network_manager_t *manager) {
    if (!manager) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->config_mutex);
    
    // 备份接口配置
    if (strlen(manager->managed_interface) > 0) {
        if (magic_network_get_interface_info(manager->managed_interface, 
                                           &manager->backup.original_interface) != 0) {
            pthread_mutex_unlock(&manager->config_mutex);
            return -1;
        }
    }
    
    // 备份DNS配置
    if (backup_dns_config(manager->backup.original_dns) != 0) {
        magic_network_log_error("backup_current_config", -1, "Failed to backup DNS config");
    }
    
    // 备份路由表（简化实现，只备份默认路由）
    route_entry_t default_route;
    if (magic_network_get_default_route(&default_route) == 0) {
        manager->backup.original_routes = malloc(sizeof(route_entry_t));
        if (manager->backup.original_routes) {
            memcpy(manager->backup.original_routes, &default_route, sizeof(route_entry_t));
            manager->backup.route_count = 1;
        }
    }
    
    manager->backup.backup_valid = true;
    pthread_mutex_unlock(&manager->config_mutex);
    
    return 0;
}

/**
 * 创建虚拟网络接口用于链路模拟器连接
 */
static int magic_network_create_virtual_interface(const char *interface_name) {
    char command[256];
    
    // 检查接口是否已存在
    snprintf(command, sizeof(command), "ip link show %s 2>/dev/null", interface_name);
    if (magic_network_execute_command_silent(command) == 0) {
        // 接口已存在，先删除
        snprintf(command, sizeof(command), "ip link delete %s 2>/dev/null", interface_name);
        magic_network_execute_command_silent(command);
    }
    
    // 创建虚拟接口
    snprintf(command, sizeof(command), "ip link add %s type dummy", interface_name);
    if (magic_network_execute_command_silent(command) != 0) {
        return -1;
    }
    
    // 启用接口
    snprintf(command, sizeof(command), "ip link set %s up", interface_name);
    if (magic_network_execute_command_silent(command) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * 删除虚拟网络接口
 */
static int magic_network_remove_virtual_interface(const char *interface_name) {
    char command[256];
    
    // 删除接口
    snprintf(command, sizeof(command), "ip link delete %s 2>/dev/null", interface_name);
    return magic_network_execute_command_silent(command);
}

/**
 * 应用网络配置
 */
int magic_network_apply_config(network_manager_t *manager, const network_config_t *config) {
    if (!manager || !config || !config->is_configured) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->config_mutex);
    
    const char *interface = "magic0"; // 使用虚拟接口
    
    // 创建虚拟接口
    if (magic_network_create_virtual_interface(interface) != 0) {
        magic_network_log_error("apply_config", -1, "Failed to create virtual interface");
        pthread_mutex_unlock(&manager->config_mutex);
        return -1;
    }
    
    // 更新管理的接口名称
    strncpy(manager->managed_interface, interface, sizeof(manager->managed_interface) - 1);
    manager->managed_interface[sizeof(manager->managed_interface) - 1] = '\0';
    
    // 配置IP地址
    if (magic_network_set_ip_address(interface, config->assigned_ip, config->netmask) != 0) {
        magic_network_log_error("apply_config", -1, "Failed to set IP address");
        pthread_mutex_unlock(&manager->config_mutex);
        return -1;
    }
    
    // 配置网关
    if (strlen(config->gateway) > 0) {
        if (magic_network_set_gateway(config->gateway) != 0) {
            magic_network_log_error("apply_config", -1, "Failed to set gateway");
        }
    }
    
    // 配置DNS
    if (strlen(config->dns_primary) > 0) {
        if (magic_network_set_dns_servers(config->dns_primary, config->dns_secondary) != 0) {
            magic_network_log_error("apply_config", -1, "Failed to set DNS servers");
        }
    }
    
    // 配置带宽限制
    if (config->bandwidth_limit > 0) {
        if (magic_network_set_bandwidth_limit(interface, config->bandwidth_limit) != 0) {
            magic_network_log_error("apply_config", -1, "Failed to set bandwidth limit");
        }
    }
    
    pthread_mutex_unlock(&manager->config_mutex);
    
    return 0;
}

/**
 * 恢复网络配置
 */
int magic_network_restore_config(network_manager_t *manager) {
    if (!manager || !manager->backup.backup_valid) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->config_mutex);
    
    const char *interface = manager->managed_interface;
    
    // 恢复IP配置
    if (strlen(manager->backup.original_interface.current_ip) > 0) {
        magic_network_set_ip_address(interface, 
                                   manager->backup.original_interface.current_ip,
                                   manager->backup.original_interface.current_netmask);
    }
    
    // 恢复网关
    if (strlen(manager->backup.original_interface.current_gateway) > 0) {
        magic_network_set_gateway(manager->backup.original_interface.current_gateway);
    }
    
    // 恢复DNS
    restore_dns_config(manager->backup.original_dns);
    
    // 移除带宽限制
    magic_network_remove_bandwidth_limit(interface);
    
    // 如果使用的是虚拟接口，删除它
    if (strncmp(interface, "magic", 5) == 0) {
        magic_network_remove_virtual_interface(interface);
        // 清空管理的接口名称
        memset(manager->managed_interface, 0, sizeof(manager->managed_interface));
    }
    
    manager->backup.backup_valid = false;
    pthread_mutex_unlock(&manager->config_mutex);
    
    return 0;
}

/**
 * 设置IP地址
 */
int magic_network_set_ip_address(const char *interface, const char *ip, const char *netmask) {
    if (!interface || !ip || !netmask) {
        return -1;
    }
    
    char command[256];
    
    // 使用ip命令设置IP地址
    snprintf(command, sizeof(command), "ip addr flush dev %s", interface);
    if (magic_network_execute_command_silent(command) != 0) {
        return -1;
    }
    
    snprintf(command, sizeof(command), "ip addr add %s/%s dev %s", ip, netmask, interface);
    if (magic_network_execute_command_silent(command) != 0) {
        return -1;
    }
    
    // 启用接口
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    if (magic_network_execute_command_silent(command) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * 设置网关
 */
int magic_network_set_gateway(const char *gateway) {
    if (!gateway) {
        return -1;
    }
    
    char command[256];
    
    // 删除默认路由
    magic_network_execute_command_silent("ip route del default");
    
    // 添加新的默认路由
    snprintf(command, sizeof(command), "ip route add default via %s", gateway);
    return magic_network_execute_command_silent(command);
}

/**
 * 设置DNS服务器
 */
int magic_network_set_dns_servers(const char *primary, const char *secondary) {
    if (!primary) {
        return -1;
    }
    
    FILE *resolv_conf = fopen("/etc/resolv.conf", "w");
    if (!resolv_conf) {
        return -1;
    }
    
    fprintf(resolv_conf, "nameserver %s\n", primary);
    if (secondary && strlen(secondary) > 0) {
        fprintf(resolv_conf, "nameserver %s\n", secondary);
    }
    
    fclose(resolv_conf);
    return 0;
}

/**
 * 添加路由
 */
int magic_network_add_route(const char *destination, const char *netmask, 
                           const char *gateway, const char *interface) {
    if (!destination || !netmask || !gateway) {
        return -1;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "ip route add %s/%s via %s", 
             destination, netmask, gateway);
    
    if (interface) {
        char temp[256];
        snprintf(temp, sizeof(temp), "%s dev %s", command, interface);
        strcpy(command, temp);
    }
    
    return magic_network_execute_command_silent(command);
}

/**
 * 删除路由
 */
int magic_network_delete_route(const char *destination, const char *netmask) {
    if (!destination || !netmask) {
        return -1;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "ip route del %s/%s", destination, netmask);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 获取默认路由
 */
int magic_network_get_default_route(route_entry_t *route) {
    if (!route) {
        return -1;
    }
    
    char output[1024];
    if (magic_network_execute_command("ip route show default", output, sizeof(output)) != 0) {
        return -1;
    }
    
    // 解析输出（简化实现）
    char *token = strtok(output, " ");
    if (token && strcmp(token, "default") == 0) {
        token = strtok(NULL, " "); // "via"
        if (token && strcmp(token, "via") == 0) {
            token = strtok(NULL, " "); // gateway IP
            if (token) {
                strncpy(route->gateway, token, sizeof(route->gateway) - 1);
                route->gateway[sizeof(route->gateway) - 1] = '\0';
                
                strcpy(route->destination, "0.0.0.0");
                strcpy(route->netmask, "0.0.0.0");
                route->metric = 0;
                
                return 0;
            }
        }
    }
    
    return -1;
}

/**
 * 设置默认路由
 */
int magic_network_set_default_route(const char *gateway, const char *interface) {
    return magic_network_set_gateway(gateway);
}

/**
 * 启用接口
 */
int magic_network_interface_up(const char *interface) {
    if (!interface) {
        return -1;
    }
    
    char command[128];
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 禁用接口
 */
int magic_network_interface_down(const char *interface) {
    if (!interface) {
        return -1;
    }
    
    char command[128];
    snprintf(command, sizeof(command), "ip link set %s down", interface);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 清空接口配置
 */
int magic_network_flush_interface(const char *interface) {
    if (!interface) {
        return -1;
    }
    
    char command[128];
    snprintf(command, sizeof(command), "ip addr flush dev %s", interface);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 启动网络监控
 */
int magic_network_start_monitoring(network_manager_t *manager) {
    if (!manager) {
        return -1;
    }
    
    // 简单实现，实际可以启动监控线程
    manager->monitoring_enabled = true;
    return 0;
}

/**
 * 停止网络监控
 */
void magic_network_stop_monitoring(network_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    manager->monitoring_enabled = false;
}

/**
 * 网络连通性测试
 */
int magic_network_ping_test(const char *target_ip, int timeout_ms) {
    if (!target_ip) {
        return -1;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "ping -c 1 -W %d %s > /dev/null 2>&1", 
             timeout_ms / 1000, target_ip);
    
    return magic_network_execute_command_silent(command);
}

/**
 * DNS测试
 */
int magic_network_dns_test(const char *hostname) {
    if (!hostname) {
        return -1;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "nslookup %s > /dev/null 2>&1", hostname);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 连通性测试
 */
int magic_network_connectivity_test(const network_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // 测试网关连通性
    if (strlen(config->gateway) > 0) {
        if (magic_network_ping_test(config->gateway, 5000) != 0) {
            return -1;
        }
    }
    
    // 测试DNS
    if (strlen(config->dns_primary) > 0) {
        if (magic_network_ping_test(config->dns_primary, 5000) != 0) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * 设置带宽限制
 */
int magic_network_set_bandwidth_limit(const char *interface, int limit_kbps) {
    if (!interface || limit_kbps <= 0) {
        return -1;
    }
    
    char command[256];
    
    // 使用tc命令设置带宽限制
    snprintf(command, sizeof(command), "tc qdisc add dev %s root handle 1: htb default 30", interface);
    magic_network_execute_command_silent(command);
    
    snprintf(command, sizeof(command), "tc class add dev %s parent 1: classid 1:1 htb rate %dkbit", 
             interface, limit_kbps);
    magic_network_execute_command_silent(command);
    
    snprintf(command, sizeof(command), "tc class add dev %s parent 1:1 classid 1:30 htb rate %dkbit", 
             interface, limit_kbps);
    return magic_network_execute_command_silent(command);
}

/**
 * 移除带宽限制
 */
int magic_network_remove_bandwidth_limit(const char *interface) {
    if (!interface) {
        return -1;
    }
    
    char command[128];
    snprintf(command, sizeof(command), "tc qdisc del dev %s root", interface);
    
    return magic_network_execute_command_silent(command);
}

/**
 * 验证IP地址
 */
bool magic_network_is_valid_ip(const char *ip) {
    if (!ip) {
        return false;
    }
    
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

/**
 * 验证子网掩码
 */
bool magic_network_is_valid_netmask(const char *netmask) {
    return magic_network_is_valid_ip(netmask);
}

/**
 * 执行系统命令
 */
int magic_network_execute_command(const char *command, char *output, size_t output_len) {
    if (!command) {
        return -1;
    }
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        return -1;
    }
    
    if (output && output_len > 0) {
        if (fgets(output, output_len, fp) == NULL) {
            output[0] = '\0';
        }
    }
    
    int result = pclose(fp);
    return WEXITSTATUS(result);
}

/**
 * 静默执行系统命令
 */
int magic_network_execute_command_silent(const char *command) {
    return magic_network_execute_command(command, NULL, 0);
}

/**
 * 记录网络错误
 */
void magic_network_log_error(const char *function, int error_code, const char *message) {
    magic_client_log("ERROR", "[%s] Error %d: %s", function, error_code, message);
}

/**
 * 获取错误字符串
 */
const char* magic_network_get_error_string(int error_code) {
    return strerror(abs(error_code));
}

// 内部函数实现

/**
 * 解析接口信息
 */
static int parse_interface_info(const char *interface_name, network_interface_t *info) {
    if (!interface_name || !info) {
        return -1;
    }
    
    memset(info, 0, sizeof(network_interface_t));
    strncpy(info->name, interface_name, sizeof(info->name) - 1);
    
    // 获取接口状态
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    // 获取接口标志
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0) {
        info->is_up = (ifr.ifr_flags & IFF_UP) != 0;
    }
    
    // 获取IP地址
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_addr;
        inet_ntop(AF_INET, &addr->sin_addr, info->current_ip, sizeof(info->current_ip));
    }
    
    // 获取子网掩码
    if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_netmask;
        inet_ntop(AF_INET, &addr->sin_addr, info->current_netmask, sizeof(info->current_netmask));
    }
    
    // 获取MAC地址
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
        unsigned char *mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
        snprintf(info->mac_address, sizeof(info->mac_address),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    
    close(sockfd);
    
    // 确定接口类型
    if (strncmp(interface_name, "eth", 3) == 0) {
        info->type = INTERFACE_TYPE_ETHERNET;
    } else if (strncmp(interface_name, "wlan", 4) == 0 || 
               strncmp(interface_name, "wifi", 4) == 0) {
        info->type = INTERFACE_TYPE_WIFI;
    } else {
        info->type = INTERFACE_TYPE_VIRTUAL;
    }
    
    return 0;
}

/**
 * 备份DNS配置
 */
static int backup_dns_config(char dns[][MAX_IP_ADDR_LEN]) {
    FILE *resolv_conf = fopen("/etc/resolv.conf", "r");
    if (!resolv_conf) {
        return -1;
    }
    
    char line[256];
    int dns_count = 0;
    
    while (fgets(line, sizeof(line), resolv_conf) && dns_count < 2) {
        if (strncmp(line, "nameserver", 10) == 0) {
            char *ip = line + 11; // 跳过"nameserver "
            char *end = strchr(ip, '\n');
            if (end) *end = '\0';
            
            strncpy(dns[dns_count], ip, MAX_IP_ADDR_LEN - 1);
            dns[dns_count][MAX_IP_ADDR_LEN - 1] = '\0';
            dns_count++;
        }
    }
    
    fclose(resolv_conf);
    return 0;
}

/**
 * 恢复DNS配置
 */
static int restore_dns_config(char dns[][MAX_IP_ADDR_LEN]) {
    if (strlen(dns[0]) == 0) {
        return -1;
    }
    
    return magic_network_set_dns_servers(dns[0], 
                                        strlen(dns[1]) > 0 ? dns[1] : NULL);
}