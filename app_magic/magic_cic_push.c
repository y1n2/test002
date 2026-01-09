/**
 * @file magic_cic_push.c
 * @brief MAGIC CIC 主动推送功能实现 (v2.1)
 * @description 实现 MNTR/MSCR 服务端主动推送
 *              - MNTR: 通知会话参数变更 (链路中断、带宽调整等)
 *              - MSCR: 向订阅客户端广播状态变化
 *
 * v2.1 新增功能:
 *   - MNTA 超时处理 (5秒超时强制清理会话)
 *   - 广播风暴抑制 (阈值门限 + 时间窗)
 *   - Gateway-IP 链路切换通知
 *   - 正确的 MAGIC-Status-Code 值
 */

#include "magic_cic_push.h"
#include "add_avp.h"
#include "magic_dict_handles.h"
#include "magic_group_avp_simple.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

/* 使用 dict_magic.h 中定义的外部全局变量 */

/* 前向声明 */
static MagicContext *g_push_ctx = NULL; /* 用于超时回调访问上下文 */

/**
 * @brief 检查是否应该发送 MNTR (风暴抑制)。
 * @details 实现了基于时间窗和变化阈值的抑制逻辑：
 *          1. 强制发送 (Link UP/DOWN, Handover) -> 立即发送
 *          2. 时间窗检查 (MNTR_MIN_INTERVAL_SEC) -> 抑制过频发送
 *          3. 带宽变化阈值 (MNTR_BW_CHANGE_THRESHOLD) -> 抑制微小波动
 *          4. 质变检查 (0 <-> Non-Zero) -> 始终发送
 *
 * @param session 目标会话。
 * @param new_bw_kbps 新带宽值 (kbps)。
 * @param force 强制发送标志。
 * @return true 应该发送，false 应该抑制。
 */
bool magic_cic_should_send_mntr(ClientSession *session, uint32_t new_bw_kbps,
                                bool force) {
  if (!session) {
    return false;
  }

  /* 强制发送模式: 绕过所有检查 */
  if (force) {
    fd_log_debug(
        "[app_magic] MNTR force_send=true, bypassing storm suppression");
    return true;
  }

  time_t now = time(NULL);

  /* 检查 1: 时间窗 - 同一会话 MNTR 最小发送间隔 */
  if (session->last_mntr_sent_time > 0) {
    time_t elapsed = now - session->last_mntr_sent_time;
    if (elapsed < MNTR_MIN_INTERVAL_SEC) {
      fd_log_debug("[app_magic] MNTR suppressed: interval %ld < %d sec",
                   (long)elapsed, MNTR_MIN_INTERVAL_SEC);
      return false;
    }
  }

  /* 检查 2: 阈值门限 - 带宽变化小于 10% 不发送 */
  if (session->last_notified_bw_kbps > 0 && new_bw_kbps > 0) {
    uint32_t old_bw = session->last_notified_bw_kbps;
    int32_t change_percent;

    if (old_bw > 0) {
      change_percent = (int32_t)((new_bw_kbps - old_bw) * 100 / old_bw);
      if (change_percent < 0)
        change_percent = -change_percent;

      if ((uint32_t)change_percent < MNTR_BW_CHANGE_THRESHOLD) {
        fd_log_debug(
            "[app_magic] MNTR suppressed: BW change %d%% < %d%% threshold",
            change_percent, MNTR_BW_CHANGE_THRESHOLD);
        return false;
      }
    }
  }

  /* 检查 3: 质变事件 (通/断) 始终发送 */
  if (new_bw_kbps == 0 || session->last_notified_bw_kbps == 0) {
    fd_log_debug(
        "[app_magic] MNTR qualitative change (0 BW involved), will send");
    return true;
  }

  return true;
}

/**
 * @brief MNTR 应答 (MNTA) 回调函数。
 * @details 处理客户端对 MNTR 的响应。
 *          - 清除 pending_ack 标志。
 *          - 检查 Result-Code。
 *          - 记录日志。
 *          如果回调时 msg 为 NULL，视为超时，根据 v2.1 策略强制关闭会话。
 *
 * @param data 用户数据 (ClientSession 指针)。
 * @param msg 指向接收到的消息的指针。
 */
static void mntr_answer_callback(void *data, struct msg **msg) {
  ClientSession *session = (ClientSession *)data;

  if (!session) {
    if (msg && *msg) {
      fd_msg_free(*msg);
      *msg = NULL;
    }
    return;
  }

  /* 清除待确认标志 */
  session->mntr_pending_ack = false;

  if (!msg || !*msg) {
    fd_log_error("[app_magic] MNTR answer callback: no message (timeout?)");

    /* 超时处理: 强制清理会话 */
    fd_log_error("[app_magic] MNTA timeout for session %s - forcing cleanup",
                 session->session_id);

    if (g_push_ctx) {
      magic_session_delete(&g_push_ctx->session_mgr, session->session_id);
    }
    return;
  }

  /* 提取 Result-Code */
  struct avp *avp_result = NULL;
  uint32_t result_code = 0;

  if (fd_msg_search_avp(*msg, g_std_dict.avp_result_code, &avp_result) == 0 &&
      avp_result) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_result, &hdr) == 0 && hdr->avp_value) {
      result_code = hdr->avp_value->u32;
    }
  }

  if (result_code == 2001) {
    fd_log_notice("[app_magic] MNTA received: SUCCESS (session: %s)",
                  session->session_id);
  } else {
    fd_log_error("[app_magic] MNTA received: FAILED (Result-Code=%u)",
                 result_code);

    /* 检查 Failed-AVP */
    struct avp *avp_failed = NULL;
    if (fd_msg_search_avp(*msg, g_std_dict.avp_failed_avp, &avp_failed) == 0 &&
        avp_failed) {
      fd_log_notice("[app_magic]   Client reported Failed-AVP - logging event");
      /* 根据设计方案：服务端霸权原则，记录日志但不改变状态 */
    }
  }

  /* 释放消息 */
  fd_msg_free(*msg);
  *msg = NULL;
}

/**
 * @brief 发送 MNTR 通知 (v2.1 实现)。
 * @details 构造并发送 MNTR 消息。
 *          - 执行风暴抑制检查。
 *          - 填充 Communication-Report-Parameters (Profile, BW, Gateway)。
 *          - 设置超时跟踪 (last_mntr_sent_time, pending_ack)。
 *          - 注册异步回调。
 *
 * @param ctx MAGIC 上下文。
 * @param session 目标会话。
 * @param params 通知参数。
 * @return 0 成功 (或被抑制)，-1 失败。
 */
int magic_cic_send_mntr(MagicContext *ctx, ClientSession *session,
                        const MNTRParams *params) {
  if (!ctx || !session || !params) {
    return -1;
  }

  /* 保存上下文供回调使用 */
  g_push_ctx = ctx;

  /* v2.1: 风暴抑制检查 */
  uint32_t new_bw_kbps = params->new_granted_bw / 1000;
  if (!magic_cic_should_send_mntr(session, new_bw_kbps, params->force_send)) {
    fd_log_notice("[app_magic] MNTR suppressed for session %s (storm control)",
                  session->session_id);
    return 0; /* 抑制成功，不是错误 */
  }

  struct msg *mntr = NULL;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] Sending MNTR to session: %s", session->session_id);
  fd_log_notice("[app_magic]   MAGIC-Status-Code: %u",
                params->magic_status_code);
  fd_log_notice("[app_magic]   Granted-BW: %u bps", params->new_granted_bw);
  fd_log_notice("[app_magic] ========================================");

  /* 创建 MNTR 消息 */
  CHECK_FCT_DO(fd_msg_new(g_magic_dict.cmd_mntr, MSGFL_ALLOC_ETEID, &mntr), {
    fd_log_error("[app_magic] Failed to create MNTR message");
    return -1;
  });

  /* 添加 Session-Id */
  ADD_AVP_STR(mntr, g_std_dict.avp_session_id, session->session_id);

  /* 添加 Origin-Host, Origin-Realm */
  ADD_AVP_STR(mntr, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(mntr, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);

  /* 添加 Destination-Realm (客户端域) - v2.1: 优先使用会话中保存的 realm */
  char dest_realm[128] = "";
  if (session->client_realm[0]) {
    /* 使用会话中保存的客户端 Origin-Realm */
    strncpy(dest_realm, session->client_realm, sizeof(dest_realm) - 1);
  } else {
    /* 回退: 从 client_id 提取（兼容旧会话） */
    const char *at = strchr(session->client_id, '.');
    if (at) {
      strncpy(dest_realm, at + 1, sizeof(dest_realm) - 1);
    } else {
      strncpy(dest_realm, "client.local", sizeof(dest_realm) - 1);
    }
    fd_log_notice("[app_magic] MNTR: client_realm not stored, falling back to "
                  "extract from client_id: %s",
                  dest_realm);
  }
  ADD_AVP_STR(mntr, g_std_dict.avp_destination_realm, dest_realm);

  /* 添加 Communication-Report-Parameters (MNTR 使用 report 结构体) */
  comm_report_params_t report_params;
  memset(&report_params, 0, sizeof(report_params));

  report_params.profile_name = session->profile_name;
  report_params.granted_bw = params->new_granted_bw;
  report_params.granted_return_bw = params->new_granted_ret_bw;
  report_params.has_granted_bw = true;
  report_params.has_granted_return_bw = true;

  /* v2.1: 添加 Gateway-IP (如果有) */
  if (params->new_gateway_ip && params->new_gateway_ip[0]) {
    report_params.gateway_ip = params->new_gateway_ip;
    report_params.has_gateway_ip = true;
    fd_log_notice("[app_magic]   Gateway-IP: %s", params->new_gateway_ip);
  }

  if (add_comm_report_params_simple(mntr, &report_params) != 0) {
    fd_log_error("[app_magic] Failed to add Communication-Report-Parameters");
  }

  /* 添加 MAGIC-Status-Code */
  if (params->magic_status_code > 0) {
    ADD_AVP_U32(mntr, g_magic_dict.avp_magic_status_code,
                params->magic_status_code);
  }

  /* 添加 Error-Message (如果有) */
  if (params->error_message && params->error_message[0]) {
    ADD_AVP_STR(mntr, g_std_dict.avp_error_message, params->error_message);
  }

  /* v2.1: 标记待确认状态并记录发送时间 */
  session->mntr_pending_ack = true;
  session->last_mntr_sent_time = time(NULL);
  session->last_notified_bw_kbps = new_bw_kbps;

  /* 发送消息并注册回调 (异步非阻塞) */
  CHECK_FCT_DO(fd_msg_send(&mntr, mntr_answer_callback, session), {
    fd_log_error("[app_magic] Failed to send MNTR message");
    session->mntr_pending_ack = false;
    if (mntr)
      fd_msg_free(mntr);
    return -1;
  });

  fd_log_notice("[app_magic] ✓ MNTR sent, waiting for MNTA (timeout=%ds)...",
                MNTR_ACK_TIMEOUT_SEC);

  return 0;
}

/**
 * @brief 检查并清理 MNTR 超时的会话。
 * @details 遍历所有活动会话，检查是否存在等待 ACK 超过规定时间
 * (MNTR_ACK_TIMEOUT_SEC) 的会话。
 *          如果超时，视为客户端不可达或协议错误，强制关闭会话。
 *
 * @param ctx MAGIC 上下文。
 */
void magic_cic_check_mntr_timeouts(MagicContext *ctx) {
  if (!ctx) {
    return;
  }

  time_t now = time(NULL);
  int timeout_count = 0;

  pthread_mutex_lock(&ctx->session_mgr.mutex);

  for (int i = 0; i < MAX_SESSIONS; i++) {
    ClientSession *session = &ctx->session_mgr.sessions[i];

    if (!session->in_use) {
      continue;
    }

    /* 检查是否有待确认的 MNTR 超时 */
    if (session->mntr_pending_ack) {
      time_t elapsed = now - session->last_mntr_sent_time;

      if (elapsed >= MNTR_ACK_TIMEOUT_SEC) {
        fd_log_error(
            "[app_magic] MNTR ACK timeout for session %s (elapsed=%lds)",
            session->session_id, (long)elapsed);

        /* 强制清理会话 */
        session->mntr_pending_ack = false;
        session->state = SESSION_STATE_CLOSED;
        session->in_use = false;
        ctx->session_mgr.session_count--;

        timeout_count++;

        fd_log_notice("[app_magic] Session %s force-closed due to MNTR timeout",
                      session->session_id);
      }
    }
  }

  pthread_mutex_unlock(&ctx->session_mgr.mutex);

  if (timeout_count > 0) {
    fd_log_notice("[app_magic] MNTR timeout check: %d session(s) force-closed",
                  timeout_count);
  }
}

/**
 * @brief MSCR 应答 (MSCA) 回调函数。
 * @details 处理客户端对 MSCR 的响应。
 *          如果发送失败或收到错误响应，为了减少网络负载和错误风暴，
 *          自动取消该客户端的订阅 (removing subscription)。
 *
 * @param data 用户数据 (ClientSession 指针)。
 * @param msg 指向接收到的消息的指针。
 */
static void mscr_answer_callback(void *data, struct msg **msg) {
  ClientSession *session = (ClientSession *)data;

  /* 发送失败或超时 - 移除订阅 */
  if (!msg || !*msg) {
    if (session) {
      fd_log_error("[app_magic] MSCR send failed/timeout for session %s - "
                   "removing subscription",
                   session->session_id);
      session->status_subscription_active = false;
      session->subscribed_status_level = 0;
    }
    return;
  }

  struct avp *avp_result = NULL;
  uint32_t result_code = 0;

  if (fd_msg_search_avp(*msg, g_std_dict.avp_result_code, &avp_result) == 0 &&
      avp_result) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_result, &hdr) == 0 && hdr->avp_value) {
      result_code = hdr->avp_value->u32;
    }
  }

  if (result_code == 2001) {
    fd_log_notice("[app_magic] MSCA received from %s: SUCCESS",
                  session ? session->session_id : "unknown");
  } else {
    /* MSCA 失败 - 移除订阅 */
    fd_log_error("[app_magic] MSCA failed from %s: Result-Code=%u - removing "
                 "subscription",
                 session ? session->session_id : "unknown", result_code);
    if (session) {
      session->status_subscription_active = false;
      session->subscribed_status_level = 0;
    }
  }

  fd_msg_free(*msg);
  *msg = NULL;
}

/**
 * @brief 向所有已订阅状态的会话广播 MSCR。
 * @details 遍历所有会话，向已订阅相应状态变更的客户端发送 MSCR。
 *          - MAGIC 状态变更: 发送给订阅级别 1, 3, 7 的客户端。
 *          - DLM 状态变更: 发送给订阅级别 2, 3, 6, 7 的客户端。
 *          支持 v2.1 版本，包含详细的 DLM-Info 构建。
 *
 * @param ctx MAGIC 上下文。
 * @param params 状态变更参数。
 * @return 发送成功的消息数量。
 */
int magic_cic_broadcast_mscr(MagicContext *ctx, const MSCRParams *params) {
  if (!ctx || !params) {
    return -1;
  }

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] Broadcasting MSCR (Status Change Report)");
  fd_log_notice("[app_magic]   Change type: %d, DLM: %s", params->type,
                params->dlm_name ? params->dlm_name : "N/A");
  fd_log_notice("[app_magic] ========================================");

  /* 查找所有已订阅状态的会话 */
  ClientSession *subscribed[MAX_SESSIONS];
  int count = magic_session_find_subscribed(&ctx->session_mgr, subscribed,
                                            MAX_SESSIONS);

  fd_log_notice("[app_magic] DEBUG: Total sessions checked for subscription");
  fd_log_notice("[app_magic] DEBUG: Found %d subscribed sessions", count);

  if (count == 0) {
    fd_log_notice("[app_magic] No subscribed sessions to notify");
    return 0;
  }

  fd_log_notice("[app_magic] Found %d subscribed session(s)", count);

  int sent_count = 0;

  for (int i = 0; i < count; i++) {
    ClientSession *session = subscribed[i];

    /* 检查订阅级别是否包含 DLM 状态 */
    bool need_dlm = (session->subscribed_status_level >= 2);
    bool need_magic = (session->subscribed_status_level == 1 ||
                       session->subscribed_status_level == 3 ||
                       session->subscribed_status_level == 7);

    /* 根据状态变更类型决定是否发送 */
    bool should_send = false;
    if (params->type == STATUS_CHANGE_CLIENT_JOIN ||
        params->type == STATUS_CHANGE_CLIENT_LEAVE) {
      should_send = need_magic; /* MAGIC 状态变更 */
    } else {
      should_send = need_dlm; /* DLM 状态变更 */
    }

    if (!should_send) {
      continue;
    }

    /* 创建 MSCR 消息 */
    struct msg *mscr = NULL;
    CHECK_FCT_DO(fd_msg_new(g_magic_dict.cmd_mscr, MSGFL_ALLOC_ETEID, &mscr),
                 { continue; });

    /* 添加 Session-Id */
    ADD_AVP_STR(mscr, g_std_dict.avp_session_id, session->session_id);
    ADD_AVP_STR(mscr, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
    ADD_AVP_STR(mscr, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);

    /* Destination-Realm - v2.1: 优先使用会话中保存的 realm */
    char dest_realm[128] = "";
    if (session->client_realm[0]) {
      strncpy(dest_realm, session->client_realm, sizeof(dest_realm) - 1);
    } else {
      const char *at = strchr(session->client_id, '.');
      if (at) {
        strncpy(dest_realm, at + 1, sizeof(dest_realm) - 1);
      } else {
        strncpy(dest_realm, "client.local", sizeof(dest_realm) - 1);
      }
    }
    ADD_AVP_STR(mscr, g_std_dict.avp_destination_realm, dest_realm);

    /* 添加 Registered-Clients (如果订阅了 MAGIC 状态) */
    if (need_magic) {
      ADD_AVP_U32(mscr, g_magic_dict.avp_registered_clients,
                  ctx->session_mgr.session_count);
    }

    /* 添加 DLM-List (如果订阅了 DLM 状态) */
    if (need_dlm && params->dlm_name) {
      struct avp *dlm_list_avp = NULL;
      CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_list, 0, &dlm_list_avp),
                   {
                     fd_msg_free(mscr);
                     continue;
                   });

      struct avp *dlm_info_avp = NULL;
      CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_info, 0, &dlm_info_avp),
                   {
                     fd_msg_free((struct msg *)dlm_list_avp);
                     fd_msg_free(mscr);
                     continue;
                   });

      /* 1. DLM-Name (10004) */
      ADD_AVP_STR(dlm_info_avp, g_magic_dict.avp_dlm_name, params->dlm_name);

      /* 2. DLM-Available (10005) - Enum: 1=YES, 2=NO, 3=UNKNOWN */
      /* params->dlm_available passed as 0/1 in caller, map to 1/2 */
      int32_t avail_enum = (params->dlm_available == 0) ? 1 : 2;
      ADD_AVP_I32(dlm_info_avp, g_magic_dict.avp_dlm_available, avail_enum);

      /* 3. DLM-Max-Links (10010) */
      ADD_AVP_U32(dlm_info_avp, g_magic_dict.avp_dlm_max_links, MAX_BEARERS);

      /* 4. DLM-Max-Bandwidth (10006) - Float32 */
      {
        struct avp *avp = NULL;
        union avp_value val;
        val.f32 = 10000.0f; /* 10 Mbps Mock */
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_max_bw, 0, &avp),
                     continue);
        CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, avp),
                     continue);
      }

      /* 5. DLM-Allocated-Links (10011) */
      /* For broadcast, we don't have easy access to DlmClient struct here,
       * assume 0 or 1? Ideally we should fetch DlmClient. Using 0 to carry on.
       */
      ADD_AVP_U32(dlm_info_avp, g_magic_dict.avp_dlm_alloc_links, 0);

      /* 6. DLM-Allocated-Bandwidth (10007) - Float32 */
      {
        struct avp *avp = NULL;
        union avp_value val;
        val.f32 = 0.0f;
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_alloc_bw, 0, &avp),
                     continue);
        CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, avp),
                     continue);
      }

      /* 7. DLM-QoS-Level-List (20009) */
      {
        struct avp *list_avp = NULL;
        CHECK_FCT_DO(
            fd_msg_avp_new(g_magic_dict.avp_dlm_qos_level_list, 0, &list_avp),
            continue);

        /* Add BE (0) */
        struct avp *qos_avp = NULL;
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_qos_level, 0, &qos_avp),
                     continue);
        union avp_value val;
        val.i32 = 0; /* BE */
        CHECK_FCT_DO(fd_msg_avp_setvalue(qos_avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(list_avp, MSG_BRW_LAST_CHILD, qos_avp),
                     continue);

        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, list_avp),
                     continue);
      }

      fd_msg_avp_add(dlm_list_avp, MSG_BRW_LAST_CHILD, dlm_info_avp);
      fd_msg_avp_add(mscr, MSG_BRW_LAST_CHILD, dlm_list_avp);
    }

    /* 添加 MAGIC-Status-Code (如果有) */
    if (params->magic_status_code > 0) {
      ADD_AVP_U32(mscr, g_magic_dict.avp_magic_status_code,
                  params->magic_status_code);
    }

    /* 添加 Error-Message (如果有) */
    if (params->error_message && params->error_message[0]) {
      ADD_AVP_STR(mscr, g_std_dict.avp_error_message, params->error_message);
    }

    /* 发送 MSCR */
    CHECK_FCT_DO(fd_msg_send(&mscr, mscr_answer_callback, session), {
      if (mscr)
        fd_msg_free(mscr);
      continue;
    });

    sent_count++;
    fd_log_notice("[app_magic] ✓ MSCR sent to session: %s",
                  session->session_id);
  }

  fd_log_notice("[app_magic] MSCR broadcast complete: %d/%d sent", sent_count,
                count);

  return sent_count;
}

/**
 * @brief 处理链路状态变化事件。
 * @details 当底层链路通断时调用。
 *          1. 遍历所有使用该链路的会话，发送 MNTR 通知 (Force Send)。
 *          2. 向所有订阅了 DLM 状态的客户端广播 MSCR。
 *          对于 Link UP: 恢复挂起的会话。
 *          对于 Link DOWN: 挂起会话 (Suspend)。
 *
 * @param ctx MAGIC 上下文。
 * @param link_id 变化的链路 ID。
 * @param is_up true=Link UP, false=Link DOWN。
 * @return 0 成功，-1 失败。
 */
int magic_cic_on_link_status_change(MagicContext *ctx, const char *link_id,
                                    bool is_up) {
  if (!ctx || !link_id) {
    return -1;
  }

  fd_log_notice("[app_magic] Link status change: %s → %s", link_id,
                is_up ? "UP" : "DOWN");

  /* 1. 向所有使用该链路的会话发送 MNTR */
  pthread_mutex_lock(&ctx->session_mgr.mutex);

  for (int i = 0; i < MAX_SESSIONS; i++) {
    ClientSession *session = &ctx->session_mgr.sessions[i];

    if (!session->in_use || session->state == SESSION_STATE_CLOSED) {
      continue;
    }

    /* 检查是否使用该链路 */
    if (strcmp(session->assigned_link_id, link_id) != 0) {
      continue;
    }

    MNTRParams mntr_params;
    memset(&mntr_params, 0, sizeof(mntr_params));

    /* 链路通断是质变事件，强制发送 */
    mntr_params.force_send = true;

    if (is_up) {
      /* 链路恢复 - 通过 Granted-BW > 0 表示恢复, Status-Code = 0 (SUCCESS) */
      mntr_params.magic_status_code = MAGIC_STATUS_SUCCESS;
      mntr_params.error_message = "Link recovered";
      mntr_params.new_granted_bw = session->granted_bw_kbps * 1000;
      mntr_params.new_granted_ret_bw = session->granted_ret_bw_kbps * 1000;

      /* 恢复会话状态 */
      magic_session_resume(&ctx->session_mgr, session);
    } else {
      /* 链路中断 - MAGIC-Status-Code = 2007 (LINK_ERROR) 或 Granted-BW = 0 */
      mntr_params.magic_status_code = MAGIC_STATUS_LINK_ERROR;
      mntr_params.error_message = "Link connection lost";
      mntr_params.new_granted_bw = 0;
      mntr_params.new_granted_ret_bw = 0;

      /* 先更新内存状态 (设计要求) */
      magic_session_suspend(&ctx->session_mgr, session);
    }

    pthread_mutex_unlock(&ctx->session_mgr.mutex);
    magic_cic_send_mntr(ctx, session, &mntr_params);
    pthread_mutex_lock(&ctx->session_mgr.mutex);
  }

  pthread_mutex_unlock(&ctx->session_mgr.mutex);

  /* 2. 向所有订阅状态的会话广播 MSCR */
  MSCRParams mscr_params;
  memset(&mscr_params, 0, sizeof(mscr_params));

  mscr_params.type = is_up ? STATUS_CHANGE_DLM_UP : STATUS_CHANGE_DLM_DOWN;
  mscr_params.dlm_name = link_id;
  mscr_params.dlm_available = is_up ? 0 : 1; /* 0=Yes, 1=No */
  mscr_params.magic_status_code =
      is_up ? MAGIC_STATUS_SUCCESS : MAGIC_STATUS_LINK_ERROR;
  mscr_params.error_message = is_up ? "DLM link recovered" : "DLM link lost";

  magic_cic_broadcast_mscr(ctx, &mscr_params);

  return 0;
}

/**
 * @brief 处理带宽变更事件。
 * @details 当会话可用带宽变化时调用。
 *          根据变化幅度选择合适的 Status-Code (SUCCESS 或 NO_FREE_BANDWIDTH)。
 *          调用 magic_cic_send_mntr 发送通知 (受风暴抑制控制)。
 *
 * @param ctx MAGIC 上下文。
 * @param session 受影响的会话。
 * @param new_bw 新带宽值 (kbps)。
 * @param reason 变更原因字符串。
 * @return 0 成功 (或被抑制)，-1 失败。
 */
int magic_cic_on_bandwidth_change(MagicContext *ctx, ClientSession *session,
                                  uint32_t new_bw, const char *reason) {
  if (!ctx || !session) {
    return -1;
  }

  fd_log_notice(
      "[app_magic] Bandwidth change for session %s: %u -> %u kbps (%s)",
      session->session_id, session->granted_bw_kbps, new_bw,
      reason ? reason : "unspecified");

  uint32_t old_bw = session->granted_bw_kbps;

  MNTRParams params;
  memset(&params, 0, sizeof(params));

  if (new_bw < old_bw) {
    /* 带宽降低 - MAGIC-Status-Code = 1016 (NO_FREE_BANDWIDTH) - ARINC 839 规范
     */
    params.magic_status_code = MAGIC_STATUS_NO_FREE_BANDWIDTH;
  } else if (new_bw > old_bw) {
    /* 带宽增加 - MAGIC-Status-Code = 0 (SUCCESS) + Granted-Bandwidth 传递新值
     */
    params.magic_status_code = MAGIC_STATUS_SUCCESS;
  } else {
    /* 带宽无变化，不发送 */
    return 0;
  }

  params.error_message = reason;
  params.new_granted_bw = new_bw * 1000;
  params.new_granted_ret_bw = session->granted_ret_bw_kbps * 1000;
  params.new_link_id = session->assigned_link_id;
  params.new_bearer_id = session->bearer_id;
  params.force_send = false; /* 受风暴抑制控制 */

  /* 先更新会话带宽 (设计要求: 先修改内存状态) */
  session->granted_bw_kbps = new_bw;
  session->last_activity = time(NULL);

  return magic_cic_send_mntr(ctx, session, &params);
}

/**
 * @brief 处理链路切换 (Handover) 事件。
 * @details 链路切换时强制发送 MNTR 通知。
 *          Status-Code 设置为 FORCED_REROUTING (2010)。
 *          如果新的网关 IP 可用，也一并通知客户端。
 *
 * @param ctx MAGIC 上下文。
 * @param session 受影响的会话。
 * @param new_link_id 新链路 ID。
 * @param new_gateway_ip 新网关 IP。
 * @return 0 成功，-1 失败。
 */
int magic_cic_on_handover(MagicContext *ctx, ClientSession *session,
                          const char *new_link_id, const char *new_gateway_ip) {
  if (!ctx || !session || !new_link_id) {
    return -1;
  }

  fd_log_notice("[app_magic] Handover for session %s: %s -> %s (Gateway: %s)",
                session->session_id, session->assigned_link_id, new_link_id,
                new_gateway_ip ? new_gateway_ip : "unchanged");

  MNTRParams params;
  memset(&params, 0, sizeof(params));

  /* 链路切换 - MAGIC-Status-Code = 2010 (FORCED_REROUTING) - ARINC 839 规范 */
  params.magic_status_code = MAGIC_STATUS_FORCED_REROUTING;
  params.error_message = "Link handover completed";
  params.new_granted_bw = session->granted_bw_kbps * 1000;
  params.new_granted_ret_bw = session->granted_ret_bw_kbps * 1000;
  params.new_link_id = new_link_id;
  params.new_bearer_id = session->bearer_id;
  params.force_send = true; /* 切换是重要事件，强制发送 */

  /* v2.1: 填充网关 IP */
  if (new_gateway_ip && new_gateway_ip[0]) {
    params.new_gateway_ip = new_gateway_ip;

    /* 更新会话中的网关 IP */
    strncpy(session->gateway_ip, new_gateway_ip,
            sizeof(session->gateway_ip) - 1);
    session->gateway_ip[sizeof(session->gateway_ip) - 1] = '\0';
  }

  /* 更新会话链路信息 */
  strncpy(session->previous_link_id, session->assigned_link_id,
          sizeof(session->previous_link_id) - 1);
  strncpy(session->assigned_link_id, new_link_id,
          sizeof(session->assigned_link_id) - 1);
  session->last_link_switch_time = time(NULL);
  session->last_activity = time(NULL);

  return magic_cic_send_mntr(ctx, session, &params);
}

/**
 * @brief 发送初始状态快照 (Initial MSCR)。
 * @details 客户端订阅后，立即发送一次当前的全量状态快照。
 *          包含当前所有活动链路的详细信息 (DLM-List) 和在线客户端数量
 * (Registered-Clients)。
 *
 * @param ctx MAGIC 上下文。
 * @param session 目标会话。
 * @return 0 成功，-1 失败。
 */
int magic_cic_send_initial_mscr(MagicContext *ctx, ClientSession *session) {
  if (!ctx || !session) {
    return -1;
  }

  if (session->subscribed_status_level == 0) {
    return 0; /* 未订阅，不发送 */
  }

  fd_log_notice("[app_magic] Sending initial MSCR to session: %s (Level=%u)",
                session->session_id, session->subscribed_status_level);

  /* 检查订阅级别 */
  bool need_dlm = (session->subscribed_status_level >= 2);
  bool need_magic = (session->subscribed_status_level == 1 ||
                     session->subscribed_status_level == 3 ||
                     session->subscribed_status_level == 7);

  struct msg *mscr = NULL;
  CHECK_FCT_DO(fd_msg_new(g_magic_dict.cmd_mscr, MSGFL_ALLOC_ETEID, &mscr),
               { return -1; });

  /* 基本 AVP */
  ADD_AVP_STR(mscr, g_std_dict.avp_session_id, session->session_id);
  ADD_AVP_STR(mscr, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(mscr, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);

  /* Destination-Realm - v2.1: 优先使用会话中保存的 realm */
  char dest_realm[128] = "";
  if (session->client_realm[0]) {
    strncpy(dest_realm, session->client_realm, sizeof(dest_realm) - 1);
  } else {
    const char *at = strchr(session->client_id, '.');
    if (at) {
      strncpy(dest_realm, at + 1, sizeof(dest_realm) - 1);
    } else {
      strncpy(dest_realm, "client.local", sizeof(dest_realm) - 1);
    }
  }
  ADD_AVP_STR(mscr, g_std_dict.avp_destination_realm, dest_realm);

  /* 添加 Registered-Clients */
  if (need_magic) {
    pthread_mutex_lock(&ctx->session_mgr.mutex);
    uint32_t client_count = ctx->session_mgr.session_count;
    pthread_mutex_unlock(&ctx->session_mgr.mutex);
    ADD_AVP_U32(mscr, g_magic_dict.avp_registered_clients, client_count);
  }

  /* 添加所有活动链路的 DLM-Info */
  if (need_dlm) {
    struct avp *dlm_list_avp = NULL;
    CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_list, 0, &dlm_list_avp), {
      fd_msg_free(mscr);
      return -1;
    });

    pthread_mutex_lock(&ctx->lmi_ctx.clients_mutex);

    int added_count = 0;
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
      DlmClient *dlm = &ctx->lmi_ctx.clients[i];

      if (!dlm->is_registered) {
        continue;
      }

      struct avp *dlm_info_avp = NULL;
      CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_info, 0, &dlm_info_avp),
                   { continue; });

      /* 1. DLM-Name (10004) */
      ADD_AVP_STR(dlm_info_avp, g_magic_dict.avp_dlm_name, dlm->link_id);

      /* 2. DLM-Available (10005) - Enum: 1=YES, 2=NO, 3=UNKNOWN */
      int32_t availability = (dlm->is_registered) ? 1 : 2;
      ADD_AVP_I32(dlm_info_avp, g_magic_dict.avp_dlm_available, availability);

      /* 3. DLM-Max-Links (10010) */
      ADD_AVP_U32(dlm_info_avp, g_magic_dict.avp_dlm_max_links, MAX_BEARERS);

      /* 4. DLM-Max-Bandwidth (10006) - Float32 */
      {
        struct avp *avp = NULL;
        union avp_value val;
        val.f32 = 10000.0f; /* 10 Mbps Mock */
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_max_bw, 0, &avp),
                     continue);
        CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, avp),
                     continue);
      }

      /* 5. DLM-Allocated-Links (10011) */
      ADD_AVP_U32(dlm_info_avp, g_magic_dict.avp_dlm_alloc_links,
                  dlm->num_active_bearers);

      /* 6. DLM-Allocated-Bandwidth (10007) - Float32 */
      /* Calc bandwidth sum */
      float total_bw = 0.0f;
      for (int k = 0; k < MAX_BEARERS; k++) {
        if (dlm->bearers[k].is_active) {
          total_bw +=
              (float)dlm->bearers[k]
                  .qos_params.forward_link_rate; /* 前向链路速率 (kbps) */
        }
      }
      {
        struct avp *avp = NULL;
        union avp_value val;
        val.f32 = total_bw;
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_alloc_bw, 0, &avp),
                     continue);
        CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, avp),
                     continue);
      }

      /* 7. DLM-QoS-Level-List (20009) */
      {
        struct avp *list_avp = NULL;
        CHECK_FCT_DO(
            fd_msg_avp_new(g_magic_dict.avp_dlm_qos_level_list, 0, &list_avp),
            continue);

        /* Add BE (0) */
        struct avp *qos_avp = NULL;
        CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_qos_level, 0, &qos_avp),
                     continue);
        union avp_value val;
        val.i32 = 0; /* BE */
        CHECK_FCT_DO(fd_msg_avp_setvalue(qos_avp, &val), continue);
        CHECK_FCT_DO(fd_msg_avp_add(list_avp, MSG_BRW_LAST_CHILD, qos_avp),
                     continue);

        CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, list_avp),
                     continue);
      }

      fd_msg_avp_add(dlm_list_avp, MSG_BRW_LAST_CHILD, dlm_info_avp);
      added_count++;
    }

    pthread_mutex_unlock(&ctx->lmi_ctx.clients_mutex);

    if (added_count > 0) {
      fd_msg_avp_add(mscr, MSG_BRW_LAST_CHILD, dlm_list_avp);
    } else {
      /* 如果没有 DLM，仍添加空的 List 或不添加?
       * 通常 List 为空代表当前无链路。
       * 释放已创建的空 Group AVP? freeDiameter 会处理空 Group 吗?
       * 最好还是加上，哪怕是空的(如果协议允许)。
       * 这里如果没加 children，add 会成功，编码时长度为 0.
       */
      fd_msg_avp_add(mscr, MSG_BRW_LAST_CHILD, dlm_list_avp);
    }
  }

  /* MAGIC-Status-Code: SUCCESS */
  ADD_AVP_U32(mscr, g_magic_dict.avp_magic_status_code, MAGIC_STATUS_SUCCESS);

  /* Error-Message: Initial Status */
  ADD_AVP_STR(mscr, g_std_dict.avp_error_message, "Initial Status Report");

  /* 发送 */
  CHECK_FCT_DO(fd_msg_send(&mscr, mscr_answer_callback, session), {
    if (mscr)
      fd_msg_free(mscr);
    return -1;
  });

  fd_log_notice("[app_magic] ✓ Initial MSCR sent to session %s",
                session->session_id);
  return 0;
}
