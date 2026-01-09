/**
 * @file magic_group_avp_simple.c
 * @brief 简化版 Grouped AVP 构造助手实现。
 * @details 实现了直接通过参数传递（而非全局配置）来构造 MAGIC 复杂 Grouped AVP
 * 的逻辑。
 */

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <string.h>

#include "add_avp.h"
#include "magic_dict_handles.h"
#include "magic_group_avp_simple.h"

/* 外部全局字典句柄 */
extern struct std_diam_dict_handles g_std_dict;
extern struct magic_dict_handles g_magic_dict;

/**
 * @brief 添加 Client-Credentials Grouped AVP (Code 20019) 的简化版本。
 * @details 构造包含用户名和双向密码校验信息的
 * AVP。该版本直接由参数指定内容，增强了灵活性。
 *
 * @param[in,out] msg             目标消息。
 * @param[in]     username        用户名。
 * @param[in]     client_password 客户端密码。
 * @param[in]     server_password 服务器密码 (可选，可为 NULL)。
 *
 * @return int 成功返回 0，参数无效返回 -1。
 */
int add_client_credentials_simple(struct msg *msg, const char *username,
                                  const char *client_password,
                                  const char *server_password) {
  if (!msg || !username || !client_password) {
    fd_log_error("[app_magic] add_client_credentials_simple: 必填参数为空");
    return -1;
  }

  if (username[0] == '\0' || client_password[0] == '\0') {
    fd_log_error("[app_magic] add_client_credentials_simple: 用户名或密码为空");
    return -1;
  }

  ADD_GROUPED(msg, g_magic_dict.avp_client_credentials, {
    /* User-Name (标准 AVP) */
    S_STD_STR(g_std_dict.avp_user_name, username);

    /* Client-Password (MAGIC AVP) */
    S_STR(g_magic_dict.avp_client_password, client_password);

    /* Server-Password (可选) */
    if (server_password && server_password[0] != '\0') {
      S_STR(g_magic_dict.avp_server_password, server_password);
    }
  });

  fd_log_debug("[app_magic] Client-Credentials 添加成功: username=%s",
               username);
  return 0;
}

/*
 * 添加 Communication-Request-Parameters Grouped AVP (Code 20001)
 */
int add_comm_req_params_simple(struct msg *msg,
                               const comm_req_params_t *params) {
  if (!msg || !params || !params->profile_name ||
      params->profile_name[0] == '\0') {
    fd_log_error(
        "[app_magic] add_comm_req_params_simple: 参数无效或 profile_name 为空");
    return -1;
  }

  ADD_GROUPED(msg, g_magic_dict.avp_comm_req_params, {
    /* 必填：Profile-Name */
    S_STR(g_magic_dict.avp_profile_name, params->profile_name);

    /* 可选字段 */
    if (params->requested_bw > 0) {
      S_U64(g_magic_dict.avp_requested_bw, params->requested_bw);
    }

    if (params->requested_return_bw > 0) {
      S_U64(g_magic_dict.avp_requested_return_bw, params->requested_return_bw);
    }

    if (params->priority_class > 0) {
      S_U32(g_magic_dict.avp_priority_class, params->priority_class);
    }

    if (params->qos_level > 0) {
      S_U32(g_magic_dict.avp_qos_level, params->qos_level);
    }

    if (params->dlm_name && params->dlm_name[0] != '\0') {
      S_STR(g_magic_dict.avp_dlm_name, params->dlm_name);
    }

    if (params->flight_phase > 0) {
      S_U32(g_magic_dict.avp_flight_phase, params->flight_phase);
    }

    if (params->altitude > 0) {
      S_U32(g_magic_dict.avp_altitude, params->altitude);
    }
  });

  fd_log_debug(
      "[app_magic] Communication-Request-Parameters 添加成功: profile=%s",
      params->profile_name);
  return 0;
}

/*
 * 添加 Communication-Answer-Parameters Grouped AVP (Code 20002)
 *
 * 根据 ARINC 839 协议 Appendix B-1.2，MCAA/MCCA 中的应答参数包含：
 * - Profile-Name, Granted-Bandwidth, Granted-Return-Bandwidth
 * - Priority-Type, Priority-Class, QoS-Level
 * - TFTtoGround-List, TFTtoAircraft-List
 * - Accounting-Enabled, DLM-Availability-List
 * - Keep-Request, Auto-Detect, Timeout
 * - Flight-Phase, Altitude, Airport (可选)
 * - NAPT-List, Gateway-IPAddress (可选)
 */
int add_comm_ans_params_simple(struct msg *msg,
                               const comm_ans_params_t *params) {
  if (!msg || !params || !params->selected_link_id ||
      params->selected_link_id[0] == '\0') {
    fd_log_error("[app_magic] add_comm_ans_params_simple: 参数无效或 "
                 "selected_link_id 为空");
    return -1;
  }

  ADD_GROUPED(msg, g_magic_dict.avp_comm_ans_params, {
    /* Profile-Name (必需) */
    if (params->profile_name && params->profile_name[0] != '\0') {
      S_STR(g_magic_dict.avp_profile_name, params->profile_name);
    } else {
      S_STR(g_magic_dict.avp_profile_name, "default");
    }

    /* Granted-Bandwidth (必需) - Float32 类型 (kbps) */
    {
      float granted_bw_kbps =
          (float)(params->granted_bw / 1000.0); /* bit/s -> kbps */
      S_FLOAT(g_magic_dict.avp_granted_bw, granted_bw_kbps);
    }

    /* Granted-Return-Bandwidth (必需) - Float32 类型 (kbps) */
    {
      float granted_ret_bw_kbps = (float)(params->granted_return_bw / 1000.0);
      S_FLOAT(g_magic_dict.avp_granted_return_bw, granted_ret_bw_kbps);
    }

    /* Priority-Type (必需) - Enumerated */
    S_U32(g_magic_dict.avp_priority_type,
          params->priority_type > 0 ? params->priority_type : 2);

    /* Priority-Class (必需) - UTF8String */
    if (params->priority_class && params->priority_class[0] != '\0') {
      S_STR(g_magic_dict.avp_priority_class, params->priority_class);
    } else {
      S_STR(g_magic_dict.avp_priority_class, "5");
    }

    /* QoS-Level (必需) - Enumerated (0=BE, 1=AF, 2=EF) */
    S_U32(g_magic_dict.avp_qos_level, params->qos_level);

    /* Accounting-Enabled (必需) */
    S_U32(g_magic_dict.avp_accounting_enabled, params->accounting_enabled);

    /* DLM-Availability-List (必需) */
    if (params->dlm_availability_list &&
        params->dlm_availability_list[0] != '\0') {
      S_STR(g_magic_dict.avp_dlm_availability_list,
            params->dlm_availability_list);
    } else {
      S_STR(g_magic_dict.avp_dlm_availability_list, params->selected_link_id);
    }

    /* Keep-Request (必需) */
    S_U32(g_magic_dict.avp_keep_request, params->keep_request);

    /* Auto-Detect (必需) */
    S_U32(g_magic_dict.avp_auto_detect, params->auto_detect);

    /* Timeout (必需) */
    S_U32(g_magic_dict.avp_timeout,
          params->session_timeout > 0 ? params->session_timeout : 300);

    /* 可选位置限制字段 */
    if (params->flight_phase && params->flight_phase[0] != '\0') {
      S_STR(g_magic_dict.avp_flight_phase, params->flight_phase);
    }

    if (params->altitude && params->altitude[0] != '\0') {
      S_STR(g_magic_dict.avp_altitude, params->altitude);
    }

    if (params->airport && params->airport[0] != '\0') {
      S_STR(g_magic_dict.avp_airport, params->airport);
    }

    /* Gateway-IPAddress (可选) */
    if (params->assigned_ip && params->assigned_ip[0] != '\0') {
      S_STR(g_magic_dict.avp_gateway_ip, params->assigned_ip);
    }

    /* DLM-Name / Link-Number for selected link */
    S_STR(g_magic_dict.avp_dlm_name, params->selected_link_id);
    if (params->bearer_id > 0) {
      S_U32(g_magic_dict.avp_link_number, params->bearer_id);
    }
  });

  fd_log_debug("[app_magic] Communication-Answer-Parameters 添加成功: link=%s, "
               "bw=%.2f/%.2f kbps",
               params->selected_link_id, (float)(params->granted_bw / 1000.0),
               (float)(params->granted_return_bw / 1000.0));
  return 0;
}

/*
 * 添加 Communication-Report-Parameters Grouped AVP (Code 20003)
 * 用于 MNTR 消息通知客户端参数变更
 */
int add_comm_report_params_simple(struct msg *msg,
                                  const comm_report_params_t *params) {
  if (!msg || !params || !params->profile_name ||
      params->profile_name[0] == '\0') {
    fd_log_error("[app_magic] add_comm_report_params_simple: 参数无效或 "
                 "profile_name 为空");
    return -1;
  }

  ADD_GROUPED(msg, g_magic_dict.avp_comm_report_params, {
    /* Profile-Name (必需) */
    S_STR(g_magic_dict.avp_profile_name, params->profile_name);

    /* 仅添加已变更的可选字段 */
    if (params->has_granted_bw) {
      float bw_kbps = (float)(params->granted_bw / 1000.0);
      S_FLOAT(g_magic_dict.avp_granted_bw, bw_kbps);
    }

    if (params->has_granted_return_bw) {
      float ret_bw_kbps = (float)(params->granted_return_bw / 1000.0);
      S_FLOAT(g_magic_dict.avp_granted_return_bw, ret_bw_kbps);
    }

    if (params->has_priority_type) {
      S_U32(g_magic_dict.avp_priority_type, params->priority_type);
    }

    if (params->has_priority_class && params->priority_class &&
        params->priority_class[0] != '\0') {
      S_STR(g_magic_dict.avp_priority_class, params->priority_class);
    }

    if (params->has_qos_level) {
      S_U32(g_magic_dict.avp_qos_level, params->qos_level);
    }

    if (params->has_dlm_availability_list && params->dlm_availability_list &&
        params->dlm_availability_list[0] != '\0') {
      S_STR(g_magic_dict.avp_dlm_availability_list,
            params->dlm_availability_list);
    }

    if (params->has_gateway_ip && params->gateway_ip &&
        params->gateway_ip[0] != '\0') {
      S_STR(g_magic_dict.avp_gateway_ip, params->gateway_ip);
    }
  });

  fd_log_debug(
      "[app_magic] Communication-Report-Parameters 添加成功: profile=%s",
      params->profile_name);
  return 0;
}

/*
 * 添加 DLM-Info Grouped AVP (Code 20008) 到 DLM-List
 */
int add_dlm_info_simple(struct avp *dlm_list_avp, const dlm_info_t *dlm) {
  if (!dlm_list_avp || !dlm || !dlm->dlm_name || dlm->dlm_name[0] == '\0') {
    fd_log_error("[app_magic] add_dlm_info_simple: 参数无效");
    return -1;
  }

  struct avp *dlm_info_avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_info, 0, &dlm_info_avp),
               return -1);

  /* DLM-Name (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_str(dlm_info_avp, g_magic_dict.avp_dlm_name,
                                  dlm->dlm_name),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Available (必需) - Enumerated (0=YES, 1=NO, 2=UNKNOWN) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(dlm_info_avp, g_magic_dict.avp_dlm_available,
                                  dlm->dlm_available),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Max-Links (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(dlm_info_avp, g_magic_dict.avp_dlm_max_links,
                                  dlm->dlm_max_links),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Max-Bandwidth (必需) - Float32 */
  CHECK_FCT_DO(fd_msg_avp_add_float(dlm_info_avp, g_magic_dict.avp_dlm_max_bw,
                                    dlm->dlm_max_bw),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Max-Return-Bandwidth (可选) */
  if (dlm->dlm_max_return_bw > 0) {
    CHECK_FCT_DO(fd_msg_avp_add_float(dlm_info_avp,
                                      g_magic_dict.avp_dlm_max_return_bw,
                                      dlm->dlm_max_return_bw),
                 {
                   fd_msg_free((struct msg *)dlm_info_avp);
                   return -1;
                 });
  }

  /* DLM-Allocated-Links (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(dlm_info_avp,
                                  g_magic_dict.avp_dlm_alloc_links,
                                  dlm->dlm_alloc_links),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Allocated-Bandwidth (必需) - Float32 */
  CHECK_FCT_DO(fd_msg_avp_add_float(dlm_info_avp, g_magic_dict.avp_dlm_alloc_bw,
                                    dlm->dlm_alloc_bw),
               {
                 fd_msg_free((struct msg *)dlm_info_avp);
                 return -1;
               });

  /* DLM-Allocated-Return-Bandwidth (可选) */
  if (dlm->dlm_alloc_return_bw > 0) {
    CHECK_FCT_DO(fd_msg_avp_add_float(dlm_info_avp,
                                      g_magic_dict.avp_dlm_alloc_return_bw,
                                      dlm->dlm_alloc_return_bw),
                 {
                   fd_msg_free((struct msg *)dlm_info_avp);
                   return -1;
                 });
  }

  /* DLM-QoS-Level-List (必需) - 包含 1-3 个 QoS-Level */
  if (dlm->qos_level_count > 0) {
    struct avp *qos_list_avp = NULL;
    CHECK_FCT_DO(
        fd_msg_avp_new(g_magic_dict.avp_dlm_qos_level_list, 0, &qos_list_avp), {
          fd_msg_free((struct msg *)dlm_info_avp);
          return -1;
        });

    for (uint32_t i = 0; i < dlm->qos_level_count && i < 3; i++) {
      CHECK_FCT_DO(fd_msg_avp_add_u32(qos_list_avp, g_magic_dict.avp_qos_level,
                                      dlm->qos_levels[i]),
                   {
                     fd_msg_free((struct msg *)qos_list_avp);
                     continue;
                   });
    }

    CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, qos_list_avp),
                 { fd_msg_free((struct msg *)qos_list_avp); });
  }

  /* 添加 DLM-Info 到 DLM-List */
  CHECK_FCT_DO(fd_msg_avp_add(dlm_list_avp, MSG_BRW_LAST_CHILD, dlm_info_avp), {
    fd_msg_free((struct msg *)dlm_info_avp);
    return -1;
  });

  return 0;
}

/*
 * 添加 Link-Status-Group Grouped AVP (Code 20011) 到 DLM-Link-Status-List
 */
int add_link_status_simple(struct avp *dlm_link_status_list_avp,
                           const link_status_t *link) {
  if (!dlm_link_status_list_avp || !link || !link->link_name ||
      link->link_name[0] == '\0') {
    fd_log_error("[app_magic] add_link_status_simple: 参数无效");
    return -1;
  }

  struct avp *link_status_avp = NULL;
  CHECK_FCT_DO(
      fd_msg_avp_new(g_magic_dict.avp_link_status_group, 0, &link_status_avp),
      return -1);

  /* Link-Name (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_str(link_status_avp, g_magic_dict.avp_link_name,
                                  link->link_name),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Number (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(link_status_avp, g_magic_dict.avp_link_number,
                                  link->link_number),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Available (必需) - Enumerated (1=YES, 2=NO) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(link_status_avp,
                                  g_magic_dict.avp_link_available,
                                  link->link_available),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* QoS-Level (必需) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(link_status_avp, g_magic_dict.avp_qos_level,
                                  link->qos_level),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Connection-Status (必需) - Enumerated (0=Disconnected, 1=Connected,
   * 2=Forced_Close) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(link_status_avp,
                                  g_magic_dict.avp_link_conn_status,
                                  link->link_conn_status),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Login-Status (必需) - Enumerated (1=Logged off, 2=Logged on) */
  CHECK_FCT_DO(fd_msg_avp_add_u32(link_status_avp,
                                  g_magic_dict.avp_link_login_status,
                                  link->link_login_status),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Max-Bandwidth (必需) - Float32 */
  CHECK_FCT_DO(fd_msg_avp_add_float(link_status_avp,
                                    g_magic_dict.avp_link_max_bw,
                                    link->link_max_bw),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Max-Return-Bandwidth (可选) */
  if (link->link_max_return_bw > 0) {
    CHECK_FCT_DO(fd_msg_avp_add_float(link_status_avp,
                                      g_magic_dict.avp_link_max_return_bw,
                                      link->link_max_return_bw),
                 {
                   fd_msg_free((struct msg *)link_status_avp);
                   return -1;
                 });
  }

  /* Link-Alloc-Bandwidth (必需) - Float32 */
  CHECK_FCT_DO(fd_msg_avp_add_float(link_status_avp,
                                    g_magic_dict.avp_link_alloc_bw,
                                    link->link_alloc_bw),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  /* Link-Alloc-Return-Bandwidth (可选) */
  if (link->link_alloc_return_bw > 0) {
    CHECK_FCT_DO(fd_msg_avp_add_float(link_status_avp,
                                      g_magic_dict.avp_link_alloc_return_bw,
                                      link->link_alloc_return_bw),
                 {
                   fd_msg_free((struct msg *)link_status_avp);
                   return -1;
                 });
  }

  /* Link-Error-String (可选) */
  if (link->link_error_string && link->link_error_string[0] != '\0') {
    CHECK_FCT_DO(fd_msg_avp_add_str(link_status_avp,
                                    g_magic_dict.avp_link_error_string,
                                    link->link_error_string),
                 {
                   fd_msg_free((struct msg *)link_status_avp);
                   return -1;
                 });
  }

  /* 添加 Link-Status-Group 到 DLM-Link-Status-List */
  CHECK_FCT_DO(fd_msg_avp_add(dlm_link_status_list_avp, MSG_BRW_LAST_CHILD,
                              link_status_avp),
               {
                 fd_msg_free((struct msg *)link_status_avp);
                 return -1;
               });

  return 0;
}

/*===========================================================================
 * 额外的 Grouped AVP 辅助函数实现
 * 基于现有 AVP 定义实现的补充功能
 *===========================================================================*/

/**
 * @brief 添加 TFT-to-Ground-List Grouped AVP (Code 20004)
 * @description 添加地面侧流量过滤模板列表
 * @param msg 目标消息
 * @param tft_rules TFT 规则字符串数组
 * @param rule_count 规则数量
 * @return 0=成功, -1=失败
 */
int add_tft_to_ground_list_simple(struct msg *msg, const char **tft_rules,
                                  uint32_t rule_count) {
  if (!msg || !tft_rules || rule_count == 0) {
    fd_log_error("[app_magic] add_tft_to_ground_list_simple: 参数无效");
    return -1;
  }

  struct avp *tft_list_avp = NULL;
  CHECK_FCT_DO(
      fd_msg_avp_new(g_magic_dict.avp_tft_to_ground_list, 0, &tft_list_avp),
      return -1);

  /* 添加每条 TFT 规则 */
  for (uint32_t i = 0; i < rule_count; i++) {
    if (tft_rules[i] && tft_rules[i][0] != '\0') {
      CHECK_FCT_DO(fd_msg_avp_add_str(tft_list_avp,
                                      g_magic_dict.avp_tft_to_ground_rule,
                                      tft_rules[i]),
                   {
                     fd_msg_free((struct msg *)tft_list_avp);
                     return -1;
                   });
    }
  }

  /* 添加 TFT-to-Ground-List 到消息 */
  CHECK_FCT_DO(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, tft_list_avp), {
    fd_msg_free((struct msg *)tft_list_avp);
    return -1;
  });

  fd_log_debug("[app_magic] TFT-to-Ground-List 添加成功: %u 条规则",
               rule_count);
  return 0;
}

/**
 * @brief 添加 TFT-to-Aircraft-List Grouped AVP (Code 20005)
 * @description 添加飞机侧流量过滤模板列表
 */
int add_tft_to_aircraft_list_simple(struct msg *msg, const char *tft_rules[],
                                    uint32_t rule_count) {
  if (!msg || !tft_rules || rule_count == 0) {
    fd_log_error("[app_magic] add_tft_to_aircraft_list_simple: 参数无效");
    return -1;
  }

  struct avp *tft_list_avp = NULL;
  CHECK_FCT_DO(
      fd_msg_avp_new(g_magic_dict.avp_tft_to_aircraft_list, 0, &tft_list_avp),
      return -1);

  /* 添加每条 TFT 规则 */
  for (uint32_t i = 0; i < rule_count; i++) {
    if (tft_rules[i] && tft_rules[i][0] != '\0') {
      CHECK_FCT_DO(fd_msg_avp_add_str(tft_list_avp,
                                      g_magic_dict.avp_tft_to_aircraft_rule,
                                      tft_rules[i]),
                   {
                     fd_msg_free((struct msg *)tft_list_avp);
                     return -1;
                   });
    }
  }

  /* 添加 TFT-to-Aircraft-List 到消息 */
  CHECK_FCT_DO(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, tft_list_avp), {
    fd_msg_free((struct msg *)tft_list_avp);
    return -1;
  });

  fd_log_debug("[app_magic] TFT-to-Aircraft-List 添加成功: %u 条规则",
               rule_count);
  return 0;
}

/**
 * @brief 添加 NAPT-List Grouped AVP (Code 20006)
 * @description 添加网络地址端口转换规则列表
 * @param msg 目标消息
 * @param napt_rules NAPT 规则字符串数组
 * @param rule_count 规则数量
 * @return 0=成功, -1=失败
 */
int add_napt_list_simple(struct msg *msg, const char **napt_rules,
                         uint32_t rule_count) {
  if (!msg || !napt_rules || rule_count == 0) {
    fd_log_error("[app_magic] add_napt_list_simple: 参数无效");
    return -1;
  }

  struct avp *napt_list_avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_napt_list, 0, &napt_list_avp),
               return -1);

  /* 添加每条 NAPT 规则 */
  for (uint32_t i = 0; i < rule_count; i++) {
    if (napt_rules[i] && napt_rules[i][0] != '\0') {
      CHECK_FCT_DO(fd_msg_avp_add_str(napt_list_avp, g_magic_dict.avp_napt_rule,
                                      napt_rules[i]),
                   {
                     fd_msg_free((struct msg *)napt_list_avp);
                     return -1;
                   });
    }
  }

  /* 添加 NAPT-List 到消息 */
  CHECK_FCT_DO(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, napt_list_avp), {
    fd_msg_free((struct msg *)napt_list_avp);
    return -1;
  });

  fd_log_debug("[app_magic] NAPT-List 添加成功: %u 条规则", rule_count);
  return 0;
}

/**
 * @brief 添加 DLM-QoS-Level-List Grouped AVP (Code 20009)
 * @description 添加 DLM 支持的 QoS 等级列表
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param qos_levels QoS 等级数组
 * @param level_count 等级数量 (1-3)
 * @return 0=成功, -1=失败
 */
int add_dlm_qos_level_list_simple(struct avp *dlm_info_avp,
                                  const uint32_t *qos_levels,
                                  uint32_t level_count) {
  if (!dlm_info_avp || !qos_levels || level_count == 0 || level_count > 3) {
    fd_log_error("[app_magic] add_dlm_qos_level_list_simple: 参数无效");
    return -1;
  }

  struct avp *qos_list_avp = NULL;
  CHECK_FCT_DO(
      fd_msg_avp_new(g_magic_dict.avp_dlm_qos_level_list, 0, &qos_list_avp),
      return -1);

  /* 添加每个 QoS 等级 */
  for (uint32_t i = 0; i < level_count; i++) {
    CHECK_FCT_DO(fd_msg_avp_add_u32(qos_list_avp, g_magic_dict.avp_qos_level,
                                    qos_levels[i]),
                 {
                   fd_msg_free((struct msg *)qos_list_avp);
                   return -1;
                 });
  }

  /* 添加到 DLM-Info */
  CHECK_FCT_DO(fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, qos_list_avp), {
    fd_msg_free((struct msg *)qos_list_avp);
    return -1;
  });

  fd_log_debug("[app_magic] DLM-QoS-Level-List 添加成功: %u 个等级",
               level_count);
  return 0;
}

/**
 * @brief 添加 DLM-List Grouped AVP (Code 20007)
 * @description 创建并返回 DLM-List AVP，供后续添加 DLM-Info
 * @param msg 目标消息
 * @param dlm_list_avp_out 输出参数：创建的 DLM-List AVP 指针
 * @return 0=成功, -1=失败
 */
int create_dlm_list_simple(struct msg *msg, struct avp **dlm_list_avp_out) {
  if (!msg || !dlm_list_avp_out) {
    fd_log_error("[app_magic] create_dlm_list_simple: 参数无效");
    return -1;
  }

  struct avp *dlm_list_avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_list, 0, &dlm_list_avp),
               return -1);

  *dlm_list_avp_out = dlm_list_avp;

  fd_log_debug("[app_magic] DLM-List AVP 创建成功");
  return 0;
}

/**
 * @brief 完成 DLM-List 并添加到消息
 * @param msg 目标消息
 * @param dlm_list_avp DLM-List AVP 指针
 * @return 0=成功, -1=失败
 */
int finalize_dlm_list_simple(struct msg *msg, struct avp *dlm_list_avp) {
  if (!msg || !dlm_list_avp) {
    fd_log_error("[app_magic] finalize_dlm_list_simple: 参数无效");
    return -1;
  }

  CHECK_FCT_DO(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, dlm_list_avp), {
    fd_msg_free((struct msg *)dlm_list_avp);
    return -1;
  });

  fd_log_debug("[app_magic] DLM-List 添加到消息成功");
  return 0;
}

/**
 * @brief 创建 DLM-Link-Status-List Grouped AVP (Code 20010)
 * @description 创建链路状态列表容器，供后续添加 Link-Status-Group
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param link_status_list_out 输出参数：创建的链路状态列表 AVP 指针
 * @return 0=成功, -1=失败
 */
int create_dlm_link_status_list_simple(struct avp *dlm_info_avp,
                                       struct avp **link_status_list_out) {
  if (!dlm_info_avp || !link_status_list_out) {
    fd_log_error("[app_magic] create_dlm_link_status_list_simple: 参数无效");
    return -1;
  }

  struct avp *link_status_list_avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_link_status_list, 0,
                              &link_status_list_avp),
               return -1);

  *link_status_list_out = link_status_list_avp;

  fd_log_debug("[app_magic] DLM-Link-Status-List AVP 创建成功");
  return 0;
}

/**
 * @brief 完成 DLM-Link-Status-List 并添加到 DLM-Info
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param link_status_list_avp 链路状态列表 AVP 指针
 * @return 0=成功, -1=失败
 */
int finalize_dlm_link_status_list_simple(struct avp *dlm_info_avp,
                                         struct avp *link_status_list_avp) {
  if (!dlm_info_avp || !link_status_list_avp) {
    fd_log_error("[app_magic] finalize_dlm_link_status_list_simple: 参数无效");
    return -1;
  }

  CHECK_FCT_DO(
      fd_msg_avp_add(dlm_info_avp, MSG_BRW_LAST_CHILD, link_status_list_avp), {
        fd_msg_free((struct msg *)link_status_list_avp);
        return -1;
      });

  fd_log_debug("[app_magic] DLM-Link-Status-List 添加到 DLM-Info 成功");
  return 0;
}

/**
 * @brief 添加完整的 QoS 参数到消息（基于现有 AVP）
 * @description 使用现有的 Priority-Type, Priority-Class, QoS-Level 构建 QoS
 * 参数集
 * @param msg 目标消息或 Grouped AVP
 * @param priority_type 优先级类型 (1=Blocking, 2=Preemption)
 * @param priority_class 优先级类别字符串
 * @param qos_level QoS 等级 (0=BE, 1=AF, 2=EF)
 * @return 0=成功, -1=失败
 */
int add_qos_params_inline(struct msg *msg, uint32_t priority_type,
                          const char *priority_class, uint32_t qos_level) {
  if (!msg || !priority_class) {
    fd_log_error("[app_magic] add_qos_params_inline: 参数无效");
    return -1;
  }

  /* Priority-Type (Enumerated) */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg,
                                  g_magic_dict.avp_priority_type,
                                  priority_type),
               return -1);

  /* Priority-Class (UTF8String) */
  CHECK_FCT_DO(fd_msg_avp_add_str((struct msg *)msg,
                                  g_magic_dict.avp_priority_class,
                                  priority_class),
               return -1);

  /* QoS-Level (Enumerated) */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg, g_magic_dict.avp_qos_level,
                                  qos_level),
               return -1);

  fd_log_debug("[app_magic] QoS 参数添加成功: type=%u, class=%s, level=%u",
               priority_type, priority_class, qos_level);
  return 0;
}

/**
 * @brief 添加链路特征信息（基于现有 Link-Status-Group AVP）
 * @description 使用 Link-Status-Group 的现有 AVP 来描述链路特征
 * @param msg 目标消息
 * @param link_name 链路名称
 * @param link_number 链路编号
 * @param max_bw 最大带宽 (kbps)
 * @param qos_level QoS 等级
 * @return 0=成功, -1=失败
 */
int add_link_characteristics_inline(struct msg *msg, const char *link_name,
                                    uint32_t link_number, float max_bw,
                                    uint32_t qos_level) {
  if (!msg || !link_name) {
    fd_log_error("[app_magic] add_link_characteristics_inline: 参数无效");
    return -1;
  }

  /* Link-Name */
  CHECK_FCT_DO(fd_msg_avp_add_str((struct msg *)msg, g_magic_dict.avp_link_name,
                                  link_name),
               return -1);

  /* Link-Number */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg,
                                  g_magic_dict.avp_link_number, link_number),
               return -1);

  /* Link-Max-Bandwidth */
  CHECK_FCT_DO(fd_msg_avp_add_float((struct msg *)msg,
                                    g_magic_dict.avp_link_max_bw, max_bw),
               return -1);

  /* QoS-Level */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg, g_magic_dict.avp_qos_level,
                                  qos_level),
               return -1);

  fd_log_debug("[app_magic] 链路特征添加成功: %s (No.%u, %.2f kbps, QoS=%u)",
               link_name, link_number, max_bw, qos_level);
  return 0;
}

/**
 * @brief 添加 Bearer 信息（基于现有 AVP）
 * @description 使用 Link-Number 作为 Bearer ID
 * @param msg 目标消息或 Grouped AVP
 * @param bearer_id Bearer 标识符 (Link-Number)
 * @param link_conn_status 链路连接状态
 * @param link_login_status 链路登录状态
 * @return 0=成功, -1=失败
 */
int add_bearer_info_inline(struct msg *msg, uint32_t bearer_id,
                           uint32_t link_conn_status,
                           uint32_t link_login_status) {
  if (!msg) {
    fd_log_error("[app_magic] add_bearer_info_inline: 参数无效");
    return -1;
  }

  /* Bearer ID (使用 Link-Number) */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg,
                                  g_magic_dict.avp_link_number, bearer_id),
               return -1);

  /* Link-Connection-Status */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg,
                                  g_magic_dict.avp_link_conn_status,
                                  link_conn_status),
               return -1);

  /* Link-Login-Status */
  CHECK_FCT_DO(fd_msg_avp_add_u32((struct msg *)msg,
                                  g_magic_dict.avp_link_login_status,
                                  link_login_status),
               return -1);

  fd_log_debug("[app_magic] Bearer 信息添加成功: ID=%u, conn=%u, login=%u",
               bearer_id, link_conn_status, link_login_status);
  return 0;
}

/*===========================================================================
 * 使用说明
 *===========================================================================
 *
 * 上述函数基于 dict_magic.h 中已有的 AVP 定义实现，无需额外添加新 AVP。
 *
 * TFT (Traffic Flow Template) 使用示例：
 *   const char *tft_rules[] = {
 *       "permit in ip from 192.168.1.0/24 to any",
 *       "deny in ip from any to 10.0.0.0/8"
 *   };
 *   add_tft_to_ground_list_simple(msg, tft_rules, 2);
 *
 * DLM-List 使用示例：
 *   struct avp *dlm_list;
 *   create_dlm_list_simple(msg, &dlm_list);
 *
 *   dlm_info_t dlm = {...};
 *   add_dlm_info_simple(dlm_list, &dlm);
 *
 *   finalize_dlm_list_simple(msg, dlm_list);
 *
 * QoS 参数使用示例：
 *   add_qos_params_inline(msg, 2, "5", 1);  // Preemption, Class 5, AF
 *
 *===========================================================================*/
