/**
 * @file magic_dataplane.h
 * @brief MAGIC 数据平面路由模块头文件 (客户端版本)。
 * @details 使用 Linux 策略路由 (ip rule/ip route) 和 iptables mangle
 * 表将客户端流量路由到指定链路。 遵循 ARINC 839 规范，支持基于标记的路由 (Mark
 * Based Routing) 和精确的 TFT 流量控制。
 */

#ifndef MAGIC_DATAPLANE_H
#define MAGIC_DATAPLANE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

/** @name 路由标记与优先级
 * @{
 */
#define MAGIC_RT_TABLE_BASE 100   /**< 路由表起始编号 (同时也是 fwmark)。 */
#define MAGIC_RT_TABLE_MAX 199    /**< 路由表最大编号。 */
#define MAGIC_FWMARK_BLACKHOLE 99 /**< 黑洞路由标记，用于阻断未授权流量。 */

#define MAGIC_FWMARK_RULE_PRIORITY                                             \
  100 /**< 静态 fwmark 规则在 ip rule 中的优先级。 */
#define MAGIC_BLACKHOLE_PRIORITY 50 /**< 黑洞规则优先级 (最高优先)。 */

#define MAGIC_RULE_PRIORITY_BASE                                               \
  1000 /**< 动态 IP Rule 优先级起始值（兼容模式使用）。 */
#define MAGIC_RULE_PRIORITY_MAX 2000 /**< 动态 IP Rule 优先级上限。 */
/** @} */

/** @name 容量限制
 * @{
 */
#define MAX_DATAPLANE_LINKS 10 /**< 最大支持的链路数。 */
#define MAX_ROUTING_RULES 256  /**< 最大路由规则数。 */
#define MAX_TFT_RULES 1024     /**< 最大 TFT (Traffic Flow Template) 规则数。 */
/** @} */

/** @name 字符串长度
 * @{
 */
#define MAX_IF_NAME_LEN 16 /**< 网络接口名称最大长度。 */
#define MAX_IP_ADDR_LEN 64 /**< IP 地址字符串最大长度。 */
#define MAX_LINK_ID_LEN 64 /**< 链路 ID 标识符最大长度。 */
#define MAX_CMD_LEN 512    /**< 系统命令字符串缓冲区长度。 */
/** @} */

/*===========================================================================
 * TFT (Traffic Flow Template) 数据结构
 *===========================================================================*/

/**
 * @brief TFT 五元组定义。
 * @details 用于精确匹配特定的流量流并进行差异化服务。
 */
typedef struct {
  char src_ip[MAX_IP_ADDR_LEN]; ///< 源 IP 地址 (通常为客户端虚拟 IP)。
  char dst_ip[MAX_IP_ADDR_LEN]; ///< 目的 IP 地址。
  uint16_t src_port;            ///< 源端口号 (0 表示任意)。
  uint16_t dst_port;            ///< 目的端口号 (0 表示任意)。
  uint8_t protocol;             ///< IP 协议号 (6=TCP, 17=UDP, 1=ICMP, 0=任意)。
} TftTuple;

/**
 * @brief TFT 规则条目记录。
 * @details 记录一个活动的 TFT 过滤规则及其关联的链路资源。
 */
typedef struct {
  bool in_use;                   ///< 槽位占用标识。
  TftTuple tuple;                ///< 匹配用的五元组。
  char session_id[64];           ///< 关联的 Diameter 会话 ID。
  char link_id[MAX_LINK_ID_LEN]; ///< 该流被分流到的物理链路 ID。
  uint32_t fwmark;               ///< 分配给该流的内部路由标记。
  time_t created_at;             ///< 规则创建时间。
} TftRule;

/*===========================================================================
 * 路由管理数据结构
 *===========================================================================*/

/**
 * @brief 物理链路路由配置详情。
 * @details 描述每个外部接口（如 SATCOM, WiFi）的路由表映射关系。
 */
typedef struct {
  char link_id[MAX_LINK_ID_LEN];        ///< 链路唯一标识 (如 "SATCOM")。
  char interface_name[MAX_IF_NAME_LEN]; ///< 对应的 Linux 网络接口名 (如
                                        ///< "ens37")。
  char gateway_ip[MAX_IP_ADDR_LEN]; ///< 该链路的网关地址 (可选)。
  uint32_t route_table_id;          ///< 对应的 Linux 策略路由表 ID。
  uint32_t fwmark;                  ///< 该链路对应的 FwMark。
  bool is_configured;               ///< 路由表是否已完成系统下发。
} LinkRouteConfig;

/**
 * @brief 客户端整机路由规则 (旧版兼容)。
 */
typedef struct {
  bool in_use;                     ///< 标识记录是否有效。
  char client_ip[MAX_IP_ADDR_LEN]; ///< 客户端 IP。
  char session_id[64];             ///< 会话 ID。
  char link_id[MAX_LINK_ID_LEN];   ///< 链路 ID。
  uint32_t rule_priority;          ///< 规则优先级。
  uint32_t route_table_id;         ///< 目标路由表。
  uint32_t fwmark;                 ///< 路由标记。
  time_t created_at;               ///< 创建时间。
} ClientRoutingRule;

/**
 * @brief 数据平面全局上下文。
 * @details 统一管理所有的路由配置、TFT 规则链及并发保护锁。
 */
typedef struct {
  char ingress_interface[MAX_IF_NAME_LEN]; ///< 客户端流量进入的“南向”接口 (如
                                           ///< "ens39")。
  char ingress_ip[MAX_IP_ADDR_LEN]; ///< 入口接口的本地 IP。

  LinkRouteConfig links[MAX_DATAPLANE_LINKS]; ///< 可用的物理出口链路库。
  uint32_t num_links;                         ///< 当前注册的链路总数。

  ClientRoutingRule rules[MAX_ROUTING_RULES]; ///< (兼容用) 客户端全局规则池。
  uint32_t num_rules;                         ///< 总规则数。
  uint32_t next_priority;                     ///< 下一个优先级序号。

  TftRule tft_rules[MAX_TFT_RULES]; ///< 精细化 TFT 规则库。
  uint32_t num_tft_rules;           ///< 当前活动的 TFT 总数。

  pthread_mutex_t mutex; ///< 全局配置锁，确保规则更新的线程安全。

  bool is_initialized;         ///< 模块初始化标识。
  bool enable_routing;         ///< 总路由开关。
  bool fwmark_rules_installed; ///< 系统层面静态规则安装标识。
} DataplaneContext;

/*===========================================================================
 * API 函数声明
 *===========================================================================*/

/**
 * @brief 初始化数据平面模块。
 * @details 执行系统层面的冷启动：清理旧规则、安装 fwmark
 * 静态策略、初始化黑洞路由表。
 * @param ctx 数据平面上下文。
 * @param ingress_if 入口网卡名。
 * @param ingress_ip 入口本地 IP。
 * @return int 0 成功。
 */
int magic_dataplane_init(DataplaneContext *ctx, const char *ingress_if,
                         const char *ingress_ip);

/**
 * @brief 注册一个新的物理出口链路。
 * @details 系统会为该链路创建独立的策略路由表，并分配唯一的 fwmark 标记。
 * @param ctx 上下文。
 * @param link_id 链路逻辑名 (如 "LTE")。
 * @param interface_name 对应的网卡 ID。
 * @param gateway_ip 网关地址。
 * @return int 分配的路由表 ID，失败返回 -1。
 */
int magic_dataplane_register_link(DataplaneContext *ctx, const char *link_id,
                                  const char *interface_name,
                                  const char *gateway_ip);

/**
 * @brief 注销物理出口链路。
 * @param ctx 上下文。
 * @param link_id 链路逻辑名。
 * @return int 0 成功。
 */
int magic_dataplane_unregister_link(DataplaneContext *ctx, const char *link_id);

/**
 * @brief 建立客户端整机默认路由。
 */
int magic_dataplane_add_client_route(DataplaneContext *ctx,
                                     const char *client_ip,
                                     const char *session_id,
                                     const char *link_id, const char *dest_ip);

/**
 * @brief 删除特定会话关联的所有路由规则。
 */
int magic_dataplane_remove_client_route(DataplaneContext *ctx,
                                        const char *session_id);

/**
 * @brief 将活跃会话的热切换到另一条链路上（无损切换）。
 */
int magic_dataplane_switch_client_link(DataplaneContext *ctx,
                                       const char *session_id,
                                       const char *new_link_id);

/**
 * @brief 根据链路 ID 检索其在 Linux 系统中的路由表 ID。
 */
uint32_t magic_dataplane_get_table_id(DataplaneContext *ctx,
                                      const char *link_id);

/**
 * @brief 检索链路对应的路由标记位 (FwMark)。
 */
uint32_t magic_dataplane_get_fwmark(DataplaneContext *ctx, const char *link_id);

/**
 * @brief 查寻链路配置的网关 IP。
 */
int magic_dataplane_get_link_gateway(DataplaneContext *ctx, const char *link_id,
                                     char *gateway_ip, size_t ip_len);

/**
 * @brief 动态更新链路的网关配置。
 */
int magic_dataplane_update_link_gateway(DataplaneContext *ctx,
                                        const char *link_id,
                                        const char *gateway_ip);

/**
 * @brief 基于会话号检索对应的旧版路由记录。
 */
ClientRoutingRule *magic_dataplane_find_rule(DataplaneContext *ctx,
                                             const char *session_id);

/*===========================================================================
 * TFT (Traffic Flow Template) 精细控制 API
 *===========================================================================*/

/**
 * @brief 添加一条基于五元组的精细化分流规则。
 * @details 对应下发 iptables mangle 规则（打标）和 filter 规则（白名单放行）。
 * @param ctx 上下文。
 * @param tuple TFT 五元组描述。
 * @param session_id 关联会话。
 * @param link_id 目标分流链路。
 * @return int 0 成功。
 */
int magic_dataplane_add_tft_rule(DataplaneContext *ctx, const TftTuple *tuple,
                                 const char *session_id, const char *link_id);

/**
 * @brief 批量删除会话关联的所有 TFT 规则。
 * @return int 实际被删除的规则条数。
 */
int magic_dataplane_remove_tft_rules(DataplaneContext *ctx,
                                     const char *session_id);

/**
 * @brief 检索会话当前下发的所有精细化规则列表。
 */
int magic_dataplane_find_tft_rules(DataplaneContext *ctx,
                                   const char *session_id, TftRule **rules,
                                   int max_rules);

/**
 * @brief 向控制台打印数据平面的完整运行快照（链路、规则表等）。
 */
void magic_dataplane_print_status(DataplaneContext *ctx);

/**
 * @brief 清理并销毁数据平面模块，恢复系统初始路由状态。
 */
void magic_dataplane_cleanup(DataplaneContext *ctx);

/*===========================================================================
 * 命令行与辅助工具
 *===========================================================================*/

/**
 * @name 白名单管理 (ipset)
 * @{
 */
int magic_dataplane_ipset_init(DataplaneContext *ctx);
int magic_dataplane_ipset_destroy(DataplaneContext *ctx);
int magic_dataplane_ipset_add_control(const char *client_ip);
int magic_dataplane_ipset_add_data(const char *client_ip);
int magic_dataplane_ipset_del(const char *client_ip);
/** @} */

/** @brief 调用 Shell 执行系统命令。 */
int magic_dataplane_exec_cmd(const char *cmd);

/** @brief 检测网卡是否在当前系统中真实存在。 */
bool magic_dataplane_interface_exists(const char *interface_name);

#endif /* MAGIC_DATAPLANE_H */
