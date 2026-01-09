#include "../../include/netmgmt/magic_netmgmt.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* 网络接口名称 */
static const char *link_interfaces[] = {
    "eth0",  /* 以太网 */
    "wlan0", /* WiFi */
    "ppp0",  /* 蜂窝 */
    "sat0"   /* 卫星 */
};

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 执行系统命令 */
static int execute_command(const char *cmd)
{
    MAGIC_LOG(FD_LOG_DEBUG, "执行命令: %s", cmd);
    return system(cmd);
}

/* 初始化网络管理模块 */
int netmgmt_init(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "初始化网络管理模块");
    return MAGIC_OK;
}

/* 清理网络管理模块 */
void netmgmt_cleanup(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "清理网络管理模块");
    pthread_mutex_destroy(&g_mutex);
}

/* 添加路由 */
int netmgmt_add_route(const char *client_ip, int link_id)
{
    char cmd[256];
    int ret;
    
    if (!client_ip || link_id < 1 || link_id > 4) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_mutex);
    
    /* 删除现有路由 */
    snprintf(cmd, sizeof(cmd), "ip route del %s 2>/dev/null", client_ip);
    execute_command(cmd);
    
    /* 添加新路由 */
    snprintf(cmd, sizeof(cmd), "ip route add %s dev %s", 
             client_ip, link_interfaces[link_id - 1]);
    ret = execute_command(cmd);
    
    pthread_mutex_unlock(&g_mutex);
    
    if (ret != 0) {
        MAGIC_ERROR("添加路由失败: %s -> %s", client_ip, link_interfaces[link_id - 1]);
        return MAGIC_ERROR_GENERAL;
    }
    
    MAGIC_LOG(FD_LOG_NOTICE, "添加路由: %s -> %s", client_ip, link_interfaces[link_id - 1]);
    return MAGIC_OK;
}

/* 删除路由 */
int netmgmt_remove_route(const char *client_ip)
{
    char cmd[256];
    int ret;
    
    if (!client_ip) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_mutex);
    
    /* 删除路由 */
    snprintf(cmd, sizeof(cmd), "ip route del %s", client_ip);
    ret = execute_command(cmd);
    
    pthread_mutex_unlock(&g_mutex);
    
    if (ret != 0) {
        MAGIC_ERROR("删除路由失败: %s", client_ip);
        return MAGIC_ERROR_GENERAL;
    }
    
    MAGIC_LOG(FD_LOG_NOTICE, "删除路由: %s", client_ip);
    return MAGIC_OK;
}

/* 添加防火墙规则 */
int netmgmt_add_firewall_rule(const char *client_ip, int link_id)
{
    char cmd[512];
    int ret;
    
    if (!client_ip || link_id < 1 || link_id > 4) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_mutex);
    
    /* 删除现有规则 */
    snprintf(cmd, sizeof(cmd), 
             "iptables -D FORWARD -s %s -j ACCEPT 2>/dev/null", client_ip);
    execute_command(cmd);
    
    snprintf(cmd, sizeof(cmd), 
             "iptables -D FORWARD -d %s -j ACCEPT 2>/dev/null", client_ip);
    execute_command(cmd);
    
    /* 添加新规则 */
    snprintf(cmd, sizeof(cmd), 
             "iptables -I FORWARD -s %s -o %s -j ACCEPT", 
             client_ip, link_interfaces[link_id - 1]);
    ret = execute_command(cmd);
    
    if (ret == 0) {
        snprintf(cmd, sizeof(cmd), 
                 "iptables -I FORWARD -d %s -i %s -j ACCEPT", 
                 client_ip, link_interfaces[link_id - 1]);
        ret = execute_command(cmd);
    }
    
    /* 启用NAT */
    if (ret == 0) {
        snprintf(cmd, sizeof(cmd), 
                 "iptables -t nat -I POSTROUTING -s %s -o %s -j MASQUERADE", 
                 client_ip, link_interfaces[link_id - 1]);
        ret = execute_command(cmd);
    }
    
    pthread_mutex_unlock(&g_mutex);
    
    if (ret != 0) {
        MAGIC_ERROR("添加防火墙规则失败: %s -> %s", client_ip, link_interfaces[link_id - 1]);
        return MAGIC_ERROR_GENERAL;
    }
    
    MAGIC_LOG(FD_LOG_NOTICE, "添加防火墙规则: %s -> %s", client_ip, link_interfaces[link_id - 1]);
    return MAGIC_OK;
}

/* 删除防火墙规则 */
int netmgmt_remove_firewall_rule(const char *client_ip)
{
    char cmd[512];
    int ret;
    
    if (!client_ip) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_mutex);
    
    /* 删除规则 */
    snprintf(cmd, sizeof(cmd), 
             "iptables -D FORWARD -s %s -j ACCEPT", client_ip);
    ret = execute_command(cmd);
    
    snprintf(cmd, sizeof(cmd), 
             "iptables -D FORWARD -d %s -j ACCEPT", client_ip);
    execute_command(cmd);
    
    /* 删除NAT规则 */
    snprintf(cmd, sizeof(cmd), 
             "iptables -t nat -D POSTROUTING -s %s -j MASQUERADE", client_ip);
    execute_command(cmd);
    
    pthread_mutex_unlock(&g_mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "删除防火墙规则: %s", client_ip);
    return MAGIC_OK;
}