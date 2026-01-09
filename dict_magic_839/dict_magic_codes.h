/**
 * @file dict_magic_codes.h
 * @brief MAGIC (ARINC 839-2014) Diameter Status Code Definitions
 * @description 定义所有 MAGIC 协议使用的 Diameter Result-Code 和
 * MAGIC-Status-Code 常量
 * @standard ARINC 839-2014, Section 1.3.2 (Page 96-98)
 * @date 2025-12-16
 */

#ifndef _DICT_MAGIC_CODES_H
#define _DICT_MAGIC_CODES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 1. 标准 Diameter Result-Code (RFC 6733)
 *    AVP Code: 268 (Base Diameter Protocol)
 *    用于 Diameter Answer 消息中的 Result-Code AVP
 *
 *    注意：Result-Code 和 MAGIC-Status-Code 是两个独立的 AVP，
 *         它们的数值空间是独立的。在协议消息中不会冲突。
 *         但在 C 代码中需要避免宏定义冲突。
 *===========================================================================*/

/* 成功类 (2xxx) */
#define DIAMETER_SUCCESS 2001 ///< 请求成功完成。

/* 协议错误类 (3xxx) */
#define DIAMETER_COMMAND_UNSUPPORTED 3001     ///< MAGIC 服务器不支持该命令。
#define DIAMETER_UNABLE_TO_DELIVER 3002       ///< 无法送达目标节点。
#define DIAMETER_REALM_NOT_SERVED 3003        ///< Realm 不被服务。
#define DIAMETER_TOO_BUSY 3004                ///< 服务器太忙，无法处理。
#define DIAMETER_LOOP_DETECTED 3005           ///< 检测到消息循环。
#define DIAMETER_REDIRECT_INDICATION 3006     ///< 重定向指示。
#define DIAMETER_APPLICATION_UNSUPPORTED 3007 ///< 应用不受支持。

/* 临时失败类 (4xxx) */
#define DIAMETER_AUTHENTICATION_REJECTED 4001 ///< 认证被拒绝。

/* 永久失败类 (5xxx) */
#define DIAMETER_AVP_UNSUPPORTED 5001        ///< 命令中包含不支持的 AVP。
#define DIAMETER_UNKNOWN_SESSION_ID 5002     ///< 未找到对应的 Session-Id。
#define DIAMETER_AUTHORIZATION_REJECTED 5003 ///< 授权被拒绝。
#define DIAMETER_INVALID_AVP_VALUE 5004      ///< AVP 包含的值超出了定义范围。
#define DIAMETER_MISSING_AVP 5005            ///< 命令中缺少必要的 AVP。
#define DIAMETER_RESOURCES_EXCEEDED 5006     ///< 资源超限。
#define DIAMETER_CONTRADICTING_AVPS 5007     ///< AVP 之间存在冲突。
#define DIAMETER_AVP_NOT_ALLOWED 5008        ///< 该命令中不允许出现此 AVP。
#define DIAMETER_AVP_OCCURS_TOO_MANY_TIMES 5009 ///< AVP 出现次数过多。
#define DIAMETER_NO_COMMON_APPLICATION 5010     ///< 没有共同的应用。
#define DIAMETER_UNSUPPORTED_VERSION 5011       ///< 不支持的版本。
#define DIAMETER_UNABLE_TO_COMPLY 5012          ///< 无法遵从请求。
#define DIAMETER_INVALID_BIT_IN_HEADER 5013     ///< 消息头中的位无效。
#define DIAMETER_INVALID_AVP_LENGTH 5014        ///< AVP 长度无效。
#define DIAMETER_INVALID_MESSAGE_LENGTH 5015    ///< 消息长度无效。
#define DIAMETER_INVALID_AVP_BIT_COMBO 5016     ///< AVP 标志位组合无效。
#define DIAMETER_NO_COMMON_SECURITY 5017        ///< 没有共同的安全机制。

/*===========================================================================
 * 2. MAGIC 专有状态码 (MAGIC-Status-Code AVP)
 *    AVP Code: 10053 (Vendor-Specific, Vendor-ID: 13712)
 *    用于更精细地描述航空空地通信场景下的错误和信息
 *
 *    注意：MAGIC-Status-Code 的数值范围与 Result-Code 重叠，
 *         但它们是不同的 AVP，在实际协议消息中不会冲突。
 *         MAGIC-Status-Code 提供比 Result-Code 更详细的错误信息。
 *
 *         在使用时：
 *         - Result-Code AVP (268) 使用 DIAMETER_* 常量
 *         - MAGIC-Status-Code AVP (10053) 使用 MAGIC_* 常量
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * 2.1 错误类 (Error Codes, 1xxx - 3xxx)
 *---------------------------------------------------------------------------*/

/* 参数错误 (1000-1037) */
/* 参数错误 (1000-1037) */
#define MAGIC_ERROR_MISSING_AVP 1000           ///< 缺少重要的 AVP。
#define MAGIC_ERROR_AUTHENTICATION_FAILED 1001 ///< 客户端用户名或密码错误。
#define MAGIC_ERROR_UNKNOWN_SESSION 1002       ///< Session-Id 不存在或已超时。
#define MAGIC_ERROR_MAGIC_NOT_RUNNING 1003     ///< MAGIC 服务已终止或未初始化。
#define MAGIC_ERROR_ENUMERATION_OUT_OF_RANGE 1004 ///< 枚举值越界。
#define MAGIC_ERROR_MALFORMED_LINK_STRING 1005    ///< LINK 字符串格式错误。
#define MAGIC_ERROR_MALFORMED_QOS_STRING 1006     ///< QoS 字符串格式错误。
#define MAGIC_ERROR_MALFORMED_TFT_STRING 1007     ///< TFT 字符串格式错误。
#define MAGIC_ERROR_MALFORMED_DATA_LINK_STRING                                 \
  1008 ///< DATA-Link 字符串格式错误。
#define MAGIC_ERROR_MALFORMED_CHANNEL_STATUS_STRING                            \
  1009                                         ///< 频道状态字符串格式错误。
#define MAGIC_ERROR_ADD_SESSION_FAILED 1010    ///< 无法创建新会话。
#define MAGIC_ERROR_MODIFY_SESSION_FAILED 1011 ///< 无法修改现有会话。
#define MAGIC_ERROR_OPEN_LINK_FAILED_DEPRECATED                                \
  1012 ///< 打开链路失败（已弃用）。
#define MAGIC_ERROR_NO_ENTRY_IN_BANDWIDTHTABLE 1013 ///< 带宽表中无客户端条目。
#define MAGIC_ERROR_NO_OPEN_LINKS 1014              ///< 没有可用的打开链路。
#define MAGIC_ERROR_AMBIGUOUS_BANDWIDTHTABLE_PERCENTAGE                        \
  1015                                       ///< 带宽分配比例含糊不清。
#define MAGIC_ERROR_NO_FREE_BANDWIDTH 1016   ///< 系统无剩余带宽。
#define MAGIC_ERROR_MISSING_PARAMETER 1017   ///< 缺少必要的会话参数。
#define MAGIC_ERROR_CLIENT_REGISTRATION 1018 ///< 新客户端开启会话（内部信息）。
#define MAGIC_ERROR_CLIENT_UNREGISTRATION 1019 ///< 客户端关闭最后一个会话。
#define MAGIC_ERROR_ILLEGAL_FLIGHT_PHASE 1020  ///< 当前飞行阶段不允许建立会话。
#define MAGIC_ERROR_ILLEGAL_PARAMETER 1021     ///< 请求参数违反预设规则。
#define MAGIC_ERROR_STATUS_ACCESS_DENIED 1022  ///< 无权获取状态信息。
#define MAGIC_ERROR_ACCOUNTING_ACCESS_DENIED 1023  ///< 无权访问计费数据。
#define MAGIC_ERROR_ACCOUNTING_CONTROL_DENIED 1024 ///< 无权控制计费记录。
#define MAGIC_ERROR_MAGIC_SHUTDOWN 1025            ///< MAGIC 服务正在关闭。
#define MAGIC_ERROR_ACCOUNTING_INVALID_CDRID 1026  ///< 提供的 CDR ID 无效。
#define MAGIC_ERROR_MALFORMED_CDR_STRING 1027      ///< CDR 字符串格式错误。
#define MAGIC_ERROR_SESSION_IS_BLOCKED_DEPRECATED                              \
  1028                                         ///< 会话被阻塞（已弃用）。
#define MAGIC_ERROR_MALFORMED_NAPT_STRING 1029 ///< NAPT 字符串格式错误。
#define MAGIC_ERROR_MALFORMED_AIRPORTLIST_STRING                               \
  1030 ///< 机场列表字符串格式错误。
#define MAGIC_ERROR_MALFORMED_FLIGHTPHASELIST_STRING                           \
  1031 ///< 飞行阶段列表格式错误。
#define MAGIC_ERROR_MALFORMED_ALTITUDELIST_STRING                              \
  1032                                            ///< 高度列表字符串格式错误。
#define MAGIC_ERROR_MALFORMED_DLMLIST_STRING 1033 ///< DLM 列表字符串格式错误。
#define MAGIC_ERROR_MALFORMED_PRIO_CLASS_STRING                                \
  1034                                          ///< 优先级类别字符串格式错误。
#define MAGIC_ERROR_PROFILE_DOES_NOT_EXIST 1035 ///< 配置文件名称不存在。
#define MAGIC_ERROR_TFT_INVALID 1036            ///< TFT 参数无效。
#define MAGIC_ERROR_NAPT_INVALID 1037           ///< NAPT 参数无效。

/* 信息/成功类 (1038-1048) */
/* 信息/成功类 (1038-1048) */
#define MAGIC_INFO_SET_LINK_QOS 1038         ///< 链路 QoS 已成功设置。
#define MAGIC_INFO_REMOVE_LINK_QOS 1039      ///< 链路 QoS 已成功移除。
#define MAGIC_INFO_SET_DEFAULTROUTE 1040     ///< 已设置默认路由。
#define MAGIC_INFO_REMOVE_DEFAULTROUTE 1041  ///< 已移除默认路由。
#define MAGIC_INFO_SET_POLICYROUTING 1042    ///< 已应用策略路由规则。
#define MAGIC_INFO_REMOVE_POLICYROUTING 1043 ///< 已移除策略路由规则。
#define MAGIC_INFO_SET_NAPT 1044             ///< 已应用 NAPT 规则。
#define MAGIC_INFO_REMOVE_NAPT 1045          ///< 已移除 NAPT 规则。
#define MAGIC_INFO_SET_SESSION_QOS 1046      ///< 会话级 QoS 已生效。
#define MAGIC_INFO_REMOVE_SESSION_QOS 1047   ///< 会话级 QoS 已移除。
#define MAGIC_INFO_INTERNAL_ERROR 1048       ///< 非致命内部错误。

/* 系统错误 (2000-2010) */
#define MAGIC_ERROR_AIS_INTERRUPTED 2000         ///< AIS 域服务中断。
#define MAGIC_ERROR_FLIGHT_PHASE_ERROR 2001      ///< 飞行阶段数据获取错误。
#define MAGIC_ERROR_AIRPORT_ERROR 2002           ///< 机场数据获取错误。
#define MAGIC_ERROR_ALTITUDE_ERROR 2003          ///< 高度数据获取错误。
#define MAGIC_ERROR_NO_BANDWIDTH_ASSIGNMENT 2004 ///< 无法执行带宽分配逻辑。
#define MAGIC_ERROR_REQUEST_NOT_PROCESSED 2005   ///< 请求未被处理。
#define MAGIC_ERROR_OPEN_LINK_FAILED 2006        ///< 打开链路失败。
#define MAGIC_ERROR_LINK_ERROR 2007              ///< 通用链路错误。
#define MAGIC_ERROR_CLOSE_LINK_FAILED 2008       ///< 关闭链路操作失败。
#define MAGIC_ERROR_MAGIC_FAILURE 2009           ///< MAGIC 内部核心功能故障。
#define MAGIC_INFO_FORCED_REROUTING 2010         ///< MAGIC 强制链路重新路由。

/* 未知错误 (3000-3001) */
#define MAGIC_ERROR_UNKNOWN_ISSUE 3000        ///< 未知问题/未定义的错误。
#define MAGIC_ERROR_AVIONICSDATA_MISSING 3001 ///< 关键航电参数数据缺失。

/*---------------------------------------------------------------------------
 * 2.2 状态码辅助宏
 *---------------------------------------------------------------------------*/

/**
 * @brief 判断是否为成功状态码。
 *
 * @param code 待检查的状态码。
 * @return 真值(非0)表示成功，假值(0)表示失败。
 */
#define IS_DIAMETER_SUCCESS(code) ((code) >= 2000 && (code) < 3000)

/**
 * @brief 判断是否为 MAGIC 错误码。
 *
 * @param code 待检查的 MAGIC 状态码。
 * @return 真值(非0)表示是错误码。
 */
#define IS_MAGIC_ERROR(code)                                                   \
  (((code) >= 1000 && (code) <= 1037) || ((code) >= 2000 && (code) <= 2009) || \
   ((code) >= 3000 && (code) <= 3001))

/**
 * @brief 判断是否为 MAGIC 信息码。
 *
 * @param code 待检查的 MAGIC 状态码。
 * @return 真值(非0)表示是信息码。
 */
#define IS_MAGIC_INFO(code) ((code) >= 1038 && (code) <= 1048)

/**
 * @brief 将 MAGIC-Status-Code (AVP 10053) 转换为可读字符串。
 * @details 遍历所有定义的 MAGIC 专有状态码，返回对应的宏名称字符串。
 *
 * @param code MAGIC 状态码 (1000-3001)。
 * @return const char* 状态码的字符串表示，用于日志和调试。
 *
 * @note 此函数仅处理 MAGIC-Status-Code AVP (10053)。
 *       对于 Diameter Result-Code AVP (268)，请使用 freeDiameter 的标准函数。
 * @warning 如果传入未定义的代码，将返回 "UNKNOWN_MAGIC_STATUS"。
 */
const char *magic_status_code_str(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif /* _DICT_MAGIC_CODES_H */
