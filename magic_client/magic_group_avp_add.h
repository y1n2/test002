/**
 * @file magic_group_avp_add.h
 * @brief ARINC 839-2014 MAGIC Diameter 协议 Grouped AVP 构造助手。
 * @details 提供了向 Diameter 消息中添加复杂分组 AVP (Grouped AVP) 的辅助函数。
 *          涵盖了认证凭据、通信请求/应答参数、TFT/NAPT 规则列表以及复杂的 DLM
 * 状态信息。
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
#ifndef MAGIC_VENDOR_ID
#define MAGIC_VENDOR_ID 13712
#endif

/* ================================================================== */
/* 必需的业务结构体定义（用于 CDR 和 DLM 相关 AVP）                    */
/* ================================================================== */

/**
 * @brief 单条 CDR 记录项。
 * @details 用于 CDRs-Active, CDRs-Finished 和 CDRs-Forwarded AVP。
 */
typedef struct {
  const char *id; ///< CDR 唯一标识符，不能为空。
  const char
      *content; ///< 完整的 CDR 内容（通常为 JSON 格式字符串），可为 NULL。
} cdr_item_t;

/**
 * @brief CDR 更新配对。
 * @details 用于 CDRs-Updated AVP，记录从旧 CDR 到新 CDR 的变更。
 */
typedef struct {
  const char *stopped; ///< 已结束的旧 CDR ID，可为 NULL。
  const char *started; ///< 新开始的 CDR ID，可为 NULL。
} cdr_start_stop_t;

/**
 * @brief 物理链路状态。
 * @details 描述 DLM 下属单一物理连接的实时状态，用于 Link-Status-Group AVP。
 */
typedef struct {
  uint32_t number;          ///< 链路编号（如 1, 2, 3）。
  uint32_t available;       ///< 是否可用：1 为可用，0 为不可用。
  uint32_t qos_level;       ///< 当前承载的 QoS 等级。
  uint32_t conn_status;     ///< 物理连接状态（具体语义见协议定义）。
  uint32_t login_status;    ///< 鉴权/登录状态。
  uint64_t max_bw;          ///< 理论最大下行带宽 (bit/s)。
  uint64_t max_return_bw;   ///< 理论最大上行带宽 (bit/s)。
  uint64_t alloc_bw;        ///< 当前已分配的下行带宽。
  uint64_t alloc_return_bw; ///< 当前已分配的上行带宽。
  const char *error_str;    ///< 链路相关的错误或状态描述字符串，可为 NULL。
} link_status_t;

/**
 * @brief 数据链路模块 (DLM) 完整信息。
 * @details 管理和通告单个 DLM 实例（如 SATCOM, Cellular）的能力和状态。
 */
typedef struct {
  const char *name;       ///< DLM 名称，例如 "SATCOM-1"。
  uint32_t available;     ///< DLM 整体是否可用。
  uint32_t max_links;     ///< 模块支持的最大并行链路数。
  uint64_t max_bw;        ///< 模块最大下行带宽。
  uint64_t max_return_bw; ///< 模块最大上行带宽。

  uint32_t allocated_links;     ///< 当前已分配（正在使用）的链路数。
  uint64_t allocated_bw;        ///< 当前总已分配下行带宽。
  uint64_t allocated_return_bw; ///< 当前总已分配上行带宽。

  int qos_count;          ///< 支持的 QoS 等级数量。
  uint32_t qos_levels[3]; ///< 支持的 QoS 等级列表。

  int link_count;       ///< 物理链路数量。
  link_status_t *links; ///< 指向 link_status_t 数组的指针。
} dlm_info_t;

/* ================================================================== */
/* 函数声明                                                           */
/* ================================================================== */

/**
 * @brief 添加 Client-Credentials (20019) AVP 到消息。
 * @details 包含 User-Name, Client-Password 和可选的 Server-Password。
 * @param msg 目标 Diameter 消息对象。
 * @return int 成功返回 0，由于缺少必要配置返回 -1。
 */
int add_client_credentials(struct msg *msg);

/**
 * @brief 添加 Communication-Request-Parameters (20001) AVP。
 * @details 客户端在发起资源连接或修改请求时必须包含的核心参数组。
 * @param msg 目标消息。
 * @return int 成功返回 0，失败（如缺少 Profile-Name）返回 -1。
 */
int add_comm_req_params(struct msg *msg);

/**
 * @brief 添加 Communication-Answer-Parameters (20002) AVP。
 * @details 服务端对通信请求的回应，包含分配的带宽、链路 ID 等。
 * @param msg 目标消息。
 * @return int 始终返回 0。
 */
int add_comm_ans_params(struct msg *msg);

/**
 * @brief 添加 Communication-Report-Parameters (20003) AVP。
 * @details 用于上报当前会话的各种统计和状态参数。
 * @param msg 目标消息。
 * @return int 始终返回 0。
 */
int add_comm_report_params(struct msg *msg);

/**
 * @brief 添加 DLM-Info (20008) 分组 AVP 到父节点。
 * @details 这是一个深层嵌套的分组 AVP，包含 QoS 和 Link Status。
 * @param parent 父节点 AVP 对象指针。
 * @param dlm 指向包含 DLM 详情的 dlm_info_t 结构体。
 * @return int 成功返回 0。
 */
int add_dlm_info(struct avp *parent, const dlm_info_t *dlm);

/**
 * @brief 添加 CDRs-Active (20012) AVP。
 * @param msg 目标消息。
 * @param list cdr_item_t 记录数组。
 * @param n 数组长度。
 * @return int 成功返回 0。
 */
int add_cdrs_active(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Finished (20013) AVP。
 * @param msg 目标消息。
 * @param list 记录数组。
 * @param n 数组长度。
 * @return int 始终返回 0。
 */
int add_cdrs_finished(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Forwarded (20014) AVP。
 * @param msg 目标消息。
 * @param list 记录数组。
 * @param n 数组长度。
 * @return int 始终返回 0。
 */
int add_cdrs_forwarded(struct msg *msg, const cdr_item_t list[], size_t n);

/**
 * @brief 添加 CDRs-Unknown (20015) AVP。
 * @param msg 目标消息。
 * @param ids CDR ID 字符串数组。
 * @param n 数组长度。
 * @return int 始终返回 0。
 */
int add_cdrs_unknown(struct msg *msg, const char *ids[], size_t n);

/**
 * @brief 添加 CDRs-Updated (20016) AVP。
 * @param msg 目标消息。
 * @param pairs cdr_start_stop_t 配对数组。
 * @param n 数组长度。
 * @return int 始终返回 0。
 */
int add_cdrs_updated(struct msg *msg, const cdr_start_stop_t pairs[], size_t n);

#endif /* MAGIC_GROUP_AVP_HELPERS_H */