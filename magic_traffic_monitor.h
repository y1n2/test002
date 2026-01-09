/**
 * @file magic_traffic_monitor.h
 * @brief MAGIC 流量监控模块 - 基于 Linux Netlink 机制
 * @description 通过 libnetfilter_conntrack 读取内核连接追踪计数器，
 *              实现实时流量统计（字节数/数据包数）
 *
 * 架构说明:
 * ┌─────────────┐      ┌───────────────────┐      ┌─────────────────┐
 * │   Client    │──────│  Linux Kernel     │──────│  MAGIC CIC      │
 * │   Traffic   │      │  Netfilter/       │      │  Traffic Monitor│
 * └─────────────┘      │  Conntrack        │      └────────┬────────┘
 *                      └───────────────────┘               │
 *                              │                           │
 *                              ▼                           ▼
 *                      ┌───────────────────┐      ┌─────────────────┐
 *                      │  Conntrack Entry  │◀────▶│  Netlink Socket │
 *                      │  - mark (session) │      │  (libnetfilter) │
 *                      │  - bytes counter  │      └─────────────────┘
 *                      │  - packets counter│
 *                      └───────────────────┘
 *
 * 工作流程:
 * 1. MCCR 会话创建时，调用 traffic_register_session() 分配 conntrack mark
 * 2. iptables/nftables 规则使用此 mark 标记匹配的连接
 * 3. MADR 请求时，调用 traffic_get_session_stats() 通过 Netlink 读取计数器
 * 4. 会话结束时，调用 traffic_unregister_session() 清理
 *
 * @author MAGIC System Development Team
 * @version 2.1
 * @date 2025-12-02
 */

#ifndef MAGIC_TRAFFIC_MONITOR_H
#define MAGIC_TRAFFIC_MONITOR_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

/* Conntrack Mark 范围: 0x100 - 0x1FF (256个会话) */
#define TRAFFIC_MARK_BASE 0x100
#define TRAFFIC_MARK_MAX 0x1FF
#define MAX_TRAFFIC_SESSIONS 256

/* 统计缓存刷新间隔 (秒) - 避免频繁 Netlink 查询 */
#define STATS_CACHE_TTL_SEC 2

/* 会话 ID 最大长度 */
#define MAX_SESSION_ID_LEN 128

/* 后端类型 */
typedef enum {
  TRAFFIC_BACKEND_IPTABLES = 0, ///< 使用 iptables + conntrack 后端。
  TRAFFIC_BACKEND_NFTABLES = 1, ///< 使用 nftables + conntrack 后端。
  TRAFFIC_BACKEND_AUTO = 2      ///< 自动检测可用后端。
} TrafficBackendType;

/*===========================================================================
 * 数据结构定义
 *===========================================================================*/

/**
 * @brief 会话流量统计
 */
typedef struct {
  uint64_t bytes_in;    ///< 入站字节数 (客户端发送)。
  uint64_t bytes_out;   ///< 出站字节数 (客户端接收)。
  uint64_t packets_in;  ///< 入站数据包数。
  uint64_t packets_out; ///< 出站数据包数。
  time_t start_time;    ///< 统计开始时间 (Unix 时间戳)。
  time_t last_update;   ///< 最后更新时间 (Unix 时间戳)。
} TrafficStats;

/**
 * @brief 会话流量跟踪条目
 */
typedef struct {
  bool in_use;                         ///< 是否使用中。
  char session_id[MAX_SESSION_ID_LEN]; ///< 关联的 Diameter 会话 ID。
  char client_id[64];                  ///< 客户端 ID (Origin-Host)。
  char client_ip[64];                  ///< 客户端 IP 地址 (IPv4/IPv6)。
  uint32_t conntrack_mark;   ///< 分配的 Conntrack 标记值 (用于内核流量识别)。
  TrafficStats stats;        ///< 实时流量统计 (从 Netlink 获取)。
  TrafficStats cached_stats; ///< 缓存的统计 (减少 Netlink 查询开销)。
  time_t cache_time;         ///< 缓存时间戳 (用于 TTL 判断)。
} TrafficSession;

/**
 * @brief 流量监控上下文
 */
typedef struct {
  TrafficSession sessions[MAX_TRAFFIC_SESSIONS]; ///< 会话数组 (固定大小池)。
  uint32_t session_count;                        ///< 当前活动会话数。
  uint32_t next_mark;    ///< 下一个可分配的 conntrack mark。
  pthread_mutex_t mutex; ///< 保护整个结构的互斥锁。

  TrafficBackendType backend; ///< 当前使用的后端类型。
  bool is_initialized;        ///< 模块是否已初始化。
  bool netlink_available;     ///< Netlink 接口是否可用。

  /* Netlink 句柄 (由实现文件管理) */
  void *nfct_handle; ///< libnetfilter_conntrack 句柄 (opaque)。
} TrafficMonitorContext;

/*===========================================================================
 * API 函数声明
 *===========================================================================*/

/**
 * @brief 初始化流量监控模块
 *
 * @param ctx 流量监控上下文指针
 * @param backend 后端类型 (IPTABLES/NFTABLES/AUTO)
 * @return 0=成功, -1=失败
 */
int traffic_monitor_init(TrafficMonitorContext *ctx,
                         TrafficBackendType backend);

/**
 * @brief 注册会话进行流量跟踪
 * 分配 conntrack mark 并添加对应的防火墙规则
 *
 * @param ctx 流量监控上下文指针
 * @param session_id 会话 ID
 * @param client_id 客户端 ID (Origin-Host)
 * @param client_ip 客户端 IP 地址
 * @return 分配的 conntrack mark, 0=失败
 */
uint32_t traffic_register_session(TrafficMonitorContext *ctx,
                                  const char *session_id, const char *client_id,
                                  const char *client_ip);

/**
 * @brief 取消注册会话
 * 删除防火墙规则并释放 conntrack mark
 *
 * @param ctx 流量监控上下文指针
 * @param session_id 会话 ID
 * @return 0=成功, -1=未找到
 */
int traffic_unregister_session(TrafficMonitorContext *ctx,
                               const char *session_id);

/**
 * @brief 获取会话流量统计
 * 通过 Netlink 查询内核 conntrack 计数器
 *
 * @param ctx 流量监控上下文指针
 * @param session_id 会话 ID
 * @param stats 输出: 流量统计
 * @return 0=成功, -1=未找到
 */
int traffic_get_session_stats(TrafficMonitorContext *ctx,
                              const char *session_id, TrafficStats *stats);

/**
 * @brief 获取客户端的所有会话流量统计 (USER_DEPENDENT)
 *
 * @param ctx 流量监控上下文指针
 * @param client_id 客户端 ID
 * @param total_stats 输出: 累计流量统计
 * @return 会话数量, -1=失败
 */
int traffic_get_client_stats(TrafficMonitorContext *ctx, const char *client_id,
                             TrafficStats *total_stats);

/**
 * @brief 获取全部会话流量统计 (ALL)
 *
 * @param ctx 流量监控上下文指针
 * @param total_stats 输出: 累计流量统计
 * @return 会话数量
 */
int traffic_get_all_stats(TrafficMonitorContext *ctx,
                          TrafficStats *total_stats);

/**
 * @brief 刷新统计缓存
 * 强制从内核重新读取所有会话的流量计数器
 *
 * @param ctx 流量监控上下文指针
 * @return 0=成功, -1=失败
 */
int traffic_refresh_stats(TrafficMonitorContext *ctx);

/**
 * @brief 查找会话
 *
 * @param ctx 流量监控上下文指针
 * @param session_id 会话 ID
 * @return 会话指针, NULL=未找到
 */
TrafficSession *traffic_find_session(TrafficMonitorContext *ctx,
                                     const char *session_id);

/**
 * @brief 根据 conntrack mark 查找会话
 *
 * @param ctx 流量监控上下文指针
 * @param mark Conntrack mark 值
 * @return 会话指针, NULL=未找到
 */
TrafficSession *traffic_find_by_mark(TrafficMonitorContext *ctx, uint32_t mark);

/**
 * @brief 打印流量监控状态
 *
 * @param ctx 流量监控上下文指针
 */
void traffic_monitor_print_status(TrafficMonitorContext *ctx);

/**
 * @brief 清理流量监控模块
 *
 * @param ctx 流量监控上下文指针
 */
void traffic_monitor_cleanup(TrafficMonitorContext *ctx);

/*===========================================================================
 * 内部辅助函数 (供 magic_cic.c 调用)
 *===========================================================================*/

/**
 * @brief 计算 session_id 的 hash 值作为 conntrack mark
 * 使用 DJB2 算法，映射到 [TRAFFIC_MARK_BASE, TRAFFIC_MARK_MAX] 范围
 *
 * @param session_id 会话 ID
 * @return hash 值 (作为 conntrack mark)
 */
uint32_t traffic_session_id_to_mark(const char *session_id);

/**
 * @brief 检测可用的后端类型
 *
 * @return 检测到的后端类型
 */
TrafficBackendType traffic_detect_backend(void);

#endif /* MAGIC_TRAFFIC_MONITOR_H */
