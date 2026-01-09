/**
 * @file magic_policy.h
 * @brief MAGIC Policy Engine (v2.0 增强版)
 * @description 支持动态流量分类、链路切换防抖动、延迟限制
 */

#ifndef MAGIC_POLICY_H
#define MAGIC_POLICY_H

#include "magic_config.h"
#include "magic_session.h"
#include <stdbool.h>

/* 前向声明 */
struct MagicLmiContext;

/*===========================================================================
 * 策略决策请求
 *===========================================================================*/

typedef struct {
  char client_id[MAX_ID_LEN];     ///< 客户端 ID (如 IMSI 或飞机注册号)。
  char profile_name[MAX_ID_LEN];  ///< 来自 magic.conf 的 PROFILE_NAME。
  uint32_t requested_bw_kbps;     ///< 请求的最大带宽 (kbps)。
  uint32_t requested_ret_bw_kbps; ///< 请求的最大上行带宽 (kbps)。
  uint32_t required_bw_kbps;      ///< 最小保证带宽 (kbps)。
  uint32_t required_ret_bw_kbps;  ///< 最小保证上行带宽 (kbps)。
  uint8_t priority_class; ///< 来自 magic.conf 的 PRIORITY_CLASS (1-10)。
  uint8_t qos_level;      ///< 来自 magic.conf 的 QOS_LEVEL (0-7)。
  uint8_t traffic_class; ///< 内部使用的流量类别索引 (已废弃，建议使用
                         ///< traffic_class_id)。
  char flight_phase[32]; ///< 飞行阶段字符串 (如 "Takeoff", "Cruise")。

  /* 链路回退机制支持字段 */
  int exclude_link_count; ///< 需要排除的链路数量 (上次尝试失败的链路)。
  char exclude_links[4][MAX_ID_LEN]; ///< 需要排除的链路列表（最多4个）。

  /* v2.0 新增: 链路切换防抖动相关 */
  char current_link_id[MAX_ID_LEN]; ///< 当前使用的链路 ID (用于切换判断)。
  uint32_t current_bw_percent;      ///< 当前链路可用带宽百分比 (0-100)。
  uint32_t new_link_bw_percent;     ///< 新链路可用带宽百分比 (0-100)。

  /* v2.0 新增: ADIF 飞机位置信息 (用于覆盖范围检查) */
  double aircraft_lat; ///< 飞机纬度 (度, -90 to 90), 0.0=未提供。
  double aircraft_lon; ///< 飞机经度 (度, -180 to 180), 0.0=未提供。
  double aircraft_alt; ///< 飞机高度 (米), 0.0=未提供。
  bool on_ground;      ///< Weight on Wheels 状态 (true=地面, false=空中)。
  bool has_adif_data;  ///< ADIF 数据有效标志。
} PolicyRequest;

/*===========================================================================
 * 策略决策响应
 *===========================================================================*/

typedef struct {
  bool success;                      ///< 策略决策是否成功。
  char selected_link_id[MAX_ID_LEN]; ///< 选择的链路 ID。
  uint32_t granted_bw_kbps;          ///< 授权的下行带宽 (kbps)。
  uint32_t granted_ret_bw_kbps;      ///< 授权的上行带宽 (kbps)。
  uint8_t qos_level;                 ///< 最终确定的 QoS 级别。
  char reason[128];                  ///< 成功或失败的原因描述。

  /* v2.0 新增: 动态分类结果 */
  char matched_traffic_class[MAX_ID_LEN]; ///< 匹配的流量类别 ID。
} PolicyResponse;

/*===========================================================================
 * 策略引擎上下文
 *===========================================================================*/

typedef struct {
  MagicConfig *config;             ///< 全局配置指针。
  bool initialized;                ///< 策略引擎初始化状态。
  struct MagicLmiContext *lmi_ctx; ///< v2.1: 指向 MagicLmiContext
                                   ///< 的指针，用于获取实时链路负载信息。
} PolicyContext;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化策略引擎。
 * @details 将全局配置绑定到策略上下文，并初始化内部状态。
 *
 * @param ctx 策略上下文指针。
 * @param config 全局配置指针。
 * @return int 成功返回 0，失败返回 -1。
 */
int magic_policy_init(PolicyContext *ctx, MagicConfig *config);

/**
 * @brief 执行策略决策（链路选择）。
 * @details 根据请求的参数 (流量类别、带宽需求、位置等) 和当前系统状态
 * (链路状态、覆盖范围)， 选择最优的通信链路。
 *
 * @param ctx 策略上下文指针。
 * @param req 策略决策请求。
 * @param resp [out] 策略决策响应。
 * @return int 成功作出决策返回 0，无法找到合适链路返回 -1。
 */
int magic_policy_select_path(PolicyContext *ctx, const PolicyRequest *req,
                             PolicyResponse *resp);

/**
 * @brief 清理策略引擎。
 * @details 释放策略引擎占用的资源 (如果有)。
 *
 * @param ctx 策略上下文指针。
 */
void magic_policy_cleanup(PolicyContext *ctx);

/*===========================================================================
 * v2.0 新增 API 函数
 *===========================================================================*/

/**
 * @brief 动态流量分类
 * @description 根据客户端属性 (priority_class, qos_level, profile_name)
 *              匹配 TrafficClassDefinitions 中定义的流量类别
 * @param policy 中央策略配置指针
 * @param priority_class 客户端优先级类 (来自 magic.conf)
 * @param qos_level 客户端 QoS 级别 (来自 magic.conf)
 * @param profile_name 配置文件名称 (来自 magic.conf)
 * @return 匹配的流量类别 ID，未匹配返回 "BEST_EFFORT"
 */
const char *magic_policy_classify_traffic(const CentralPolicyProfile *policy,
                                          uint8_t priority_class,
                                          uint8_t qos_level,
                                          const char *profile_name);

/**
 * @brief 检查是否满足链路切换条件（防抖动）
 * @description 根据 SwitchingPolicy 配置检查最小驻留时间和迟滞百分比
 * @param policy 中央策略配置指针
 * @param session 客户端会话指针
 * @param new_link_id 目标链路 ID
 * @param new_bw_percent 新链路可用带宽百分比 (0-100)
 * @return true=允许切换, false=不满足切换条件
 */
bool magic_policy_can_switch_link(const CentralPolicyProfile *policy,
                                  const ClientSession *session,
                                  const char *new_link_id,
                                  uint32_t new_bw_percent);

/**
 * @brief 简单通配符匹配
 * @description 自定义实现，不依赖 fnmatch()，支持 * 和 ? 通配符
 * @param pattern 匹配模式 (如 "*maint*", "voice_?")
 * @param str 待匹配字符串
 * @return true=匹配成功, false=不匹配
 */
bool magic_policy_wildcard_match(const char *pattern, const char *str);

/**
 * @brief 检查飞机是否在 DLM 覆盖范围内
 * @description 根据 DLM 的 CoverageConfig 检查飞机当前位置是否在覆盖区域
 * @param coverage DLM 的覆盖范围配置指针
 * @param aircraft_lat 飞机纬度 (度, -90 to 90)
 * @param aircraft_lon 飞机经度 (度, -180 to 180)
 * @param aircraft_alt_m 飞机高度 (米)
 * @return true=在覆盖范围内, false=不在覆盖范围或配置未启用
 */
bool magic_policy_check_coverage(const CoverageConfig *coverage,
                                 double aircraft_lat, double aircraft_lon,
                                 double aircraft_alt_m);

#endif /* MAGIC_POLICY_H */
