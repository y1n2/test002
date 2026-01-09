/**
 * @file magic_dataplane.c
 * @brief MAGIC 数据平面路由模块实现
 * @description 使用 Linux 策略路由和 iptables mangle 表将客户端流量路由到指定链路
 *
 * ARINC 839 合规实现原理 (Mark Based Routing):
 * 1. 系统初始化: 创建基于 fwmark 的静态策略路由规则 (ip rule add fwmark N lookup N)
 * 2. 系统初始化: 创建基于 mark 的静态 NAT 规则
 * 3. 链路注册: 为每个链路创建路由表 (ip route add default via <gw> dev <if> table N)
 * 4. 会话建立: 根据 TFT 五元组动态添加 mangle PREROUTING 打标规则
 * 5. 会话建立: 添加 filter FORWARD 精确放行规则 (TFT 五元组)
 * 6. 会话关闭: 删除对应的 mangle 和 filter 规则，清理 conntrack
 *
 * @author MAGIC System Development Team
 * @version 2.0
 * @date 2025-12-26
 */

#include "magic_dataplane.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*===========================================================================
 * 内部辅助函数
 *===========================================================================*/

/**
 * @brief 执行系统命令并返回退出码
 */
int magic_dataplane_exec_cmd(const char* cmd)
{
    if (!cmd) return -1;
    
    fd_log_debug("[dataplane] 执行命令: %s", cmd);
    
    int ret = system(cmd);
    if (ret == -1) {
        fd_log_error("[dataplane] system() 执行失败: %s", strerror(errno));
        return -1;
    }
    
    int exit_code = WEXITSTATUS(ret);
    if (exit_code != 0) {
        fd_log_debug("[dataplane] 命令退出码: %d", exit_code);
    }
    
    return exit_code;
}

/**
 * @brief 检查网络接口是否存在
 */
bool magic_dataplane_interface_exists(const char* interface_name)
{
    if (!interface_name) return false;
    
    /* 使用 if_nametoindex 检查接口是否存在 */
    unsigned int idx = if_nametoindex(interface_name);
    return (idx != 0);
}

/*=========================================================================
 * ipset helpers
 *-------------------------------------------------------------------------
 * 使用 ipset 管理两个集合:
 *   - magic_control : MCAR 阶段注册 (control 白名单)
 *   - magic_data    : MCCR 成功放行 (data 白名单)
 *
 * 这些函数使用 `ipset` 命令并依赖于 magic_dataplane_exec_cmd 执行。
 *=======================================================================*/

int magic_dataplane_ipset_init(DataplaneContext* ctx)
{
    (void)ctx;
    char cmd[MAX_CMD_LEN];

    /* 创建 control 集合 (hash:ip)，如果已存在则忽略 */
    snprintf(cmd, sizeof(cmd), "ipset create magic_control hash:ip family inet -exist");
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] ipset create magic_control 失败");
        /* 继续尝试创建 data 集合 */
    }

    /* 清空 control 集合中的历史条目 */
    snprintf(cmd, sizeof(cmd), "ipset flush magic_control");
    magic_dataplane_exec_cmd(cmd);
    fd_log_notice("[dataplane] ✓ ipset: magic_control 已准备（历史条目已清空）");

    /* 创建 data 集合 */
    snprintf(cmd, sizeof(cmd), "ipset create magic_data hash:ip family inet -exist");
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] ipset create magic_data 失败");
        return -1;
    }

    /* 清空 data 集合中的历史条目 */
    snprintf(cmd, sizeof(cmd), "ipset flush magic_data");
    magic_dataplane_exec_cmd(cmd);
    fd_log_notice("[dataplane] ✓ ipset: magic_data 已准备（历史条目已清空）");
    
    return 0;
}

int magic_dataplane_ipset_destroy(DataplaneContext* ctx)
{
    (void)ctx;
    char cmd[MAX_CMD_LEN];

    /* 销毁集合，忽略不存在的错误 */
    snprintf(cmd, sizeof(cmd), "ipset destroy magic_control 2>/dev/null");
    magic_dataplane_exec_cmd(cmd);
    snprintf(cmd, sizeof(cmd), "ipset destroy magic_data 2>/dev/null");
    magic_dataplane_exec_cmd(cmd);

    fd_log_notice("[dataplane] ✓ ipset: magic_control/magic_data 已销毁");
    return 0;
}

int magic_dataplane_ipset_add_control(const char* client_ip)
{
    if (!client_ip) return -1;
    char cmd[MAX_CMD_LEN];

    /* 使用 -exist 保证幂等 */
    snprintf(cmd, sizeof(cmd), "ipset add magic_control %s -exist", client_ip);
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] ipset add magic_control %s 失败", client_ip);
        return -1;
    }
    fd_log_notice("[dataplane] ✓ ipset add magic_control: %s", client_ip);
    return 0;
}

int magic_dataplane_ipset_add_data(const char* client_ip)
{
    if (!client_ip) return -1;
    char cmd[MAX_CMD_LEN];

    snprintf(cmd, sizeof(cmd), "ipset add magic_data %s -exist", client_ip);
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] ipset add magic_data %s 失败", client_ip);
        return -1;
    }
    fd_log_notice("[dataplane] ✓ ipset add magic_data: %s", client_ip);
    return 0;
}

int magic_dataplane_ipset_del(const char* client_ip)
{
    if (!client_ip) return -1;
    char cmd[MAX_CMD_LEN];

    /* 从 control 和 data 集合中删除（忽略不存在） */
    snprintf(cmd, sizeof(cmd), "ipset del magic_control %s 2>/dev/null", client_ip);
    magic_dataplane_exec_cmd(cmd);
    snprintf(cmd, sizeof(cmd), "ipset del magic_data %s 2>/dev/null", client_ip);
    magic_dataplane_exec_cmd(cmd);

    fd_log_notice("[dataplane] ✓ ipset del (all): %s", client_ip);
    return 0;
}


/**
 * @brief 获取网络接口的 IP 地址
 * @param interface_name 接口名称
 * @param ip_buf 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return 0 成功, -1 失败
 */
static int get_interface_ip(const char* interface_name, char* ip_buf, size_t buf_len)
{
    if (!interface_name || !ip_buf || buf_len == 0) return -1;
    
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        close(fd);
        return -1;
    }
    close(fd);
    
    struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
    const char* ip_str = inet_ntoa(addr->sin_addr);
    if (ip_str) {
        strncpy(ip_buf, ip_str, buf_len - 1);
        ip_buf[buf_len - 1] = '\0';
        return 0;
    }
    return -1;
}

/**
 * @brief 查找空闲的规则槽位
 */
static ClientRoutingRule* find_free_rule_slot(DataplaneContext* ctx)
{
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (!ctx->rules[i].in_use) {
            return &ctx->rules[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查某个客户端 IP 是否有其他活跃会话（排除指定会话）
 * 
 * @param ctx 数据平面上下文
 * @param client_ip 客户端 IP
 * @param exclude_session_id 要排除的会话 ID（NULL 表示不排除任何会话）
 * @return 该 IP 的其他活跃会话数量
 */
static int count_client_ip_sessions(DataplaneContext* ctx, const char* client_ip, 
                                    const char* exclude_session_id)
{
    int count = 0;
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (ctx->rules[i].in_use && 
            strcmp(ctx->rules[i].client_ip, client_ip) == 0) {
            /* 如果指定了要排除的会话，跳过它 */
            if (exclude_session_id && 
                strcmp(ctx->rules[i].session_id, exclude_session_id) == 0) {
                continue;
            }
            count++;
        }
    }
    return count;
}

/**
 * @brief 根据链路 ID 查找路由配置
 */
static LinkRouteConfig* find_link_config(DataplaneContext* ctx, const char* link_id)
{
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        if (strcmp(ctx->links[i].link_id, link_id) == 0) {
            return &ctx->links[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * 路由表管理
 *===========================================================================*/

/**
 * @brief 创建链路的路由表
 * 执行: ip route add default via <gateway> dev <interface> table <table_id>
 * 
 * 注意: 如果网关 IP 等于接口自身 IP，则使用直连路由（无网关）
 */
static int create_route_table(LinkRouteConfig* link)
{
    char cmd[MAX_CMD_LEN];
    char interface_ip[MAX_IP_ADDR_LEN] = {0};
    bool use_direct_route = false;
    
    if (!link || !link->interface_name[0]) {
        fd_log_error("[dataplane] 无效的链路配置");
        return -1;
    }
    
    /* 确保接口启动 */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", link->interface_name);
    if (magic_dataplane_exec_cmd(cmd) == 0) {
        fd_log_notice("[dataplane] ✓ 启动接口: %s", link->interface_name);
    } else {
        fd_log_error("[dataplane] ✗ 启动接口失败: %s", link->interface_name);
    }
    
    /* 清理可能存在的旧路由 */
    snprintf(cmd, sizeof(cmd), "ip route flush table %u 2>/dev/null", link->route_table_id);
    magic_dataplane_exec_cmd(cmd);
    
    /* 检查网关 IP 是否等于接口 IP（测试环境：服务器和DLM在同一机器） */
    if (link->gateway_ip[0]) {
        if (get_interface_ip(link->interface_name, interface_ip, sizeof(interface_ip)) == 0) {
            if (strcmp(link->gateway_ip, interface_ip) == 0) {
                fd_log_notice("[dataplane] 网关 IP (%s) = 接口 IP (测试环境)，使用直连路由", 
                              link->gateway_ip);
                use_direct_route = true;
            }
        }
    }
    
    /* 创建默认路由 */
    if (link->gateway_ip[0] && !use_direct_route) {
        /* 有网关的情况，使用 onlink 参数强制添加 */
        snprintf(cmd, sizeof(cmd), 
                 "ip route add default via %s dev %s table %u onlink",
                 link->gateway_ip, link->interface_name, link->route_table_id);
    } else {
        /* 直连路由 (无网关 或 网关=接口IP 的测试环境) */
        snprintf(cmd, sizeof(cmd), 
                 "ip route add default dev %s table %u",
                 link->interface_name, link->route_table_id);
    }
    
    fd_log_notice("[dataplane] 执行命令: %s", cmd);
    
    int ret = magic_dataplane_exec_cmd(cmd);
    if (ret != 0) {
        fd_log_error("[dataplane] ✗ 创建路由表失败: %s", cmd);
        fd_log_error("[dataplane]   接口=%s, 网关=%s, 表=%u", 
                     link->interface_name, 
                     link->gateway_ip[0] ? link->gateway_ip : "无",
                     link->route_table_id);
        /* 检查接口状态 */
        snprintf(cmd, sizeof(cmd), "ip link show %s 2>&1", link->interface_name);
        magic_dataplane_exec_cmd(cmd);
        return -1;
    }
    
    link->is_configured = true;
    fd_log_notice("[dataplane] ✓ 创建路由表 %u: %s via %s (gw=%s, mode=%s)",
                  link->route_table_id, link->link_id, link->interface_name,
                  link->gateway_ip[0] ? link->gateway_ip : "无",
                  use_direct_route ? "直连" : (link->gateway_ip[0] ? "网关" : "直连"));
    
    return 0;
}

/**
 * @brief 删除链路的路由表
 */
static int delete_route_table(LinkRouteConfig* link)
{
    char cmd[MAX_CMD_LEN];
    
    if (!link) return -1;
    
    snprintf(cmd, sizeof(cmd), "ip route flush table %u 2>/dev/null", link->route_table_id);
    magic_dataplane_exec_cmd(cmd);
    
    link->is_configured = false;
    fd_log_notice("[dataplane] 删除路由表 %u: %s", link->route_table_id, link->link_id);
    
    return 0;
}

/*===========================================================================
 * IP Rule 管理
 *===========================================================================*/

/**
 * @brief 添加 IP 策略规则
 * 执行: ip rule add from <client_ip> lookup <table_id> priority <priority>
 */
static int add_ip_rule(const char* client_ip, uint32_t table_id, uint32_t priority)
{
    char cmd[MAX_CMD_LEN];
    
    snprintf(cmd, sizeof(cmd),
             "ip rule add from %s lookup %u priority %u",
             client_ip, table_id, priority);
    
    int ret = magic_dataplane_exec_cmd(cmd);
    if (ret != 0) {
        fd_log_error("[dataplane] 添加 ip rule 失败: from %s lookup %u", client_ip, table_id);
        return -1;
    }
    
    fd_log_notice("[dataplane] ✓ 添加 ip rule: from %s lookup %u prio %u",
                  client_ip, table_id, priority);
    
    return 0;
}

/**
 * @brief 删除 IP 策略规则
 * 执行: ip rule del from <client_ip> lookup <table_id>
 */
static int delete_ip_rule(const char* client_ip, uint32_t table_id)
{
    char cmd[MAX_CMD_LEN];
    
    snprintf(cmd, sizeof(cmd),
             "ip rule del from %s lookup %u 2>/dev/null",
             client_ip, table_id);
    
    int ret = magic_dataplane_exec_cmd(cmd);
    if (ret != 0) {
        fd_log_debug("[dataplane] 删除 ip rule 失败 (可能不存在): from %s lookup %u", 
                     client_ip, table_id);
        return -1;
    }
    
    fd_log_notice("[dataplane] ✓ 删除 ip rule: from %s lookup %u", client_ip, table_id);
    
    return 0;
}

/*===========================================================================
 * 流量控制 (使用 fwmark + mangle 表)
 * ARINC 839 合规: 基于 TFT 五元组的精确流量控制
 * 
 * 实现原理:
 * - 使用 fwmark 路由规则 (ip rule add fwmark N lookup N) - 静态配置
 * - 使用 mangle PREROUTING 打标 - 动态配置 (基于 TFT)
 * - 使用 filter FORWARD 放行 - 动态配置 (基于 TFT)
 * - 使用专用路由表 (MAGIC_FWMARK_BLACKHOLE=99) 存放黑洞路由
 *===========================================================================*/

#define MAGIC_BLACKHOLE_TABLE     MAGIC_FWMARK_BLACKHOLE

/**
 * @brief 初始化基于 fwmark 的静态策略路由规则
 * 这些规则在系统初始化时创建一次，不需要动态增删
 */
static int init_fwmark_rules(DataplaneContext* ctx)
{
    char cmd[MAX_CMD_LEN];
    
    fd_log_notice("[dataplane] 初始化 fwmark 策略路由规则 (ARINC 839 合规)...");
    
    /* 清理可能存在的旧 fwmark 规则 */
    for (uint32_t mark = MAGIC_FWMARK_BLACKHOLE; mark <= MAGIC_RT_TABLE_MAX; mark++) {
        snprintf(cmd, sizeof(cmd), "ip rule del fwmark %u 2>/dev/null", mark);
        magic_dataplane_exec_cmd(cmd);
    }
    
    /* 创建黑洞路由表 */
    snprintf(cmd, sizeof(cmd), "ip route flush table %d 2>/dev/null", MAGIC_BLACKHOLE_TABLE);
    magic_dataplane_exec_cmd(cmd);
    snprintf(cmd, sizeof(cmd), "ip route add blackhole default table %d", MAGIC_BLACKHOLE_TABLE);
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] 创建黑洞路由表失败");
        return -1;
    }
    fd_log_notice("[dataplane] ✓ 黑洞路由表 (table=%d)", MAGIC_BLACKHOLE_TABLE);
    
    /* 创建黑洞 fwmark 规则 */
    snprintf(cmd, sizeof(cmd), 
             "ip rule add fwmark %d lookup %d priority %d",
             MAGIC_BLACKHOLE_TABLE, MAGIC_BLACKHOLE_TABLE, MAGIC_BLACKHOLE_PRIORITY);
    magic_dataplane_exec_cmd(cmd);
    fd_log_notice("[dataplane] ✓ fwmark %d → blackhole", MAGIC_BLACKHOLE_TABLE);
    
    /* 预创建链路 fwmark 规则 (100-109) */
    for (uint32_t i = 0; i < MAX_DATAPLANE_LINKS; i++) {
        uint32_t mark = MAGIC_RT_TABLE_BASE + i;
        snprintf(cmd, sizeof(cmd),
                 "ip rule add fwmark %u lookup %u priority %u",
                 mark, mark, mark);
        if (magic_dataplane_exec_cmd(cmd) == 0) {
            fd_log_notice("[dataplane] ✓ fwmark %u → table %u", mark, mark);
        }
    }
    
    ctx->fwmark_rules_installed = true;
    return 0;
}

/**
 * @brief 初始化基于 mark 的静态 NAT 规则
 */
static int init_mark_based_nat(void)
{
    char cmd[MAX_CMD_LEN];
    
    fd_log_notice("[dataplane] 初始化基于 mark 的 NAT 规则...");
    
    /* 清理旧的 mark 匹配 NAT 规则 */
    for (uint32_t mark = MAGIC_RT_TABLE_BASE; mark < MAGIC_RT_TABLE_BASE + MAX_DATAPLANE_LINKS; mark++) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -t nat -D POSTROUTING -m mark --mark %u -j MASQUERADE 2>/dev/null",
                 mark);
        magic_dataplane_exec_cmd(cmd);
    }
    
    /* 创建基于 mark 的 NAT 规则 */
    for (uint32_t mark = MAGIC_RT_TABLE_BASE; mark < MAGIC_RT_TABLE_BASE + MAX_DATAPLANE_LINKS; mark++) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -t nat -A POSTROUTING -m mark --mark %u -j MASQUERADE",
                 mark);
        if (magic_dataplane_exec_cmd(cmd) == 0) {
            fd_log_notice("[dataplane] ✓ NAT: mark %u → MASQUERADE", mark);
        }
    }
    
    return 0;
}

/**
 * @brief 删除客户端的所有 iptables 规则
 * 清理 ACCEPT 和 DROP 规则
 *
 * 如果提供了 gateway_ip，则优先尝试删除形如
 *   iptables -D OUTPUT -s <client> -d <gateway> -j ACCEPT
 * 的精确规则（setup_client_link_access 插入的是带 -d 的规则）。
 */
static int remove_client_iptables_rules(const char* client_ip, const char* gateway_ip)
{
    char cmd[MAX_CMD_LEN];
    (void)gateway_ip;  /* gateway_ip 参数保留用于向后兼容，但实际不再需要 */

    /* 使用 iptables-save/iptables-restore 高效删除所有包含该 client_ip 的规则
     * 这种方法可以删除任何形式的规则：
     * - iptables ... -s <client_ip> -j ACCEPT
     * - iptables ... -s <client_ip> -d <any_dest> -j ACCEPT
     * - iptables ... -s <client_ip> -d <any_dest> -p <proto> --dport <port> -j ACCEPT
     * 不管目的IP/端口/协议是什么，只要源IP匹配就删除
     */
    
    /* 删除 OUTPUT 链中所有源为 client_ip 的规则（任意目的地） */
    snprintf(cmd, sizeof(cmd),
             "iptables-save | grep -v -- '-A OUTPUT.*-s %s' | iptables-restore 2>/dev/null",
             client_ip);
    magic_dataplane_exec_cmd(cmd);
    
    /* 删除 FORWARD 链中所有源为 client_ip 的规则（任意目的地） */
    snprintf(cmd, sizeof(cmd),
             "iptables-save | grep -v -- '-A FORWARD.*-s %s' | iptables-restore 2>/dev/null",
             client_ip);
    magic_dataplane_exec_cmd(cmd);
    
    /* 删除 FORWARD 链中所有目的为 client_ip 的规则（回程流量） */
    snprintf(cmd, sizeof(cmd),
             "iptables-save | grep -v -- '-A FORWARD.*-d %s' | iptables-restore 2>/dev/null",
             client_ip);
    magic_dataplane_exec_cmd(cmd);

    /* 删除 NAT 表中该客户端的所有 MASQUERADE 规则
     * 使用 iptables-save/restore 方式，因为添加时可能带有 -d 参数 */
    snprintf(cmd, sizeof(cmd),
             "iptables-save -t nat | grep -v -- '-A POSTROUTING.*-s %s.*MASQUERADE' | iptables-restore 2>/dev/null",
             client_ip);
    magic_dataplane_exec_cmd(cmd);

    fd_log_notice("[dataplane] ✓ 清理所有 iptables 规则: %s (包括精确目的IP规则和NAT)", client_ip);
    return 0;
}

/**
 * @brief 删除客户端的黑洞规则，允许流量通过
 * 删除: ip rule
 * 
 * 注意: 不再在这里清理 iptables 规则，因为同一 IP 可能有多个会话。
 * iptables 清理由 setup_client_link_access 控制。
 */
static int remove_blackhole_rule(const char* client_ip, const char* gateway_ip)
{
    char cmd[MAX_CMD_LEN];
    (void)gateway_ip;  /* 不再使用 */
    
    /* 删除可能存在的黑洞路由规则 */
    snprintf(cmd, sizeof(cmd),
             "ip rule del from %s lookup %d priority %d 2>/dev/null",
             client_ip, MAGIC_BLACKHOLE_TABLE, MAGIC_BLACKHOLE_PRIORITY);
    magic_dataplane_exec_cmd(cmd);
    
    /* 不再在这里清理 iptables 规则，避免删除其他会话的 TFT 规则 */
    
    fd_log_debug("[dataplane] 删除黑洞规则: %s (ip rule only)", client_ip);
    return 0;
}

/**
 * @brief 设置客户端的链路访问规则
 * 
 * 实现精确的流量控制：
 * - 在网段级 DROP 规则之前插入 ACCEPT 规则
 * - 如果提供了 dest_ip，则只允许访问该精确 IP
 * - 如果未提供 dest_ip，则允许访问整个链路网段（从 gateway_ip 推导）
 * 
 * @param client_ip 客户端 IP
 * @param gateway_ip 链路网关 IP（用于推导网段）
 * @param dest_ip 精确目的 IP（NULL = 允许整个网段）
 * @param skip_cleanup 是否跳过清理步骤（当同一 IP 有多个会话时使用）
 */
static int setup_client_link_access(const char* client_ip, const char* gateway_ip, 
                                    const char* dest_ip, bool skip_cleanup)
{
    char cmd[MAX_CMD_LEN];
    char target[MAX_IP_ADDR_LEN];
    
    /* 只有在没有其他会话使用该 IP 时才清理规则 */
    if (!skip_cleanup) {
        remove_client_iptables_rules(client_ip, gateway_ip);
    } else {
        fd_log_notice("[dataplane] ℹ 跳过清理: 该 IP 有其他活跃会话");
    }
    
    /* 确定目标地址：精确 IP 或网段 */
    if (dest_ip && dest_ip[0]) {
        /* 使用 TFT 中指定的精确目的 IP */
        strncpy(target, dest_ip, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        fd_log_notice("[dataplane] 使用精确目的 IP: %s", target);
    } else if (gateway_ip && gateway_ip[0]) {
        /* 从网关 IP 推导网段（/24）*/
        strncpy(target, gateway_ip, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        char* last_dot = strrchr(target, '.');
        if (last_dot) {
            strcpy(last_dot + 1, "0/24");
            fd_log_notice("[dataplane] 使用链路网段: %s", target);
        } else {
            fd_log_error("[dataplane] 无效的网关 IP: %s", gateway_ip);
            return -1;
        }
    } else {
        fd_log_error("[dataplane] 缺少目的地址信息");
        return -1;
    }
    
    /* 在 OUTPUT 链最前面插入 ACCEPT 规则 (在网段 DROP 之前) */
    snprintf(cmd, sizeof(cmd),
             "iptables -I OUTPUT 1 -s %s -d %s -j ACCEPT",
             client_ip, target);
    int ret = magic_dataplane_exec_cmd(cmd);
    if (ret == 0) {
        fd_log_notice("[dataplane] ✓ iptables OUTPUT: %s → %s", client_ip, target);
    }
    
    /* ★★★ ARINC 839 安全合规修改 ★★★
     * 
     * 不再在 MCAR 阶段添加宽松的 FORWARD ACCEPT 规则！
     * 
     * 原因: MCAR 阶段只是控制层面的授权,不应该开放数据平面
     * FORWARD 规则应该在 MCCR 阶段根据授权的 TFT 五元组精确添加
     * 
     * 这确保了:
     * 1. 只有符合 TFT 白名单的流量才能通过
     * 2. 客户端不能绕过 TFT 规则发送任意端口的流量
     * 
     * 之前的代码在这里添加了:
     *   iptables -I FORWARD 1 -s <client> -d <target> -j ACCEPT
     * 这太宽松了,允许了所有端口的流量通过
     */
    fd_log_notice("[dataplane] ℹ FORWARD 规则将在 MCCR 阶段根据 TFT 添加");
    
    /* 回程流量规则 - 保留,因为这是状态相关的回复流量 */
    snprintf(cmd, sizeof(cmd),
             "iptables -I FORWARD 1 -s %s -d %s -j ACCEPT",
             target, client_ip);
    magic_dataplane_exec_cmd(cmd);
    fd_log_notice("[dataplane] ✓ 回程流量: %s → %s ACCEPT", target, client_ip);
    
    /* 添加 SNAT/MASQUERADE 规则，将客户端流量的源IP转换为出口接口IP
     * 这是必须的，因为客户端网段(如192.168.126.0/24)与目标网段(如10.2.2.0/24)不同
     * 目标设备收到数据包后需要知道如何回复，MASQUERADE会自动选择出口接口的IP */
    snprintf(cmd, sizeof(cmd),
             "iptables -t nat -A POSTROUTING -s %s -d %s -j MASQUERADE",
             client_ip, target);
    ret = magic_dataplane_exec_cmd(cmd);
    if (ret == 0) {
        fd_log_notice("[dataplane] ✓ NAT MASQUERADE: %s → %s", client_ip, target);
    } else {
        fd_log_error("[dataplane] ✗ NAT MASQUERADE 失败: %s → %s", client_ip, target);
    }
    
    return 0;
}

/**
 * @brief 添加客户端的黑洞规则，阻断所有流量
 * 
 * 会话关闭时调用：
 * 1. 删除该客户端的 ACCEPT 规则（使网段 DROP 规则生效）
 * 2. 添加 ip rule 黑洞路由（用于外部流量）
 */
static int add_blackhole_rule(const char* client_ip, const char* gateway_ip)
{
    char cmd[MAX_CMD_LEN];
    
    /* 先删除可能存在的旧规则（包括 ACCEPT 规则） */
    remove_blackhole_rule(client_ip, gateway_ip);
    
    /* 1. 添加黑洞路由规则 (用于外部流量) */
    snprintf(cmd, sizeof(cmd),
             "ip rule add from %s lookup %d priority %d",
             client_ip, MAGIC_BLACKHOLE_TABLE, MAGIC_BLACKHOLE_PRIORITY);
    
    int ret = magic_dataplane_exec_cmd(cmd);
    if (ret != 0) {
        fd_log_error("[dataplane] 添加黑洞规则失败: %s", client_ip);
        return -1;
    }
    
    /* 不需要单独添加 iptables DROP 规则，因为网段级 DROP 已存在 */
    /* remove_blackhole_rule 已经删除了 ACCEPT 规则，所以网段 DROP 会生效 */
    
    fd_log_notice("[dataplane] ✓ 添加黑洞规则: %s → blackhole (流量已阻断)", client_ip);
    return 0;
}

/*===========================================================================
 * 公共 API 实现
 *===========================================================================*/


int magic_dataplane_init(DataplaneContext* ctx, 
                         const char* ingress_if, 
                         const char* ingress_ip)
{
    if (!ctx) {
        fd_log_error("[dataplane] 上下文指针为空");
        return -1;
    }
    
    /* 初始化结构体 */
    memset(ctx, 0, sizeof(DataplaneContext));
    pthread_mutex_init(&ctx->mutex, NULL);
    
    /* 设置入口接口 */
    if (ingress_if) {
        strncpy(ctx->ingress_interface, ingress_if, MAX_IF_NAME_LEN - 1);
    }
    if (ingress_ip) {
        strncpy(ctx->ingress_ip, ingress_ip, MAX_IP_ADDR_LEN - 1);
    }
    
    /* 初始化优先级计数器 */
    ctx->next_priority = MAGIC_RULE_PRIORITY_BASE;
    
    /* 验证入口接口 */
    if (ingress_if && !magic_dataplane_interface_exists(ingress_if)) {
        fd_log_error("[dataplane] 入口接口不存在: %s", ingress_if);
        /* 不返回错误，允许接口稍后创建 */
    }
    
    ctx->is_initialized = true;
    ctx->enable_routing = true;
    
    /* === 第一步：初始化 fwmark 静态路由规则 (ARINC 839 合规) === */
    fd_log_notice("[dataplane] [ARINC 839] 初始化 Mark Based Routing...");
    init_fwmark_rules(ctx);
    
    /* === 第二步：初始化基于 fwmark 的 NAT 规则 === */
    init_mark_based_nat();
    
    /* === 第三步：清理所有残留的客户端网段规则 === */
    fd_log_notice("[dataplane] 清理客户端网段残留规则...");
    
    /* 清理 iptables 中所有 192.168.126.0/24 相关规则 */
    magic_dataplane_exec_cmd("iptables-save | grep -v '192\\.168\\.126\\.' | iptables-restore 2>/dev/null || true");
    
    /* 清理所有客户端 IP 的策略路由规则 */
    for (int i = 5; i <= 254; i++) {
        char client_ip[32];
        char cmd[MAX_CMD_LEN];
        snprintf(client_ip, sizeof(client_ip), "192.168.126.%d", i);
        /* 删除可能存在的策略路由规则 */
        snprintf(cmd, sizeof(cmd), "ip rule del from %s 2>/dev/null || true", client_ip);
        magic_dataplane_exec_cmd(cmd);
        /* 删除黑洞规则 */
        snprintf(cmd, sizeof(cmd), 
                 "ip rule del from %s lookup %d priority %d 2>/dev/null || true",
                 client_ip, MAGIC_BLACKHOLE_TABLE, MAGIC_BLACKHOLE_PRIORITY);
        magic_dataplane_exec_cmd(cmd);
    }
    
    /* 清理所有客户端 IP 的连接跟踪条目 */
    magic_dataplane_exec_cmd("conntrack -D -s 192.168.126.0/24 2>/dev/null || true");
    
    fd_log_notice("[dataplane] ✓ 客户端网段残留规则已清理");
    
    /* === 第二步：设置默认阻断规则 === */
    /* 阻断整个客户端网段 (192.168.126.0/24) 的出站和转发流量 */
    /* 只有在会话建立后，才为特定客户端放行到特定链路的流量 */
    fd_log_notice("[dataplane] 设置默认流量阻断规则...");
    
    /* OUTPUT 链：阻断本地发出的客户端流量 */
    magic_dataplane_exec_cmd("iptables -D OUTPUT -s 192.168.126.0/24 -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -A OUTPUT -s 192.168.126.0/24 -j DROP");
    fd_log_notice("[dataplane] ✓ 默认阻断 OUTPUT: 192.168.126.0/24 → DROP");
    
    /* FORWARD 链：阻断转发的客户端流量（关键！客户端包通常走这条链） */
    magic_dataplane_exec_cmd("iptables -D FORWARD -s 192.168.126.0/24 -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -A FORWARD -s 192.168.126.0/24 -j DROP");
    fd_log_notice("[dataplane] ✓ 默认阻断 FORWARD: 192.168.126.0/24 → DROP");
    
    /* INPUT 链：阻止客户端直接访问链路网关IP（防止绕过策略路由）
     * 客户端不应该能直接ping或访问网关，只能通过策略路由访问目标网段 */
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.1.1.1 -p icmp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.1.1.1 -p icmp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.2.2.2 -p icmp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.2.2.2 -p icmp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.3.3.3 -p icmp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.3.3.3 -p icmp -j DROP");
    fd_log_notice("[dataplane] ✓ 阻止客户端 ICMP 访问链路网关: 10.1.1.1, 10.2.2.2, 10.3.3.3");
    
    /* 阻止客户端访问网关的TCP/UDP服务（除了Diameter控制端口） */
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.1.1.1 -p tcp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.1.1.1 -p tcp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.2.2.2 -p tcp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.2.2.2 -p tcp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.3.3.3 -p tcp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.3.3.3 -p tcp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.1.1.1 -p udp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.1.1.1 -p udp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.2.2.2 -p udp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.2.2.2 -p udp -j DROP");
    magic_dataplane_exec_cmd("iptables -D INPUT -s 192.168.126.0/24 -d 10.3.3.3 -p udp -j DROP 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I INPUT 1 -s 192.168.126.0/24 -d 10.3.3.3 -p udp -j DROP");
    fd_log_notice("[dataplane] ✓ 阻止客户端 TCP/UDP 访问链路网关");
    
    /* 在 DROP 规则前插入服务器 IP 白名单，确保服务器自身可以发送响应 */
    if (ingress_ip && strlen(ingress_ip) > 0) {
        char whitelist_cmd[MAX_CMD_LEN];
        /* 先删除旧的白名单规则（避免重复） */
        snprintf(whitelist_cmd, sizeof(whitelist_cmd),
                 "iptables -D OUTPUT -s %s -j ACCEPT 2>/dev/null", ingress_ip);
        magic_dataplane_exec_cmd(whitelist_cmd);
        /* 在最前面插入服务器 IP 白名单（优先级高于 DROP） */
        snprintf(whitelist_cmd, sizeof(whitelist_cmd),
                 "iptables -I OUTPUT 1 -s %s -j ACCEPT", ingress_ip);
        magic_dataplane_exec_cmd(whitelist_cmd);
        fd_log_notice("[dataplane] ✓ 服务器白名单: %s → ACCEPT", ingress_ip);
    }

    /* === ARINC 839 合规: 添加连接追踪规则 === */
    /* FORWARD 链首条规则: 允许已建立连接快速通过 */
    magic_dataplane_exec_cmd("iptables -D FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -I FORWARD 1 -m state --state ESTABLISHED,RELATED -j ACCEPT");
    fd_log_notice("[dataplane] ✓ FORWARD 连接追踪: ESTABLISHED,RELATED → ACCEPT");

    /* 初始化 ipset 集合 (magic_control / magic_data) */
    magic_dataplane_ipset_init(ctx);

    /* 在 OUTPUT 链插入针对 ipset 的匹配规则，优先于网段 DROP */
    /* 注意: ARINC 839 合规 - 仅在 OUTPUT 链使用 ipset，FORWARD 链使用 TFT 精确规则 */
    magic_dataplane_exec_cmd("iptables -D OUTPUT -m set --match-set magic_data src -j ACCEPT 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -D OUTPUT -m set --match-set magic_control src -j ACCEPT 2>/dev/null");

    /* 插入位置：如果存在服务器白名单（已插入为 pos1），则插入到 pos2，否则插入到 pos1 */
    if (ingress_ip && strlen(ingress_ip) > 0) {
        /* 仅在 OUTPUT 链使用 ipset（用于控制面流量） */
        magic_dataplane_exec_cmd("iptables -I OUTPUT 2 -m set --match-set magic_control src -j ACCEPT");
        /* ARINC 839: FORWARD 链不再使用 ipset 直接放行，避免"全通"漏洞 */
        /* TFT 精确规则将在 magic_dataplane_add_tft_rule() 中动态添加 */
    } else {
        magic_dataplane_exec_cmd("iptables -I OUTPUT 1 -m set --match-set magic_control src -j ACCEPT");
    }
    
    /* === 初始化基于 fwmark 的策略路由规则 (静态) === */
    init_fwmark_rules(ctx);
    
    /* === 初始化基于 mark 的 NAT 规则 (静态) === */
    init_mark_based_nat();
    
    /* === 关键修复：确保 FORWARD 链默认策略允许 TFT 规则生效 === */
    /* 如果 FORWARD 链默认是 DROP，且没有其他规则允许，TFT 规则可能被后续规则屏蔽 */
    /* 这里我们不修改默认策略，而是确保 TFT 规则插入到正确位置 */
    
    fd_log_notice("[dataplane] ========================================");
    fd_log_notice("[dataplane] MAGIC 数据平面初始化 (ARINC 839 合规)");
    fd_log_notice("[dataplane]   入口接口: %s", ingress_if ? ingress_if : "未指定");
    fd_log_notice("[dataplane]   入口 IP: %s", ingress_ip ? ingress_ip : "未指定");
    fd_log_notice("[dataplane]   流量控制: mangle打标 + fwmark路由");
    fd_log_notice("[dataplane]   放行策略: 连接追踪 + TFT精确规则");
    fd_log_notice("[dataplane]   默认策略: 阻断 192.168.126.0/24");
    fd_log_notice("[dataplane] ========================================");
    
    return 0;
}

int magic_dataplane_register_link(DataplaneContext* ctx,
                                  const char* link_id,
                                  const char* interface_name,
                                  const char* gateway_ip)
{
    if (!ctx || !ctx->is_initialized) {
        fd_log_error("[dataplane] 数据平面未初始化");
        return -1;
    }
    
    if (!link_id || !interface_name) {
        fd_log_error("[dataplane] 链路参数无效");
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    /* 检查链路是否已注册 */
    LinkRouteConfig* existing = find_link_config(ctx, link_id);
    if (existing) {
        /* 已注册的链路不允许修改，直接返回 */
        fd_log_notice("[dataplane] 链路已注册: %s (table=%u, interface=%s)", 
                      link_id, existing->route_table_id, existing->interface_name);
        pthread_mutex_unlock(&ctx->mutex);
        return (int)existing->route_table_id;
    }
    
    /* 检查是否达到最大链路数 */
    if (ctx->num_links >= MAX_DATAPLANE_LINKS) {
        fd_log_error("[dataplane] 已达到最大链路数: %u", MAX_DATAPLANE_LINKS);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 验证接口是否存在 */
    if (!magic_dataplane_interface_exists(interface_name)) {
        fd_log_error("[dataplane] 出口接口不存在: %s", interface_name);
        /* 继续注册，允许接口稍后创建 */
    }
    
    /* 分配路由表 ID 和 fwmark (两者相同) */
    uint32_t table_id = MAGIC_RT_TABLE_BASE + ctx->num_links;
    
    /* 创建链路配置 */
    LinkRouteConfig* link = &ctx->links[ctx->num_links];
    memset(link, 0, sizeof(LinkRouteConfig));
    
    strncpy(link->link_id, link_id, MAX_LINK_ID_LEN - 1);
    strncpy(link->interface_name, interface_name, MAX_IF_NAME_LEN - 1);
    if (gateway_ip) {
        strncpy(link->gateway_ip, gateway_ip, MAX_IP_ADDR_LEN - 1);
    }
    link->route_table_id = table_id;
    link->fwmark = table_id;  /* fwmark 与 table_id 相同 */
    
    /* 先增加链路计数，确保链路被注册（即使路由表创建失败） */
    ctx->num_links++;
    
    /* 尝试创建路由表 - 失败不影响链路注册 */
    if (create_route_table(link) != 0) {
        fd_log_notice("[dataplane] ⚠ 路由表创建失败（稍后可通过 DLM 更新）: %s", link_id);
        /* 继续，不返回错误 - 链路已注册，只是未配置路由 */
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    fd_log_notice("[dataplane] ✓ 注册链路: %s → %s (table=%u, fwmark=%u, configured=%s)",
                  link_id, interface_name, table_id, link->fwmark,
                  link->is_configured ? "yes" : "no");
    
    return (int)table_id;
}

int magic_dataplane_add_client_route(DataplaneContext* ctx,
                                     const char* client_ip,
                                     const char* session_id,
                                     const char* link_id,
                                     const char* dest_ip)
{
    if (!ctx || !ctx->is_initialized) {
        fd_log_error("[dataplane] 数据平面未初始化");
        return -1;
    }
    
    if (!client_ip || !session_id || !link_id) {
        fd_log_error("[dataplane] 参数无效");
        return -1;
    }
    
    /* dest_ip 可以为 NULL，表示允许访问整个链路网段 */
    if (dest_ip && dest_ip[0]) {
        fd_log_notice("[dataplane] 目的 IP 限制: %s", dest_ip);
    } else {
        fd_log_notice("[dataplane] 无目的 IP 限制，使用链路网段");
    }
    
    if (!ctx->enable_routing) {
        fd_log_notice("[dataplane] 路由功能已禁用，跳过添加规则");
        return 0;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    /* 查找链路配置 */
    LinkRouteConfig* link = find_link_config(ctx, link_id);
    if (!link) {
        fd_log_error("[dataplane] 链路未注册: %s", link_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 如果路由表未配置，尝试重新创建 */
    if (!link->is_configured) {
        fd_log_notice("[dataplane] ⚠ 链路路由表未配置，尝试创建: %s (table=%u)", 
                      link_id, link->route_table_id);
        if (create_route_table(link) != 0) {
            fd_log_error("[dataplane] ✗ 路由表创建失败: %s (接口=%s, 网关=%s)", 
                        link_id, link->interface_name, link->gateway_ip);
            fd_log_error("[dataplane]   可能原因: 接口未就绪、网关不可达、或权限不足");
            pthread_mutex_unlock(&ctx->mutex);
            return -1;
        }
        fd_log_notice("[dataplane] ✓ 路由表创建成功: %s (table=%u)", 
                      link_id, link->route_table_id);
    }
    
    /* 检查会话是否已有规则 */
    ClientRoutingRule* existing = magic_dataplane_find_rule(ctx, session_id);
    if (existing) {
        fd_log_notice("[dataplane] 会话已有路由规则，先删除: %s", session_id);
        delete_ip_rule(existing->client_ip, existing->route_table_id);
        existing->in_use = false;
        ctx->num_rules--;
    }
    
    /* 查找空闲槽位 */
    ClientRoutingRule* rule = find_free_rule_slot(ctx);
    if (!rule) {
        fd_log_error("[dataplane] 路由规则已满: %u", MAX_ROUTING_RULES);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 分配优先级 */
    uint32_t priority = ctx->next_priority++;
    if (ctx->next_priority > MAGIC_RULE_PRIORITY_MAX) {
        ctx->next_priority = MAGIC_RULE_PRIORITY_BASE;  /* 回绕 */
    }
    
    /* 保存网关 IP 用于 iptables 规则 */
    char gateway_ip[MAX_IP_ADDR_LEN] = {0};
    if (link->gateway_ip[0]) {
        strncpy(gateway_ip, link->gateway_ip, MAX_IP_ADDR_LEN - 1);
    }
    
    /* 删除可能存在的黑洞规则 */
    remove_blackhole_rule(client_ip, gateway_ip[0] ? gateway_ip : NULL);
    
    /* ★★★ 关键修复：检查该客户端 IP 是否有其他活跃会话 ★★★
     * 如果有其他会话，不能清理 iptables 规则，否则会删除其他会话的 TFT 规则
     */
    int other_sessions = count_client_ip_sessions(ctx, client_ip, session_id);
    bool skip_cleanup = (other_sessions > 0);
    if (skip_cleanup) {
        fd_log_notice("[dataplane] ⚠ 该 IP (%s) 还有 %d 个其他活跃会话，保留现有规则",
                      client_ip, other_sessions);
    }
    
    /* 设置精确的链路访问规则：使用 TFT 目的 IP 或链路网段 */
    setup_client_link_access(client_ip, gateway_ip, dest_ip, skip_cleanup);
    
    /* 添加 ip rule */
    if (add_ip_rule(client_ip, link->route_table_id, priority) != 0) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 记录规则 */
    memset(rule, 0, sizeof(ClientRoutingRule));
    rule->in_use = true;
    strncpy(rule->client_ip, client_ip, MAX_IP_ADDR_LEN - 1);
    strncpy(rule->session_id, session_id, 63);
    strncpy(rule->link_id, link_id, MAX_LINK_ID_LEN - 1);
    rule->rule_priority = priority;
    rule->route_table_id = link->route_table_id;
    rule->created_at = time(NULL);
    
    ctx->num_rules++;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    fd_log_notice("[dataplane] ✓ 客户端路由: %s → %s (table=%u, prio=%u, 流量已允许)",
                  client_ip, link_id, link->route_table_id, priority);
    
    /* 打印诊断信息 - 验证路由配置 */
    fd_log_notice("[dataplane] === 路由配置验证 ===");
    char diag_cmd[256];
    
    /* 显示相关的 ip rule */
    snprintf(diag_cmd, sizeof(diag_cmd), 
             "ip rule list | grep -E '%s|lookup %u' 2>/dev/null", 
             client_ip, link->route_table_id);
    fd_log_notice("[dataplane] IP Rules (grep %s):", client_ip);
    system(diag_cmd);
    
    /* 显示路由表内容 */
    snprintf(diag_cmd, sizeof(diag_cmd), 
             "ip route show table %u 2>/dev/null", 
             link->route_table_id);
    fd_log_notice("[dataplane] Route Table %u:", link->route_table_id);
    system(diag_cmd);
    
    /* 显示出接口状态 */
    snprintf(diag_cmd, sizeof(diag_cmd), 
             "ip addr show %s | head -3 2>/dev/null", 
             link->interface_name);
    fd_log_notice("[dataplane] Interface %s:", link->interface_name);
    system(diag_cmd);
    
    fd_log_notice("[dataplane] === 验证结束 ===");
    
    return 0;
}

int magic_dataplane_remove_client_route(DataplaneContext* ctx,
                                        const char* session_id)
{
    if (!ctx || !ctx->is_initialized || !session_id) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    ClientRoutingRule* rule = magic_dataplane_find_rule(ctx, session_id);
    if (!rule) {
        fd_log_debug("[dataplane] 未找到会话的路由规则: %s", session_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 保存 client_ip 用于后续 iptables 操作 */
    char client_ip[MAX_IP_ADDR_LEN];
    strncpy(client_ip, rule->client_ip, MAX_IP_ADDR_LEN - 1);
    client_ip[MAX_IP_ADDR_LEN - 1] = '\0';
    /* 保存 link_id 用于后续查找 gateway (在释放后需要) */
    char saved_link_id[MAX_LINK_ID_LEN];
    strncpy(saved_link_id, rule->link_id, MAX_LINK_ID_LEN - 1);
    saved_link_id[MAX_LINK_ID_LEN - 1] = '\0';
    
    /* 删除 ip rule */
    delete_ip_rule(rule->client_ip, rule->route_table_id);
    
    /* 清除规则记录 */
    fd_log_notice("[dataplane] ✓ 删除客户端路由: %s → %s",
                  rule->client_ip, rule->link_id);
    
    rule->in_use = false;
    ctx->num_rules--;
    
    /* ★★★ 关键修复：检查该 IP 是否还有其他活跃会话 ★★★
     * 如果还有其他会话，不能添加黑洞规则，否则会阻断其他会话的流量
     */
    int other_sessions = count_client_ip_sessions(ctx, client_ip, NULL);
    bool should_add_blackhole = (other_sessions == 0);
    
    if (!should_add_blackhole) {
        fd_log_notice("[dataplane] ⚠ 该 IP (%s) 还有 %d 个其他活跃会话，不添加黑洞规则",
                      client_ip, other_sessions);
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    /* 只有在没有其他会话时才添加黑洞规则 */
    if (should_add_blackhole) {
        /* 添加黑洞规则，阻断客户端流量 */
        /* 注意：在 mutex 之外执行，避免长时间持有锁 */
        /* 尝试获取链路的 gateway 并传入，以便删除带 -d 的 ACCEPT 规则 */
        char gateway_ip[MAX_IP_ADDR_LEN] = {0};
        if (magic_dataplane_get_link_gateway(ctx, saved_link_id, gateway_ip, sizeof(gateway_ip)) != 0) {
            /* 未找到 gateway 时传空字符串 */
            gateway_ip[0] = '\0';
        }
        add_blackhole_rule(client_ip, gateway_ip[0] ? gateway_ip : NULL);

        /* 从 ipset 中删除客户端 (control/data) 并清理 conntrack 条目 */
        magic_dataplane_ipset_del(client_ip);
        {
            char cmd[MAX_CMD_LEN];
            snprintf(cmd, sizeof(cmd), "conntrack -D -s %s 2>/dev/null", client_ip);
            magic_dataplane_exec_cmd(cmd);
        }
    }
    
    return 0;
}

int magic_dataplane_switch_client_link(DataplaneContext* ctx,
                                       const char* session_id,
                                       const char* new_link_id)
{
    if (!ctx || !ctx->is_initialized || !session_id || !new_link_id) {
        return -1;
    }
    
    /* ★★★ 只使用 TFT 规则切换（精确到五元组）★★★ */
    /* 检查是否有 TFT 规则 */
    TftRule* tft_rules[MAX_TFT_RULES];
    int tft_count = magic_dataplane_find_tft_rules(ctx, session_id, tft_rules, MAX_TFT_RULES);
    
    if (tft_count > 0) {
        fd_log_notice("[dataplane] 会话 %s 有 %d 条 TFT 规则，切换到链路 %s", 
                      session_id, tft_count, new_link_id);
        return magic_dataplane_switch_tft_link(ctx, session_id, new_link_id);
    }
    
    /* 没有 TFT 规则时，跳过数据平面配置（等待 MCCR 添加 TFT 后再控制）*/
    fd_log_notice("[dataplane] 会话 %s 尚无 TFT 规则，跳过数据平面切换 (等待 MCCR)", session_id);
    return 0;  /* 返回成功，让上层更新会话链路记录 */
}

uint32_t magic_dataplane_get_table_id(DataplaneContext* ctx, const char* link_id)
{
    if (!ctx || !link_id) return 0;
    
    pthread_mutex_lock(&ctx->mutex);
    
    LinkRouteConfig* link = find_link_config(ctx, link_id);
    uint32_t table_id = link ? link->route_table_id : 0;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return table_id;
}

int magic_dataplane_get_link_gateway(DataplaneContext* ctx, const char* link_id,
                                     char* gateway_ip, size_t ip_len)
{
    if (!ctx || !link_id || !gateway_ip || ip_len == 0) return -1;
    
    gateway_ip[0] = '\0';
    
    pthread_mutex_lock(&ctx->mutex);
    
    LinkRouteConfig* link = find_link_config(ctx, link_id);
    if (link && link->gateway_ip[0]) {
        strncpy(gateway_ip, link->gateway_ip, ip_len - 1);
        gateway_ip[ip_len - 1] = '\0';
        pthread_mutex_unlock(&ctx->mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return -1;
}

int magic_dataplane_unregister_link(DataplaneContext* ctx, const char* link_id)
{
    if (!ctx || !ctx->is_initialized) {
        fd_log_error("[dataplane] 数据平面未初始化");
        return -1;
    }
    
    if (!link_id) {
        fd_log_error("[dataplane] 链路 ID 无效");
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    /* 查找链路配置 */
    int link_index = -1;
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        if (strcmp(ctx->links[i].link_id, link_id) == 0) {
            link_index = (int)i;
            break;
        }
    }
    
    if (link_index < 0) {
        fd_log_error("[dataplane] 注销链路失败，未找到: %s", link_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    LinkRouteConfig* link = &ctx->links[link_index];
    uint32_t table_id = link->route_table_id;
    
    /* 删除所有使用该链路的客户端路由规则 */
    uint32_t removed_rules = 0;
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (ctx->rules[i].in_use && ctx->rules[i].route_table_id == table_id) {
            delete_ip_rule(ctx->rules[i].client_ip, ctx->rules[i].route_table_id);
            ctx->rules[i].in_use = false;
            ctx->num_rules--;
            removed_rules++;
        }
    }
    
    /* 删除路由表 */
    if (link->is_configured) {
        delete_route_table(link);
    }
    
    /* 从数组中移除链路配置（将最后一个元素移到当前位置） */
    if ((uint32_t)link_index < ctx->num_links - 1) {
        memcpy(&ctx->links[link_index], &ctx->links[ctx->num_links - 1], sizeof(LinkRouteConfig));
    }
    ctx->num_links--;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    fd_log_notice("[dataplane] ✓ 注销链路: %s (table=%u, 删除 %u 条路由规则)",
                  link_id, table_id, removed_rules);
    
    return 0;
}

int magic_dataplane_update_link_gateway(DataplaneContext* ctx, const char* link_id,
                                       const char* gateway_ip)
{
    if (!ctx || !link_id) return -1;

    pthread_mutex_lock(&ctx->mutex);

    LinkRouteConfig* link = find_link_config(ctx, link_id);
    if (!link) {
        fd_log_error("[dataplane] 更新网关失败，链路未注册: %s", link_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    /* 如果提供了新的网关，保存并尝试重建路由表 */
    if (gateway_ip && gateway_ip[0]) {
        /* 如果路由表已配置，先删除旧表 */
        if (link->is_configured) {
            delete_route_table(link);
        }

        /* 始终保存网关 IP（用于返回给客户端） */
        strncpy(link->gateway_ip, gateway_ip, MAX_IP_ADDR_LEN - 1);
        link->gateway_ip[MAX_IP_ADDR_LEN - 1] = '\0';

        /* 尝试创建路由表（失败不影响 gateway IP 的保存） */
        if (create_route_table(link) != 0) {
            fd_log_notice("[dataplane] ⚠ 路由表创建失败（网关可能不可达），但 Gateway IP 已保存: %s -> %s", 
                          link_id, link->gateway_ip);
            /* 不返回错误，gateway IP 已保存供客户端使用 */
        } else {
            fd_log_notice("[dataplane] ✓ 更新链路网关并重建路由表: %s -> %s", link_id, link->gateway_ip);
        }
    } else {
        /* 如果没有提供网关，仅记录 */
        fd_log_debug("[dataplane] 更新网关: 未提供网关参数, 跳过: %s", link_id);
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;  /* 始终返回成功，gateway IP 已保存 */
}

ClientRoutingRule* magic_dataplane_find_rule(DataplaneContext* ctx, 
                                             const char* session_id)
{
    if (!ctx || !session_id) return NULL;
    
    /* 注意: 调用者应已持有锁，或此函数内部加锁 */
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (ctx->rules[i].in_use && 
            strcmp(ctx->rules[i].session_id, session_id) == 0) {
            return &ctx->rules[i];
        }
    }
    
    return NULL;
}

void magic_dataplane_print_status(DataplaneContext* ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->mutex);
    
    fd_log_notice("[dataplane] ════════════════════════════════════════");
    fd_log_notice("[dataplane] 数据平面状态");
    fd_log_notice("[dataplane] ════════════════════════════════════════");
    fd_log_notice("[dataplane] 入口: %s (%s)", 
                  ctx->ingress_interface[0] ? ctx->ingress_interface : "未配置",
                  ctx->ingress_ip[0] ? ctx->ingress_ip : "无 IP");
    fd_log_notice("[dataplane] 路由功能: %s", ctx->enable_routing ? "启用" : "禁用");
    fd_log_notice("[dataplane] fwmark规则: %s", ctx->fwmark_rules_installed ? "已安装" : "未安装");
    
    fd_log_notice("[dataplane] ─────────────────────────────────────");
    fd_log_notice("[dataplane] 已注册链路 (%u):", ctx->num_links);
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        LinkRouteConfig* link = &ctx->links[i];
        fd_log_notice("[dataplane]   [%u] %s → %s (table=%u, fwmark=%u, gw=%s) %s",
                      i + 1, link->link_id, link->interface_name,
                      link->route_table_id, link->fwmark,
                      link->gateway_ip[0] ? link->gateway_ip : "直连",
                      link->is_configured ? "✓" : "✗");
    }
    
    fd_log_notice("[dataplane] ─────────────────────────────────────");
    fd_log_notice("[dataplane] 活动TFT规则 (%u):", ctx->num_tft_rules);
    for (uint32_t i = 0; i < MAX_TFT_RULES; i++) {
        if (ctx->tft_rules[i].in_use) {
            TftRule* rule = &ctx->tft_rules[i];
            fd_log_notice("[dataplane]   %s:%u → %s:%u proto=%u → %s (fwmark=%u)",
                          rule->tuple.src_ip, rule->tuple.src_port,
                          rule->tuple.dst_ip, rule->tuple.dst_port,
                          rule->tuple.protocol,
                          rule->link_id, rule->fwmark);
        }
    }
    
    fd_log_notice("[dataplane] ─────────────────────────────────────");
    fd_log_notice("[dataplane] 活动路由规则 (%u) [兼容旧API]:", ctx->num_rules);
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (ctx->rules[i].in_use) {
            ClientRoutingRule* rule = &ctx->rules[i];
            fd_log_notice("[dataplane]   %s → %s (table=%u, fwmark=%u)",
                          rule->client_ip, rule->link_id,
                          rule->route_table_id, rule->fwmark);
        }
    }
    fd_log_notice("[dataplane] ════════════════════════════════════════");
    
    pthread_mutex_unlock(&ctx->mutex);
}

void magic_dataplane_cleanup(DataplaneContext* ctx)
{
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->mutex);
    
    fd_log_notice("[dataplane] 正在清理数据平面...");
    
    /* 删除所有 TFT 规则 */
    for (uint32_t i = 0; i < MAX_TFT_RULES; i++) {
        if (ctx->tft_rules[i].in_use) {
            /* 删除 mangle 和 filter 规则 */
            TftRule* rule = &ctx->tft_rules[i];
            char cmd[MAX_CMD_LEN];
            
            /* 删除 mangle 打标规则 */
            if (rule->tuple.protocol == 6 || rule->tuple.protocol == 17) {
                snprintf(cmd, sizeof(cmd),
                         "iptables -t mangle -D PREROUTING -s %s -d %s -p %s --dport %u -j MARK --set-mark %u 2>/dev/null",
                         rule->tuple.src_ip, rule->tuple.dst_ip,
                         rule->tuple.protocol == 6 ? "tcp" : "udp",
                         rule->tuple.dst_port, rule->fwmark);
            } else {
                snprintf(cmd, sizeof(cmd),
                         "iptables -t mangle -D PREROUTING -s %s -d %s -j MARK --set-mark %u 2>/dev/null",
                         rule->tuple.src_ip, rule->tuple.dst_ip, rule->fwmark);
            }
            magic_dataplane_exec_cmd(cmd);
            
            ctx->tft_rules[i].in_use = false;
        }
    }
    ctx->num_tft_rules = 0;
    
    /* 删除所有客户端路由规则 (兼容旧API) */
    for (uint32_t i = 0; i < MAX_ROUTING_RULES; i++) {
        if (ctx->rules[i].in_use) {
            delete_ip_rule(ctx->rules[i].client_ip, ctx->rules[i].route_table_id);
            ctx->rules[i].in_use = false;
        }
    }
    ctx->num_rules = 0;
    
    /* 删除所有路由表 */
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        delete_route_table(&ctx->links[i]);
    }
    ctx->num_links = 0;
    
    /* 删除 fwmark 路由规则 */
    if (ctx->fwmark_rules_installed) {
        char cmd[MAX_CMD_LEN];
        for (uint32_t mark = MAGIC_FWMARK_BLACKHOLE; mark <= MAGIC_RT_TABLE_MAX; mark++) {
            snprintf(cmd, sizeof(cmd), "ip rule del fwmark %u 2>/dev/null", mark);
            magic_dataplane_exec_cmd(cmd);
        }
        ctx->fwmark_rules_installed = false;
    }
    
    ctx->is_initialized = false;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    /* 删除 ipset 相关的 iptables 规则（忽略错误） */
    magic_dataplane_exec_cmd("iptables -D OUTPUT -m set --match-set magic_data src -j ACCEPT 2>/dev/null");
    magic_dataplane_exec_cmd("iptables -D OUTPUT -m set --match-set magic_control src -j ACCEPT 2>/dev/null");
    
    /* 删除连接追踪规则 */
    magic_dataplane_exec_cmd("iptables -D FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null");

    /* 销毁 ipset 集合 (如果存在) */
    magic_dataplane_ipset_destroy(ctx);

    pthread_mutex_destroy(&ctx->mutex);
    
    fd_log_notice("[dataplane] ✓ 数据平面已清理");
}

/*===========================================================================
 * TFT (Traffic Flow Template) 规则管理
 * ARINC 839 合规: 基于五元组的精确流量控制
 *===========================================================================*/

/**
 * @brief 获取链路的 fwmark 值
 */
uint32_t magic_dataplane_get_fwmark(DataplaneContext* ctx, const char* link_id)
{
    if (!ctx || !link_id) return 0;
    
    pthread_mutex_lock(&ctx->mutex);
    
    LinkRouteConfig* link = find_link_config(ctx, link_id);
    uint32_t fwmark = link ? link->fwmark : 0;
    
    pthread_mutex_unlock(&ctx->mutex);
    return fwmark;
}

/**
 * @brief 添加 TFT 规则
 * 添加基于五元组的 mangle 打标规则和 filter 放行规则
 */
int magic_dataplane_add_tft_rule(DataplaneContext* ctx,
                                 const TftTuple* tuple,
                                 const char* session_id,
                                 const char* link_id)
{
    if (!ctx || !ctx->is_initialized || !tuple || !session_id || !link_id) {
        fd_log_error("[dataplane] TFT 规则参数无效");
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    /* 查找链路配置 */
    LinkRouteConfig* link = find_link_config(ctx, link_id);
    if (!link) {
        fd_log_error("[dataplane] 链路未注册: %s", link_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 查找空闲槽位 */
    TftRule* rule = NULL;
    for (uint32_t i = 0; i < MAX_TFT_RULES; i++) {
        if (!ctx->tft_rules[i].in_use) {
            rule = &ctx->tft_rules[i];
            break;
        }
    }
    
    if (!rule) {
        fd_log_error("[dataplane] TFT 规则已满: %u", MAX_TFT_RULES);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    /* 记录规则 */
    memset(rule, 0, sizeof(TftRule));
    rule->in_use = true;
    memcpy(&rule->tuple, tuple, sizeof(TftTuple));
    strncpy(rule->session_id, session_id, sizeof(rule->session_id) - 1);
    strncpy(rule->link_id, link_id, sizeof(rule->link_id) - 1);
    rule->fwmark = link->fwmark;
    rule->created_at = time(NULL);
    
    ctx->num_tft_rules++;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    /* === 添加 iptables 规则 === */
    char cmd[MAX_CMD_LEN];
    const char* proto_str = NULL;
    
    /* 确定协议字符串 */
    switch (tuple->protocol) {
        case 6:  proto_str = "tcp"; break;
        case 17: proto_str = "udp"; break;
        case 1:  proto_str = "icmp"; break;
        default: proto_str = NULL; break;
    }
    
    /* 1. 添加 mangle PREROUTING 打标规则 */
    if (proto_str && tuple->dst_port > 0 && tuple->src_port > 0) {
        /* 带源端口和目标端口的规则（精确控制）*/
        snprintf(cmd, sizeof(cmd),
                 "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --sport %u --dport %u -j MARK --set-mark %u",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->src_port, tuple->dst_port, rule->fwmark);
    } else if (proto_str && tuple->dst_port > 0) {
        /* 只有目标端口的规则 */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --dport %u -j MARK --set-mark %u",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->dst_port, rule->fwmark);
    } else if (proto_str && tuple->src_port > 0) {
        /* 只有源端口的规则 */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --sport %u -j MARK --set-mark %u",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->src_port, rule->fwmark);
    } else if (proto_str) {
        /* 只有协议，没有端口 */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t mangle -I PREROUTING -s %s -d %s -p %s -j MARK --set-mark %u",
                 tuple->src_ip, tuple->dst_ip, proto_str, rule->fwmark);
    } else {
        /* 无协议限制 */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t mangle -I PREROUTING -s %s -d %s -j MARK --set-mark %u",
                 tuple->src_ip, tuple->dst_ip, rule->fwmark);
    }
    
    if (magic_dataplane_exec_cmd(cmd) != 0) {
        fd_log_error("[dataplane] 添加 mangle 规则失败");
        return -1;
    }
    fd_log_notice("[dataplane] ✓ mangle PREROUTING: %s:%u → %s:%u (mark=%u)",
                  tuple->src_ip, tuple->src_port, tuple->dst_ip, tuple->dst_port, rule->fwmark);
    
    /* 2. 添加 filter FORWARD TFT 精确放行规则 */
    /* 关键修复：使用 -I FORWARD 1 确保规则在最前面，避免被其他规则屏蔽 */
    if (proto_str && tuple->dst_port > 0 && tuple->src_port > 0) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -I FORWARD 1 -s %s -d %s -p %s --sport %u --dport %u -j ACCEPT",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->src_port, tuple->dst_port);
    } else if (proto_str && tuple->dst_port > 0) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -I FORWARD 1 -s %s -d %s -p %s --dport %u -j ACCEPT",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->dst_port);
    } else if (proto_str && tuple->src_port > 0) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -I FORWARD 1 -s %s -d %s -p %s --sport %u -j ACCEPT",
                 tuple->src_ip, tuple->dst_ip, proto_str, tuple->src_port);
    } else if (proto_str) {
        snprintf(cmd, sizeof(cmd),
                 "iptables -I FORWARD 1 -s %s -d %s -p %s -j ACCEPT",
                 tuple->src_ip, tuple->dst_ip, proto_str);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "iptables -I FORWARD 1 -s %s -d %s -j ACCEPT",
                 tuple->src_ip, tuple->dst_ip);
    }
    
    magic_dataplane_exec_cmd(cmd);
    fd_log_notice("[dataplane] ✓ filter FORWARD: %s → %s ACCEPT", tuple->src_ip, tuple->dst_ip);
    
    /* 3. 将客户端加入 magic_data ipset (用于状态管理) */
    magic_dataplane_ipset_add_data(tuple->src_ip);
    
    fd_log_notice("[dataplane] ✓ TFT 规则已添加: %s:%u → %s:%u (proto=%u, link=%s, fwmark=%u)",
                  tuple->src_ip, tuple->src_port,
                  tuple->dst_ip, tuple->dst_port,
                  tuple->protocol, link_id, rule->fwmark);
    
    return 0;
}

/**
 * @brief 删除会话的所有 TFT 规则
 */
int magic_dataplane_remove_tft_rules(DataplaneContext* ctx, const char* session_id)
{
    if (!ctx || !ctx->is_initialized || !session_id) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int removed = 0;
    char client_ip[MAX_IP_ADDR_LEN] = {0};
    
    for (uint32_t i = 0; i < MAX_TFT_RULES; i++) {
        TftRule* rule = &ctx->tft_rules[i];
        if (!rule->in_use) continue;
        if (strcmp(rule->session_id, session_id) != 0) continue;
        
        /* 保存客户端 IP 用于后续清理 */
        if (!client_ip[0]) {
            strncpy(client_ip, rule->tuple.src_ip, sizeof(client_ip) - 1);
        }
        
        /* 删除 mangle 打标规则 */
        char cmd[MAX_CMD_LEN];
        const char* proto_str = NULL;
        switch (rule->tuple.protocol) {
            case 6:  proto_str = "tcp"; break;
            case 17: proto_str = "udp"; break;
            case 1:  proto_str = "icmp"; break;
            default: proto_str = NULL; break;
        }
        
        if (proto_str && rule->tuple.dst_port > 0) {
            snprintf(cmd, sizeof(cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s --dport %u -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.dst_port, rule->fwmark);
        } else if (proto_str) {
            snprintf(cmd, sizeof(cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str, rule->fwmark);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, rule->fwmark);
        }
        magic_dataplane_exec_cmd(cmd);
        
        /* 删除 filter FORWARD 规则 */
        if (proto_str && rule->tuple.dst_port > 0) {
            snprintf(cmd, sizeof(cmd),
                     "iptables -D FORWARD -s %s -d %s -p %s --dport %u -j ACCEPT 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str, rule->tuple.dst_port);
        } else if (proto_str) {
            snprintf(cmd, sizeof(cmd),
                     "iptables -D FORWARD -s %s -d %s -p %s -j ACCEPT 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "iptables -D FORWARD -s %s -d %s -j ACCEPT 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip);
        }
        magic_dataplane_exec_cmd(cmd);
        
        fd_log_notice("[dataplane] ✓ 删除 TFT 规则: %s → %s", 
                      rule->tuple.src_ip, rule->tuple.dst_ip);
        
        rule->in_use = false;
        ctx->num_tft_rules--;
        removed++;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    /* 清理客户端状态 */
    if (client_ip[0]) {
        /* 从 ipset 删除 */
        magic_dataplane_ipset_del(client_ip);
        
        /* 清理 conntrack */
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "conntrack -D -s %s 2>/dev/null", client_ip);
        magic_dataplane_exec_cmd(cmd);
    }
    
    fd_log_notice("[dataplane] ✓ 删除会话 %s 的 %d 条 TFT 规则", session_id, removed);
    return removed;
}

/**
 * @brief 查找会话的 TFT 规则
 */
int magic_dataplane_find_tft_rules(DataplaneContext* ctx,
                                   const char* session_id,
                                   TftRule** rules,
                                   int max_rules)
{
    if (!ctx || !session_id || !rules || max_rules <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int found = 0;
    for (uint32_t i = 0; i < MAX_TFT_RULES && found < max_rules; i++) {
        TftRule* rule = &ctx->tft_rules[i];
        if (rule->in_use && strcmp(rule->session_id, session_id) == 0) {
            rules[found++] = rule;
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return found;
}

/**
 * @brief 切换会话的 TFT 规则到新链路
 * 通过修改 mangle 表的 fwmark 实现精确的流量切换
 */
int magic_dataplane_switch_tft_link(DataplaneContext* ctx,
                                    const char* session_id,
                                    const char* new_link_id)
{
    if (!ctx || !ctx->is_initialized || !session_id || !new_link_id) {
        return -1;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    /* 查找新链路 */
    LinkRouteConfig* new_link = find_link_config(ctx, new_link_id);
    if (!new_link) {
        fd_log_error("[dataplane] 新链路未注册: %s", new_link_id);
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }
    
    uint32_t new_fwmark = new_link->fwmark;
    int switched = 0;
    
    for (uint32_t i = 0; i < MAX_TFT_RULES; i++) {
        TftRule* rule = &ctx->tft_rules[i];
        if (!rule->in_use) continue;
        if (strcmp(rule->session_id, session_id) != 0) continue;
        
        /* 如果已经是同一链路，跳过 */
        if (strcmp(rule->link_id, new_link_id) == 0) {
            continue;
        }
        
        uint32_t old_fwmark = rule->fwmark;
        
        /* 构建协议字符串 */
        const char* proto_str = NULL;
        switch (rule->tuple.protocol) {
            case 6:  proto_str = "tcp"; break;
            case 17: proto_str = "udp"; break;
            case 1:  proto_str = "icmp"; break;
            default: proto_str = NULL; break;
        }
        
        char del_cmd[MAX_CMD_LEN];
        char add_cmd[MAX_CMD_LEN];
        
        /* 删除旧的 mangle 规则，添加新的 mangle 规则 */
        if (proto_str && rule->tuple.src_port > 0 && rule->tuple.dst_port > 0) {
            snprintf(del_cmd, sizeof(del_cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s --sport %u --dport %u -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.src_port, rule->tuple.dst_port, old_fwmark);
            snprintf(add_cmd, sizeof(add_cmd),
                     "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --sport %u --dport %u -j MARK --set-mark %u",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.src_port, rule->tuple.dst_port, new_fwmark);
        } else if (proto_str && rule->tuple.dst_port > 0) {
            snprintf(del_cmd, sizeof(del_cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s --dport %u -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.dst_port, old_fwmark);
            snprintf(add_cmd, sizeof(add_cmd),
                     "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --dport %u -j MARK --set-mark %u",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.dst_port, new_fwmark);
        } else if (proto_str && rule->tuple.src_port > 0) {
            snprintf(del_cmd, sizeof(del_cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s --sport %u -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.src_port, old_fwmark);
            snprintf(add_cmd, sizeof(add_cmd),
                     "iptables -t mangle -I PREROUTING -s %s -d %s -p %s --sport %u -j MARK --set-mark %u",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str,
                     rule->tuple.src_port, new_fwmark);
        } else if (proto_str) {
            snprintf(del_cmd, sizeof(del_cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -p %s -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str, old_fwmark);
            snprintf(add_cmd, sizeof(add_cmd),
                     "iptables -t mangle -I PREROUTING -s %s -d %s -p %s -j MARK --set-mark %u",
                     rule->tuple.src_ip, rule->tuple.dst_ip, proto_str, new_fwmark);
        } else {
            snprintf(del_cmd, sizeof(del_cmd),
                     "iptables -t mangle -D PREROUTING -s %s -d %s -j MARK --set-mark %u 2>/dev/null",
                     rule->tuple.src_ip, rule->tuple.dst_ip, old_fwmark);
            snprintf(add_cmd, sizeof(add_cmd),
                     "iptables -t mangle -I PREROUTING -s %s -d %s -j MARK --set-mark %u",
                     rule->tuple.src_ip, rule->tuple.dst_ip, new_fwmark);
        }
        
        /* 执行删除和添加 */
        magic_dataplane_exec_cmd(del_cmd);
        int ret = magic_dataplane_exec_cmd(add_cmd);
        
        if (ret == 0) {
            /* 更新规则记录 */
            strncpy(rule->link_id, new_link_id, MAX_LINK_ID_LEN - 1);
            rule->fwmark = new_fwmark;
            switched++;
            
            fd_log_notice("[dataplane] ✓ TFT 切换: %s:%u → %s:%u (fwmark %u→%u, link=%s)",
                          rule->tuple.src_ip, rule->tuple.src_port,
                          rule->tuple.dst_ip, rule->tuple.dst_port,
                          old_fwmark, new_fwmark, new_link_id);
        } else {
            fd_log_error("[dataplane] ✗ TFT 切换失败: %s → %s",
                         rule->tuple.src_ip, rule->tuple.dst_ip);
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    if (switched > 0) {
        fd_log_notice("[dataplane] ✓ 会话 %s 切换 %d 条 TFT 规则到 %s",
                      session_id, switched, new_link_id);
    } else {
        fd_log_notice("[dataplane] ⚠ 会话 %s 无 TFT 规则需要切换", session_id);
    }
    
    return switched > 0 ? 0 : -1;
}
