/**
 * @file magic_session.c
 * @brief MAGIC 会话管理实现。
 * @details 实现了会话生命周期管理、资源分配、状态机流转及多线程并发控制。
 *          涵盖了 magic_session.h 中定义的所有 API。
 *
 * 主要模块：
 * - 初始化与清理 (Init/Cleanup)
 * - 会话查找与创建 (CRUD)
 * - 链路资源管理 (Assign/Release Link)
 * - 状态机管理 (Set State/Suspend/Resume)
 * - 客户端配额管理 (ClientContext)
 */

#include "magic_session.h" // 包含会话管理头文件，定义会话相关的结构体和函数声明
#include "app_magic.h"
#include <freeDiameter/freeDiameter-host.h> // 包含freeDiameter主机头文件，提供Diameter协议主机功能
#include <freeDiameter/libfdcore.h> // 包含freeDiameter核心库，提供Diameter协议核心功能
#include <string.h> // 包含字符串处理函数，如memset、strncpy、strcmp等

/*===========================================================================
 * 初始化
 *===========================================================================*/

/**
 * @brief 初始化会话管理器。
 * @details 清零所有会话槽位，初始化互斥锁。
 * @param mgr 指向会话管理器实例的指针。
 * @return 0 成功，-1 失败（参数为空）。
 */
int magic_session_init(SessionManager *mgr) {
  if (!mgr)
    return -1;

  memset(mgr->sessions, 0, sizeof(mgr->sessions));
  memset(mgr->clients, 0, sizeof(mgr->clients));
  mgr->session_count = 0;
  mgr->client_count = 0;

  // 初始化互斥锁，使用默认属性
  pthread_mutex_init(&mgr->mutex, NULL);

  fd_log_notice("[app_magic] Session manager initialized (max sessions: %d, "
                "max clients: %d)",
                MAX_SESSIONS, MAX_CLIENTS);
  return 0;
}

/*===========================================================================
 * 查找会话 - 提供根据客户端ID统计会话数量和根据会话ID查找会话的功能
 *===========================================================================*/

int magic_session_count_by_client(
    SessionManager *mgr,
    const char *client_id) // 根据客户端ID统计活动会话数量的函数
{
  if (!mgr || !client_id)
    return 0; // 检查会话管理器和客户端ID是否有效，如果无效则返回0，表示无会话

  int count = 0; // 初始化计数器为0，用于累加活动会话数量
  pthread_mutex_lock(
      &mgr->mutex); // 加锁互斥锁，确保对会话数组的访问是线程安全的

  for (int i = 0; i < MAX_SESSIONS;
       i++) {                      // 遍历所有会话槽位，查找匹配的客户端会话
    if (mgr->sessions[i].in_use && // 检查该槽位是否正在使用
        strcmp(mgr->sessions[i].client_id, client_id) ==
            0 && // 检查客户端ID是否匹配
        mgr->sessions[i].state ==
            SESSION_STATE_ACTIVE) { // 检查会话状态是否为活动状态
      count++;                      // 如果所有条件满足，计数器加1
    }
  }

  pthread_mutex_unlock(&mgr->mutex); // 解锁互斥锁，释放对会话数组的访问控制
  return count;                      // 返回统计到的活动会话数量
}

ClientSession *
magic_session_find_by_id(SessionManager *mgr,
                         const char *session_id) // 根据会话ID查找会话的函数
{
  if (!mgr || !session_id)
    return NULL; // 检查会话管理器和会话ID是否有效，如果无效则返回NULL，表示未找到

  pthread_mutex_lock(&mgr->mutex); // 加锁互斥锁，确保查找过程的线程安全

  for (int i = 0; i < MAX_SESSIONS; i++) { // 遍历所有会话槽位，查找匹配的会话ID
    if (mgr->sessions[i].in_use &&         // 检查该槽位是否正在使用
        strcmp(mgr->sessions[i].session_id, session_id) ==
            0) {                         // 检查会话ID是否匹配
      pthread_mutex_unlock(&mgr->mutex); // 找到匹配会话后立即解锁互斥锁
      return &mgr->sessions[i];          // 返回指向匹配会话的指针
    }
  }

  pthread_mutex_unlock(&mgr->mutex); // 如果未找到匹配会话，解锁互斥锁
  return NULL;                       // 返回NULL，表示未找到指定的会话
}

/*===========================================================================
 * 创建会话
 *===========================================================================*/

/**
 * @brief 创建新会话。
 * @details
 * 1. 加锁保护。
 * 2. 遍历查找空闲槽位。
 * 3. 初始化会话结构体，设置创建时间、状态为 INIT。
 * 4. 关联或创建 ClientContext。
 * 5. 增加会话计数并解锁。
 *
 * @param mgr 会话管理器。
 * @param session_id 全局唯一会话 ID。
 * @param client_id 客户端标识。
 * @param client_realm 客户端域。
 * @return 成功返回会话指针，失败（无空位）返回 NULL。
 */
ClientSession *magic_session_create(SessionManager *mgr, const char *session_id,
                                    const char *client_id,
                                    const char *client_realm) {
  if (!mgr || !session_id || !client_id)
    return NULL;

  pthread_mutex_lock(&mgr->mutex);

  /* 查找空闲槽位 */
  int free_slot = -1;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!mgr->sessions[i].in_use) {
      free_slot = i;
      break;
    }
  }

  if (free_slot == -1) {
    pthread_mutex_unlock(&mgr->mutex);
    fd_log_error("[app_magic] No available session slots");
    return NULL;
  }

  ClientSession *session = &mgr->sessions[free_slot];
  memset(session, 0, sizeof(ClientSession));

  session->in_use = true;
  strncpy(session->session_id, session_id, sizeof(session->session_id) - 1);
  strncpy(session->client_id, client_id, sizeof(session->client_id) - 1);

  /* v2.1 新增: 保存客户端域名，用于 MNTR 消息路由 */
  if (client_realm && client_realm[0]) {
    strncpy(session->client_realm, client_realm,
            sizeof(session->client_realm) - 1);
  }
  session->state = SESSION_STATE_INIT;
  session->created_at = time(NULL);
  session->last_activity = time(NULL);
  session->bearer_id = 0;
  session->subscribed_status_level = 0;
  session->status_subscription_active = false;
  session->num_tft_rules = 0;

  /* 关联到客户端上下文 */
  ClientContext *ctx = magic_client_context_get_or_create(mgr, client_id);
  if (ctx) {
    magic_client_add_session(ctx, free_slot);
  }

  mgr->session_count++;
  pthread_mutex_unlock(&mgr->mutex);

  fd_log_notice(
      "[app_magic] ✓ Session created: %s (client: %s, slot: %d) [total: %d]",
      session_id, client_id, free_slot, mgr->session_count);
  return session;
}

/*===========================================================================
 * 分配链路 - 为会话分配指定的链路资源，包括承载ID和带宽信息
 *===========================================================================*/

int magic_session_assign_link(
    ClientSession *session,       // 指向要分配链路的会话指针
    const char *link_id,          // 要分配的链路ID
    uint8_t bearer_id,            // 承载ID，用于标识承载
    uint32_t granted_bw_kbps,     // 授予的下行带宽，单位kbps
    uint32_t granted_ret_bw_kbps) // 授予的上行带宽，单位kbps
{
  if (!session || !link_id)
    return -1; // 检查会话指针和链路ID是否有效，如果无效则返回错误码-1

  /* v2.0 新增: 记录链路切换时间戳和上一链路ID */
  if (session->assigned_link_id[0] != '\0' &&
      strcmp(session->assigned_link_id, link_id) != 0) {
    /* 发生了链路切换 */
    strncpy(session->previous_link_id, session->assigned_link_id,
            sizeof(session->previous_link_id) - 1);
    session->last_link_switch_time = time(NULL);
    fd_log_debug("[app_magic] Link switch detected: %s -> %s",
                 session->previous_link_id, link_id);
  } else if (session->assigned_link_id[0] == '\0') {
    /* 首次分配链路 */
    session->last_link_switch_time = time(NULL);
  }

  strncpy(session->assigned_link_id, link_id,
          sizeof(session->assigned_link_id) -
              1);                 // 将链路ID复制到会话的分配链路字段
  session->bearer_id = bearer_id; // 设置会话的承载ID
  session->granted_bw_kbps = granted_bw_kbps;         // 设置授予的下行带宽
  session->granted_ret_bw_kbps = granted_ret_bw_kbps; // 设置授予的上行带宽
  session->state =
      SESSION_STATE_ACTIVE; // 将会话状态设置为活动状态，表示链路已分配
  session->last_activity = time(NULL); // 更新最后活动时间为当前时间

  fd_log_notice("[app_magic] ✓ Session assigned: %s → %s (Bearer %u, %u/%u "
                "kbps)", // 记录通知日志，显示链路分配成功
                session->client_id, link_id,
                bearer_id, // 记录客户端ID、链路ID和承载ID
                granted_bw_kbps, granted_ret_bw_kbps); // 记录授予的带宽信息
  return 0; // 返回成功码0，表示链路分配成功
}

/*===========================================================================
 * 释放链路资源 - 释放会话占用的链路资源，清理承载和带宽分配
 *===========================================================================*/

int magic_session_release_link(SessionManager *mgr,
                               ClientSession *session) // 释放会话链路资源的函数
{
  if (!mgr || !session)
    return -1; // 检查会话管理器和会话指针是否有效，如果无效则返回错误码-1

  if (session->bearer_id == 0 ||
      session->assigned_link_id[0] ==
          '\0') { // 检查会话是否已分配资源（承载ID不为0且链路ID不为空）
    /* 没有分配资源 - 如果没有分配资源，则无需释放，直接返回成功 */
    return 0; // 返回成功码0，表示无需释放
  }

  fd_log_notice(
      "[app_magic] Releasing link resources: %s (Bearer %u on %s)", // 记录通知日志，表示正在释放链路资源
      session->client_id, session->bearer_id,
      session->assigned_link_id); // 记录客户端ID、承载ID和链路ID

  /* TODO: 发送 MIH_Link_Resource.request(RELEASE) 到 DLM -
   * 待实现：向DLM发送释放链路资源的MIH请求 */
  /* 这里暂时只清理会话状态 - 当前仅清理本地会话状态，未来需要与DLM交互 */

  /* 在释放会话资源时，确保同时从数据平面移除对应的路由规则，避免遗留规则 */
  if (g_magic_ctx.dataplane_ctx.is_initialized && session->session_id[0]) {
    magic_dataplane_remove_client_route(&g_magic_ctx.dataplane_ctx,
                                        session->session_id);
  }

  session->bearer_id = 0;              // 将承载ID重置为0，表示释放承载
  session->assigned_link_id[0] = '\0'; // 将分配的链路ID清空
  session->granted_bw_kbps = 0;        // 将授予的下行带宽重置为0
  session->granted_ret_bw_kbps = 0;    // 将授予的上行带宽重置为0

  return 0; // 返回成功码0，表示链路资源释放成功
}

/*===========================================================================
 * 删除会话 - 根据会话ID删除指定的会话，释放所有相关资源
 *===========================================================================*/

int magic_session_delete(SessionManager *mgr,
                         const char *session_id) // 删除指定会话的函数
{
  if (!mgr || !session_id)
    return -1; // 检查会话管理器和会话ID是否有效，如果无效则返回错误码-1

  pthread_mutex_lock(&mgr->mutex); // 加锁互斥锁，确保删除过程的线程安全

  for (int i = 0; i < MAX_SESSIONS; i++) { // 遍历所有会话槽位，查找匹配的会话ID
    if (mgr->sessions[i].in_use &&         // 检查该槽位是否正在使用
        strcmp(mgr->sessions[i].session_id, session_id) ==
            0) { // 检查会话ID是否匹配

      ClientSession *session = &mgr->sessions[i];

      /* 更新客户端上下文的带宽配额 */
      ClientContext *ctx = NULL;
      for (int j = 0; j < MAX_CLIENTS; j++) {
        if (mgr->clients[j].in_use &&
            strcmp(mgr->clients[j].client_id, session->client_id) == 0) {
          ctx = &mgr->clients[j];
          break;
        }
      }

      if (ctx) {
        /* 回收带宽配额 */
        magic_client_update_allocated_bandwidth(
            ctx, -(int32_t)session->granted_bw_kbps,
            -(int32_t)session->granted_ret_bw_kbps);
        /* 从客户端上下文移除会话 */
        magic_client_remove_session(ctx, i);
      }

      /* 清除 TFT 规则 */
      magic_session_clear_tfts(session);

      /* 释放资源 - 在删除会话前，先释放其占用的链路资源 */
      pthread_mutex_unlock(
          &mgr->mutex); // 临时解锁，因为 release_link 可能需要锁
      magic_session_release_link(mgr, session); // 调用释放链路资源函数
      pthread_mutex_lock(&mgr->mutex);          // 重新加锁

      fd_log_notice(
          "[app_magic] Session deleted: %s (client: %s)", // 记录通知日志，表示会话已删除
          session_id, session->client_id); // 记录会话ID和客户端ID

      memset(session, 0, sizeof(ClientSession)); // 将会话槽位清零，标记为未使用
      if (mgr->session_count > 0)
        mgr->session_count--;            // 减少会话计数
      pthread_mutex_unlock(&mgr->mutex); // 解锁互斥锁
      return 0;                          // 返回成功码0，表示删除成功
    }
  }

  pthread_mutex_unlock(&mgr->mutex); // 如果未找到匹配会话，解锁互斥锁
  return -1;                         // 返回错误码-1，表示未找到指定的会话
}

/*===========================================================================
 * 清理 - 清理超时会话和完全清理会话管理器
 *===========================================================================*/

int magic_session_cleanup_timeout(SessionManager *mgr,
                                  uint32_t timeout_sec) // 清理超时的会话函数
{
  if (!mgr)
    return -1; // 检查会话管理器是否有效，如果无效则返回错误码-1

  time_t now = time(NULL); // 获取当前时间，用于计算超时
  int cleaned = 0;         // 初始化清理计数器为0，用于记录清理的会话数量

  pthread_mutex_lock(&mgr->mutex); // 加锁互斥锁，确保清理过程的线程安全

  for (int i = 0; i < MAX_SESSIONS; i++) { // 遍历所有会话槽位，查找超时的会话
    if (mgr->sessions[i].in_use &&         // 检查该槽位是否正在使用
        (now - mgr->sessions[i].last_activity) >
            timeout_sec) { // 检查最后活动时间是否超过超时阈值

      fd_log_notice(
          "[app_magic] Cleaning up timeout session: %s (idle %ld "
          "sec)",                      // 记录通知日志，表示正在清理超时会话
          mgr->sessions[i].session_id, // 记录会话ID
          (long)(now - mgr->sessions[i].last_activity)); // 记录空闲时间（秒）

      magic_session_release_link(mgr,
                                 &mgr->sessions[i]); // 释放超时会话的链路资源
      memset(&mgr->sessions[i], 0, sizeof(ClientSession)); // 将会话槽位清零
      cleaned++;                                           // 清理计数器加1
    }
  }

  pthread_mutex_unlock(&mgr->mutex); // 解锁互斥锁

  if (cleaned > 0) { // 如果有会话被清理
    fd_log_notice("[app_magic] Cleaned up %d timeout sessions",
                  cleaned); // 记录通知日志，显示清理的会话数量
  }

  return cleaned; // 返回清理的会话数量
}

void magic_session_cleanup(SessionManager *mgr) // 完全清理会话管理器的函数
{
  if (!mgr)
    return; // 检查会话管理器是否有效，如果无效则直接返回

  pthread_mutex_lock(&mgr->mutex); // 加锁互斥锁，确保清理过程的线程安全

  for (int i = 0; i < MAX_SESSIONS; i++) { // 遍历所有会话槽位
    if (mgr->sessions[i].in_use) {         // 检查该槽位是否正在使用
      magic_session_release_link(mgr,
                                 &mgr->sessions[i]); // 释放该会话的链路资源
    }
  }

  memset(mgr->sessions, 0, sizeof(mgr->sessions)); // 将整个会话数组清零
  pthread_mutex_unlock(&mgr->mutex);               // 解锁互斥锁

  pthread_mutex_destroy(&mgr->mutex); // 销毁互斥锁，释放系统资源

  fd_log_notice(
      "[app_magic] Session manager cleaned up"); // 记录通知日志，表示会话管理器已完全清理
}

/*===========================================================================
 * 状态管理 - 设置会话状态和订阅级别
 *===========================================================================*/

const char *magic_session_state_name(SessionState state) {
  switch (state) {
  case SESSION_STATE_INIT:
    return "INIT";
  case SESSION_STATE_AUTHENTICATED:
    return "AUTHENTICATED";
  case SESSION_STATE_ACTIVE:
    return "ACTIVE";
  case SESSION_STATE_MODIFYING:
    return "MODIFYING";
  case SESSION_STATE_SUSPENDED:
    return "SUSPENDED";
  case SESSION_STATE_TERMINATING:
    return "TERMINATING";
  case SESSION_STATE_CLOSED:
    return "CLOSED";
  default:
    return "UNKNOWN";
  }
}

int magic_session_set_state(ClientSession *session, SessionState new_state) {
  if (!session)
    return -1;

  SessionState old_state = session->state;
  session->state = new_state;
  session->last_activity = time(NULL);

  fd_log_notice("[app_magic] Session %s state: %s -> %s", session->session_id,
                magic_session_state_name(old_state),
                magic_session_state_name(new_state));

  return 0;
}

int magic_session_set_subscription(ClientSession *session, uint32_t level) {
  if (!session)
    return -1;

  session->subscribed_status_level = level;
  session->status_subscription_active = (level > 0);

  fd_log_notice("[app_magic] Session %s subscription level: %u",
                session->session_id, level);

  return 0;
}

/*===========================================================================
 * 订阅查询 - 查找所有已订阅状态推送的会话
 *===========================================================================*/

int magic_session_find_subscribed(SessionManager *mgr, ClientSession **sessions,
                                  int max_count) {
  if (!mgr || !sessions || max_count <= 0)
    return 0;

  int count = 0;
  pthread_mutex_lock(&mgr->mutex);

  fd_log_notice("[app_magic] DEBUG: Searching for subscribed sessions...");
  for (int i = 0; i < MAX_SESSIONS && count < max_count; i++) {
    if (mgr->sessions[i].in_use) {
      fd_log_notice("[app_magic] DEBUG:   Session[%d]: in_use=1, "
                    "subscribed=%d, level=%u, state=%d",
                    i, mgr->sessions[i].status_subscription_active,
                    mgr->sessions[i].subscribed_status_level,
                    mgr->sessions[i].state);
    }
    if (mgr->sessions[i].in_use &&
        mgr->sessions[i].status_subscription_active &&
        (mgr->sessions[i].state == SESSION_STATE_AUTHENTICATED ||
         mgr->sessions[i].state == SESSION_STATE_ACTIVE)) {
      sessions[count++] = &mgr->sessions[i];
      fd_log_notice(
          "[app_magic] DEBUG:   ✓ Session[%d] added to broadcast list", i);
    }
  }

  pthread_mutex_unlock(&mgr->mutex);
  return count;
}

ClientSession *magic_session_find_by_client(SessionManager *mgr,
                                            const char *client_id) {
  if (!mgr || !client_id)
    return NULL;

  pthread_mutex_lock(&mgr->mutex);

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (mgr->sessions[i].in_use &&
        strcmp(mgr->sessions[i].client_id, client_id) == 0 &&
        mgr->sessions[i].state != SESSION_STATE_CLOSED &&
        mgr->sessions[i].state != SESSION_STATE_TERMINATING) {
      pthread_mutex_unlock(&mgr->mutex);
      return &mgr->sessions[i];
    }
  }

  pthread_mutex_unlock(&mgr->mutex);
  return NULL;
}

/*===========================================================================
 * 挂起/恢复 - 链路中断时挂起会话，链路恢复时恢复会话
 *===========================================================================*/

/**
 * @brief 挂起会话 (Suspend)。
 * @details 当底层链路 (DLM) 报告断开连接时调用。
 *          暂时释放带宽资源，但保留会话上下文以便快速恢复。
 *          状态流转: ACTIVE -> SUSPENDED。
 *
 * @param mgr 会话管理器。
 * @param session 目标会话。
 * @return 0 成功，-1 失败（会话非活跃）。
 */
int magic_session_suspend(SessionManager *mgr, ClientSession *session) {
  if (!mgr || !session)
    return -1;

  if (session->state != SESSION_STATE_ACTIVE) {
    fd_log_notice("[app_magic] Session %s not active, cannot suspend",
                  session->session_id);
    return -1;
  }

  /* 保存当前资源信息以便恢复 */
  uint32_t saved_bw = session->granted_bw_kbps;
  uint32_t saved_ret_bw = session->granted_ret_bw_kbps;

  /* 暂时回收带宽但保留会话 */
  session->granted_bw_kbps = 0;
  session->granted_ret_bw_kbps = 0;
  session->state = SESSION_STATE_SUSPENDED;
  session->last_activity = time(NULL);

  fd_log_notice("[app_magic] Session %s suspended (was: %u/%u kbps)",
                session->session_id, saved_bw, saved_ret_bw);

  return 0;
}

/**
 * @brief 恢复会话 (Resume)。
 * @details 当链路重新连接时调用。状态流转: SUSPENDED -> ACTIVE。
 *
 * @param mgr 会话管理器。
 * @param session 目标会话。
 * @return 0 成功，-1 失败（会话未挂起）。
 */
int magic_session_resume(SessionManager *mgr, ClientSession *session) {
  if (!mgr || !session)
    return -1;

  if (session->state != SESSION_STATE_SUSPENDED) {
    fd_log_notice("[app_magic] Session %s not suspended, cannot resume",
                  session->session_id);
    return -1;
  }

  session->state = SESSION_STATE_ACTIVE;
  session->last_activity = time(NULL);

  fd_log_notice("[app_magic] Session %s resumed", session->session_id);

  return 0;
}

int magic_session_get_active_sessions(SessionManager *mgr,
                                      ClientSession **sessions, int max_count) {
  if (!mgr || !sessions || max_count <= 0)
    return 0;

  int count = 0;

  pthread_mutex_lock(&mgr->mutex);

  for (int i = 0; i < MAX_SESSIONS && count < max_count; i++) {
    ClientSession *sess = &mgr->sessions[i];
    if (sess->in_use && (sess->state == SESSION_STATE_ACTIVE ||
                         sess->state == SESSION_STATE_AUTHENTICATED)) {
      sessions[count++] = sess;
    }
  }

  pthread_mutex_unlock(&mgr->mutex);

  return count;
}

/*===========================================================================
 * ClientContext API 实现 - 客户端级别的配额管理
 *===========================================================================*/

ClientContext *magic_client_context_get_or_create(SessionManager *mgr,
                                                  const char *client_id) {
  if (!mgr || !client_id)
    return NULL;

  /* 注意: 调用者应该已经持有 mgr->mutex 锁 */

  /* 首先查找是否已存在 */
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (mgr->clients[i].in_use &&
        strcmp(mgr->clients[i].client_id, client_id) == 0) {
      return &mgr->clients[i];
    }
  }

  /* 不存在，创建新的 */
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!mgr->clients[i].in_use) {
      ClientContext *ctx = &mgr->clients[i];
      memset(ctx, 0, sizeof(ClientContext));

      ctx->in_use = true;
      strncpy(ctx->client_id, client_id, sizeof(ctx->client_id) - 1);
      ctx->first_seen = time(NULL);
      ctx->last_activity = time(NULL);
      ctx->max_concurrent_sessions = MAX_SESSIONS_PER_CLIENT; // 默认值

      /* 初始化活跃会话索引数组为-1 (表示无效) */
      for (int j = 0; j < MAX_SESSIONS_PER_CLIENT; j++) {
        ctx->active_session_indices[j] = -1;
      }

      mgr->client_count++;
      fd_log_notice("[app_magic] ClientContext created: %s [total clients: %d]",
                    client_id, mgr->client_count);
      return ctx;
    }
  }

  fd_log_error("[app_magic] No available client context slots");
  return NULL;
}

ClientContext *magic_client_context_find(SessionManager *mgr,
                                         const char *client_id) {
  if (!mgr || !client_id)
    return NULL;

  pthread_mutex_lock(&mgr->mutex);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (mgr->clients[i].in_use &&
        strcmp(mgr->clients[i].client_id, client_id) == 0) {
      pthread_mutex_unlock(&mgr->mutex);
      return &mgr->clients[i];
    }
  }

  pthread_mutex_unlock(&mgr->mutex);
  return NULL;
}

void magic_client_context_set_quota(ClientContext *ctx,
                                    uint32_t max_forward_kbps,
                                    uint32_t max_return_kbps,
                                    uint32_t guaranteed_forward,
                                    uint32_t guaranteed_return) {
  if (!ctx)
    return;

  ctx->max_forward_bw_kbps = max_forward_kbps;
  ctx->max_return_bw_kbps = max_return_kbps;
  ctx->guaranteed_forward_bw_kbps = guaranteed_forward;
  ctx->guaranteed_return_bw_kbps = guaranteed_return;

  fd_log_notice(
      "[app_magic] ClientContext %s quota set: fwd=%u/%u kbps, ret=%u/%u kbps",
      ctx->client_id, ctx->total_allocated_forward_bw, max_forward_kbps,
      ctx->total_allocated_return_bw, max_return_kbps);
}

/**
 * @brief 检查是否可以为客户端分配带宽。
 * @details 这是一个关键的配额检查函数。
 *          算法逻辑：
 *          1. 若配额 (max_quota) 为 0，表示无限制，直接批准。
 *          2. 计算剩余可用带宽 (max - current_allocated)。
 *          3. 确保分配后的剩余带宽不低于保障带宽 (guaranteed)。
 *          4. 最终批准带宽取 (Request, Available) 的较小值。
 *
 * @param ctx 客户端上下文。
 * @param request_forward_kbps 请求下行。
 * @param request_return_kbps 请求上行。
 * @param granted_forward_kbps [out] 批准下行。
 * @param granted_return_kbps [out] 批准上行。
 * @return 0 成功（可分配），-1 失败（超出配额）。
 */
int magic_client_can_allocate_bandwidth(ClientContext *ctx,
                                        uint32_t request_forward_kbps,
                                        uint32_t request_return_kbps,
                                        uint32_t *granted_forward_kbps,
                                        uint32_t *granted_return_kbps) {
  if (!ctx || !granted_forward_kbps || !granted_return_kbps)
    return -1;

  /* 如果配额为0，表示无限制 */
  if (ctx->max_forward_bw_kbps == 0 && ctx->max_return_bw_kbps == 0) {
    *granted_forward_kbps = request_forward_kbps;
    *granted_return_kbps = request_return_kbps;
    return 0;
  }

  /* 计算剩余可用带宽 */
  uint32_t available_forward = 0;
  uint32_t available_return = 0;

  if (ctx->max_forward_bw_kbps > ctx->total_allocated_forward_bw) {
    available_forward =
        ctx->max_forward_bw_kbps - ctx->total_allocated_forward_bw;
  }
  if (ctx->max_return_bw_kbps > ctx->total_allocated_return_bw) {
    available_return = ctx->max_return_bw_kbps - ctx->total_allocated_return_bw;
  }

  fd_log_debug("[app_magic] ClientContext %s bandwidth check: "
               "request=%u/%u, available=%u/%u, allocated=%u/%u, max=%u/%u",
               ctx->client_id, request_forward_kbps, request_return_kbps,
               available_forward, available_return,
               ctx->total_allocated_forward_bw, ctx->total_allocated_return_bw,
               ctx->max_forward_bw_kbps, ctx->max_return_bw_kbps);

  /* 如果剩余带宽低于保证带宽，拒绝请求 */
  if (available_forward < ctx->guaranteed_forward_bw_kbps ||
      available_return < ctx->guaranteed_return_bw_kbps) {
    fd_log_notice("[app_magic] ClientContext %s: insufficient bandwidth for "
                  "guaranteed allocation",
                  ctx->client_id);
    *granted_forward_kbps = 0;
    *granted_return_kbps = 0;
    return -1;
  }

  /* 分配：取请求和可用之间的较小值 */
  *granted_forward_kbps = (request_forward_kbps < available_forward)
                              ? request_forward_kbps
                              : available_forward;
  *granted_return_kbps = (request_return_kbps < available_return)
                             ? request_return_kbps
                             : available_return;

  return 0;
}

void magic_client_update_allocated_bandwidth(ClientContext *ctx,
                                             int32_t delta_forward,
                                             int32_t delta_return) {
  if (!ctx)
    return;

  /* 安全地更新带宽计数，防止下溢 */
  if (delta_forward >= 0) {
    ctx->total_allocated_forward_bw += (uint32_t)delta_forward;
  } else {
    uint32_t dec = (uint32_t)(-delta_forward);
    ctx->total_allocated_forward_bw =
        (ctx->total_allocated_forward_bw >= dec)
            ? (ctx->total_allocated_forward_bw - dec)
            : 0;
  }

  if (delta_return >= 0) {
    ctx->total_allocated_return_bw += (uint32_t)delta_return;
  } else {
    uint32_t dec = (uint32_t)(-delta_return);
    ctx->total_allocated_return_bw =
        (ctx->total_allocated_return_bw >= dec)
            ? (ctx->total_allocated_return_bw - dec)
            : 0;
  }

  ctx->last_activity = time(NULL);

  fd_log_debug("[app_magic] ClientContext %s allocated bandwidth updated: "
               "fwd=%u, ret=%u",
               ctx->client_id, ctx->total_allocated_forward_bw,
               ctx->total_allocated_return_bw);
}

bool magic_client_can_create_session(ClientContext *ctx) {
  if (!ctx)
    return false;

  if (ctx->max_concurrent_sessions == 0) {
    return true; // 无限制
  }

  bool can_create =
      (ctx->active_session_count < (int)ctx->max_concurrent_sessions);

  if (!can_create) {
    fd_log_notice(
        "[app_magic] ClientContext %s: max concurrent sessions reached (%d/%u)",
        ctx->client_id, ctx->active_session_count,
        ctx->max_concurrent_sessions);
  }

  return can_create;
}

int magic_client_add_session(ClientContext *ctx, int session_index) {
  if (!ctx || session_index < 0 || session_index >= MAX_SESSIONS)
    return -1;

  /* 检查是否已达上限 */
  if (!magic_client_can_create_session(ctx)) {
    return -1;
  }

  /* 检查是否已经存在 */
  for (int i = 0; i < ctx->active_session_count; i++) {
    if (ctx->active_session_indices[i] == session_index) {
      return 0; // 已存在，不重复添加
    }
  }

  /* 添加到列表 */
  if (ctx->active_session_count < MAX_SESSIONS_PER_CLIENT) {
    ctx->active_session_indices[ctx->active_session_count] = session_index;
    ctx->active_session_count++;
    ctx->total_sessions_created++;
    ctx->last_activity = time(NULL);

    fd_log_debug(
        "[app_magic] ClientContext %s: session added (index=%d, count=%d)",
        ctx->client_id, session_index, ctx->active_session_count);
    return 0;
  }

  return -1;
}

int magic_client_remove_session(ClientContext *ctx, int session_index) {
  if (!ctx || session_index < 0)
    return -1;

  for (int i = 0; i < ctx->active_session_count; i++) {
    if (ctx->active_session_indices[i] == session_index) {
      /* 移除：将后面的元素前移 */
      for (int j = i; j < ctx->active_session_count - 1; j++) {
        ctx->active_session_indices[j] = ctx->active_session_indices[j + 1];
      }
      ctx->active_session_indices[ctx->active_session_count - 1] = -1;
      ctx->active_session_count--;
      ctx->last_activity = time(NULL);

      fd_log_debug(
          "[app_magic] ClientContext %s: session removed (index=%d, count=%d)",
          ctx->client_id, session_index, ctx->active_session_count);
      return 0;
    }
  }

  return -1; // 未找到
}

int magic_client_get_sessions(SessionManager *mgr, ClientContext *ctx,
                              ClientSession **sessions, int max_count) {
  if (!mgr || !ctx || !sessions || max_count <= 0)
    return 0;

  int count = 0;

  pthread_mutex_lock(&mgr->mutex);

  for (int i = 0; i < ctx->active_session_count && count < max_count; i++) {
    int idx = ctx->active_session_indices[i];
    if (idx >= 0 && idx < MAX_SESSIONS && mgr->sessions[idx].in_use) {
      sessions[count++] = &mgr->sessions[idx];
    }
  }

  pthread_mutex_unlock(&mgr->mutex);

  return count;
}

/*===========================================================================
 * TFT 规则管理 API 实现
 *===========================================================================*/

int magic_session_add_tft(ClientSession *session, const SessionTftRule *tft) {
  if (!session || !tft)
    return -2;

  if (session->num_tft_rules >= MAX_TFT_PER_SESSION) {
    fd_log_error("[app_magic] Session %s: TFT array full", session->session_id);
    return -1;
  }

  /* 复制 TFT 规则 */
  session->tft_rules[session->num_tft_rules] = *tft;
  session->tft_rules[session->num_tft_rules].in_use = true;
  session->num_tft_rules++;

  fd_log_debug("[app_magic] Session %s: TFT added (proto=%u, src=%s:%u-%u, "
               "dst=%s:%u-%u)",
               session->session_id, tft->protocol, tft->src_ip,
               tft->src_port_start, tft->src_port_end, tft->dst_ip,
               tft->dst_port_start, tft->dst_port_end);

  return 0;
}

void magic_session_clear_tfts(ClientSession *session) {
  if (!session)
    return;

  uint32_t old_count = session->num_tft_rules;
  memset(session->tft_rules, 0, sizeof(session->tft_rules));
  session->num_tft_rules = 0;

  fd_log_debug("[app_magic] Session %s: %u TFT rules cleared",
               session->session_id, old_count);
}

SessionTftRule *magic_session_find_tft(ClientSession *session,
                                       const char *src_ip, const char *dst_ip,
                                       uint8_t protocol, uint16_t src_port,
                                       uint16_t dst_port) {
  if (!session)
    return NULL;

  for (uint32_t i = 0; i < session->num_tft_rules; i++) {
    SessionTftRule *tft = &session->tft_rules[i];
    if (!tft->in_use)
      continue;

    /* 协议匹配 (0=任意) */
    if (tft->protocol != 0 && tft->protocol != protocol)
      continue;

    /* 源 IP 匹配 (空=任意) */
    if (src_ip && tft->src_ip[0] != '\0' && strcmp(tft->src_ip, src_ip) != 0)
      continue;

    /* 目的 IP 匹配 (空=任意) */
    if (dst_ip && tft->dst_ip[0] != '\0' && strcmp(tft->dst_ip, dst_ip) != 0)
      continue;

    /* 源端口范围匹配 (0=任意) */
    if (tft->src_port_start != 0 || tft->src_port_end != 0) {
      if (src_port < tft->src_port_start || src_port > tft->src_port_end)
        continue;
    }

    /* 目的端口范围匹配 (0=任意) */
    if (tft->dst_port_start != 0 || tft->dst_port_end != 0) {
      if (dst_port < tft->dst_port_start || dst_port > tft->dst_port_end)
        continue;
    }

    return tft; // 匹配成功
  }

  return NULL; // 未找到匹配
}
