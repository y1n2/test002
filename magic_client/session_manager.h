/**
 * @file session_manager.h
 * @brief MAGIC 客户端会话与 DLM 状态管理器
 * @details 负责管理并发的 Diameter 会话生命周期，并缓存来自服务端的 DLM
 * 及链路实时状态。 支持 ARINC 839 规范中定义的多种会话状态迁移。
 */

#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/** @brief 最大允许的并发客户端会话数。 */
#define MAX_CLIENT_SESSIONS 10
/** @brief Session-Id 字符串的最大长度。 */
#define MAX_SESSION_ID_LEN 128

/*===========================================================================
 * DLM 状态存储结构 (v2.1: MSCR 解析支持)
 *===========================================================================*/

/** @brief 系统最大可跟踪的 DLM 物理模块数量。 */
#define MAX_DLM_COUNT 8
/** @brief 每个 DLM 下属最大物理链路数。 */
#define MAX_LINKS_PER_DLM 4

/**
 * @brief 单条链路状态记录。
 * @details 对应 Link-Status-Group AVP (20011)，保存物理层的实时连接指标。
 */
typedef struct {
  char link_name[64];          ///< 链路名称。
  uint32_t link_number;        ///< 链路编号（1-based）。
  uint32_t link_available;     ///< 可用状态 (0=不可用, 1=可用)。
  uint32_t link_conn_status;   ///< 连接状态 (0=断开, 1=连接, 2=强制关闭)。
  uint32_t link_login_status;  ///< 登录状态 (1=未登录, 2=已登录)。
  int32_t signal_strength_dbm; ///< 信号强度 (dBm)。
  char error_string[128];      ///< 链路异常时的描述信息。
  uint64_t max_bw_kbps;        ///< 本链路最大下行带宽 (kbps)。
  uint64_t alloc_bw_kbps;      ///< 本链路当前已分配下行带宽 (kbps)。
} LinkStatusRecord;

/**
 * @brief DLM 状态记录。
 * @details 对应 DLM-Info AVP (20008)，汇总单个物理模块的资源和链路情况。
 */
typedef struct {
  bool in_use;                 ///< 记录是否处于活动状态。
  char dlm_name[64];           ///< DLM 名称 (如 "Satcom-Ku")。
  uint32_t dlm_available;      ///< DLM 整体可用性 (0=不可用, 1=可用, 2=未知)。
  uint32_t dlm_max_links;      ///< 该模块支持的最大并行链路数。
  uint32_t dlm_alloc_links;    ///< 该模块当前正在使用的链路数。
  float dlm_max_bw_kbps;       ///< 模块最大下行总带宽 (kbps)。
  float dlm_alloc_bw_kbps;     ///< 模块当前已分配下行总带宽 (kbps)。
  float dlm_max_ret_bw_kbps;   ///< 模块最大上行总带宽 (kbps)。
  float dlm_alloc_ret_bw_kbps; ///< 模块当前已分配上行总带宽 (kbps)。

  LinkStatusRecord links[MAX_LINKS_PER_DLM]; ///< 物理链路状态数组。
  uint32_t link_count;                       ///< 有效链路状态记录数。

  time_t last_update; ///< 最后一次收到该 DLM 更新的时间戳。
} DLMStatusRecord;

/**
 * @brief DLM 状态总管理器。
 * @details 缓存所有 DLM 的快照。
 */
typedef struct {
  DLMStatusRecord records[MAX_DLM_COUNT]; ///< DLM 记录库。
  uint32_t count;                         ///< 当前已记录的 DLM 种类总数。
  uint32_t registered_clients; ///< 从服务端同步的当前总在线客户端数。
  time_t last_mscr_time;       ///< 最后收到 MSCR 推送的时间。
} DLMStatusManager;

/** @brief 全局 DLM 状态管理器实例。 */
extern DLMStatusManager g_dlm_status_mgr;

/**
 * @brief 初始化 DLM 状态管理器。
 */
void dlm_status_init(void);

/**
 * @brief 查找或创建指定的 DLM 状态存储记录空间。
 * @param dlm_name DLM 的唯一名称。
 * @return DLMStatusRecord* 指向对应记录的指针，若库满则返回 NULL。
 */
DLMStatusRecord *dlm_status_find_or_create(const char *dlm_name);

/**
 * @brief 更新指定 DLM 的可用性状态并检测是否发生跳变。
 * @param dlm_name DLM 名称。
 * @param new_available 新的可用性数值。
 * @return bool 如果状态发生变化返回 true，否则返回 false。
 */
bool dlm_status_update_available(const char *dlm_name, uint32_t new_available);

/**
 * @brief 在控制台格式化打印当前所有的 DLM 硬件与链路状态表。
 */
void dlm_status_print_all(void);

/*===========================================================================
 * 客户端会话管理
 *===========================================================================*/

/**
 * @brief 客户端会话状态枚举。
 * @details 遵循 ARINC 839 状态机设计。
 */
typedef enum {
  CLIENT_SESSION_IDLE = 0,       ///< 槽位空闲。
  CLIENT_SESSION_AUTHENTICATING, ///< 正在进行 MCAR 认证交换。
  CLIENT_SESSION_AUTHENTICATED,  ///< 认证成功，尚未分配具体链路资源。
  CLIENT_SESSION_ESTABLISHING,   ///< 正在进行 MCCR 资源申请交换。
  CLIENT_SESSION_ACTIVE,         ///< 会话活跃中，已分配 Bearer 且链路已连通。
  CLIENT_SESSION_MODIFYING,      ///< 正在修改会话参数（带宽、优先级等）。
  CLIENT_SESSION_TERMINATING     ///< 正在执行 STR 会话终止。
} ClientSessionState;

/**
 * @brief 单个客户端会话上下文。
 * @details 存储特定 Session-Id 关联的所有业务参数和统计信息。
 */
typedef struct {
  bool in_use;                         ///< 是否被占用。
  char session_id[MAX_SESSION_ID_LEN]; ///< Diameter Session-Id。
  ClientSessionState state;            ///< 当前会话状态。
  time_t created_at;                   ///< 会话创建时间。
  time_t last_activity;                ///< 最后活跃时间。

  /* 会话参数 */
  char profile_name[64];          ///< 使用的业务 Profile 名称（如 "IP_DATA"）。
  uint32_t requested_bw_kbps;     ///< 期望申请的下行带宽。
  uint32_t requested_ret_bw_kbps; ///< 期望申请的上行带宽。
  uint32_t granted_bw_kbps;       ///< 服务端实批的下行带宽。
  uint32_t granted_ret_bw_kbps;   ///< 服务端实批的上行带宽。
  uint8_t bearer_id;              ///< 分配的承载 ID。
  char assigned_link[64];         ///< 服务端指派的物理链路 ID。

  /* 统计信息 */
  uint32_t packets_sent;     ///< 该会话发送的包总数（占位）。
  uint32_t packets_received; ///< 该会话接收的包总数（占位）。
} ClientSessionRecord;

/**
 * @brief 会话管理器容器。
 * @details 管理客户端并发维护的所有会话。
 */
typedef struct SessionManager {
  ClientSessionRecord sessions[MAX_CLIENT_SESSIONS]; ///< 会话池。
  int num_active;                                    ///< 当前活动会话总激。
  char current_session_id
      [MAX_SESSION_ID_LEN]; ///< CLI 或管理接口当前操作的“焦点”会话。
} SessionManager;

/** @brief 全局会话管理器。 */
extern SessionManager g_session_manager;

/**
 * @brief 初始化会话管理器。
 * @param mgr 指向管理器的指针。
 */
void session_manager_init(SessionManager *mgr);

/**
 * @brief 构造并生成符合 RFC 标准的 Session-Id 字符串。
 * @param mgr 管理器实例。
 * @param session_id 存储生成的 ID 缓冲区。
 * @param size 缓冲区上限。
 * @return int 0 成功。
 */
int session_manager_generate_id(SessionManager *mgr, char *session_id,
                                size_t size);

/**
 * @brief 在内存池中创建一个新的会话记录并置为 AUTHENTICATING 状态。
 * @param mgr 管理器。
 * @param session_id 新会话的 ID。
 * @return ClientSessionRecord* 指向新记录的指针，失败返回 NULL。
 */
ClientSessionRecord *session_manager_create(SessionManager *mgr,
                                            const char *session_id);

/**
 * @brief 根据 Session-Id 字符串查找对应的会话记录。
 */
ClientSessionRecord *session_manager_find(SessionManager *mgr,
                                          const char *session_id);

/**
 * @brief 强行更新特定会话的状态。
 */
int session_manager_update_state(SessionManager *mgr, const char *session_id,
                                 ClientSessionState new_state);

/**
 * @brief 将会话状态标记为已通过认证。
 */
int session_manager_authenticated(SessionManager *mgr, const char *session_id);

/**
 * @brief 处理链路建立成功的反馈，更新会话的资源分配参数。
 * @param mgr 管理器。
 * @param session_id 目标会话。
 * @param granted_bw_kbps 批准的下行带宽。
 * @param granted_ret_bw_kbps 批准的上行带宽。
 * @param bearer_id 承载 ID。
 * @param link_id 指派的物理链路名。
 * @return int 0 成功。
 */
int session_manager_link_established(SessionManager *mgr,
                                     const char *session_id,
                                     uint32_t granted_bw_kbps,
                                     uint32_t granted_ret_bw_kbps,
                                     uint8_t bearer_id, const char *link_id);

/**
 * @brief 从管理器中彻底删除该会话记录（在收到终止确认后调用）。
 */
int session_manager_delete(SessionManager *mgr, const char *session_id);

/**
 * @brief 在终端列出所有正处于活动状态的会话及其详情。
 */
void session_manager_list_active(SessionManager *mgr);

/**
 * @brief 切换当前 CLI 交互的作用会话。
 */
int session_manager_set_current(SessionManager *mgr, const char *session_id);

/**
 * @brief 返回当前焦点会话的 ID 字符串。
 */
const char *session_manager_get_current(SessionManager *mgr);

/** @brief 返回当前总活跃会话数。 */
int session_manager_count_active(SessionManager *mgr);

/**
 * @brief 强制清理所有会话资源。
 */
void session_manager_cleanup(SessionManager *mgr);

#endif /* SESSION_MANAGER_H */
