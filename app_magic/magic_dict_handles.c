/**
 * @file magic_dict_handles.c
 * @brief MAGIC 协议字典句柄管理器实现文件。
 * @details 负责在 freeDiameter 核心启动时，从全局字典中检索标准 Diameter AVP 和
 * MAGIC 私有 AVP 的引用，并存储在全局句柄结构体中
 *          以便后续各业务模块快速访问，避免运行时的高频字符串查找开销。
 */

#include "magic_dict_handles.h"

/* 两个全局字典实例 */
struct std_diam_dict_handles g_std_dict = {0}; // 标准 Diameter AVP 句柄
struct magic_dict_handles g_magic_dict = {0};  // MAGIC 自定义协议句柄

/* 辅助宏：查找标准 AVP（Vendor=0） */
#define SEARCH_STD_AVP(code, target)                                           \
  do {                                                                         \
    struct dict_avp_request req = {.avp_vendor = 0, .avp_code = code};         \
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP,                  \
                             AVP_BY_CODE_AND_VENDOR, &req, &target, ENOENT));  \
  } while (0)

/* 辅助宏：查找 MAGIC 自定义 AVP */
#define SEARCH_MAGIC_AVP(code, target)                                         \
  do {                                                                         \
    struct dict_avp_request req = {.avp_vendor = MAGIC_VENDOR_ID,              \
                                   .avp_code = code};                          \
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP,                  \
                             AVP_BY_CODE_AND_VENDOR, &req, &target, ENOENT));  \
  } while (0)

/* 辅助宏：查找命令 */
#define SEARCH_CMD(code, target)                                               \
  do {                                                                         \
    command_code_t cmd_code = code;                                            \
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND,              \
                             CMD_BY_CODE_R, &cmd_code, &target, ENOENT));      \
  } while (0)

/**
 * @brief 初始化 MAGIC 协议字典句柄。
 * @details 该函数通过调用 `fd_dict_search` 查找所有预定义的命令和 AVP 代码。
 *          如果任何核心 AVP（如 Session-Id）或 MAGIC 必需的私有 AVP
 * 查找失败，系统将报错。
 *
 * @return int 成功返回 0；如果任何必需的字典对象未找到则返回非 0 错误码。
 *
 * @note 该函数必须在 freeDiameter 库初始化完成之后、MAGIC
 * 业务逻辑启动之前调用。
 * @warning 查找失败通常意味着 `dict_magic` 依赖词典未正确加载。
 */
int magic_dict_init(void) {
  /* ------------------ 1. 查找 Vendor 和 Application 对象 ------------------ */
  // 通过名称查找，避免硬编码 ID（名称在 dict_magic_839 中定义）
  CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_VENDOR, VENDOR_BY_NAME,
                           "AEEC (ARINC)", &g_magic_dict.vendor, ENOENT));
  CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION,
                           APPLICATION_BY_NAME, "MAGIC-ARINC839",
                           &g_magic_dict.app, ENOENT));

  /* ------------------ 2. 查找标准核心 AVP（Vendor=0） ------------------ */
  SEARCH_STD_AVP(263, g_std_dict.avp_session_id);
  SEARCH_STD_AVP(264, g_std_dict.avp_origin_host);
  SEARCH_STD_AVP(296, g_std_dict.avp_origin_realm);
  SEARCH_STD_AVP(293, g_std_dict.avp_destination_host);
  SEARCH_STD_AVP(283, g_std_dict.avp_destination_realm);
  SEARCH_STD_AVP(258, g_std_dict.avp_auth_application_id);
  SEARCH_STD_AVP(268, g_std_dict.avp_result_code);
  SEARCH_STD_AVP(298, g_std_dict.avp_experimental_result_code);
  SEARCH_STD_AVP(1, g_std_dict.avp_user_name);

  SEARCH_STD_AVP(483, g_std_dict.avp_accounting_realtime_required);
  SEARCH_STD_AVP(485, g_std_dict.avp_accounting_record_number);
  SEARCH_STD_AVP(480, g_std_dict.avp_accounting_record_type);
  SEARCH_STD_AVP(44, g_std_dict.avp_accounting_session_id);
  SEARCH_STD_AVP(287, g_std_dict.avp_accounting_sub_session_id);
  SEARCH_STD_AVP(259, g_std_dict.avp_acct_application_id);
  SEARCH_STD_AVP(85, g_std_dict.avp_acct_interim_interval);
  SEARCH_STD_AVP(50, g_std_dict.avp_acct_multi_session_id);
  SEARCH_STD_AVP(276, g_std_dict.avp_auth_grace_period);
  SEARCH_STD_AVP(291, g_std_dict.avp_authorization_lifetime);
  SEARCH_STD_AVP(274, g_std_dict.avp_auth_request_type);
  SEARCH_STD_AVP(277, g_std_dict.avp_auth_session_state);
  SEARCH_STD_AVP(25, g_std_dict.avp_class);
  SEARCH_STD_AVP(273, g_std_dict.avp_disconnect_cause);
  SEARCH_STD_AVP(281, g_std_dict.avp_error_message);
  SEARCH_STD_AVP(294, g_std_dict.avp_error_reporting_host);
  SEARCH_STD_AVP(55, g_std_dict.avp_event_timestamp);
  SEARCH_STD_AVP(297, g_std_dict.avp_experimental_result);
  SEARCH_STD_AVP(279, g_std_dict.avp_failed_avp);
  SEARCH_STD_AVP(267, g_std_dict.avp_firmware_revision);
  SEARCH_STD_AVP(257, g_std_dict.avp_host_ip_address);
  SEARCH_STD_AVP(299, g_std_dict.avp_inband_security_id);
  SEARCH_STD_AVP(272, g_std_dict.avp_multi_round_time_out);
  SEARCH_STD_AVP(278, g_std_dict.avp_origin_state_id);
  SEARCH_STD_AVP(269, g_std_dict.avp_product_name);
  SEARCH_STD_AVP(280, g_std_dict.avp_proxy_host);
  SEARCH_STD_AVP(284, g_std_dict.avp_proxy_info);
  SEARCH_STD_AVP(33, g_std_dict.avp_proxy_state);
  SEARCH_STD_AVP(285, g_std_dict.avp_re_auth_request_type);
  SEARCH_STD_AVP(292, g_std_dict.avp_redirect_host);
  SEARCH_STD_AVP(261, g_std_dict.avp_redirect_host_usage);
  SEARCH_STD_AVP(262, g_std_dict.avp_redirect_max_cache_time);
  SEARCH_STD_AVP(282, g_std_dict.avp_route_record);
  SEARCH_STD_AVP(270, g_std_dict.avp_session_binding);
  SEARCH_STD_AVP(271, g_std_dict.avp_session_server_failover);
  SEARCH_STD_AVP(27, g_std_dict.avp_session_timeout);
  SEARCH_STD_AVP(265, g_std_dict.avp_supported_vendor_id);
  SEARCH_STD_AVP(295, g_std_dict.avp_termination_cause);
  SEARCH_STD_AVP(266, g_std_dict.avp_vendor_id);
  SEARCH_STD_AVP(260, g_std_dict.avp_vendor_specific_application_id);

  /* ------------------ 3. 查找自定义命令 ------------------ */
  SEARCH_CMD(CMD_MCAR_CODE, g_magic_dict.cmd_mcar);
  SEARCH_CMD(CMD_MCCR_CODE, g_magic_dict.cmd_mccr);
  SEARCH_CMD(CMD_MNTR_CODE, g_magic_dict.cmd_mntr);
  SEARCH_CMD(CMD_MSCR_CODE, g_magic_dict.cmd_mscr);
  SEARCH_CMD(CMD_MSXR_CODE, g_magic_dict.cmd_msxr);
  SEARCH_CMD(CMD_MADR_CODE, g_magic_dict.cmd_madr);
  SEARCH_CMD(CMD_MACR_CODE, g_magic_dict.cmd_macr);

  /* ------------------ 4. 查找自定义普通 AVP ------------------ */
  SEARCH_MAGIC_AVP(MAGIC_AVP_CLIENT_PASSWORD, g_magic_dict.avp_client_password);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REQ_STATUS_INFO, g_magic_dict.avp_req_status_info);
  SEARCH_MAGIC_AVP(MAGIC_AVP_STATUS_TYPE, g_magic_dict.avp_status_type);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_NAME, g_magic_dict.avp_dlm_name);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_AVAILABLE, g_magic_dict.avp_dlm_available);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_MAX_BW, g_magic_dict.avp_dlm_max_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_ALLOC_BW, g_magic_dict.avp_dlm_alloc_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_MAX_RETURN_BW,
                   g_magic_dict.avp_dlm_max_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_ALLOC_RETURN_BW,
                   g_magic_dict.avp_dlm_alloc_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_MAX_LINKS, g_magic_dict.avp_dlm_max_links);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_ALLOC_LINKS, g_magic_dict.avp_dlm_alloc_links);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_NAME, g_magic_dict.avp_link_name);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_NUMBER, g_magic_dict.avp_link_number);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_AVAILABLE, g_magic_dict.avp_link_available);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_CONN_STATUS,
                   g_magic_dict.avp_link_conn_status);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_LOGIN_STATUS,
                   g_magic_dict.avp_link_login_status);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_MAX_BW, g_magic_dict.avp_link_max_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_MAX_RETURN_BW,
                   g_magic_dict.avp_link_max_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_ALLOC_BW, g_magic_dict.avp_link_alloc_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_ALLOC_RETURN_BW,
                   g_magic_dict.avp_link_alloc_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_ERROR_STRING,
                   g_magic_dict.avp_link_error_string);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REQUESTED_BW, g_magic_dict.avp_requested_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REQUESTED_RETURN_BW,
                   g_magic_dict.avp_requested_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REQUIRED_BW, g_magic_dict.avp_required_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REQUIRED_RETURN_BW,
                   g_magic_dict.avp_required_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_PRIORITY_CLASS, g_magic_dict.avp_priority_class);
  SEARCH_MAGIC_AVP(MAGIC_AVP_PRIORITY_TYPE, g_magic_dict.avp_priority_type);
  SEARCH_MAGIC_AVP(MAGIC_AVP_QOS_LEVEL, g_magic_dict.avp_qos_level);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_AVAILABILITY_LIST,
                   g_magic_dict.avp_dlm_availability_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_GATEWAY_IP, g_magic_dict.avp_gateway_ip);
  SEARCH_MAGIC_AVP(MAGIC_AVP_TFT_TO_GROUND_RULE,
                   g_magic_dict.avp_tft_to_ground_rule);
  SEARCH_MAGIC_AVP(MAGIC_AVP_TFT_TO_AIRCRAFT_RULE,
                   g_magic_dict.avp_tft_to_aircraft_rule);
  SEARCH_MAGIC_AVP(MAGIC_AVP_NAPT_RULE, g_magic_dict.avp_napt_rule);
  SEARCH_MAGIC_AVP(MAGIC_AVP_FLIGHT_PHASE, g_magic_dict.avp_flight_phase);
  SEARCH_MAGIC_AVP(MAGIC_AVP_ALTITUDE, g_magic_dict.avp_altitude);
  SEARCH_MAGIC_AVP(MAGIC_AVP_AIRPORT, g_magic_dict.avp_airport);
  SEARCH_MAGIC_AVP(MAGIC_AVP_ACCOUNTING_ENABLED,
                   g_magic_dict.avp_accounting_enabled);
  SEARCH_MAGIC_AVP(MAGIC_AVP_KEEP_REQUEST, g_magic_dict.avp_keep_request);
  SEARCH_MAGIC_AVP(MAGIC_AVP_AUTO_DETECT, g_magic_dict.avp_auto_detect);
  SEARCH_MAGIC_AVP(MAGIC_AVP_TIMEOUT, g_magic_dict.avp_timeout);
  SEARCH_MAGIC_AVP(MAGIC_AVP_PROFILE_NAME, g_magic_dict.avp_profile_name);
  SEARCH_MAGIC_AVP(MAGIC_AVP_REGISTERED_CLIENTS,
                   g_magic_dict.avp_registered_clients);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_TYPE, g_magic_dict.avp_cdr_type);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_LEVEL, g_magic_dict.avp_cdr_level);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_REQ_ID, g_magic_dict.avp_cdr_req_id);
  SEARCH_MAGIC_AVP(MAGIC_AVP_SERVER_PASSWORD, g_magic_dict.avp_server_password);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_ID, g_magic_dict.avp_cdr_id);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_CONTENT, g_magic_dict.avp_cdr_content);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_RESTART_SESS_ID,
                   g_magic_dict.avp_cdr_restart_sess_id);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_STOPPED, g_magic_dict.avp_cdr_stopped);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_STARTED, g_magic_dict.avp_cdr_started);
  SEARCH_MAGIC_AVP(MAGIC_AVP_GRANTED_BW, g_magic_dict.avp_granted_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_GRANTED_RETURN_BW,
                   g_magic_dict.avp_granted_return_bw);
  SEARCH_MAGIC_AVP(MAGIC_AVP_MAGIC_STATUS_CODE,
                   g_magic_dict.avp_magic_status_code);

  /* ------------------ 5. 查找自定义 Grouped AVP ------------------ */
  SEARCH_MAGIC_AVP(MAGIC_AVP_COMM_REQ_PARAMS, g_magic_dict.avp_comm_req_params);
  SEARCH_MAGIC_AVP(MAGIC_AVP_COMM_ANS_PARAMS, g_magic_dict.avp_comm_ans_params);
  SEARCH_MAGIC_AVP(MAGIC_AVP_COMM_REPORT_PARAMS,
                   g_magic_dict.avp_comm_report_params);
  SEARCH_MAGIC_AVP(MAGIC_AVP_TFT_TO_GROUND_LIST,
                   g_magic_dict.avp_tft_to_ground_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_TFT_TO_AIRCRAFT_LIST,
                   g_magic_dict.avp_tft_to_aircraft_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_NAPT_LIST, g_magic_dict.avp_napt_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_LIST, g_magic_dict.avp_dlm_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_INFO, g_magic_dict.avp_dlm_info);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_QOS_LEVEL_LIST,
                   g_magic_dict.avp_dlm_qos_level_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_DLM_LINK_STATUS_LIST,
                   g_magic_dict.avp_dlm_link_status_list);
  SEARCH_MAGIC_AVP(MAGIC_AVP_LINK_STATUS_GROUP,
                   g_magic_dict.avp_link_status_group);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDRS_ACTIVE, g_magic_dict.avp_cdrs_active);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDRS_FINISHED, g_magic_dict.avp_cdrs_finished);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDRS_FORWARDED, g_magic_dict.avp_cdrs_forwarded);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDRS_UNKNOWN, g_magic_dict.avp_cdrs_unknown);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDRS_UPDATED, g_magic_dict.avp_cdrs_updated);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_INFO, g_magic_dict.avp_cdr_info);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CDR_START_STOP_PAIR,
                   g_magic_dict.avp_cdr_start_stop_pair);
  SEARCH_MAGIC_AVP(MAGIC_AVP_CLIENT_CREDENTIALS,
                   g_magic_dict.avp_client_credentials);

  return 0; // 初始化成功
}