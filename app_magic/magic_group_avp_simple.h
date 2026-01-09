/**
 * @file magic_group_avp_simple.h
 * @brief 简化版 Grouped AVP 构造助手头文件。
 * @details 该模块提供了一组不依赖全局 `g_cfg` 的 AVP
 * 构造函数，允许调用者显式传递所有参数。
 *          适用于服务端需要构造针对不同客户端的差异化应答或在测试环境中使用。
 */

#ifndef MAGIC_GROUP_AVP_SIMPLE_H
#define MAGIC_GROUP_AVP_SIMPLE_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stdbool.h>

/**
 * @brief 添加 Client-Credentials Grouped AVP (Code 20019) 的简化版本。
 * @details 允许直接传入身份验证信息，不强制读取全局配置。
 *
 * @param[in,out] msg             目标 Diameter 消息。
 * @param[in]     username        用户名 (REQUIRED)。
 * @param[in]     client_password 客户端密码 (REQUIRED)。
 * @param[in]     server_password 服务器密码 (OPTIONAL, 可为 NULL)。
 *
 * @return int 成功返回 0，失败返回 -1。
 */
int add_client_credentials_simple(struct msg *msg, const char *username,
                                  const char *client_password,
                                  const char *server_password);

/*
 * 添加 Communication-Request-Parameters Grouped AVP (Code 20001)
 * 用于客户端发起通信请求
 */
typedef struct {
  const char *profile_name;     // 必填：会话类型 (如 "VOICE", "IP_DATA")
  uint64_t requested_bw;        // 可选：请求的下行带宽 (bit/s), 0=不添加
  uint64_t requested_return_bw; // 可选：请求的上行带宽 (bit/s), 0=不添加
  uint32_t priority_class;      // 可选：优先级 1-8, 0=不添加
  uint32_t qos_level;           // 可选：QoS 等级 0-3, 0=不添加
  const char *dlm_name;         // 可选：指定链路模块名称, NULL=不添加
  uint32_t flight_phase;        // 可选：飞行阶段, 0=不添加
  uint32_t altitude;            // 可选：高度(米), 0=不添加
} comm_req_params_t;

int add_comm_req_params_simple(struct msg *msg,
                               const comm_req_params_t *params);

/*
 * 添加 Communication-Answer-Parameters Grouped AVP (Code 20002)
 * 用于服务端应答客户端请求
 *
 * 根据 ARINC 839 协议 Appendix B-1.2，包含以下 AVP：
 * - Profile-Name (必需)
 * - Granted-Bandwidth, Granted-Return-Bandwidth
 * - Priority-Type, Priority-Class, QoS-Level
 * - TFTtoGround-List, TFTtoAircraft-List (流量模板)
 * - Accounting-Enabled, DLM-Availability-List
 * - Keep-Request, Auto-Detect, Timeout
 * - Flight-Phase, Altitude, Airport (位置限制)
 * - NAPT-List, Gateway-IPAddress
 */
typedef struct {
  const char *profile_name;     // 必填：配置文件名
  const char *selected_link_id; // 必填：选中的链路 ID (DLM-Name)
  uint32_t bearer_id;           // 可选：分配的 Bearer ID (Link-Number)
  uint64_t granted_bw;          // 必填：授予的下行带宽 (bit/s)
  uint64_t granted_return_bw;   // 必填：授予的上行带宽 (bit/s)
  uint32_t priority_type;       // 必填：优先级类型 (1=Blocking, 2=Preemption)
  const char *priority_class;   // 必填：优先级类别
  uint32_t qos_level;           // 必填：QoS 等级 (0=BE, 1=AF, 2=EF)
  uint32_t accounting_enabled;  // 必填：是否启用计费 (0=否, 其他=是)
  const char *dlm_availability_list; // 可选：可用 DLM 列表
  uint32_t keep_request;             // 必填：是否保持请求 (0/1)
  uint32_t auto_detect;              // 必填：自动检测模式 (0/1/2)
  uint32_t session_timeout;          // 必填：超时时间(秒)
  const char *flight_phase;          // 可选：允许的飞行阶段
  const char *altitude;              // 可选：允许的高度范围
  const char *airport;               // 可选：允许的机场代码
  const char *assigned_ip;           // 可选：Gateway-IPAddress
} comm_ans_params_t;

/*
 * 添加 Communication-Report-Parameters Grouped AVP (Code 20003)
 * 用于 MNTR 消息通知客户端参数变更
 * 仅包含已变更的参数
 */
typedef struct {
  const char *profile_name;          // 必填：配置文件名
  uint64_t granted_bw;               // 可选：变更后的下行带宽
  uint64_t granted_return_bw;        // 可选：变更后的上行带宽
  uint32_t priority_type;            // 可选：变更后的优先级类型
  const char *priority_class;        // 可选：变更后的优先级类别
  uint32_t qos_level;                // 可选：变更后的 QoS 等级
  const char *dlm_availability_list; // 可选：变更后的可用 DLM 列表
  const char *gateway_ip;            // 可选：变更后的网关 IP
  /* 标记哪些字段已设置 */
  bool has_granted_bw;
  bool has_granted_return_bw;
  bool has_priority_type;
  bool has_priority_class;
  bool has_qos_level;
  bool has_dlm_availability_list;
  bool has_gateway_ip;
} comm_report_params_t;

int add_comm_ans_params_simple(struct msg *msg,
                               const comm_ans_params_t *params);

/*
 * 添加 Communication-Report-Parameters Grouped AVP (Code 20003)
 * 用于 MNTR 消息通知客户端参数变更
 */
int add_comm_report_params_simple(struct msg *msg,
                                  const comm_report_params_t *params);

/*
 * 添加 DLM-Info Grouped AVP (Code 20008)
 * 用于描述单个 DLM 的状态信息
 */
typedef struct {
  const char *dlm_name;      // 必填：DLM 名称
  uint32_t dlm_available;    // 必填：可用状态 (0=YES, 1=NO, 2=UNKNOWN)
  uint32_t dlm_max_links;    // 必填：最大链路数
  float dlm_max_bw;          // 必填：最大下行带宽 (kbps)
  float dlm_max_return_bw;   // 可选：最大上行带宽 (kbps), 0=对称
  uint32_t dlm_alloc_links;  // 必填：已分配链路数
  float dlm_alloc_bw;        // 必填：已分配下行带宽 (kbps)
  float dlm_alloc_return_bw; // 可选：已分配上行带宽 (kbps)
  uint32_t qos_levels[3];    // 必填：支持的 QoS 等级列表
  uint32_t qos_level_count;  // QoS 等级数量 (1-3)
} dlm_info_t;

int add_dlm_info_simple(struct avp *dlm_list_avp, const dlm_info_t *dlm);

/*
 * 添加 Link-Status-Group Grouped AVP (Code 20011)
 * 用于描述单条链路的详细状态
 */
typedef struct {
  const char *link_name;   // 必填：链路描述名称
  uint32_t link_number;    // 必填：链路编号
  uint32_t link_available; // 必填：可用状态 (1=YES, 2=NO)
  uint32_t qos_level;      // 必填：QoS 等级
  uint32_t link_conn_status; // 必填：连接状态 (0=Disconnected, 1=Connected,
                             // 2=Forced_Close)
  uint32_t link_login_status;    // 必填：登录状态 (1=Logged off, 2=Logged on)
  float link_max_bw;             // 必填：最大下行带宽 (kbps)
  float link_max_return_bw;      // 可选：最大上行带宽 (kbps)
  float link_alloc_bw;           // 必填：已分配下行带宽 (kbps)
  float link_alloc_return_bw;    // 可选：已分配上行带宽 (kbps)
  const char *link_error_string; // 可选：错误描述
} link_status_t;

int add_link_status_simple(struct avp *dlm_link_status_list_avp,
                           const link_status_t *link);

/*===========================================================================
 * 额外的 Grouped AVP 辅助函数
 * 基于现有 AVP 定义实现的补充功能
 *===========================================================================*/

/**
 * @brief 添加 TFT-to-Ground-List Grouped AVP (Code 20004)
 * @param msg 目标消息
 * @param tft_rules TFT 规则字符串数组
 * @param rule_count 规则数量
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   const char *rules[] = {"permit in ip from 192.168.1.0/24 to any"};
 *   add_tft_to_ground_list_simple(msg, rules, 1);
 */
int add_tft_to_ground_list_simple(struct msg *msg, const char **tft_rules,
                                  uint32_t rule_count);

/**
 * @brief 添加 TFT-to-Aircraft-List Grouped AVP (Code 20005)
 * @param msg 目标消息
 * @param tft_rules TFT 规则字符串数组
 * @param rule_count 规则数量
 * @return 0=成功, -1=失败
 */
int add_tft_to_aircraft_list_simple(struct msg *msg, const char **tft_rules,
                                    uint32_t rule_count);

/**
 * @brief 添加 NAPT-List Grouped AVP (Code 20006)
 * @param msg 目标消息
 * @param napt_rules NAPT 规则字符串数组
 * @param rule_count 规则数量
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   const char *napt[] = {"192.168.1.100:8080 -> 10.0.0.1:80"};
 *   add_napt_list_simple(msg, napt, 1);
 */
int add_napt_list_simple(struct msg *msg, const char **napt_rules,
                         uint32_t rule_count);

/**
 * @brief 添加 DLM-QoS-Level-List Grouped AVP (Code 20009)
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param qos_levels QoS 等级数组
 * @param level_count 等级数量 (1-3)
 * @return 0=成功, -1=失败
 */
int add_dlm_qos_level_list_simple(struct avp *dlm_info_avp,
                                  const uint32_t *qos_levels,
                                  uint32_t level_count);

/**
 * @brief 创建 DLM-List Grouped AVP (Code 20007)
 * @param msg 目标消息
 * @param dlm_list_avp_out 输出：创建的 DLM-List AVP 指针
 * @return 0=成功, -1=失败
 *
 * 使用流程：
 *   1. create_dlm_list_simple(msg, &dlm_list)
 *   2. add_dlm_info_simple(dlm_list, &dlm_info) - 多次调用
 *   3. finalize_dlm_list_simple(msg, dlm_list)
 */
int create_dlm_list_simple(struct msg *msg, struct avp **dlm_list_avp_out);

/**
 * @brief 完成 DLM-List 并添加到消息
 * @param msg 目标消息
 * @param dlm_list_avp DLM-List AVP 指针
 * @return 0=成功, -1=失败
 */
int finalize_dlm_list_simple(struct msg *msg, struct avp *dlm_list_avp);

/**
 * @brief 创建 DLM-Link-Status-List Grouped AVP (Code 20010)
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param link_status_list_out 输出：创建的链路状态列表 AVP 指针
 * @return 0=成功, -1=失败
 */
int create_dlm_link_status_list_simple(struct avp *dlm_info_avp,
                                       struct avp **link_status_list_out);

/**
 * @brief 完成 DLM-Link-Status-List 并添加到 DLM-Info
 * @param dlm_info_avp DLM-Info AVP 指针
 * @param link_status_list_avp 链路状态列表 AVP 指针
 * @return 0=成功, -1=失败
 */
int finalize_dlm_link_status_list_simple(struct avp *dlm_info_avp,
                                         struct avp *link_status_list_avp);

/**
 * @brief 添加内联 QoS 参数（基于现有 AVP）
 * @param msg 目标消息或 Grouped AVP
 * @param priority_type 优先级类型 (1=Blocking, 2=Preemption)
 * @param priority_class 优先级类别字符串 ("1"-"8")
 * @param qos_level QoS 等级 (0=BE, 1=AF, 2=EF)
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   add_qos_params_inline(msg, 2, "5", 1);  // Preemption, Class 5, AF
 */
int add_qos_params_inline(struct msg *msg, uint32_t priority_type,
                          const char *priority_class, uint32_t qos_level);

/**
 * @brief 添加内联链路特征信息（基于现有 AVP）
 * @param msg 目标消息
 * @param link_name 链路名称
 * @param link_number 链路编号
 * @param max_bw 最大带宽 (kbps)
 * @param qos_level QoS 等级
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   add_link_characteristics_inline(msg, "SAT1", 1, 2048.0, 1);
 */
int add_link_characteristics_inline(struct msg *msg, const char *link_name,
                                    uint32_t link_number, float max_bw,
                                    uint32_t qos_level);

/**
 * @brief 添加内联 Bearer 信息（基于现有 AVP）
 * @param msg 目标消息或 Grouped AVP
 * @param bearer_id Bearer 标识符 (Link-Number)
 * @param link_conn_status 链路连接状态 (0=Disconnected, 1=Connected)
 * @param link_login_status 链路登录状态 (1=Logged off, 2=Logged on)
 * @return 0=成功, -1=失败
 *
 * 示例：
 *   add_bearer_info_inline(msg, 1, 1, 2);  // Bearer 1, Connected, Logged on
 */
int add_bearer_info_inline(struct msg *msg, uint32_t bearer_id,
                           uint32_t link_conn_status,
                           uint32_t link_login_status);

#endif /* MAGIC_GROUP_AVP_SIMPLE_H */
