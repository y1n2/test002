/**
 * @file magic_group_avp_add.h
 * @brief ARINC 839 MAGIC 协议 Grouped AVP 构造助手头文件。
 * @details 本文件定义了构造 MAGIC 协议中所有复杂 Grouped AVP（如 CDR 列表、DLM
 * 状态信息等）的助手函数接口和业务数据结构。
 *          涵盖了认证凭据、通信请求/应答参数及 30001 相关的计费 AVP。
 *
 * @author MAGIC System Development Team
 * @date 2025-11-20
 */

#ifndef MAGIC_GROUP_AVP_HELPERS_H
#define MAGIC_GROUP_AVP_HELPERS_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stddef.h>
#include <stdint.h>

/* 引入配置与字典头以获得真实类型定义 */
#include "config.h" /* 定义了 app_config_t 和 extern app_config_t g_cfg */
#include "magic_dict_handles.h" /* 定义了 struct magic_dict_handles 和 g_magic_dict */

/* 外部全局变量（已在其他模块定义） */
extern app_config_t g_cfg;
extern struct magic_dict_handles g_magic_dict;

/* Vendor ID 常量 */
#define MAGIC_VENDOR_ID 13712

/* ================================================================== */
/* 必需的业务结构体定义（用于 CDR 和 DLM 相关 AVP）                    */
/* ================================================================== */

/**
 * @brief 单条 CDR 记录结构（用于 CDRs-Active/Finished/Forwarded）
 */
typedef struct {
  const char *id;      // CDR 唯一标识，必填
  const char *content; // 可选的完整 CDR 内容（通常为 JSON 字符串），可为 NULL
} cdr_item_t;

/**
 * @brief CDR 更新配对结构（用于 CDRs-Updated）
 */
typedef struct {
  const char *stopped; // 已结束的旧 CDR-ID（可选）
  const char *started; // 新开始的 CDR-ID（可选）
} cdr_start_stop_t;

/**
 * @brief 单条物理链路状态（用于 Link-Status-Group）
 */
typedef struct {
  uint32_t number;          // 链路编号（如 1、2、3）
  uint32_t available;       // 是否可用：1=可用，0=不可用
  uint32_t qos_level;       // 当前 QoS 等级
  uint32_t conn_status;     // 连接状态（如 Connected、Disconnected）
  uint32_t login_status;    // 登录状态（如 LoggedIn、LoggedOut）
  uint64_t max_bw;          // 最大下行带宽（bit/s）
  uint64_t max_return_bw;   // 最大上行带宽，可为 0
  uint64_t alloc_bw;        // 已分配下行带宽，可为 0
  uint64_t alloc_return_bw; // 已分配上行带宽，可为 0
  const char *error_str;    // 错误描述字符串，可为 NULL 或空字符串
} link_status_t;

/**
 * @brief 单个 DLM (Data Link Module) 完整信息结构。
 * @details 该结构体整合了 DLM 的基本属性、实时带宽状态、支持的 QoS
 * 等级以及其下属物理链路的详细状态。
 */
typedef struct {
  const char *name;       ///< DLM 名称，如 "SATCOM1"、"IRIDIUM"。
  uint32_t available;     ///< 是否可用：1 = 可用，0 = 不可用。
  uint32_t max_links;     ///< 该模块支持的最大物理链路数。
  uint64_t max_bw;        ///< 最大下行带宽 (bit/s)。
  uint64_t max_return_bw; ///< 最大上行带宽 (bit/s)，可选。

  uint32_t allocated_links;     ///< 当前已分配/激活的链路数。
  uint64_t allocated_bw;        ///< 当前已分配的总下行带宽。
  uint64_t allocated_return_bw; ///< 当前已分配的总上行带宽，可选。

  int qos_count;          ///< 当前支持的 QoS 等级数量 (0~3)。
  uint32_t qos_levels[3]; ///< 支持的 QoS 等级列表。

  int link_count;       ///< 该模块下管理的物理链路总数。
  link_status_t *links; ///< 指向链路状态数组的指针。
} dlm_info_t;

/* ================================================================== */
/* 函数声明（按 AVP Code 顺序排列，带详细中文注释）                    */
/* ================================================================== */

/**
 * @brief 添加 Client-Credentials (20019) —— 认证凭据认证
 * @param msg 目标 Diameter 消息句柄
 * @return 0 成功，-1 失败（通常是密码未配置）
 */
int add_client_credentials(struct msg *msg);

/**
 * @brief 添加 Communication-Request-Parameters (20001) —— 客户端发起通信请求
 * @param msg 目标消息
 * @return 0 成功，-1 失败（Profile-Name 必填）
 */
int add_comm_req_params(struct msg *msg);

/**
 * @brief 添加 Communication-Answer-Parameters (20002) —— 服务端应答通信请求
 * @param msg 目标消息
 * @return 始终返回 0（所有字段均有默认值处理）
 */
int add_comm_ans_params(struct msg *msg);

/**
 * @brief 添加 Communication-Report-Parameters (20003) —— 客户端上报通信状态
 * @param msg 目标消息
 * @return 始终返回 0
 */
int add_comm_report_params(struct msg *msg);

/**
 * @brief 添加 TFTtoGround-List (20004) —— 地→机流量过滤规则列表
 * @param parent 父 AVP（通常是 20001/20002/20003）
 * @return 始终返回 0
 */
static int add_tft_to_ground_list(struct msg *parent);

/**
 * @brief 添加 TFTtoAircraft-List (20005) —— 机→地流量过滤规则列表
 * @param parent 父 AVP
 * @return 始终返回 0
 */
static int add_tft_to_aircraft_list(struct msg *parent);

/**
 * @brief 添加 NAPT-List (20006) —— 端口映射规则列表
 * @param parent 父 AVP
 * @return 始终返回 0
 */
static int add_napt_list(struct msg *parent);

/**
 * @brief 添加 DLM-Info (20008) —— 数据链路模块完整状态（最复杂嵌套）
 * @param parent 父 AVP（通常是 Communication-Answer-Parameters）
 * @param dlm    指向 dlm_info_t 结构体的指针（可为 NULL 表示无 DLM）
 * @return 始终返回 0
 */
int add_dlm_info(struct avp *parent, const dlm_info_t *dlm);

/**
 * @brief 添加 CDRs-Active (20012) —— 当前活跃的计费记录列表
 * @param msg   目标消息
 * @param list  cdr_item_t 数组
 * @param n     数组元素个数
 * @return 始终返回 0
 */
int add_cdrs_active(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Finished (20013) —— 已结束的计费记录
 * @param msg   目标消息
 * @param list  cdr_item_t 数组
 * @param n     数组元素个数
 * @return 始终返回 0
 */
int add_cdrs_finished(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Forwarded (20014) —— 已成功转发给计费中心的 CDR
 * @param msg   目标消息
 * @param list  cdr_item_t 数组
 * @param n     数组元素个数
 * @return 始终返回 0
 */
int add_cdrs_forwarded(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Unknown (20015) —— 服务端不认识的 CDR-ID 列表
 * @param msg  目标消息
 * @param ids  CDR-ID 字符串数组
 * @param n    数组元素个数
 * @return 始终返回 0
 */
int add_cdrs_unknown(struct msg *msg, const char *ids[], size_t n);

/**
 * @brief 添加 CDRs-Updated (20016) —— CDR 记录更新通知（Start/Stop 配对）
 * @param msg   目标消息
 * @param pairs cdr_start_stop_t 数组
 * @param n     数组元素个数
 * @return 始终返回 0
 */
int add_cdrs_updated(struct msg *msg, const cdr_start_stop_t pairs[], size_t n);

#endif /* MAGIC_GROUP_AVP_HELPERS_H */