/**
 * @file magic_policy.c
 * @brief MAGIC Policy Engine - Production Implementation (v2.0)
 * @description 完整的策略引擎，支持多飞行阶段、动态流量分类、
 *              链路切换防抖动、延迟限制
 */

#include "magic_policy.h" // 包含策略引擎头文件，定义了策略相关的结构体和函数声明
#include "magic_lmi.h"    // 包含LMI头文件，用于访问DLM客户端信息
#include <ctype.h>        // 包含字符处理函数，如 tolower
#include <freeDiameter/extension.h> // 包含freeDiameter扩展库，用于Diameter协议扩展功能
#include <string.h>                 // 包含字符串处理函数，如strcmp、strncpy等

/*===========================================================================
 * v2.0 新增: 自定义通配符匹配实现
 * 支持 * (匹配任意字符序列) 和 ? (匹配单个字符)
 * 不依赖 fnmatch()，提高移植性
 *===========================================================================*/

/**
 * @brief 简单通配符匹配 (递归实现)。
 * @details 自定义实现，不依赖 system fnmatch()。支持 '*' (匹配任意字符序列) 和
 * '?' (匹配单个字符)。 用于策略规则中的模式匹配 (如 profile_name 匹配)。
 *
 * @param pattern 匹配模式 (如 "*video*", "voice_?").
 * @param str 待匹配的字符串。
 * @return true 匹配成功。
 * @return false 匹配失败。
 */
bool magic_policy_wildcard_match(const char *pattern, const char *str) {
  if (!pattern || !str)
    return false;

  while (*pattern && *str) {
    if (*pattern == '*') {
      /* '*' 匹配任意字符序列 (包括空序列) */
      pattern++;
      if (*pattern == '\0') {
        /* 模式以 * 结尾，匹配成功 */
        return true;
      }
      /* 尝试在 str 的每个位置继续匹配 */
      while (*str) {
        if (magic_policy_wildcard_match(pattern, str)) {
          return true;
        }
        str++;
      }
      /* 尝试空匹配 (str 为空) */
      return magic_policy_wildcard_match(pattern, str);
    } else if (*pattern == '?') {
      /* '?' 匹配任意单个字符 */
      pattern++;
      str++;
    } else {
      /* 普通字符: 忽略大小写比较 */
      if (tolower((unsigned char)*pattern) != tolower((unsigned char)*str)) {
        return false;
      }
      pattern++;
      str++;
    }
  }

  /* 处理尾部的 '*' */
  while (*pattern == '*') {
    pattern++;
  }

  /* 两个字符串都到达结尾才算完全匹配 */
  return (*pattern == '\0' && *str == '\0');
}

/*===========================================================================
 * v2.0 新增: 动态流量分类
 *===========================================================================*/

/**
 * @brief 动态流量分类。
 * @details 根据客户端的属性 (优先级类、QoS 级别、Profile 名称) 匹配 magic.conf
 * 中定义的 TrafficClass。 匹配顺序:
 *          1. 优先级类 (Priority Class)
 *          2. QoS 级别 (QoS Level)
 *          3. Profile 名称模式 (Pattern Match)
 *          4. 默认分类 (is_default)
 *
 * @param policy 中央策略配置指针。
 * @param priority_class 客户端优先级类别 (1-10)。
 * @param qos_level 客户端 QoS 级别 (0-7)。
 * @param profile_name 客户端 Profile 名称。
 * @return const char* 匹配到的流量类别 ID (如 "VIDEO",
 * "VOICE")，若无匹配则返回默认值 "BEST_EFFORT"。
 */
const char *magic_policy_classify_traffic(const CentralPolicyProfile *policy,
                                          uint8_t priority_class,
                                          uint8_t qos_level,
                                          const char *profile_name) {
  static const char *default_class = "BEST_EFFORT";
  const char *matched_default = default_class;

  if (!policy || policy->num_traffic_class_defs == 0) {
    /* 没有流量类别定义，返回默认 */
    return default_class;
  }

  fd_log_debug("[app_magic] Classifying traffic: prio=%u, qos=%u, profile=%s",
               priority_class, qos_level,
               profile_name ? profile_name : "(null)");

  for (uint32_t i = 0; i < policy->num_traffic_class_defs; i++) {
    const TrafficClassDefinition *def = &policy->traffic_class_defs[i];

    /* 记录默认类以备最后使用 */
    if (def->is_default) {
      matched_default = def->traffic_class_id;
      continue; /* 默认类最后匹配 */
    }

    /* 优先级类匹配 */
    if (def->has_priority_class_match &&
        def->match_priority_class == priority_class) {
      fd_log_debug("[app_magic]   Matched by PriorityClass: %s",
                   def->traffic_class_id);
      return def->traffic_class_id;
    }

    /* QoS 级别匹配 */
    if (def->has_qos_level_match && def->match_qos_level == qos_level) {
      fd_log_debug("[app_magic]   Matched by QoSLevel: %s",
                   def->traffic_class_id);
      return def->traffic_class_id;
    }

    /* Profile 名称模式匹配 */
    if (profile_name && def->num_patterns > 0) {
      for (uint32_t p = 0; p < def->num_patterns; p++) {
        if (magic_policy_wildcard_match(def->match_patterns[p], profile_name)) {
          fd_log_debug("[app_magic]   Matched by pattern '%s': %s",
                       def->match_patterns[p], def->traffic_class_id);
          return def->traffic_class_id;
        }
      }
    }
  }

  fd_log_debug("[app_magic]   No match, using default: %s", matched_default);
  return matched_default;
}

/*===========================================================================
 * v2.0 新增: 地理覆盖检查
 *===========================================================================*/

/**
 * @brief 检查飞机是否在 DLM 覆盖范围内。
 * @details 根据 DLM 配置的覆盖范围 (经纬度、高度) 和实时的 ADIF 飞机位置数据，
 *          判断该链路是否可用。常用于 GEO 卫星或地面站链路的适用性检查。
 *
 * @param coverage DLM 的覆盖范围配置指针。
 * @param aircraft_lat 飞机纬度 (度, -90 to 90)。
 * @param aircraft_lon 飞机经度 (度, -180 to 180)。
 * @param aircraft_alt_m 飞机高度 (米)。
 * @return true 在覆盖范围内或未启用覆盖检查。
 * @return false 不在覆盖范围内。
 */
bool magic_policy_check_coverage(const CoverageConfig *coverage,
                                 double aircraft_lat, double aircraft_lon,
                                 double aircraft_alt_m) {
  /* 参数校验 */
  if (!coverage) {
    fd_log_debug("[app_magic] Coverage check: NULL coverage config");
    return true; /* 未提供配置时允许通过 */
  }

  /* 检查是否启用覆盖限制 */
  if (!coverage->enabled) {
    fd_log_debug("[app_magic] Coverage check: Disabled, allowing all");
    return true; /* 未启用覆盖限制时允许通过 */
  }

  /* 检查纬度范围 */
  if (aircraft_lat < coverage->min_latitude ||
      aircraft_lat > coverage->max_latitude) {
    fd_log_debug(
        "[app_magic] Coverage check: Latitude %.2f out of range [%.2f, %.2f]",
        aircraft_lat, coverage->min_latitude, coverage->max_latitude);
    return false;
  }

  /* 检查经度范围 */
  if (aircraft_lon < coverage->min_longitude ||
      aircraft_lon > coverage->max_longitude) {
    fd_log_debug(
        "[app_magic] Coverage check: Longitude %.2f out of range [%.2f, %.2f]",
        aircraft_lon, coverage->min_longitude, coverage->max_longitude);
    return false;
  }

  /* 检查高度范围 (将米转换为英尺: 1m ≈ 3.28084ft) */
  double aircraft_alt_ft = aircraft_alt_m * 3.28084;
  if (aircraft_alt_ft < coverage->min_altitude_ft ||
      aircraft_alt_ft > coverage->max_altitude_ft) {
    fd_log_debug("[app_magic] Coverage check: Altitude %.0fm (%.0fft) out of "
                 "range [%u, %u]ft",
                 aircraft_alt_m, aircraft_alt_ft, coverage->min_altitude_ft,
                 coverage->max_altitude_ft);
    return false;
  }

  fd_log_debug(
      "[app_magic] Coverage check: PASS (lat=%.2f, lon=%.2f, alt=%.0fm/%.0fft)",
      aircraft_lat, aircraft_lon, aircraft_alt_m, aircraft_alt_ft);
  return true;
}

/*===========================================================================
 * v2.0 新增: 链路切换防抖动
 *===========================================================================*/

/**
 * @brief 检查是否满足链路切换条件 (防抖动)。
 * @details 为了防止频繁的乒乓切换 (Ping-Pong Effect)，策略引擎应用迟滞
 * (Hysteresis) 和最小驻留时间 (Dwell Time) 机制。
 *          仅当新链路的质量显著优于当前链路 (超过迟滞阈值)
 * 且当前链路已使用足够长时间时，才允许切换。
 *
 * @param policy 中央策略配置指针。
 * @param session 客户端会话状态指针。
 * @param new_link_id 目标链路 ID。
 * @param new_bw_percent 新链路的可用带宽百分比 (作为质量指标)。
 * @return true 满足切换条件，允许切换。
 * @return false 不满足切换条件，保持当前链路。
 */
bool magic_policy_can_switch_link(const CentralPolicyProfile *policy,
                                  const ClientSession *session,
                                  const char *new_link_id,
                                  uint32_t new_bw_percent) {
  if (!policy || !session || !new_link_id) {
    return true; /* 参数无效时允许切换 */
  }

  const SwitchingPolicy *sw = &policy->switching_policy;
  time_t now = time(NULL);

  /* 如果是第一次分配链路，允许 */
  if (session->assigned_link_id[0] == '\0') {
    fd_log_debug("[app_magic] First link assignment, allow switch");
    return true;
  }

  /* 如果是同一链路，无需切换 */
  if (strcmp(session->assigned_link_id, new_link_id) == 0) {
    fd_log_debug("[app_magic] Same link, no switch needed");
    return false;
  }

  /* 检查最小驻留时间 */
  if (session->last_link_switch_time > 0) {
    time_t elapsed = now - session->last_link_switch_time;
    if (elapsed < (time_t)sw->min_dwell_time_sec) {
      fd_log_debug("[app_magic] Dwell time not met: %ld < %u sec, deny switch",
                   (long)elapsed, sw->min_dwell_time_sec);
      return false;
    }
  }

  /* 检查信号迟滞 (使用可用带宽百分比作为信号质量指标) */
  uint32_t current_bw = session->current_bw_percent;
  if (current_bw > 0 && new_bw_percent > 0) {
    /* 新链路必须比当前链路好 hysteresis_percentage 以上 */
    uint32_t threshold =
        current_bw + (current_bw * sw->hysteresis_percentage / 100);

    if (new_bw_percent < threshold) {
      fd_log_debug("[app_magic] Hysteresis not met: new=%u%% < threshold=%u%% "
                   "(current=%u%% + %u%%), deny switch",
                   new_bw_percent, threshold, current_bw,
                   sw->hysteresis_percentage);
      return false;
    }
  }

  fd_log_debug("[app_magic] Switch conditions met: %s -> %s",
               session->assigned_link_id, new_link_id);
  return true;
}

/*===========================================================================
 * 辅助函数：链路评分 (v2.0 增强版)
 *===========================================================================*/

/**
 * @brief 计算链路评分（分数越高越优先）
 *
 * 评分因素：
 * 1. 策略规则中的 ranking (ranking 越小分数越高)
 * 2. 带宽余量 (bandwidth headroom)
 * 3. 延迟 (latency)
 * 4. 成本指数 (cost)
 * 5. v2.0 新增: max_latency_ms 硬性限制
 */
/**
 * @brief 计算链路评分 (分数越高越优先)。
 * @details 综合考虑策略排名、带宽余量、延迟和成本等因素，为候选链路打分。
 *          评分规则:
 *          1. 策略排名 (Ranking): 权重最高，排名越靠前分数越高。
 *          2. 带宽余量 (Headroom): 满足请求带宽的前提下，余量越大分数越高 (每
 * 100kbps +1 分)。
 *          3. 延迟 (Latency): 低于 50ms 奖励，高于 500ms 惩罚。
 *          4. 链路类型 (Type): 卫星链路等高稳定性类型给予少量加分。
 *          5. 硬性限制 (Constraints): 超过最大延迟限制直接淘汰。
 *
 * @param dlm DLM 配置指针，包含链路的静态参数 (最大带宽、类型)。
 * @param pref 路径偏好配置指针，包含策略定义的排名和限制。
 * @param requested_bw 请求的带宽 (kbps)。
 * @return int 计算出的链路评分 (越高越好)，若被淘汰则返回极低负值。
 */
static int calculate_link_score(const DLMConfig *dlm,
                                const PathPreference *pref,
                                uint32_t requested_bw) {
  int score = 0; // 初始化链路评分变量为0，后续根据各种因素累加或扣减分数

  /* v2.0 新增: 检查 max_latency_ms 硬性限制 */
  if (pref->has_max_latency && dlm->latency_ms > pref->max_latency_ms) {
    fd_log_debug("[app_magic]     DLM %s: REJECTED (latency %u > max %u ms)",
                 dlm->dlm_name, dlm->latency_ms, pref->max_latency_ms);
    return -999999; /* 直接淘汰，返回极低分数 */
  }

  /* 策略排名权重（最高权重） */
  score +=
      (10 - pref->ranking) *
      1000; // 根据策略规则中的排名计算权重，排名越小（优先级越高），分数越高；例如ranking
            // 1得9000分，ranking 2得8000分

  /* 带宽余量权重（v2.0 使用 max_forward_bw_kbps） */
  if (dlm->max_forward_bw_kbps >=
      requested_bw) { // 检查链路的最大前向带宽是否满足请求带宽
    uint32_t headroom = dlm->max_forward_bw_kbps -
                        requested_bw; // 计算带宽余量，即最大速率减去请求带宽
    score += (headroom / 100); // 每100kbps余量加1分，鼓励选择有足够余量的链路
  } else {
    score -= 5000; // 如果带宽不足，严重扣分5000分，防止选择无法满足需求的链路
  }

  /* 延迟权重（延迟越低越好，v2.0 使用 latency_ms） */
  if (dlm->latency_ms < 50) {         // 如果延迟小于50ms，认为延迟很低
    score += 100;                     // 给予低延迟奖励，加100分
  } else if (dlm->latency_ms > 500) { // 如果延迟大于500ms，认为延迟很高
    score -= 50;                      // 给予高延迟惩罚，扣50分
  }

  /* v2.0 新增: DLM 类型评分（替代旧的 cost_index） */
  switch (dlm->dlm_type) {
  case DLM_TYPE_SATELLITE:
    score += 5; // 卫星链路：最稳定，加5分
    break;
  case DLM_TYPE_CELLULAR:
    score += 3; // 蜂窝链路：中等，加3分
    break;
  case DLM_TYPE_HYBRID:
    score += 4; // 混合链路：较好，加4分
    break;
  default:
    break; // 未知类型不加分
  }

  return score; // 返回计算出的链路评分
}

/*===========================================================================
 * 策略引擎核心实现
 *===========================================================================*/

/**
 * @brief 初始化策略引擎。
 * @details 绑定全局配置，清空上下文，并输出初始化日志。
 *
 * @param ctx 策略上下文指针。
 * @param config 全局配置指针。
 * @return int 成功返回 0，失败返回 -1 (参数为空)。
 */
int magic_policy_init(PolicyContext *ctx, MagicConfig *config) {
  if (!ctx || !config) { // 检查输入参数是否为空，如果为空则返回错误
    fd_log_error(
        "[app_magic] Policy init: NULL parameter"); // 记录错误日志，提示参数为空
    return -1;                                      // 返回错误码-1
  }

  memset(ctx, 0,
         sizeof(PolicyContext)); // 将策略上下文结构体清零，初始化所有成员为0
  ctx->config = config;          // 将传入的配置指针赋值给上下文的config成员
  ctx->initialized = true;       // 设置初始化标志为true，表示策略引擎已初始化

  fd_log_notice("[app_magic] ✓ Policy Engine Initialized (v2.0)");
  fd_log_notice("[app_magic]     DLMs: %u", config->num_dlm_configs);
  fd_log_notice("[app_magic]     Rulesets: %u", config->policy.num_rulesets);
  fd_log_notice("[app_magic]     Clients: %u", config->num_clients);

  return 0; // 返回成功码0
}

/**
 * @brief 执行策略决策 (核心函数)。
 * @details 根据客户端请求和当前环境状态，从可用链路中选择最优的一条。
 *          决策流程:
 *          1. 查找并验证客户端配置 (Client Profile)。
 *          2. 查找适用的策略规则集 (RuleSet)，通常基于飞行阶段。
 *          3. 进行动态流量分类，匹配具体的策略规则 (Policy Rule)。
 *          4. 遍历规则中的路径偏好 (Preferences)，对每条候选链路进行评分。
 *             - 检查是否被禁止 (PROHIBIT)
 *             - 检查是否在允许列表 (Allowed DLMs)
 *             - 检查链路状态 (Active)
 *             - 检查覆盖范围 (Coverage)
 *             - 检查飞行状态限制 (WoW)
 *             - 计算综合评分 (Score)
 *             - 应用负载均衡 (Load Balancing)
 *          5. 选择评分最高的链路并返回结果。
 *
 * @param ctx 策略上下文指针。
 * @param req 策略决策请求，包含客户端 ID、带宽需求、位置信息等。
 * @param resp [out] 策略决策响应，包含选择结果、授权参数和原因。
 * @return int 成功作出决策返回 0，失败 (无可用链路或错误) 返回 -1。
 */
int magic_policy_select_path(PolicyContext *ctx, const PolicyRequest *req,
                             PolicyResponse *resp) {
  if (!ctx || !req ||
      !resp) { // 检查输入参数是否为空，如果有任何一个为空则返回错误
    return -1; // 返回错误码-1
  }

  memset(resp, 0,
         sizeof(PolicyResponse)); // 将响应结构体清零，初始化所有成员为0

  fd_log_debug(
      "[app_magic] === Policy Decision Start ==="); // 记录调试日志，开始策略决策过程
  fd_log_debug("[app_magic]   Client: %s",
               req->client_id); // 记录调试日志，显示客户端ID
  fd_log_debug("[app_magic]   Flight Phase: %s",
               req->flight_phase); // 记录调试日志，显示飞行阶段
  fd_log_debug("[app_magic]   Required BW: %u kbps",
               req->requested_bw_kbps); // 记录调试日志，显示请求带宽

  /* ========================================
   * 步骤 1: 查找客户端配置
   * ======================================== */

  ClientProfile *client = magic_config_find_client(
      ctx->config, req->client_id); // 根据客户端ID查找客户端配置文件
  if (!client) {                    // 如果未找到客户端配置
    snprintf(resp->reason, sizeof(resp->reason), // 格式化错误原因字符串
             "Client '%s' not found in configuration",
             req->client_id); // 设置错误原因：客户端未在配置中找到
    fd_log_error("[app_magic] %s", resp->reason); // 记录错误日志
    return -1;                                    // 返回错误码-1
  }

  /* v2.0: 检查客户端配置是否启用 */
  if (!client->enabled) {
    snprintf(resp->reason, sizeof(resp->reason),
             "Client '%s' profile is disabled", req->client_id);
    fd_log_error("[app_magic] %s", resp->reason);
    return -1;
  }

  fd_log_debug(
      "[app_magic]   Client Profile Found:"); // 记录调试日志，客户端配置文件已找到
  fd_log_debug("[app_magic]     Profile Name: %s",
               client->profile_name); // 记录调试日志，显示配置文件名
  fd_log_debug(
      "[app_magic]     Max Forward BW: %u kbps",
      client->bandwidth.max_forward_kbps); // 记录调试日志，显示最大前向带宽
  fd_log_debug("[app_magic]     Allowed DLMs: %u",
               client->link_policy
                   .num_allowed_dlms); // 记录调试日志，显示允许的 DLM 数量

  /* v2.0: 检查客户端带宽限制 (使用新的 bandwidth 结构) */
  uint32_t max_client_bw = client->bandwidth.max_forward_kbps;
  if (max_client_bw == 0) {
    max_client_bw = 10000; /* 默认 10 Mbps */
  }
  if (req->requested_bw_kbps >
      max_client_bw) { // 检查请求带宽是否超过客户端限制
    snprintf(
        resp->reason, sizeof(resp->reason), // 格式化错误原因字符串
        "Requested BW (%u kbps) exceeds client limit (%u kbps)", // 设置错误原因：请求带宽超过限制
        req->requested_bw_kbps, max_client_bw);
    fd_log_error("[app_magic] %s", resp->reason); // 记录错误日志
    return -1;                                    // 返回错误码-1
  }

  /* ========================================
   * 步骤 2: 查找适用的策略规则集
   * ======================================== */

  PolicyRuleSet *ruleset = magic_config_find_ruleset(
      ctx->config, req->flight_phase); // 根据飞行阶段查找对应的策略规则集
  if (!ruleset) {                      // 如果未找到特定阶段的规则集
    fd_log_debug("[app_magic]   No specific ruleset for phase '%s', using "
                 "default", // 记录调试日志，使用默认规则集
                 req->flight_phase);
    /* 使用第一个规则集作为默认 */
    if (ctx->config->policy.num_rulesets > 0) {   // 检查是否有任何规则集配置
      ruleset = &ctx->config->policy.rulesets[0]; // 使用第一个规则集作为默认
    } else {
      snprintf(
          resp->reason, sizeof(resp->reason),
          "No policy rulesets configured"); // 设置错误原因：未配置策略规则集
      fd_log_error("[app_magic] %s", resp->reason); // 记录错误日志
      return -1;                                    // 返回错误码-1
    }
  }

  fd_log_debug("[app_magic]   Using Ruleset: %s",
               ruleset->ruleset_id); // 记录调试日志，显示使用的规则集ID

  /* ========================================
   * 步骤 3: v2.0 动态流量分类 + 查找匹配的策略规则
   * ======================================== */

  /* v2.0: 使用动态流量分类替代静态 traffic_class_id */
  const char *dynamic_traffic_class = magic_policy_classify_traffic(
      &ctx->config->policy,
      req->priority_class, /* 来自 magic.conf 的 PRIORITY_CLASS */
      req->qos_level,      /* 来自 magic.conf 的 QOS_LEVEL */
      req->profile_name);  /* 来自 magic.conf 的 PROFILE_NAME */

  /* 记录动态分类结果到响应中 */
  strncpy(resp->matched_traffic_class, dynamic_traffic_class, MAX_ID_LEN - 1);

  fd_log_debug(
      "[app_magic]   Dynamic Traffic Class: %s (prio=%u, qos=%u, profile=%s)",
      dynamic_traffic_class, req->priority_class, req->qos_level,
      req->profile_name);

  PolicyRule *matched_rule = NULL; // 初始化匹配规则指针为NULL
  for (uint32_t i = 0; i < ruleset->num_rules; i++) { // 遍历规则集中的所有规则
    if (strcmp(ruleset->rules[i].traffic_class, dynamic_traffic_class) ==
        0) { // 检查规则的流量类别是否匹配动态分类结果
      matched_rule = &ruleset->rules[i]; // 找到匹配规则，设置指针
      break;                             // 找到后跳出循环
    }
  }

  if (!matched_rule) { // 如果未找到精确匹配的规则
    /* 尝试匹配 "ALL_TRAFFIC" 通配规则 */
    for (uint32_t i = 0; i < ruleset->num_rules;
         i++) { // 再次遍历规则集，查找通配规则
      if (strcmp(ruleset->rules[i].traffic_class, "ALL_TRAFFIC") ==
          0) {                             // 检查是否为"ALL_TRAFFIC"通配规则
        matched_rule = &ruleset->rules[i]; // 设置为通配规则
        fd_log_debug(
            "[app_magic]   Using wildcard rule: ALL_TRAFFIC"); // 记录调试日志，使用通配规则
        break;                                                 // 找到后跳出循环
      }
    }
  }

  /* v2.0: 如果仍未找到，尝试用 QoS.priority_class 作为流量类别 */
  if (!matched_rule && client->qos.priority_class > 0) {
    char prio_class_str[16];
    snprintf(prio_class_str, sizeof(prio_class_str), "PRIORITY_%u",
             client->qos.priority_class);
    for (uint32_t i = 0; i < ruleset->num_rules; i++) {
      if (strcmp(ruleset->rules[i].traffic_class, prio_class_str) == 0) {
        matched_rule = &ruleset->rules[i];
        fd_log_debug("[app_magic]   Fallback to priority_class: %s",
                     prio_class_str);
        break;
      }
    }
  }

  if (!matched_rule) { // 如果仍然未找到匹配规则
    snprintf(
        resp->reason, sizeof(resp->reason), // 格式化错误原因字符串
        "No policy rule for traffic class '%s'",
        dynamic_traffic_class); // 设置错误原因：未找到对应流量类别的策略规则
    fd_log_error("[app_magic] %s", resp->reason); // 记录错误日志
    return -1;                                    // 返回错误码-1
  }

  fd_log_debug(
      "[app_magic]   Matched Rule: %s (%u preferences)", // 记录调试日志，显示匹配的规则和偏好数量
      matched_rule->traffic_class, matched_rule->num_preferences);

  /* ========================================
   * 步骤 4: 按优先级选择最优链路 (v2.0: 增加 allowed_dlms 过滤)
   * ======================================== */

  DatalinkProfile *selected_link = NULL; // 初始化选择的链路指针为NULL
  PathPreference *selected_pref = NULL;  // 初始化选择的路径偏好指针为NULL
  int best_score =
      -999999; // 初始化最佳评分变量为很小的负数，确保任何有效评分都能更新

  for (uint32_t i = 0; i < matched_rule->num_preferences;
       i++) { // 遍历匹配规则中的所有路径偏好
    PathPreference *pref = &matched_rule->preferences[i]; // 获取当前偏好指针

    /* 跳过 PROHIBIT 动作的链路 */
    if (pref->action == ACTION_PROHIBIT) { // 检查偏好动作是否为禁止
      fd_log_debug("[app_magic]     Link %s: PROHIBITED",
                   pref->link_id); // 记录调试日志，该链路被禁止
      continue;                    // 跳过该偏好，继续下一个
    }

    /* v2.0: 检查客户端是否允许使用该 DLM */
    if (!magic_config_is_dlm_allowed(client, pref->link_id)) {
      fd_log_debug("[app_magic]     Link %s: NOT in client's allowed_dlms",
                   pref->link_id);
      continue; // 跳过不被允许的链路
    }

    /* v2.0: 查找 DLM 配置 */
    DLMConfig *dlm = magic_config_find_dlm(ctx->config, pref->link_id);
    if (!dlm) {
      fd_log_debug("[app_magic]     DLM %s: Not found in config",
                   pref->link_id);
      continue;
    }

    /* 检查 DLM 是否在线 */
    if (!dlm->is_active) {
      fd_log_debug("[app_magic]     DLM %s: Offline", pref->link_id);
      continue;
    }

    /* v2.2: ADIF 覆盖范围检查 (基于实时位置数据) */
    if (!ctx->config->adif_degraded_mode && dlm->coverage.enabled &&
        req->has_adif_data) {
      /* 从请求中获取飞机位置并检查覆盖范围 */
      if (req->aircraft_lat != 0.0 || req->aircraft_lon != 0.0) {
        bool in_coverage =
            magic_policy_check_coverage(&dlm->coverage, req->aircraft_lat,
                                        req->aircraft_lon, req->aircraft_alt);

        if (!in_coverage) {
          fd_log_debug("[app_magic]     DLM %s: Aircraft out of coverage "
                       "(lat=%.2f, lon=%.2f, alt=%.0fm)",
                       pref->link_id, req->aircraft_lat, req->aircraft_lon,
                       req->aircraft_alt);
          continue;
        }
        fd_log_debug("[app_magic]     DLM %s: Aircraft in coverage",
                     pref->link_id);
      }
    }

    /* v2.2: WoW (Weight on Wheels) 感知检查 */
    if (req->has_adif_data) {
      /* 检查 on_ground_only: 仅地面可用的链路 */
      if (pref->on_ground_only && !req->on_ground) {
        fd_log_debug(
            "[app_magic]     DLM %s: Requires on-ground (aircraft is airborne)",
            pref->link_id);
        continue;
      }
      /* 检查 airborne_only: 仅空中可用的链路 */
      if (pref->airborne_only && req->on_ground) {
        fd_log_debug(
            "[app_magic]     DLM %s: Requires airborne (aircraft is on-ground)",
            pref->link_id);
        continue;
      }
    }

    /* 计算链路评分 */
    int score = calculate_link_score(dlm, pref, req->requested_bw_kbps);

    /* v2.1: 负载均衡 - 根据当前活跃会话数调整评分 */
    int active_sessions = 0;
    if (ctx->lmi_ctx) {
      for (int j = 0; j < MAX_DLM_CLIENTS; j++) {
        if (ctx->lmi_ctx->clients[j].is_registered &&
            strcmp(ctx->lmi_ctx->clients[j].link_id, pref->link_id) == 0) {
          active_sessions = ctx->lmi_ctx->clients[j].num_active_bearers;
          fd_log_notice("[app_magic]     DLM %s 当前活跃会话数: %d",
                        pref->link_id, active_sessions);
          break;
        }
      }
    } else {
      fd_log_notice("[app_magic]     警告: lmi_ctx 为 NULL，无法进行负载均衡");
    }
    /* 每个活跃会话扣600分，强力负载均衡（2个会话=1200分 > ranking差1000分） */
    int load_penalty = active_sessions * 600;
    score -= load_penalty;

    /* v2.0: 如果是客户端的首选 DLM，给予额外加分 */
    if (client->link_policy.preferred_dlm[0] &&
        strcmp(pref->link_id, client->link_policy.preferred_dlm) == 0) {
      score += 500;
      fd_log_debug("[app_magic]     DLM %s: +500 bonus (PreferredDLM)",
                   pref->link_id);
    }

    fd_log_notice("[app_magic]     链路评分: %s = %d (ranking=%u加%d分, "
                  "负载惩罚-%d分, 带宽%.0f/%u kbps, 延迟%u ms)",
                  pref->link_id, score, pref->ranking,
                  (10 - pref->ranking) * 1000, load_penalty,
                  dlm->max_forward_bw_kbps, req->requested_bw_kbps,
                  dlm->latency_ms);

    /* 更新最优选择 */
    if (score > best_score) {
      best_score = score;
      selected_link = dlm;
      selected_pref = pref;
    }
  }

  /* ========================================
   * 步骤 5: 返回决策结果
   * ======================================== */

  if (selected_link && selected_pref) {
    resp->success = true;
    strncpy(resp->selected_link_id, selected_link->dlm_name,
            MAX_ID_LEN - 1); /* v2.0: 使用 dlm_name */
    resp->granted_bw_kbps = req->requested_bw_kbps;
    resp->granted_ret_bw_kbps = req->requested_ret_bw_kbps;
    resp->qos_level = req->qos_level;

    snprintf(resp->reason, sizeof(resp->reason),
             "Selected %s (ranking %u, score %d)", selected_link->dlm_name,
             selected_pref->ranking, best_score); /* v2.0: 使用 dlm_name */

    fd_log_notice("[app_magic] ✓ Policy Decision SUCCESS");
    fd_log_notice("[app_magic]     Client: %s", req->client_id);
    fd_log_notice("[app_magic]     Selected DLM: %s", /* v2.0 */
                  selected_link->dlm_name);
    fd_log_notice("[app_magic]     Granted BW: %u/%u kbps",
                  resp->granted_bw_kbps, resp->granted_ret_bw_kbps);
    fd_log_notice("[app_magic]     QoS Level: %u", resp->qos_level);
    fd_log_notice("[app_magic]     Reason: %s", resp->reason);

    return 0;
  } else {
    snprintf(
        resp->reason, sizeof(resp->reason), // 格式化失败原因字符串
        "No suitable link available (all offline or prohibited)"); // 设置原因：无合适链路可用
    fd_log_error("[app_magic] ✗ Policy Decision FAILED: %s",
                 resp->reason); // 记录错误日志，策略决策失败
    return -1;                  // 返回错误码-1
  }
}

/**
 * @brief 清理策略引擎。
 * @details
 * 重置初始化状态。由于策略引擎主要使用全局配置的引用，不需要释放太多动态内存。
 *
 * @param ctx 策略上下文指针。
 */
void magic_policy_cleanup(PolicyContext *ctx) {
  if (ctx) {                  // 如果上下文指针不为空
    ctx->initialized = false; // 设置初始化标志为false，表示引擎已清理
    fd_log_notice(
        "[app_magic] Policy engine cleaned up"); // 记录通知日志，策略引擎已清理
  }
}
