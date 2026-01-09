/**
 * @file magic_session.h
 * @brief MAGIC 会话管理子系统头文件。
 * @details 该模块负责跟踪每个客户端的活动会话生命周期、链路资源分配以及 TFT
 * 流量隔离规则。 基于 ARINC 839
 * 标准设计，实现了完整的会话状态机和多客户端并发控制。
 *
 * 主要功能：
 * - 会话状态机管理 (INIT -> AUTHENTICATED -> ACTIVE -> CLOSED)
 * - 客户端级别的带宽配额与并发限制 (ClientContext)
 * - 基于 IP 五元组的流量过滤规则管理 (TFT)
 * - 状态订阅与推送机制 (REQ-Status-Info)
 *
 * @author MAGIC System Development Team
 * @date 2025-11-30
 */

#ifndef MAGIC_SESSION_H
#define MAGIC_SESSION_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_SESSIONS 100
#define MAX_SESSION_ID_LEN 128     /* 与 magic_traffic_monitor.h 保持一致 */
#define MAX_SESSIONS_PER_CLIENT 10 /* 每个客户端最大并发会话数 */
#define MAX_TFT_PER_SESSION 8      /* 每个会话最大 TFT 规则数 */

/*===========================================================================
 * 会话状态 - 根据 ARINC 839 / 4.1.3.1 设计
 *
 * 状态流转图:
 *   INIT ──(认证成功)──> AUTHENTICATED ──(资源分配)──> ACTIVE
 *                                          │            │
 *                                          │      (修改请求)
 *                                          │            ↓
 *                                          │       MODIFYING ──(修改完成)──>
 * ACTIVE │            │ ↓            │ (链路断开)        │ │            │ ↓
 *        │ SUSPENDED ──(恢复)──> ACTIVE │ (超时/终止) │ ↓ TERMINATING
 * ──(清理完成)──> CLOSED
 *===========================================================================*/

/**
 * @brief 会话状态枚举。
 * @details 定义了 ARINC 839 协议中会话生命周期的各个阶段。
 */
typedef enum {
  SESSION_STATE_INIT = 0, ///< [0] 初始态 - 收到请求，尚未分配资源。
  SESSION_STATE_AUTHENTICATED =
      1,                       ///< [1] 已认证 - 身份验证通过，未建立数据通道。
  SESSION_STATE_ACTIVE = 2,    ///< [2] 活跃态 - 资源已分配，数据传输中。
  SESSION_STATE_MODIFYING = 3, ///< [3] 修改中 - 正在变更带宽或切换链路。
  SESSION_STATE_SUSPENDED = 4, ///< [4] 挂起态 - 链路断开，会话上下文保持。
  SESSION_STATE_TERMINATING = 5, ///< [5] 拆除中 - 正在释放资源，生成最终 CDR。
  SESSION_STATE_CLOSED = 6       ///< [6] 历史态 - 会话已销毁。
} SessionState;

/**
 * @brief 状态信息订阅级别 (对应 REQ-Status-Info AVP)。
 * @details 决定了服务端向客户端推送哪些类型的状态更新 (MNTR)。
 */
typedef enum {
  STATUS_SUBSCRIBE_NONE = 0,  ///< [0] 不订阅任何状态。
  STATUS_SUBSCRIBE_MAGIC = 1, ///< [1] 仅订阅 MAGIC 系统状态。
  STATUS_SUBSCRIBE_DLM_GENERAL =
      2, ///< [2] 仅订阅 DLM 一般状态 (Available/LinkStatus)。
  STATUS_SUBSCRIBE_MAGIC_DLM = 3,    ///< [3] 订阅 MAGIC + DLM 一般状态。
  STATUS_SUBSCRIBE_DLM_DETAILED = 6, ///< [6] 订阅 DLM 详细状态 (含信号强度等)。
  STATUS_SUBSCRIBE_ALL = 7           ///< [7] 订阅所有可用状态。
} StatusSubscribeLevel;

/*===========================================================================
 * TFT 规则存储结构 (会话级别)
 * 用于记录该会话声明的流量过滤规则，确保流量隔离
 *===========================================================================*/

/**
 * @brief TFT (Traffic Flow Template) 规则结构 (会话级别)。
 * @details 用于记录该会话声明的流量过滤规则，确保流量在不同会话间隔离。
 */
typedef struct {
  bool in_use;       ///< 该规则槽位是否被占用。
  uint8_t tft_id;    ///< TFT 规则 ID (唯一标识)。
  uint8_t direction; ///< 流量方向: 1=ToGround (地->机), 2=ToAircraft (机->地)。
  uint8_t protocol;  ///< IP 协议号: 6=TCP, 17=UDP, 0=Any。
  char src_ip[64];   ///< 源 IP 地址字符串 (支持 CIDR)。
  char dst_ip[64];   ///< 目的 IP 地址字符串 (支持 CIDR)。
  uint16_t src_port_start; ///< 源端口范围起始 (0=Any)。
  uint16_t src_port_end;   ///< 源端口范围结束。
  uint16_t dst_port_start; ///< 目的端口范围起始 (0=Any)。
  uint16_t dst_port_end;   ///< 目的端口范围结束。
} SessionTftRule;

/*===========================================================================
 * 客户端会话 (SessionContext)
 * 描述单个会话的完整上下文信息
 *===========================================================================*/

/**
 * @brief 客户端会话 (SessionContext)。
 * @details 描述单个 Diameter 会话的完整上下文信息，包括状态、资源、TFT
 * 和统计数据。
 */
typedef struct {
  bool in_use;                         ///< 槽位使用标志。
  char session_id[MAX_SESSION_ID_LEN]; ///< Diameter Session-Id (全球唯一)。
  char client_id[64];                  ///< 客户端标识 (Origin-Host)。
  char client_realm[64];       ///< 客户端域 (Origin-Realm)，用于 MNTR 路由。
  char assigned_link_id[64];   ///< 当前分配的 DLM 链路名称 (如 "SATCOM1")。
  char assigned_interface[32]; ///< 当前绑定的网络接口名 (如 "eth0.100")。
  uint8_t bearer_id;           ///< 分配的承载 ID (Bearer-Identifier)。
  SessionState state;          ///< 当前会话状态。
  time_t created_at;           ///< 会话创建时间戳。
  time_t last_activity;        ///< 最后活动时间戳 (用于超时清理)。

  /* 资源契约信息 */
  uint32_t granted_bw_kbps;     ///< 批准的下行带宽 (kbps)。
  uint32_t granted_ret_bw_kbps; ///< 批准的上行带宽 (kbps)。
  uint8_t priority_class;       ///< 业务优先级类别 (1-9)。
  uint8_t qos_level;            ///< 此会话的 QoS 级别 (0=BE, 1=AF, 2=EF)。

  /* TFT 流量模板 - 确保会话间流量隔离 */
  SessionTftRule tft_rules[MAX_TFT_PER_SESSION]; ///< 会话绑定的 TFT 规则数组。
  uint32_t num_tft_rules;                        ///< 当前有效的 TFT 规则数量。

  /* 状态订阅信息 (REQ-Status-Info) */
  uint32_t subscribed_status_level; ///< 客户端订阅的状态级别。
  bool status_subscription_active;  ///< 状态订阅是否处于激活状态。

  /* Profile 信息 */
  char profile_name[64]; ///< 该会话使用的 Profile 名称 (如 "VOICE")。
  char client_ip[64];    ///< 客户端 IP 地址 (用于 TFT 校验)。

  /* 认证信息 */
  time_t auth_expire_time;    ///< 认证过期时间戳。
  uint32_t auth_grace_period; ///< 认证宽限期 (秒)。

  /* 链路切换防抖动跟踪 */
  time_t last_link_switch_time; ///< 上一次发生链路切换的时间。
  char previous_link_id[64];    ///< 切换前的链路 ID。
  uint32_t current_bw_percent;  ///< 当前链路带宽利用率百分比 (0-100)。

  /* MNTR 广播风暴抑制 */
  time_t last_mntr_sent_time;     ///< 上一次发送 MNTR 的时间。
  uint32_t last_notified_bw_kbps; ///< 上一次通知客户端的带宽值 (防抖动)。
  bool mntr_pending_ack;          ///< 是否有 MNTR 未收到 ACK。

  /* 网关 IP (用于链路切换通知) */
  char gateway_ip[64]; ///< 当前分配的网关 IP 地址。

  /* 流量监控 (Netlink conntrack) */
  uint32_t conntrack_mark;   ///< Conntrack 标记值 (用于流量统计)。
  uint64_t bytes_in;         ///< 累计入站字节数 (缓存)。
  uint64_t bytes_out;        ///< 累计出站字节数 (缓存)。
  time_t traffic_start_time; ///< 流量统计开始时间。

  /* CDR 关联 */
  char cdr_id[64];   ///< 当前关联的 CDR 记录 ID。
  char cdr_uuid[64]; ///< 当前关联的 CDR UUID。

  /* Keep-Alive 策略 */
  bool keep_request; ///< 是否请求链路断开时保持会话 (Keep-Alive)。
} ClientSession;

/*===========================================================================
 * 客户端上下文 (ClientContext)
 * 管理客户端级别的资源配额和会话列表
 *
 * 设计说明:
 * 1. 每个 client_id (Origin-Host) 对应一个 ClientContext
 * 2. ClientContext 跟踪该客户端的所有活跃会话
 * 3. 支持带宽配额累计检查 (canAllocate)
 * 4. 支持并发会话数限制
 *===========================================================================*/

#define MAX_CLIENTS 50 /* 最大客户端数量 */

/**
 * @brief 客户端上下文 (ClientContext)。
 * @details 管理客户端级别的资源配额和会话列表。
 *          - 每个 client_id (Origin-Host) 对应一个 ClientContext。
 *          - 跟踪该客户端的所有活跃会话。
 *          - 支持带宽配额累计检查 (canAllocate)。
 *          - 支持并发会话数限制。
 */
typedef struct {
  bool in_use;        ///< 槽位使用标志。
  char client_id[64]; ///< 客户端 ID (Origin-Host)。

  /* 配置文件信息 */
  char profile_name[64]; ///< 关联的配置文件名 (如 "DEFAULT")。

  /* 带宽配额 (来自 ClientProfile.Bandwidth) */
  uint32_t max_forward_bw_kbps;        ///< 最大下行带宽配额 (kbps)。
  uint32_t max_return_bw_kbps;         ///< 最大上行带宽配额 (kbps)。
  uint32_t guaranteed_forward_bw_kbps; ///< 保证下行带宽 (kbps)。
  uint32_t guaranteed_return_bw_kbps;  ///< 保证上行带宽 (kbps)。

  /* 会话配额 (来自 ClientProfile.Session) */
  uint32_t max_concurrent_sessions; ///< 最大允许的并发会话数。
  uint32_t session_timeout_sec;     ///< 会话超时时间 (秒)。

  /* 当前资源使用情况 */
  uint32_t total_allocated_forward_bw; ///< 已分配的总下行带宽 (kbps)。
  uint32_t total_allocated_return_bw;  ///< 已分配的总上行带宽 (kbps)。

  /* 活跃会话跟踪 */
  int active_session_indices
      [MAX_SESSIONS_PER_CLIENT]; ///< 活跃会话在全局数组中的索引列表。
  int active_session_count;      ///< 当前活跃会话数量。

  /* 统计信息 */
  uint64_t total_sessions_created; ///< 累计创建的会话总数。
  uint64_t total_bytes_in;         ///< 累计入站字节数 (流量)。
  uint64_t total_bytes_out;        ///< 累计出站字节数 (流量)。

  time_t first_seen;    ///< 首次看到该客户端的时间。
  time_t last_activity; ///< 最后一次活动时间。
} ClientContext;

/*===========================================================================
 * 会话管理上下文
 *===========================================================================*/

/**
 * @brief 会话管理器上下文。
 * @details 全局单例 (g_magic_ctx.session_mgr)，持有所有会话和客户端上下文。
 */
typedef struct {
  ClientSession sessions[MAX_SESSIONS]; ///< 全局会话存储池。
  int session_count;                    ///< 当前全局活跃会话数量。

  /* 客户端上下文管理 */
  ClientContext clients[MAX_CLIENTS]; ///< 全局客户端上下文存储池。
  int client_count;                   ///< 当前已记录的客户端数量。

  pthread_mutex_t mutex; ///< 保护会话和客户端数组的互斥锁。
} SessionManager;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化会话管理器。
 * @param mgr 指向会话管理器实例的指针。
 * @return 0 成功，-1 失败（参数为空）。
 */
int magic_session_init(SessionManager *mgr);

/**
 * @brief 统计指定客户端的活动会话数量。
 * @param mgr 会话管理器。
 * @param client_id 客户端标识。
 * @return 活动会话数，若未找到则返回 0。
 */
int magic_session_count_by_client(SessionManager *mgr, const char *client_id);

/**
 * @brief 根据 Session-Id 查找会话。
 * @param mgr 会话管理器。
 * @param session_id 全球唯一的 Diameter Session-Id。
 * @return 成功返回会话指针，未找到返回 NULL。
 */
ClientSession *magic_session_find_by_id(SessionManager *mgr,
                                        const char *session_id);

/**
 * @brief 创建新会话。
 * @details 在全局池中分配一个空闲槽位并初始化。会自动创建或关联 ClientContext。
 *
 * @param mgr 会话管理器。
 * @param session_id 新会话 ID。
 * @param client_id 客户端 Origin-Host。
 * @param client_realm 客户端 Origin-Realm。
 * @return 成功返回新会话指针，失败（池满）返回 NULL。
 */
ClientSession *magic_session_create(SessionManager *mgr, const char *session_id,
                                    const char *client_id,
                                    const char *client_realm);

/**
 * @brief 更新会话的链路分配信息。
 * @details 记录 assigned_link_id, bearer_id 及授权带宽。
 *
 * @param session 目标会话。
 * @param link_id 新分配的链路标识。
 * @param bearer_id 承载标识。
 * @param granted_bw_kbps 授权下行带宽。
 * @param granted_ret_bw_kbps 授权上行带宽。
 * @return 0 成功，-1 参数错误。
 */
int magic_session_assign_link(ClientSession *session, const char *link_id,
                              uint8_t bearer_id, uint32_t granted_bw_kbps,
                              uint32_t granted_ret_bw_kbps);

/**
 * @brief 释放会话占用的链路资源。
 * @details 清空链路 ID 和承载 ID，但不会删除会话本身（状态变为 SUSPENDED
 * 或待处理）。
 * @param mgr 会话管理器。
 * @param session 待释放资源的会话。
 * @return 0 成功，-1 失败。
 */
int magic_session_release_link(SessionManager *mgr, ClientSession *session);

/**
 * @brief 删除并销毁会话。
 * @details 释放所有资源（包括配额），将状态置为 CLOSED，并回收槽位。
 * @param mgr 会话管理器。
 * @param session_id 待删除的会话 ID。
 * @return 0 成功，-1 未找到。
 */
int magic_session_delete(SessionManager *mgr, const char *session_id);

/**
 * @brief 清理超时会话
 * @param timeout_sec 超时时间(秒)
 */
int magic_session_cleanup_timeout(SessionManager *mgr, uint32_t timeout_sec);

/**
 * @brief 清理所有会话
 */
void magic_session_cleanup(SessionManager *mgr);

/**
 * @brief 设置会话状态
 */
int magic_session_set_state(ClientSession *session, SessionState new_state);

/**
 * @brief 设置会话的状态订阅级别
 */
int magic_session_set_subscription(ClientSession *session, uint32_t level);

/**
 * @brief 查找所有已订阅状态的会话
 * @param mgr 会话管理器
 * @param sessions 输出: 会话指针数组
 * @param max_count 数组最大容量
 * @return 返回找到的会话数量
 */
int magic_session_find_subscribed(SessionManager *mgr, ClientSession **sessions,
                                  int max_count);

/**
 * @brief 根据客户端ID查找会话
 */
ClientSession *magic_session_find_by_client(SessionManager *mgr,
                                            const char *client_id);

/**
 * @brief 挂起会话 (链路中断时调用)
 */
int magic_session_suspend(SessionManager *mgr, ClientSession *session);

/**
 * @brief 恢复会话 (链路恢复时调用)
 */
int magic_session_resume(SessionManager *mgr, ClientSession *session);

/**
 * @brief 获取会话状态名称字符串
 */
const char *magic_session_state_name(SessionState state);

/**
 * @brief 获取所有活跃会话（用于动态策略验证）
 * @param mgr 会话管理器
 * @param sessions 输出: 会话指针数组
 * @param max_count 数组最大容量
 * @return 返回找到的会话数量
 */
int magic_session_get_active_sessions(SessionManager *mgr,
                                      ClientSession **sessions, int max_count);

/*===========================================================================
 * ClientContext API - 客户端级别的配额管理
 *===========================================================================*/

/**
 * @brief 获取或创建客户端上下文
 * @param mgr 会话管理器
 * @param client_id 客户端 ID
 * @return 成功返回 ClientContext 指针，失败返回 NULL
 */
ClientContext *magic_client_context_get_or_create(SessionManager *mgr,
                                                  const char *client_id);

/**
 * @brief 查找客户端上下文
 * @param mgr 会话管理器
 * @param client_id 客户端 ID
 * @return 找到返回 ClientContext 指针，未找到返回 NULL
 */
ClientContext *magic_client_context_find(SessionManager *mgr,
                                         const char *client_id);

/**
 * @brief 设置客户端带宽配额
 * @param ctx 客户端上下文
 * @param max_forward_kbps 最大下行带宽 (kbps)
 * @param max_return_kbps 最大上行带宽 (kbps)
 * @param guaranteed_forward 保证下行带宽 (kbps)
 * @param guaranteed_return 保证上行带宽 (kbps)
 */
void magic_client_context_set_quota(ClientContext *ctx,
                                    uint32_t max_forward_kbps,
                                    uint32_t max_return_kbps,
                                    uint32_t guaranteed_forward,
                                    uint32_t guaranteed_return);

/**
 * @brief 检查是否可以为客户端分配带宽
 * @param ctx 客户端上下文
 * @param request_forward_kbps 请求的下行带宽 (kbps)
 * @param request_return_kbps 请求的上行带宽 (kbps)
 * @param granted_forward_kbps [out] 批准的下行带宽 (kbps)
 * @param granted_return_kbps [out] 批准的上行带宽 (kbps)
 * @return 0=可以分配, -1=超过配额
 *
 * 算法:
 * 1. 检查 total_allocated + request <= max_quota
 * 2. 如果超出配额，尝试分配剩余可用带宽
 * 3. 如果剩余带宽不足保证带宽，则拒绝
 */
int magic_client_can_allocate_bandwidth(ClientContext *ctx,
                                        uint32_t request_forward_kbps,
                                        uint32_t request_return_kbps,
                                        uint32_t *granted_forward_kbps,
                                        uint32_t *granted_return_kbps);

/**
 * @brief 更新客户端已分配的带宽
 * @param ctx 客户端上下文
 * @param delta_forward 下行带宽增量 (正数=增加，负数=减少)
 * @param delta_return 上行带宽增量 (正数=增加，负数=减少)
 */
void magic_client_update_allocated_bandwidth(ClientContext *ctx,
                                             int32_t delta_forward,
                                             int32_t delta_return);

/**
 * @brief 检查客户端是否可以创建新会话
 * @param ctx 客户端上下文
 * @return true=可以创建, false=已达上限
 */
bool magic_client_can_create_session(ClientContext *ctx);

/**
 * @brief 将会话关联到客户端上下文
 * @param ctx 客户端上下文
 * @param session_index 会话在 sessions[] 数组中的索引
 * @return 0=成功, -1=失败
 */
int magic_client_add_session(ClientContext *ctx, int session_index);

/**
 * @brief 从客户端上下文移除会话
 * @param ctx 客户端上下文
 * @param session_index 会话索引
 * @return 0=成功, -1=未找到
 */
int magic_client_remove_session(ClientContext *ctx, int session_index);

/**
 * @brief 获取客户端的所有活跃会话
 * @param mgr 会话管理器
 * @param ctx 客户端上下文
 * @param sessions [out] 会话指针数组
 * @param max_count 数组最大容量
 * @return 返回找到的会话数量
 */
int magic_client_get_sessions(SessionManager *mgr, ClientContext *ctx,
                              ClientSession **sessions, int max_count);

/*===========================================================================
 * TFT 规则管理 API
 *===========================================================================*/

/**
 * @brief 向会话添加 TFT 规则
 * @param session 会话指针
 * @param tft TFT 规则
 * @return 0=成功, -1=数组已满, -2=参数错误
 */
int magic_session_add_tft(ClientSession *session, const SessionTftRule *tft);

/**
 * @brief 清除会话的所有 TFT 规则
 * @param session 会话指针
 */
void magic_session_clear_tfts(ClientSession *session);

/**
 * @brief 查找会话中匹配的 TFT 规则
 * @param session 会话指针
 * @param src_ip 源 IP
 * @param dst_ip 目的 IP
 * @param protocol 协议号
 * @param src_port 源端口
 * @param dst_port 目的端口
 * @return 找到返回 TFT 指针，未找到返回 NULL
 */
SessionTftRule *magic_session_find_tft(ClientSession *session,
                                       const char *src_ip, const char *dst_ip,
                                       uint8_t protocol, uint16_t src_port,
                                       uint16_t dst_port);

#endif /* MAGIC_SESSION_H */
