/*
 * dict_magic.h
 * MAGIC 系统自定义 Diameter 协议字典定义头文件（带完整中文注释）
 */

#ifndef _MAGIC_DEFS_H_
#define _MAGIC_DEFS_H_

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

/* ==========================================================================
 * 1. 基础协议常量
 * ========================================================================== */
#define MAGIC_VENDOR_ID 13712   // 自定义 Vendor ID（测试/私有使用）
#define MAGIC_APP_ID 1094202169 // MAGIC 应用 ID

/* ==========================================================================
 * 2. 命令代码定义（7 对请求/应答命令）
 * ========================================================================== */
#define CMD_MCAR_CODE 100000 // MCAR/MCAA：客户端认证请求/应答
#define CMD_MCCR_CODE 100001 // MCCR/MCCA：通信参数变更请求/应答
#define CMD_MNTR_CODE 100002 // MNTR/MNTA：通知报告请求/应答
#define CMD_MSCR_CODE 100003 // MSCR/MSCA：状态变更报告请求/应答
#define CMD_MSXR_CODE 100004 // MSXR/MSXA：状态查询请求/应答
#define CMD_MADR_CODE 100005 // MADR/MADA：计费数据请求/应答
#define CMD_MACR_CODE 100006 // MACR/MACA：计费控制请求/应答

/* ==========================================================================

 * 3. MAGIC 自定义 AVP 代码（Vendor=13712）
 * ========================================================================== */

/* --- 普通 AVP（10001 ~ 10053，共53个）--- */
#define MAGIC_AVP_CLIENT_PASSWORD 10001       // 客户端密码
#define MAGIC_AVP_REQ_STATUS_INFO 10002       // 请求状态信息
#define MAGIC_AVP_STATUS_TYPE 10003           // 状态类型
#define MAGIC_AVP_DLM_NAME 10004              // 数据链路管理器名称
#define MAGIC_AVP_DLM_AVAILABLE 10005         // DLM可用状态（Enumerated）
#define MAGIC_AVP_DLM_MAX_BW 10006            // DLM最大上行带宽 (Float32)
#define MAGIC_AVP_DLM_ALLOC_BW 10007          // DLM已分配上行带宽
#define MAGIC_AVP_DLM_MAX_RETURN_BW 10008     // DLM最大下行带宽
#define MAGIC_AVP_DLM_ALLOC_RETURN_BW 10009   // DLM已分配下行带宽
#define MAGIC_AVP_DLM_MAX_LINKS 10010         // DLM最大可管理链路数
#define MAGIC_AVP_DLM_ALLOC_LINKS 10011       // DLM已分配链路数
#define MAGIC_AVP_LINK_NUMBER 10012           // 链路编号（Link ID）
#define MAGIC_AVP_LINK_AVAILABLE 10013        // 链路可用状态
#define MAGIC_AVP_LINK_CONN_STATUS 10014      // 链路连接状态
#define MAGIC_AVP_LINK_LOGIN_STATUS 10015     // 链路登录/认证状态
#define MAGIC_AVP_LINK_MAX_BW 10016           // 单链路最大上行带宽
#define MAGIC_AVP_LINK_MAX_RETURN_BW 10017    // 单链路最大下行带宽
#define MAGIC_AVP_LINK_ALLOC_BW 10018         // 单链路已分配上行带宽
#define MAGIC_AVP_LINK_ALLOC_RETURN_BW 10019  // 单链路已分配下行带宽
#define MAGIC_AVP_LINK_ERROR_STRING 10020     // 链路错误描述
#define MAGIC_AVP_REQUESTED_BW 10021          // 请求的上行带宽
#define MAGIC_AVP_REQUESTED_RETURN_BW 10022   // 请求的下行带宽
#define MAGIC_AVP_REQUIRED_BW 10023           // 要求的最低保障上行带宽
#define MAGIC_AVP_REQUIRED_RETURN_BW 10024    // 要求的最低保障下行带宽
#define MAGIC_AVP_PRIORITY_CLASS 10025        // 优先级类别（字符串）
#define MAGIC_AVP_PRIORITY_TYPE 10026         // 优先级类型（Enumerated）
#define MAGIC_AVP_QOS_LEVEL 10027             // QoS等级
#define MAGIC_AVP_DLM_AVAILABILITY_LIST 10028 // 可用的DLM名称列表
#define MAGIC_AVP_GATEWAY_IP 10029            // 网关IP地址
#define MAGIC_AVP_TFT_TO_GROUND_RULE 10030    // 地面侧流量过滤规则
#define MAGIC_AVP_TFT_TO_AIRCRAFT_RULE 10031  // 飞机侧流量过滤规则
#define MAGIC_AVP_NAPT_RULE 10032             // NAPT（端口地址转换）规则
#define MAGIC_AVP_FLIGHT_PHASE 10033          // 当前飞行阶段（如起飞、巡航等）
#define MAGIC_AVP_ALTITUDE 10034              // 海拔高度
#define MAGIC_AVP_AIRPORT 10035               // 允许通信的机场代码
#define MAGIC_AVP_ACCOUNTING_ENABLED 10036    // 是否启用计费
#define MAGIC_AVP_KEEP_REQUEST 10037          // 是否保持原有请求
#define MAGIC_AVP_AUTO_DETECT 10038           // 自动检测标志
#define MAGIC_AVP_TIMEOUT 10039               // 超时时间（秒）
#define MAGIC_AVP_PROFILE_NAME 10040          // 配置文件名称
#define MAGIC_AVP_REGISTERED_CLIENTS 10041    // 已注册客户端列表
#define MAGIC_AVP_CDR_TYPE 10042              // 计费记录（CDR）类型
#define MAGIC_AVP_CDR_LEVEL 10043             // CDR级别
#define MAGIC_AVP_CDR_REQ_ID 10044            // CDR请求ID
#define MAGIC_AVP_SERVER_PASSWORD 10045       // 服务器密码
#define MAGIC_AVP_CDR_ID 10046                // CDR唯一标识
#define MAGIC_AVP_CDR_CONTENT 10047           // CDR具体内容
#define MAGIC_AVP_CDR_RESTART_SESS_ID 10048   // 重启后的会话ID
#define MAGIC_AVP_CDR_STOPPED 10049           // CDR停止时间/状态
#define MAGIC_AVP_CDR_STARTED 10050           // CDR开始时间/状态
#define MAGIC_AVP_GRANTED_BW 10051            // 实际授权的上行带宽
#define MAGIC_AVP_GRANTED_RETURN_BW 10052     // 实际授权的下行带宽
#define MAGIC_AVP_MAGIC_STATUS_CODE 10053     // MAGIC协议自定义状态码
#define MAGIC_AVP_LINK_NAME 10054             // 链路描述名称

/* --- Grouped AVP（20001 ~ 20019，共19个）--- */
#define MAGIC_AVP_COMM_REQ_PARAMS 20001      // 通信请求参数组
#define MAGIC_AVP_COMM_ANS_PARAMS 20002      // 通信应答参数组
#define MAGIC_AVP_COMM_REPORT_PARAMS 20003   // 通信报告参数组
#define MAGIC_AVP_TFT_TO_GROUND_LIST 20004   // 地面侧TFT规则列表
#define MAGIC_AVP_TFT_TO_AIRCRAFT_LIST 20005 // 飞机侧TFT规则列表
#define MAGIC_AVP_NAPT_LIST 20006            // NAPT规则列表
#define MAGIC_AVP_DLM_LIST 20007             // DLM列表
#define MAGIC_AVP_DLM_INFO 20008             // 单个DLM详细信息组
#define MAGIC_AVP_DLM_QOS_LEVEL_LIST 20009   // DLM QoS等级列表
#define MAGIC_AVP_DLM_LINK_STATUS_LIST 20010 // DLM下所有链路状态列表
#define MAGIC_AVP_LINK_STATUS_GROUP 20011    // 单条链路完整状态信息组
#define MAGIC_AVP_CDRS_ACTIVE 20012          // 活动中CDR列表
#define MAGIC_AVP_CDRS_FINISHED 20013        // 已结束CDR列表
#define MAGIC_AVP_CDRS_FORWARDED 20014       // 已转发CDR列表
#define MAGIC_AVP_CDRS_UNKNOWN 20015         // 未知状态CDR列表
#define MAGIC_AVP_CDRS_UPDATED 20016         // 已更新CDR列表
#define MAGIC_AVP_CDR_INFO 20017             // 单条CDR详细信息组
#define MAGIC_AVP_CDR_START_STOP_PAIR 20018  // CDR开始-停止配对信息
#define MAGIC_AVP_CLIENT_CREDENTIALS                                           \
  20019 // 客户端凭证信息组（密码+其他认证信息）

/* ==========================================================================
 * 4. 标准 Diameter 核心 AVP 句柄结构体（Vendor=0） - 已加完整中文注释
 * ========================================================================== */
/**
 * @brief 标准 Diameter 核心 AVP 句柄结构体 (Vendor=0)。
 * @details 存储常用的 Diameter 基础协议 AVP 对象引用，用于快速构建消息。
 */
struct std_diam_dict_handles {
  struct dict_object *avp_session_id;   ///< 会话标识（Session-Id）
  struct dict_object *avp_origin_host;  ///< 源主机标识（Origin-Host）
  struct dict_object *avp_origin_realm; ///< 源域标识（Origin-Realm）
  struct dict_object
      *avp_destination_host; ///< 目的主机标识（Destination-Host）
  struct dict_object
      *avp_destination_realm; ///< 目的域标识（Destination-Realm）
  struct dict_object
      *avp_auth_application_id;        ///< 认证应用ID（Auth-Application-Id）
  struct dict_object *avp_result_code; ///< 标准结果码（Result-Code）
  struct dict_object *
      avp_experimental_result_code; ///< 实验性结果码（Experimental-Result-Code）
  struct dict_object *avp_user_name; ///< 用户名（User-Name）

  struct dict_object *avp_accounting_realtime_required; ///< 计费实时要求
  struct dict_object *avp_accounting_record_number;     ///< 计费记录序号
  struct dict_object
      *avp_accounting_record_type; ///< 计费记录类型（Event/Start/Interim/Stop）
  struct dict_object *avp_accounting_session_id;     ///< 计费会话ID
  struct dict_object *avp_accounting_sub_session_id; ///< 计费子会话ID
  struct dict_object *avp_acct_application_id;       ///< 计费应用ID
  struct dict_object *avp_acct_interim_interval;     ///< 计费中间报告间隔
  struct dict_object *avp_acct_multi_session_id;     ///< 多会话计费ID
  struct dict_object *avp_auth_grace_period;         ///< 认证宽限期
  struct dict_object *avp_authorization_lifetime;    ///< 授权生命周期
  struct dict_object *avp_auth_request_type;         ///< 认证请求类型
  struct dict_object *avp_auth_session_state; ///< 认证会话状态（有状态/无状态）
  struct dict_object *avp_class;              ///< Class AVP（用于会话恢复）
  struct dict_object *avp_disconnect_cause;   ///< 断开原因
  struct dict_object *avp_error_message;      ///< 错误消息文本
  struct dict_object *avp_error_reporting_host;    ///< 报告错误的节点
  struct dict_object *avp_event_timestamp;         ///< 事件时间戳
  struct dict_object *avp_experimental_result;     ///< 实验性结果（Grouped）
  struct dict_object *avp_failed_avp;              ///< 导致失败的AVP（Grouped）
  struct dict_object *avp_firmware_revision;       ///< 固件版本
  struct dict_object *avp_host_ip_address;         ///< 主机IP地址
  struct dict_object *avp_inband_security_id;      ///< 带内安全ID（TLS/IPsec）
  struct dict_object *avp_multi_round_time_out;    ///< 多轮认证超时
  struct dict_object *avp_origin_state_id;         ///< 源状态ID（用于故障恢复）
  struct dict_object *avp_product_name;            ///< 产品名称
  struct dict_object *avp_proxy_host;              ///< 代理主机
  struct dict_object *avp_proxy_info;              ///< 代理信息（Grouped）
  struct dict_object *avp_proxy_state;             ///< 代理状态
  struct dict_object *avp_re_auth_request_type;    ///< 重认证请求类型
  struct dict_object *avp_redirect_host;           ///< 重定向主机
  struct dict_object *avp_redirect_host_usage;     ///< 重定向主机使用策略
  struct dict_object *avp_redirect_max_cache_time; ///< 重定向缓存最大时间
  struct dict_object *avp_route_record;            ///< 路由记录
  struct dict_object *avp_session_binding;         ///< 会话绑定标志
  struct dict_object *avp_session_server_failover; ///< 会话服务器故障转移策略
  struct dict_object *avp_session_timeout;         ///< 会话超时时间
  struct dict_object *avp_supported_vendor_id;     ///< 支持的Vendor-ID
  struct dict_object *avp_termination_cause;       ///< 会话终止原因
  struct dict_object *avp_vendor_id;               ///< Vendor-ID
  struct dict_object *
      avp_vendor_specific_application_id; ///< Vendor-Specific-Application-Id（Grouped）
};

/* ==========================================================================
 * 5. MAGIC 自定义协议专属句柄结构体（Vendor=13712） - 已加完整中文注释
 * ========================================================================== */
/**
 * @brief MAGIC 自定义协议专属句柄结构体 (Vendor=13712)。
 * @details 包含 MAGIC 协议定义的所有自定义命令和 AVP 对象。
 */
struct magic_dict_handles {
  struct dict_object *vendor; ///< Vendor 对象（13712）
  struct dict_object *app;    ///< Application 对象（16777300）

  /* ==================== 7个自定义命令 ==================== */
  struct dict_object *cmd_mcar; ///< MCAR：客户端认证请求/应答 (100000)
  struct dict_object *cmd_mccr; ///< MCCR：通信参数变更请求/应答 (100001)
  struct dict_object *cmd_mntr; ///< MNTR：通知报告请求/应答 (100002)
  struct dict_object *cmd_mscr; ///< MSCR：状态变更报告请求/应答 (100003)
  struct dict_object *cmd_msxr; ///< MSXR：状态查询请求/应答 (100004)
  struct dict_object *cmd_madr; ///< MADR：计费数据请求/应答 (100005)
  struct dict_object *cmd_macr; ///< MACR：计费控制请求/应答 (100006)

  /* ==================== 53个普通自定义 AVP ==================== */
  struct dict_object *avp_client_password;      ///< 10001：客户端密码
  struct dict_object *avp_req_status_info;      ///< 10002：请求状态信息
  struct dict_object *avp_status_type;          ///< 10003：状态类型
  struct dict_object *avp_dlm_name;             ///< 10004：数据链路管理器名称
  struct dict_object *avp_dlm_available;        ///< 10005：DLM可用状态
  struct dict_object *avp_dlm_max_bw;           ///< 10006：DLM最大上行带宽
  struct dict_object *avp_dlm_alloc_bw;         ///< 10007：DLM已分配上行带宽
  struct dict_object *avp_dlm_max_return_bw;    ///< 10008：DLM最大下行带宽
  struct dict_object *avp_dlm_alloc_return_bw;  ///< 10009：DLM已分配下行带宽
  struct dict_object *avp_dlm_max_links;        ///< 10010：DLM最大链路数
  struct dict_object *avp_dlm_alloc_links;      ///< 10011：DLM已分配链路数
  struct dict_object *avp_link_number;          ///< 10012：链路编号（Link ID）
  struct dict_object *avp_link_available;       ///< 10013：链路可用状态
  struct dict_object *avp_link_conn_status;     ///< 10014：链路连接状态
  struct dict_object *avp_link_login_status;    ///< 10015：链路登录/认证状态
  struct dict_object *avp_link_max_bw;          ///< 10016：单链路最大上行带宽
  struct dict_object *avp_link_max_return_bw;   ///< 10017：单链路最大下行带宽
  struct dict_object *avp_link_alloc_bw;        ///< 10018：单链路已分配上行带宽
  struct dict_object *avp_link_alloc_return_bw; ///< 10019：单链路已分配下行带宽
  struct dict_object *avp_link_error_string;    ///< 10020：链路错误描述
  struct dict_object *avp_requested_bw;         ///< 10021：请求的上行带宽
  struct dict_object *avp_requested_return_bw;  ///< 10022：请求的下行带宽
  struct dict_object *avp_required_bw;          ///< 10023：要求的最低上行带宽
  struct dict_object *avp_required_return_bw;   ///< 10024：要求的最低下行带宽
  struct dict_object *avp_priority_class;       ///< 10025：优先级类别
  struct dict_object *avp_priority_type;        ///< 10026：优先级类型
  struct dict_object *avp_qos_level;            ///< 10027：QoS等级
  struct dict_object *avp_dlm_availability_list; ///< 10028：可用DLM名称列表
  struct dict_object *avp_gateway_ip;            ///< 10029：网关IP地址
  struct dict_object *avp_tft_to_ground_rule;    ///< 10030：地面侧流量过滤规则
  struct dict_object *avp_tft_to_aircraft_rule;  ///< 10031：飞机侧流量过滤规则
  struct dict_object *avp_napt_rule;             ///< 10032：NAPT规则
  struct dict_object *avp_flight_phase;          ///< 10033：当前飞行阶段
  struct dict_object *avp_altitude;              ///< 10034：海拔高度
  struct dict_object *avp_airport;               ///< 10035：允许通信的机场代码
  struct dict_object *avp_accounting_enabled;    ///< 10036：是否启用计费
  struct dict_object *avp_keep_request;          ///< 10037：保留原有请求标志
  struct dict_object *avp_auto_detect;           ///< 10038：自动检测标志
  struct dict_object *avp_timeout;               ///< 10039：超时时间（秒）
  struct dict_object *avp_profile_name;          ///< 10040：配置文件名称
  struct dict_object *avp_registered_clients;    ///< 10041：已注册客户端列表
  struct dict_object *avp_cdr_type;              ///< 10042：CDR类型
  struct dict_object *avp_cdr_level;             ///< 10043：CDR级别
  struct dict_object *avp_cdr_req_id;            ///< 10044：CDR请求ID
  struct dict_object *avp_server_password;       ///< 10045：服务器密码
  struct dict_object *avp_cdr_id;                ///< 10046：CDR唯一标识
  struct dict_object *avp_cdr_content;           ///< 10047：CDR内容
  struct dict_object *avp_cdr_restart_sess_id;   ///< 10048：重启后的会话ID
  struct dict_object *avp_cdr_stopped;           ///< 10049：CDR停止时间/状态
  struct dict_object *avp_cdr_started;           ///< 10050：CDR开始时间/状态
  struct dict_object *avp_granted_bw;            ///< 10051：实际授权上行带宽
  struct dict_object *avp_granted_return_bw;     ///< 10052：实际授权下行带宽
  struct dict_object *avp_magic_status_code;     ///< 10053：MAGIC自定义状态码
  struct dict_object *avp_link_name;             ///< 10054：链路描述名称

  /* ==================== 19个 Grouped 自定义 AVP ==================== */
  struct dict_object *avp_comm_req_params;      ///< 20001：通信请求参数组
  struct dict_object *avp_comm_ans_params;      ///< 20002：通信应答参数组
  struct dict_object *avp_comm_report_params;   ///< 20003：通信报告参数组
  struct dict_object *avp_tft_to_ground_list;   ///< 20004：地面侧TFT规则列表
  struct dict_object *avp_tft_to_aircraft_list; ///< 20005：飞机侧TFT规则列表
  struct dict_object *avp_napt_list;            ///< 20006：NAPT规则列表
  struct dict_object *avp_dlm_list;             ///< 20007：DLM列表
  struct dict_object *avp_dlm_info;             ///< 20008：单个DLM信息组
  struct dict_object *avp_dlm_qos_level_list;   ///< 20009：DLM QoS等级列表
  struct dict_object
      *avp_dlm_link_status_list;               ///< 20010：DLM下所有链路状态列表
  struct dict_object *avp_link_status_group;   ///< 20011：单条链路完整状态组
  struct dict_object *avp_cdrs_active;         ///< 20012：活动中的CDR列表
  struct dict_object *avp_cdrs_finished;       ///< 20013：已完成的CDR列表
  struct dict_object *avp_cdrs_forwarded;      ///< 20014：已转发的CDR列表
  struct dict_object *avp_cdrs_unknown;        ///< 20015：未知状态CDR列表
  struct dict_object *avp_cdrs_updated;        ///< 20016：已更新的CDR列表
  struct dict_object *avp_cdr_info;            ///< 20017：单条CDR详细信息组
  struct dict_object *avp_cdr_start_stop_pair; ///< 20018：CDR开始-停止配对信息
  struct dict_object *avp_client_credentials;  ///< 20019：客户端凭证信息组
};

/* ==========================================================================
 * 6. 全局字典实例与初始化函数
 * ========================================================================== */
extern struct std_diam_dict_handles g_std_dict; // 标准 Diameter 字典句柄
extern struct magic_dict_handles g_magic_dict;  // MAGIC 自定义协议字典句柄

int magic_dict_init(void);

#endif /* _MAGIC_DEFS_H_ */