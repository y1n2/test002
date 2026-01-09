/**
 * @file magic_dataplane.h
 * @brief MAGIC 数据平面路由模块
 * @description 使用 Linux 策略路由 (ip rule/ip route) 和 iptables mangle
 * 表将客户端流量路由到指定链路
 *
 * ARINC 839 合规说明:
 * 采用基于标记的路由（Mark Based Routing），而非基于源IP的路由。
 * 这允许同一客户端的不同业务流走不同链路（如VoIP走SATCOM，文件下载走WIFI）。
 *
 * 架构说明:
 * ┌─────────────┐      ┌───────────────┐      ┌─────────────┐ ┌─────────────┐
 * │   Client    │──UDP─→│  ens39 (入口) │──────→│ mangle表    │──────→│
 * 策略路由   │ │192.168.10.x │      │192.168.126.1  │      │ TFT打标     │ │
 * fwmark rule │ └─────────────┘      └───────────────┘      └──────┬──────┘
 * └──────┬──────┘ │                    │
 *                    ┌────────────────────────────────┼────────────────────┼────────────┐
 *                    ▼                                ▼                    ▼ ▼
 *             ┌──────────────┐               ┌──────────────┐ ┌──────────────┐
 *             │  fwmark 100  │               │  fwmark 101  │      │  fwmark
 * 102  │ │  → table 100 │               │  → table 101 │      │  → table 102 │
 *             │  SATCOM      │               │   WIFI       │      │  CELLULAR
 * │ │  via ens37   │               │   via ens38  │      │   via ens35  │
 *             └──────────────┘               └──────────────┘ └──────────────┘
 *
 * 工作流程:
 * 1. 系统初始化: 创建基于 fwmark 的静态路由规则 (ip rule add fwmark N lookup N)
 * 2. 客户端通过 MCCR 请求通信资源，提供 TFT 五元组
 * 3. MAGIC 策略引擎选择链路 (如 SATCOM, mark=100)
 * 4. 数据平面模块:
 *    - 添加 mangle PREROUTING 打标规则 (基于 TFT 五元组)
 *    - 添加 filter FORWARD 精确放行规则
 * 5. 客户端后续匹配 TFT 的流量自动打标并路由到对应链路
 * 6. 会话结束时删除对应的 mangle 和 filter 规则
 *
 * @author MAGIC System Development Team
 * @version 2.0
 * @date 2025-12-26
 */

#ifndef MAGIC_DATAPLANE_H
#define MAGIC_DATAPLANE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

/* 路由表 ID 和 FwMark 范围: 100-199 用于 MAGIC 数据平面 */
#define MAGIC_RT_TABLE_BASE 100   /* 路由表起始编号 (同时也是 fwmark) */
#define MAGIC_RT_TABLE_MAX 199    /* 路由表最大编号 */
#define MAGIC_FWMARK_BLACKHOLE 99 /* 黑洞路由标记 */

/* IP Rule 优先级范围: 用于基于 fwmark 的静态规则 */
#define MAGIC_FWMARK_RULE_PRIORITY 100 /* fwmark 规则优先级 (静态) */
#define MAGIC_BLACKHOLE_PRIORITY 50    /* 黑洞规则优先级 (最高) */

/* 动态 IP Rule 优先级范围 (用于向后兼容的 from <ip> 规则) */
#define MAGIC_RULE_PRIORITY_BASE 1000
#define MAGIC_RULE_PRIORITY_MAX 2000

/* 最大链路数和规则数 */
#define MAX_DATAPLANE_LINKS 10 /* 最大支持的链路数 */
#define MAX_ROUTING_RULES 256  /* 最大路由规则数 */
#define MAX_TFT_RULES 1024     /* 最大 TFT 规则数 */

/* 字符串长度 */
#define MAX_IF_NAME_LEN 16 /* 网络接口名称最大长度 */
#define MAX_IP_ADDR_LEN 64 /* IP 地址字符串最大长度 */
#define MAX_LINK_ID_LEN 64 /* 链路 ID 最大长度 */
#define MAX_CMD_LEN 512    /* 命令字符串最大长度 */

/*===========================================================================
 * TFT (Traffic Flow Template) 数据结构
 * 符合 ARINC 839 规范的五元组流量控制
 *===========================================================================*/

/**
 * @brief TFT 五元组定义
 * 用于精确匹配和控制特定的流量流
 */
typedef struct {
  char src_ip[MAX_IP_ADDR_LEN]; ///< 源 IP 地址 (客户端)。
  char dst_ip[MAX_IP_ADDR_LEN]; ///< 目的 IP 地址。
  uint16_t src_port;            ///< 源端口 (0 = 任意)。
  uint16_t dst_port;            ///< 目的端口 (0 = 任意)。
  uint8_t protocol;             ///< 协议号: 6=TCP, 17=UDP, 1=ICMP, 0=任意。
} TftTuple;

/**
 * @brief TFT 规则条目
 * 包含五元组和对应的链路标记
 */
typedef struct {
  bool in_use;                   ///< 规则是否在使用中。
  TftTuple tuple;                ///< TFT 五元组。
  char session_id[64];           ///< 关联的会话 ID。
  char link_id[MAX_LINK_ID_LEN]; ///< 分配的链路 ID。
  uint32_t fwmark;               ///< 分配的 fwmark 值。
  time_t created_at;             ///< 规则创建时间。
} TftRule;

/*===========================================================================
 * 数据结构定义
 *===========================================================================*/

/**
 * @brief 链路路由配置
 * 描述一个数据链路的路由配置信息
 */
typedef struct {
  char link_id[MAX_LINK_ID_LEN]; ///< 链路标识符 (如 "CELLULAR", "SATCOM")。
  char interface_name[MAX_IF_NAME_LEN]; ///< 出口网络接口名 (如 "ens33",
                                        ///< "ens37")。
  char gateway_ip[MAX_IP_ADDR_LEN];     ///< 网关 IP 地址 (可选)。
  uint32_t route_table_id;              ///< 路由表 ID (100-199)。
  uint32_t fwmark;    ///< 对应的 fwmark 值 (与 table_id 相同)。
  bool is_configured; ///< 是否已配置路由表。
} LinkRouteConfig;

/**
 * @brief 客户端路由规则 (兼容旧 API)
 * 描述一个客户端的路由规则
 */
typedef struct {
  bool in_use;                     ///< 规则是否在使用中。
  char client_ip[MAX_IP_ADDR_LEN]; ///< 客户端源 IP 地址。
  char session_id[64];             ///< 关联的会话 ID。
  char link_id[MAX_LINK_ID_LEN];   ///< 分配的链路 ID。
  uint32_t rule_priority;          ///< (废弃) ip rule 优先级。
  uint32_t route_table_id;         ///< 使用的路由表 ID。
  uint32_t fwmark;                 ///< 分配的 fwmark 值。
  time_t created_at;               ///< 规则创建时间。
} ClientRoutingRule;

/**
 * @brief 数据平面上下文
 * 数据平面模块的全局上下文
 */
typedef struct {
  /* 入口接口配置 */
  char ingress_interface[MAX_IF_NAME_LEN]; /* 入口接口名 (如 "ens39") */
  char ingress_ip[MAX_IP_ADDR_LEN]; /* 入口接口 IP (如 "192.168.126.1") */

  /* 链路路由配置 */
  LinkRouteConfig links[MAX_DATAPLANE_LINKS]; /* 链路配置数组 */
  uint32_t num_links;                         /* 已配置的链路数量 */

  /* 客户端路由规则 (兼容旧 API) */
  ClientRoutingRule rules[MAX_ROUTING_RULES]; /* 路由规则数组 */
  uint32_t num_rules;                         /* 活动规则数量 */
  uint32_t next_priority;                     /* (废弃) 下一个可用优先级 */

  /* TFT 规则表 (新增: 精确五元组控制) */
  TftRule tft_rules[MAX_TFT_RULES]; /* TFT 规则数组 */
  uint32_t num_tft_rules;           /* 活动 TFT 规则数量 */

  /* 同步锁 */
  pthread_mutex_t mutex; /* 保护规则数组的互斥锁 */

  /* 状态标志 */
  bool is_initialized;         /* 是否已初始化 */
  bool enable_routing;         /* 是否启用路由功能 */
  bool fwmark_rules_installed; /* fwmark 静态规则是否已安装 */
} DataplaneContext;

/*===========================================================================
 * API 函数声明
 *===========================================================================*/

/**
 * @brief 初始化数据平面模块
 *
 * @param ctx 数据平面上下文指针
 * @param ingress_if 入口接口名称 (如 "ens39")
 * @param ingress_ip 入口接口 IP (如 "192.168.126.1")
 * @return 0=成功, -1=失败
 */
int magic_dataplane_init(DataplaneContext *ctx, const char *ingress_if,
                         const char *ingress_ip);

/**
 * @brief 注册数据链路
 * 为指定链路创建路由表
 *
 * @param ctx 数据平面上下文指针
 * @param link_id 链路标识符 (如 "CELLULAR")
 * @param interface_name 出口接口名 (如 "ens33")
 * @param gateway_ip 网关 IP (可选, 可为 NULL)
 * @return 分配的路由表 ID, -1=失败
 */
int magic_dataplane_register_link(DataplaneContext *ctx, const char *link_id,
                                  const char *interface_name,
                                  const char *gateway_ip);

/**
 * @brief 注销数据链路
 * 删除链路的路由表和所有关联的客户端路由规则
 *
 * @param ctx 数据平面上下文指针
 * @param link_id 链路标识符
 * @return 0=成功, -1=失败 (未找到)
 */
int magic_dataplane_unregister_link(DataplaneContext *ctx, const char *link_id);

/**
 * @brief 添加客户端路由规则
 * 将客户端的流量路由到指定链路
 *
 * @param ctx 数据平面上下文指针
 * @param client_ip 客户端 IP 地址
 * @param session_id 会话 ID
 * @param link_id 分配的链路 ID
 * @param dest_ip 目的 IP 地址（来自 TFT，可为 NULL 表示允许访问整个网段）
 * @return 0=成功, -1=失败
 *
 * @note 此函数执行: ip rule add from <client_ip> lookup <table_id> priority
 * <prio>
 */
int magic_dataplane_add_client_route(DataplaneContext *ctx,
                                     const char *client_ip,
                                     const char *session_id,
                                     const char *link_id, const char *dest_ip);

/**
 * @brief 删除客户端路由规则
 * 移除客户端的路由规则
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @return 0=成功, -1=失败 (未找到)
 */
int magic_dataplane_remove_client_route(DataplaneContext *ctx,
                                        const char *session_id);

/**
 * @brief 切换客户端链路
 * 将客户端的流量切换到新链路
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @param new_link_id 新的链路 ID
 * @return 0=成功, -1=失败
 */
int magic_dataplane_switch_client_link(DataplaneContext *ctx,
                                       const char *session_id,
                                       const char *new_link_id);

/**
 * @brief 查找链路的路由表 ID
 *
 * @param ctx 数据平面上下文指针
 * @param link_id 链路 ID
 * @return 路由表 ID, 0=未找到
 */
uint32_t magic_dataplane_get_table_id(DataplaneContext *ctx,
                                      const char *link_id);

/**
 * @brief 获取链路的 fwmark 值
 *
 * @param ctx 数据平面上下文指针
 * @param link_id 链路 ID
 * @return fwmark 值, 0=未找到
 */
uint32_t magic_dataplane_get_fwmark(DataplaneContext *ctx, const char *link_id);

/**
 * @brief 获取链路的网关 IP 地址
 *
 * @param ctx 数据平面上下文指针
 * @param link_id 链路 ID
 * @param gateway_ip 输出: 网关 IP 地址缓冲区
 * @param ip_len 缓冲区长度
 * @return 0=成功, -1=未找到
 */
int magic_dataplane_get_link_gateway(DataplaneContext *ctx, const char *link_id,
                                     char *gateway_ip, size_t ip_len);

/* 更新已注册链路的网关 IP（如果链路已存在则重建路由表）
 * 返回 0=成功, -1=失败
 */
int magic_dataplane_update_link_gateway(DataplaneContext *ctx,
                                        const char *link_id,
                                        const char *gateway_ip);

/**
 * @brief 查找会话的路由规则
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @return 规则指针, NULL=未找到
 */
ClientRoutingRule *magic_dataplane_find_rule(DataplaneContext *ctx,
                                             const char *session_id);

/*===========================================================================
 * TFT (Traffic Flow Template) API - ARINC 839 合规
 * 基于五元组的精确流量控制
 *===========================================================================*/

/**
 * @brief 添加 TFT 规则
 * 添加基于五元组的 mangle 打标规则和 filter 放行规则
 *
 * @param ctx 数据平面上下文指针
 * @param tuple TFT 五元组
 * @param session_id 会话 ID
 * @param link_id 分配的链路 ID
 * @return 0=成功, -1=失败
 */
int magic_dataplane_add_tft_rule(DataplaneContext *ctx, const TftTuple *tuple,
                                 const char *session_id, const char *link_id);

/**
 * @brief 删除会话的所有 TFT 规则
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @return 删除的规则数量
 */
int magic_dataplane_remove_tft_rules(DataplaneContext *ctx,
                                     const char *session_id);

/**
 * @brief 查找会话的 TFT 规则
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @param rules 输出: 规则数组指针
 * @param max_rules 最大返回数量
 * @return 找到的规则数量
 */
int magic_dataplane_find_tft_rules(DataplaneContext *ctx,
                                   const char *session_id, TftRule **rules,
                                   int max_rules);

/**
 * @brief 切换会话的 TFT 规则到新链路
 * 修改会话所有 TFT 规则的 fwmark，将流量导向新链路
 *
 * @param ctx 数据平面上下文指针
 * @param session_id 会话 ID
 * @param new_link_id 新的链路 ID
 * @return 0=成功, -1=失败
 *
 * @note 此函数通过修改 mangle 表的 fwmark 实现精确的流量切换，
 *       不影响同一源IP的其他会话
 */
int magic_dataplane_switch_tft_link(DataplaneContext *ctx,
                                    const char *session_id,
                                    const char *new_link_id);

/**
 * @brief 打印数据平面状态
 * 打印当前所有链路和路由规则状态
 *
 * @param ctx 数据平面上下文指针
 */
void magic_dataplane_print_status(DataplaneContext *ctx);

/**
 * @brief 清理数据平面模块
 * 删除所有路由规则和路由表
 *
 * @param ctx 数据平面上下文指针
 */
void magic_dataplane_cleanup(DataplaneContext *ctx);

/*===========================================================================
 * 辅助函数
 *===========================================================================*/

/* ------------------ ipset 管理 (用于源 IP 白名单) ------------------ */
/**
 * @brief 初始化 ipset 集合 (magic_control, magic_data)
 *
 * @param ctx 数据平面上下文指针 (可选, 目前未使用)
 * @return 0=成功, -1=失败
 */
int magic_dataplane_ipset_init(DataplaneContext *ctx);

/**
 * @brief 销毁 ipset 集合
 *
 * @param ctx 数据平面上下文指针 (可选)
 * @return 0=成功, -1=失败
 */
int magic_dataplane_ipset_destroy(DataplaneContext *ctx);

/**
 * @brief 将客户端加入 control 白名单 (MCAR 注册阶段使用)
 *
 * @param client_ip 客户端 IP
 * @return 0=成功, -1=失败
 */
int magic_dataplane_ipset_add_control(const char *client_ip);

/**
 * @brief 将客户端加入 data 白名单 (MCCR 成功放行使用)
 *
 * @param client_ip 客户端 IP
 * @return 0=成功, -1=失败
 */
int magic_dataplane_ipset_add_data(const char *client_ip);

/**
 * @brief 从所有白名单集合中删除客户端
 *
 * @param client_ip 客户端 IP
 * @return 0=成功, -1=失败
 */
int magic_dataplane_ipset_del(const char *client_ip);

/**
 * @brief 执行系统命令
 *
 * @param cmd 命令字符串
 * @return 命令退出码
 */
int magic_dataplane_exec_cmd(const char *cmd);

/**
 * @brief 检查接口是否存在
 *
 * @param interface_name 接口名称
 * @return true=存在, false=不存在
 */
bool magic_dataplane_interface_exists(const char *interface_name);

#endif /* MAGIC_DATAPLANE_H */
