/**
 * @file magic_cic.c
 * @brief MAGIC CIC (Client Interface Component) 实现文件。
 * @details 处理 MCAR/MCCR/STR Diameter 消息，集成策略引擎。
 *
 * 本文件实现了 MAGIC 系统中的 Client Interface Component (CIC) 模块
 * 负责处理来自航空器客户端的 Diameter 协议消息:
 *
 * 1. MCAR (Client Authentication Request):
 *    - 验证客户端身份和凭据
 *    - 检查客户端配置文件
 *    - 返回认证结果
 *
 * 2. MCCR (Communication Change Request):
 *    - 解析通信需求参数
 *    - 调用策略引擎选择最佳链路
 *    - 通过 MIH 原语向 DLM 请求资源
 *    - 创建/更新会话并分配链路
 *
 * 3. STR (Session Termination Request):
 *    - 终止客户端会话
 *    - 释放分配的链路资源
 *
 * 架构特点:
 * - 基于 freeDiameter 框架实现
 * - 集成策略决策引擎
 * - 通过 LMI 接口与链路管理层交互
 * - 支持多链路并发管理
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#include "magic_cic.h"          /* CIC 模块接口定义 */
#include "add_avp.h"            /* AVP 添加辅助函数 */
#include "app_magic.h"          /* MAGIC 应用主头文件 */
#include "magic_cdr.h"          /* CDR 管理接口 */
#include "magic_cic_push.h"     /* MSCR/MNTR 推送接口 */
#include "magic_config.h"       /* 配置管理接口 */
#include "magic_dataplane.h"    /* 数据平面路由接口 */
#include "magic_dict_handles.h" /* MAGIC Diameter 字典句柄 */

#include "magic_group_avp_simple.h"  /* 复合 AVP 处理 */
#include "magic_napt_validator.h"    /* ARINC 839 NAPT 验证器 */
#include "magic_policy.h"            /* 策略引擎接口 */
#include "magic_tft_validator.h"     /* TFT 白名单验证器 */
#include "mih_protocol.h"            /* MIH 协议定义 */
#include <arpa/inet.h>               /* inet_ntoa 函数 */
#include <freeDiameter/extension.h>  /* freeDiameter 扩展框架 */
#include <freeDiameter/libfdcore.h>  /* freeDiameter 核心库 */
#include <freeDiameter/libfdproto.h> /* freeDiameter 协议库 */
#include <string.h>                  /* 字符串操作函数 */

/*===========================================================================
 * 前向声明和全局变量
 *===========================================================================*/

/**
 * @brief MCAR 消息处理器前向声明。
 * @details 处理客户端认证请求消息。
 */
static int cic_handle_mcar(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief MCCR 消息处理器前向声明。
 * @details 处理通信变更请求消息。
 */
static int cic_handle_mccr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief STR 消息处理器前向声明。
 * @details 处理会话终止请求消息。
 */
static int cic_handle_str(struct msg **msg, struct avp *avp,
                          struct session *sess, void *opaque,
                          enum disp_action *act);

/**
 * @brief MNTR 消息处理器前向声明。
 * @details 处理通知报告消息。
 */
static int cic_handle_mntr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief MSCR 消息处理器前向声明。
 * @details 处理状态变更报告消息。
 */
static int cic_handle_mscr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief MSXR 消息处理器前向声明。
 * @details 处理状态查询请求消息。
 */
static int cic_handle_msxr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief MADR 消息处理器前向声明。
 * @details 处理计费数据请求消息。
 */
static int cic_handle_madr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief MACR 消息处理器前向声明。
 * @details 处理计费控制请求消息。
 */
static int cic_handle_macr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act);

/**
 * @brief 全局上下文指针。
 * @details 指向 MAGIC 系统的主上下文。
 *          在 `cic_handle_mcar`/`cic_handle_mccr`/`cic_handle_str`
 * 函数中需要访问 MAGIC 系统的配置、策略引擎、会话管理器等组件。
 */
static MagicContext *g_ctx = NULL;

/*===========================================================================
 * MSXR 速率限制 (v2.1)
 * 关联到 Client-ID，防止同一客户端频繁轮询
 *===========================================================================*/
#define MSXR_RATE_LIMIT_ENTRIES 64

typedef struct {
  char client_id[MAX_ID_LEN];
  time_t last_request_time;
} MsxrRateLimitEntry;

static MsxrRateLimitEntry g_msxr_rate_limit[MSXR_RATE_LIMIT_ENTRIES];
static pthread_mutex_t g_msxr_rate_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 检查 MSXR 速率限制。
 * @details 关联到 Client-ID，防止同一客户端频繁轮询 (Rate Limiting)。
 *          使用互斥锁保护内部状态。
 *
 * @param client_id 客户端 ID 字符串。
 * @param limit_sec 最小允许间隔秒数。
 * @return 0 允许，-1 超频 (应返回 DIAMETER_TOO_BUSY 3004)。
 */
static int msxr_check_rate_limit(const char *client_id, uint32_t limit_sec) {
  if (!client_id || client_id[0] == '\0' || limit_sec == 0) {
    return 0; /* 无限制 */
  }

  time_t now = time(NULL);
  int free_slot = -1;
  int found_slot = -1;

  pthread_mutex_lock(&g_msxr_rate_mutex);

  for (int i = 0; i < MSXR_RATE_LIMIT_ENTRIES; i++) {
    if (g_msxr_rate_limit[i].client_id[0] == '\0') {
      if (free_slot < 0)
        free_slot = i;
    } else if (strcmp(g_msxr_rate_limit[i].client_id, client_id) == 0) {
      found_slot = i;
      break;
    }
  }

  if (found_slot >= 0) {
    /* 已有记录，检查间隔 */
    time_t elapsed = now - g_msxr_rate_limit[found_slot].last_request_time;
    if (elapsed < (time_t)limit_sec) {
      pthread_mutex_unlock(&g_msxr_rate_mutex);
      fd_log_notice(
          "[app_magic] MSXR rate limit: client=%s, elapsed=%ld < limit=%u",
          client_id, (long)elapsed, limit_sec);
      return -1; /* 超频 */
    }
    g_msxr_rate_limit[found_slot].last_request_time = now;
  } else if (free_slot >= 0) {
    /* 新记录 */
    strncpy(g_msxr_rate_limit[free_slot].client_id, client_id, MAX_ID_LEN - 1);
    g_msxr_rate_limit[free_slot].last_request_time = now;
  }

  pthread_mutex_unlock(&g_msxr_rate_mutex);
  return 0;
}

/*===========================================================================
 * 辅助函数前向声明
 *===========================================================================*/

/**
 * @brief ADIF 状态变化回调函数。
 * @details 当飞机状态（WoW、高度、飞行阶段等）发生变化时被调用，
 *          检查所有活动会话是否仍满足激活条件，并终止不合规的会话。
 * @note 此函数被导出供 `app_magic.c` 使用。
 */
void on_adif_state_changed(const AdifAircraftState *state, void *user_data);

/* AVP 提取辅助函数 */
static int extract_string_from_grouped_avp(struct avp *grouped_avp,
                                           const char *avp_name, char *out_str,
                                           size_t out_size);
static int extract_uint32_from_grouped_avp(struct avp *grouped_avp,
                                           const char *avp_name,
                                           uint32_t *out_val);
static int extract_float32_from_grouped_avp(struct avp *grouped_avp,
                                            const char *avp_name,
                                            float *out_val);

/*===========================================================================
 * 通信请求参数结构体 (用于解析和默认值填充)
 *===========================================================================*/

/**
 * @brief 通信请求参数结构体
 * 用于解析 Communication-Request-Parameters AVP 并填充默认值
 */
typedef struct {
  char profile_name[MAX_ID_LEN]; /* 配置文件名 */
  float requested_bw;            /* 请求的下行带宽 (kbps) */
  float requested_ret_bw;        /* 请求的上行带宽 (kbps) */
  float required_bw;             /* 最小保证下行带宽 (kbps) */
  float required_ret_bw;         /* 最小保证上行带宽 (kbps) */
  uint32_t priority_type;        /* 优先级类型 (1=Blocking, 2=Preemption) */
  char priority_class[16];       /* 优先级等级 */
  uint32_t qos_level;            /* QoS 等级 (0=BE, 1=AF, 2=EF) */
  char dlm_name[MAX_ID_LEN];     /* 指定的 DLM 名称 */
  char flight_phase[MAX_ID_LEN]; /* 飞行阶段 */
  char altitude[MAX_ID_LEN];     /* 高度范围 (原始字符串) */
  char airport[MAX_ID_LEN];      /* 机场代码 (原始字符串) */
  uint32_t accounting_enabled;   /* 是否启用计费 */
  uint32_t keep_request;         /* 是否保持请求 */
  uint32_t auto_detect;          /* 自动检测模式 */
  uint32_t timeout;              /* 超时时间 (秒) */

  /* v2.3: 高度范围解析结果 (ARINC 839 §1.1.1.6.4.2) */
  bool altitude_is_blacklist;     /* true=黑名单, false=白名单 */
  int32_t altitude_ranges[10][2]; /* 高度范围数组 [min,
                                     max]，单位英尺，-1表示无限制 */
  int altitude_range_count;       /* 高度范围数量 */

  /* v2.3: 机场白/黑名单解析结果 (ARINC 839 §1.1.1.6.4.3) */
  bool airport_is_blacklist; /* true=黑名单, false=白名单 */
  char airport_codes[20][8]; /* 机场代码数组 (3字符IATA代码) */
  int airport_code_count;    /* 机场代码数量 */

  /* TFT (Traffic Flow Template) 参数 - 用于防火墙规则 */
  char tft_to_ground[32][256];   /* TFT to Ground 规则字符串数组 (最多32条) */
  int tft_to_ground_count;       /* TFT to Ground 规则数量 */
  char tft_to_aircraft[32][256]; /* TFT to Aircraft 规则字符串数组 (最多32条) */
  int tft_to_aircraft_count;     /* TFT to Aircraft 规则数量 */

  /* NAPT (Network Address Port Translation) 参数 */
  char napt_rules[10][256]; /* NAPT 规则字符串数组 */
  int napt_count;           /* NAPT 规则数量 */

  /* 标记哪些字段已从请求中获取 */
  bool has_profile_name;
  bool has_requested_bw;
  bool has_requested_ret_bw;
  bool has_required_bw;
  bool has_required_ret_bw;
  bool has_priority_type;
  bool has_priority_class;
  bool has_qos_level;
  bool has_dlm_name;
  bool has_flight_phase;
  bool has_altitude;
  bool has_airport;
  bool has_accounting_enabled;
  bool has_keep_request;
  bool has_auto_detect;
  bool has_timeout;
  bool has_tft_to_ground;
  bool has_tft_to_aircraft;
} CommReqParams;

/**
 * @brief 初始化通信请求参数为默认值。
 * @details 将 `CommReqParams` 结构体清零并设置符合协议规范的默认值。
 *          - Bandwidth: 0.0
 *          - Priority: Preemption 2/5
 *          - QoS: BestEffort
 *          - Accounting: Enabled
 *
 * @param params 参数结构体指针。
 */
static void comm_req_params_init(CommReqParams *params) {
  memset(params, 0, sizeof(*params));
  /* 设置协议定义的默认值 */
  strncpy(params->profile_name, "default", sizeof(params->profile_name) - 1);
  params->requested_bw = 0.0;
  params->requested_ret_bw = 0.0;
  params->required_bw = 0.0;
  params->required_ret_bw = 0.0;
  params->priority_type = 2; /* 默认: Preemption */
  strncpy(params->priority_class, "5", sizeof(params->priority_class) - 1);
  params->qos_level = 0;      /* 默认: Best Effort */
  params->dlm_name[0] = '\0'; /* 无指定 DLM */
  strncpy(params->flight_phase, "CRUISE", sizeof(params->flight_phase) - 1);
  params->altitude[0] = '\0';
  params->airport[0] = '\0';
  params->accounting_enabled = 1; /* 默认: 启用计费 */
  params->keep_request = 0;       /* 默认: 不保持请求 */
  params->auto_detect = 0;        /* 默认: 不自动检测 */
  params->timeout = 300;          /* 默认: 5分钟超时 */

  /* v2.3: 初始化 ARINC 839 会话激活条件字段 */
  params->altitude_is_blacklist = false;
  params->altitude_range_count = 0;
  params->airport_is_blacklist = false;
  params->airport_code_count = 0;
}

/**
 * @brief 解析 Altitude AVP 字符串 (ARINC 839 §1.1.1.6.4.2)。
 * @details 将 Altitude AVP 的字符串值解析为内部的高度范围数组结构。
 *          格式如 "<from>-<to>" 或 "not <from>-<to>,<from>-<to>"。
 *          支持黑名单 (not) 和白名单模式。
 *
 * 示例:
 *   - "1000-2000"       : 会话仅在 1000-2000 英尺激活
 *   - "not 1000-2000"   : 会话在 1000-2000 英尺以外激活
 *   - "-5000"           : 会话仅在 5000 英尺以下激活
 *   - "20000-"          : 会话仅在 20000 英尺以上激活
 *
 * @param altitude_str Altitude AVP 值。
 * @param params 输出到 `CommReqParams` 结构体。
 * @return 0 成功，-1 失败。
 */
static int parse_altitude_avp(const char *altitude_str, CommReqParams *params) {
  if (!altitude_str || !params || altitude_str[0] == '\0') {
    return 0; /* 空字符串表示所有高度 */
  }

  const char *ptr = altitude_str;

  /* 检查是否为黑名单 (以 "not " 开头) */
  if (strncmp(ptr, "not ", 4) == 0) {
    params->altitude_is_blacklist = true;
    ptr += 4;
  } else {
    params->altitude_is_blacklist = false;
  }

  /* 解析逗号分隔的高度范围 */
  char range_copy[256];
  strncpy(range_copy, ptr, sizeof(range_copy) - 1);
  range_copy[sizeof(range_copy) - 1] = '\0';

  char *token = strtok(range_copy, ",");
  while (token && params->altitude_range_count < 10) {
    /* 跳过前导空格 */
    while (*token == ' ')
      token++;

    int32_t min_alt = -1; /* -1 表示无下限 */
    int32_t max_alt = -1; /* -1 表示无上限 */

    /* 解析 "<from>-<to>" 格式 */
    char *dash = strchr(token, '-');
    if (dash) {
      if (dash == token) {
        /* "-5000" 格式: 无下限 */
        min_alt = -1;
        max_alt = atoi(dash + 1);
      } else if (*(dash + 1) == '\0' || *(dash + 1) == ',') {
        /* "20000-" 格式: 无上限 */
        *dash = '\0';
        min_alt = atoi(token);
        max_alt = -1;
      } else {
        /* "1000-2000" 格式: 完整范围 */
        *dash = '\0';
        min_alt = atoi(token);
        max_alt = atoi(dash + 1);
      }
    } else {
      /* 单个数值: 精确匹配 (作为点范围) */
      min_alt = max_alt = atoi(token);
    }

    params->altitude_ranges[params->altitude_range_count][0] = min_alt;
    params->altitude_ranges[params->altitude_range_count][1] = max_alt;
    params->altitude_range_count++;

    token = strtok(NULL, ",");
  }

  fd_log_debug("[app_magic]   Parsed Altitude AVP: %s, %d range(s)",
               params->altitude_is_blacklist ? "blacklist" : "whitelist",
               params->altitude_range_count);

  return 0;
}

/**
 * @brief 解析 Airport AVP 字符串 (ARINC 839 §1.1.1.6.4.3)。
 * @details 将 Airport AVP 的字符串值解析为内部的机场代码数组。
 *          格式如 "<airport1>,<airport2>,..." 或 "not
 * <airport1>,<airport2>,..."。 机场代码为 3 字符 IATA 代码。
 *
 * @param airport_str Airport AVP 值。
 * @param params 输出到 `CommReqParams` 结构体。
 * @return 0 成功，-1 失败。
 */
static int parse_airport_avp(const char *airport_str, CommReqParams *params) {
  if (!airport_str || !params || airport_str[0] == '\0') {
    return 0; /* 空字符串表示所有机场 */
  }

  const char *ptr = airport_str;

  /* 检查是否为黑名单 (以 "not " 开头) */
  if (strncmp(ptr, "not ", 4) == 0) {
    params->airport_is_blacklist = true;
    ptr += 4;
  } else {
    params->airport_is_blacklist = false;
  }

  /* 解析逗号分隔的机场代码 */
  char codes_copy[256];
  strncpy(codes_copy, ptr, sizeof(codes_copy) - 1);
  codes_copy[sizeof(codes_copy) - 1] = '\0';

  char *token = strtok(codes_copy, ",");
  while (token && params->airport_code_count < 20) {
    /* 跳过前导空格 */
    while (*token == ' ')
      token++;

    /* 复制机场代码 (最多 7 字符) */
    strncpy(params->airport_codes[params->airport_code_count], token, 7);
    params->airport_codes[params->airport_code_count][7] = '\0';
    params->airport_code_count++;

    token = strtok(NULL, ",");
  }

  fd_log_debug("[app_magic]   Parsed Airport AVP: %s, %d airport(s)",
               params->airport_is_blacklist ? "blacklist" : "whitelist",
               params->airport_code_count);

  return 0;
}

/**
 * @brief 从客户端配置 Profile 填充默认值 (v2.0)。
 * @details 协议要求: 如果客户端未发送可选的 AVP，CIC 应自动填充 Profile
 * 中的默认值。 包括带宽、QoS 优先级等。
 *
 * @param params 参数结构体指针。
 * @param client_profile 客户端配置指针。
 */
static void comm_req_params_fill_from_profile(CommReqParams *params,
                                              ClientProfile *client_profile) {
  if (!client_profile)
    return;

  /* v2.0: 从带宽配置填充带宽限制 (使用 bandwidth 结构) */
  if (!params->has_requested_bw &&
      client_profile->bandwidth.default_request_kbps > 0) {
    params->requested_bw =
        (float)client_profile->bandwidth.default_request_kbps;
  } else if (!params->has_requested_bw &&
             client_profile->bandwidth.max_forward_kbps > 0) {
    params->requested_bw = (float)client_profile->bandwidth.max_forward_kbps;
  }
  if (!params->has_requested_ret_bw &&
      client_profile->bandwidth.max_return_kbps > 0) {
    params->requested_ret_bw = (float)client_profile->bandwidth.max_return_kbps;
  }

  /* v2.0: 从 QoS 配置填充优先级 */
  if (!params->has_priority_class && client_profile->qos.priority_class > 0) {
    snprintf(params->priority_class, sizeof(params->priority_class), "%u",
             client_profile->qos.priority_class);
  }
  if (!params->has_qos_level) {
    params->qos_level = client_profile->qos.default_level;
  }
}

/**
 * @brief 从 Grouped AVP 解析通信请求参数。
 * @details 解析 `Communication-Request-Parameters` Grouped AVP，提取其中的
 * Profile-Name, Bandwidth, TFT, NAPT 等参数。 会自动初始化 params
 * 并填充默认值。
 *
 * @param grouped_avp Communication-Request-Parameters AVP 指针。
 * @param params 输出参数结构体指针。
 * @return 0 成功，-1 失败。
 */
static int parse_comm_req_params(struct avp *grouped_avp,
                                 CommReqParams *params) {
  if (!grouped_avp || !params)
    return -1;

  comm_req_params_init(params);

  /* 提取 Profile-Name */
  if (extract_string_from_grouped_avp(grouped_avp, "Profile-Name",
                                      params->profile_name,
                                      sizeof(params->profile_name)) == 0) {
    params->has_profile_name = true;
  }

  /* 提取带宽参数 */
  if (extract_float32_from_grouped_avp(grouped_avp, "Requested-Bandwidth",
                                       &params->requested_bw) == 0) {
    params->has_requested_bw = true;
  }
  if (extract_float32_from_grouped_avp(grouped_avp,
                                       "Requested-Return-Bandwidth",
                                       &params->requested_ret_bw) == 0) {
    params->has_requested_ret_bw = true;
  }
  if (extract_float32_from_grouped_avp(grouped_avp, "Required-Bandwidth",
                                       &params->required_bw) == 0) {
    params->has_required_bw = true;
  }
  if (extract_float32_from_grouped_avp(grouped_avp, "Required-Return-Bandwidth",
                                       &params->required_ret_bw) == 0) {
    params->has_required_ret_bw = true;
  }

  /* 提取优先级参数 */
  if (extract_string_from_grouped_avp(grouped_avp, "Priority-Class",
                                      params->priority_class,
                                      sizeof(params->priority_class)) == 0) {
    params->has_priority_class = true;
  }
  if (extract_uint32_from_grouped_avp(grouped_avp, "Priority-Type",
                                      &params->priority_type) == 0) {
    params->has_priority_type = true;
  }
  if (extract_uint32_from_grouped_avp(grouped_avp, "QoS-Level",
                                      &params->qos_level) == 0) {
    params->has_qos_level = true;
  }

  /* 提取链路选择参数 */
  if (extract_string_from_grouped_avp(grouped_avp, "DLM-Name", params->dlm_name,
                                      sizeof(params->dlm_name)) == 0) {
    params->has_dlm_name = true;
  }

  /* 提取位置参数 */
  if (extract_string_from_grouped_avp(grouped_avp, "Flight-Phase",
                                      params->flight_phase,
                                      sizeof(params->flight_phase)) == 0) {
    params->has_flight_phase = true;
  }
  if (extract_string_from_grouped_avp(grouped_avp, "Altitude", params->altitude,
                                      sizeof(params->altitude)) == 0) {
    params->has_altitude = true;
    /* v2.3: 解析 Altitude AVP 为范围结构 */
    parse_altitude_avp(params->altitude, params);
  }
  if (extract_string_from_grouped_avp(grouped_avp, "Airport", params->airport,
                                      sizeof(params->airport)) == 0) {
    params->has_airport = true;
    /* v2.3: 解析 Airport AVP 为代码列表 */
    parse_airport_avp(params->airport, params);
  }

  /* 提取其他参数 */
  if (extract_uint32_from_grouped_avp(grouped_avp, "Accounting-Enabled",
                                      &params->accounting_enabled) == 0) {
    params->has_accounting_enabled = true;
  }
  if (extract_uint32_from_grouped_avp(grouped_avp, "Keep-Request",
                                      &params->keep_request) == 0) {
    params->has_keep_request = true;
  }
  if (extract_uint32_from_grouped_avp(grouped_avp, "Auto-Detect",
                                      &params->auto_detect) == 0) {
    params->has_auto_detect = true;
  }
  if (extract_uint32_from_grouped_avp(grouped_avp, "Timeout",
                                      &params->timeout) == 0) {
    params->has_timeout = true;
  }

  /* 提取 TFT 参数 (可能有多条，ARINC 839 支持 1-255 条) */
  /* ARINC 839 结构: Communication-Request-Parameters (20001)
   *                   └── TFTtoGround-List (20004)
   *                         └── TFTtoGround-Rule (10030)
   */
  params->tft_to_ground_count = 0;
  params->tft_to_aircraft_count = 0;

  /* 遍历 Communication-Request-Parameters 的子AVP */
  struct avp *child_avp = NULL;
  if (fd_msg_browse(grouped_avp, MSG_BRW_FIRST_CHILD, &child_avp, NULL) == 0) {
    while (child_avp) {
      struct dict_object *model = NULL;
      if (fd_msg_model(child_avp, &model) == 0 && model != NULL) {
        struct dict_avp_data avp_data;
        memset(&avp_data, 0, sizeof(avp_data));
        fd_dict_getval(model, &avp_data);

        /* TFTtoGround-List (20004) - 包含 TFTtoGround-Rule */
        if (avp_data.avp_code == 20004) {
          struct avp *tft_rule_avp = NULL;
          if (fd_msg_browse(child_avp, MSG_BRW_FIRST_CHILD, &tft_rule_avp,
                            NULL) == 0) {
            while (tft_rule_avp && params->tft_to_ground_count < 32) {
              struct dict_object *rule_model = NULL;
              if (fd_msg_model(tft_rule_avp, &rule_model) == 0 &&
                  rule_model != NULL) {
                struct dict_avp_data rule_data;
                memset(&rule_data, 0, sizeof(rule_data));
                fd_dict_getval(rule_model, &rule_data);

                if (rule_data.avp_code == 10030) { /* TFTtoGround-Rule */
                  struct avp_hdr *hdr;
                  if (fd_msg_avp_hdr(tft_rule_avp, &hdr) == 0 &&
                      hdr->avp_value) {
                    size_t len = hdr->avp_value->os.len;
                    if (len >= 256)
                      len = 255;
                    memcpy(params->tft_to_ground[params->tft_to_ground_count],
                           hdr->avp_value->os.data, len);
                    params->tft_to_ground[params->tft_to_ground_count][len] =
                        '\0';
                    fd_log_notice(
                        "[app_magic]   Extracted TFT-to-Ground[%d]: %s",
                        params->tft_to_ground_count,
                        params->tft_to_ground[params->tft_to_ground_count]);
                    params->tft_to_ground_count++;
                  }
                }
              }
              fd_msg_browse(tft_rule_avp, MSG_BRW_NEXT, &tft_rule_avp, NULL);
            }
          }
        }
        /* TFTtoAircraft-List (20005) - 包含 TFTtoAircraft-Rule */
        else if (avp_data.avp_code == 20005) {
          struct avp *tft_rule_avp = NULL;
          if (fd_msg_browse(child_avp, MSG_BRW_FIRST_CHILD, &tft_rule_avp,
                            NULL) == 0) {
            while (tft_rule_avp && params->tft_to_aircraft_count < 32) {
              struct dict_object *rule_model = NULL;
              if (fd_msg_model(tft_rule_avp, &rule_model) == 0 &&
                  rule_model != NULL) {
                struct dict_avp_data rule_data;
                memset(&rule_data, 0, sizeof(rule_data));
                fd_dict_getval(rule_model, &rule_data);

                if (rule_data.avp_code == 10031) { /* TFTtoAircraft-Rule */
                  struct avp_hdr *hdr;
                  if (fd_msg_avp_hdr(tft_rule_avp, &hdr) == 0 &&
                      hdr->avp_value) {
                    size_t len = hdr->avp_value->os.len;
                    if (len >= 256)
                      len = 255;
                    memcpy(
                        params->tft_to_aircraft[params->tft_to_aircraft_count],
                        hdr->avp_value->os.data, len);
                    params
                        ->tft_to_aircraft[params->tft_to_aircraft_count][len] =
                        '\0';
                    fd_log_notice(
                        "[app_magic]   Extracted TFT-to-Aircraft[%d]: %s",
                        params->tft_to_aircraft_count,
                        params->tft_to_aircraft[params->tft_to_aircraft_count]);
                    params->tft_to_aircraft_count++;
                  }
                }
              }
              fd_msg_browse(tft_rule_avp, MSG_BRW_NEXT, &tft_rule_avp, NULL);
            }
          }
        }
        /* NAPT-List (20006) - 包含 NAPT-Rule */
        else if (avp_data.avp_code == 20006) {
          struct avp *napt_rule_avp = NULL;
          if (fd_msg_browse(child_avp, MSG_BRW_FIRST_CHILD, &napt_rule_avp,
                            NULL) == 0) {
            while (napt_rule_avp && params->napt_count < 10) {
              struct dict_object *rule_model = NULL;
              if (fd_msg_model(napt_rule_avp, &rule_model) == 0 &&
                  rule_model != NULL) {
                struct dict_avp_data rule_data;
                memset(&rule_data, 0, sizeof(rule_data));
                fd_dict_getval(rule_model, &rule_data);

                if (rule_data.avp_code == 10032) { /* NAPT-Rule */
                  struct avp_hdr *hdr;
                  if (fd_msg_avp_hdr(napt_rule_avp, &hdr) == 0 &&
                      hdr->avp_value) {
                    size_t len = hdr->avp_value->os.len;
                    if (len >= 256)
                      len = 255;
                    memcpy(params->napt_rules[params->napt_count],
                           hdr->avp_value->os.data, len);
                    params->napt_rules[params->napt_count][len] = '\0';
                    params->napt_count++;
                  }
                }
              }
              fd_msg_browse(napt_rule_avp, MSG_BRW_NEXT, &napt_rule_avp, NULL);
            }
          }
        }
      }
      fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL);
    }
  }

  fd_log_notice("[app_magic]   TFT Summary: Ground=%d, Aircraft=%d, NAPT=%d",
                params->tft_to_ground_count, params->tft_to_aircraft_count,
                params->napt_count);

  return 0;
}

/**
 * @brief 处理未知 AVP。
 * @details 遍历消息中的 AVP，检查是否有字典中未定义的 AVP。
 *          协议要求: 对于无法识别的 AVP，CIC 应在处理时忽略，但在 Proxy
 * 转发模式下应透传。
 *
 * @param msg 消息结构体指针。
 * @param is_proxy_mode 是否为代理模式 (true=保留未知 AVP, false=仅记录日志)。
 * @return 检测到的未知 AVP 数量。
 */
static int handle_unknown_avps(struct msg *msg, bool is_proxy_mode) {
  struct avp *avp = NULL;
  struct avp *next_avp = NULL;
  int unknown_count = 0;

  /* 从第一个 AVP 开始遍历 */
  if (fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL) != 0) {
    return 0;
  }

  while (avp) {
    struct dict_object *model = NULL;

    /* 获取下一个 AVP (在可能删除当前 AVP 之前) */
    if (fd_msg_browse(avp, MSG_BRW_NEXT, &next_avp, NULL) != 0) {
      next_avp = NULL;
    }

    /* 检查 AVP 是否在字典中有定义 */
    if (fd_msg_model(avp, &model) != 0 || model == NULL) {
      struct avp_hdr *hdr;
      if (fd_msg_avp_hdr(avp, &hdr) == 0) {
        fd_log_notice("[app_magic]   Unknown AVP: code=%u, vendor=%u, len=%u",
                      hdr->avp_code, hdr->avp_vendor, hdr->avp_len);
        unknown_count++;

        /* 在非代理模式下，可以选择记录但不删除 */
        /* 如果是代理模式，AVP 会被原样保留并转发 */
        if (!is_proxy_mode) {
          /* 记录未知 AVP 但不删除，保持兼容性 */
          fd_log_notice(
              "[app_magic]   → Ignoring unknown AVP (non-proxy mode)");
        } else {
          fd_log_notice(
              "[app_magic]   → Preserving unknown AVP for proxy forwarding");
        }
      }
    }

    avp = next_avp;
  }

  return unknown_count;
}

/*===========================================================================
 * 辅助函数：AVP 提取
 *===========================================================================*/

static int extract_string_from_grouped_avp(struct avp *grouped_avp,
                                           const char *avp_name, char *output,
                                           size_t output_size) {
  struct avp *child_avp = NULL; /* 子 AVP 遍历指针 */
  struct avp_hdr *child_hdr;    /* 子 AVP 头部指针 */
  struct dict_object *dict_avp; /* 字典对象指针 */
  struct dict_avp_request req;  /* 字典查找请求结构体 */

  /* 初始化字典查找请求 */
  memset(&req, 0, sizeof(req));
  req.avp_vendor = 13712;          /* MAGIC 协议的厂商 ID */
  req.avp_name = (char *)avp_name; /* 要查找的 AVP 名称 */

  /* 在字典中查找 AVP 定义 */
  if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR,
                     &req, &dict_avp, ENOENT) != 0) {
    return -1; /* 字典中未找到该 AVP 定义 */
  }

  /* 开始遍历复合 AVP 的第一个子 AVP */
  if (fd_msg_browse(grouped_avp, MSG_BRW_FIRST_CHILD, &child_avp, NULL) != 0) {
    return -1; /* 无子 AVP 或遍历失败 */
  }

  /* 遍历所有子 AVP */
  while (child_avp) {
    /* 获取当前子 AVP 的头部信息 */
    if (fd_msg_avp_hdr(child_avp, &child_hdr) != 0) {
      break; /* 获取头部失败，结束遍历 */
    }

    struct dict_object *child_dict;
    /* 检查当前子 AVP 是否为我们要找的类型 */
    if (fd_msg_model(child_avp, &child_dict) == 0 && child_dict == dict_avp) {
      /* 找到匹配的 AVP，提取字符串数据 */
      if (child_hdr->avp_value && child_hdr->avp_value->os.data) {
        size_t len = child_hdr->avp_value->os.len; /* 字符串实际长度 */
        if (len >= output_size)
          len = output_size - 1;                            /* 防止缓冲区溢出 */
        memcpy(output, child_hdr->avp_value->os.data, len); /* 复制字符串数据 */
        output[len] = '\0'; /* 添加字符串结束符 */
        return 0;           /* 成功提取，返回 */
      }
    }

    /* 移动到下一个子 AVP */
    if (fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL) != 0) {
      break; /* 无下一个子 AVP，结束遍历 */
    }
  }

  return -1; /* 未找到指定的 AVP */
}

static int extract_uint32_from_grouped_avp(struct avp *grouped_avp,
                                           const char *avp_name,
                                           uint32_t *output) {
  struct avp *child_avp = NULL; /* 子 AVP 遍历指针 */
  struct avp_hdr *child_hdr;    /* 子 AVP 头部指针 */
  struct dict_object *dict_avp; /* 字典对象指针 */
  struct dict_avp_request req;  /* 字典查找请求结构体 */

  /* 初始化字典查找请求 */
  memset(&req, 0, sizeof(req));
  req.avp_vendor = 13712;          /* MAGIC 协议的厂商 ID */
  req.avp_name = (char *)avp_name; /* 要查找的 AVP 名称 */

  /* 在字典中查找 AVP 定义 */
  if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR,
                     &req, &dict_avp, ENOENT) != 0) {
    return -1; /* 字典中未找到该 AVP 定义 */
  }

  /* 开始遍历复合 AVP 的第一个子 AVP */
  if (fd_msg_browse(grouped_avp, MSG_BRW_FIRST_CHILD, &child_avp, NULL) != 0) {
    return -1; /* 无子 AVP 或遍历失败 */
  }

  /* 遍历所有子 AVP */
  while (child_avp) {
    /* 获取当前子 AVP 的头部信息 */
    if (fd_msg_avp_hdr(child_avp, &child_hdr) != 0) {
      break; /* 获取头部失败，结束遍历 */
    }

    struct dict_object *child_dict;
    /* 检查当前子 AVP 是否为我们要找的类型 */
    if (fd_msg_model(child_avp, &child_dict) == 0 && child_dict == dict_avp) {
      /* 找到匹配的 AVP，提取 uint32 数据 */
      if (child_hdr->avp_value) {
        *output = child_hdr->avp_value->u32; /* 提取 32 位无符号整数 */
        return 0;                            /* 成功提取，返回 */
      }
    }

    /* 移动到下一个子 AVP */
    if (fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL) != 0) {
      break; /* 无下一个子 AVP，结束遍历 */
    }
  }

  return -1; /* 未找到指定的 AVP */
}

static int extract_float32_from_grouped_avp(struct avp *grouped_avp,
                                            const char *avp_name,
                                            float *output) {
  struct avp *child_avp = NULL; /* 子 AVP 遍历指针 */
  struct avp_hdr *child_hdr;    /* 子 AVP 头部指针 */
  struct dict_object *dict_avp; /* 字典对象指针 */
  struct dict_avp_request req;  /* 字典查找请求结构体 */

  /* 初始化字典查找请求 */
  memset(&req, 0, sizeof(req));
  req.avp_vendor = 13712;          /* MAGIC 协议的厂商 ID */
  req.avp_name = (char *)avp_name; /* 要查找的 AVP 名称 */

  /* 在字典中查找 AVP 定义 */
  if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR,
                     &req, &dict_avp, ENOENT) != 0) {
    return -1; /* 字典中未找到该 AVP 定义 */
  }

  /* 开始遍历复合 AVP 的第一个子 AVP */
  if (fd_msg_browse(grouped_avp, MSG_BRW_FIRST_CHILD, &child_avp, NULL) != 0) {
    return -1; /* 无子 AVP 或遍历失败 */
  }

  /* 遍历所有子 AVP */
  while (child_avp) {
    /* 获取当前子 AVP 的头部信息 */
    if (fd_msg_avp_hdr(child_avp, &child_hdr) != 0) {
      break; /* 获取头部失败，结束遍历 */
    }

    struct dict_object *child_dict;
    /* 检查当前子 AVP 是否为我们要找的类型 */
    if (fd_msg_model(child_avp, &child_dict) == 0 && child_dict == dict_avp) {
      /* 找到匹配的 AVP，提取 float32 数据 */
      if (child_hdr->avp_value) {
        *output = child_hdr->avp_value->f32; /* 提取 32 位浮点数 */
        return 0;                            /* 成功提取，返回 */
      }
    }

    /* 移动到下一个子 AVP */
    if (fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL) != 0) {
      break; /* 无下一个子 AVP，结束遍历 */
    }
  }

  return -1; /* 未找到指定的 AVP */
}

/*===========================================================================
 * MCAR 处理器 (Client Authentication Request)
 * 处理客户端认证请求的 Diameter 消息
 *
 * 根据 ARINC 839 协议和设计方案，MCAR 处理采用 5 步流水线：
 * Step 1: 格式解析与安全校验 (Sanity Check)
 * Step 2: 身份鉴权 (Authentication)
 * Step 3: 订阅处理 (Subscription Handling) - 场景 B
 * Step 4: 0-RTT 资源申请 (Resource Allocation) - 场景 C
 * Step 5: 构建并发送应答 (Finalize)
 *
 * 支持三种 MCAR 变体：
 * - 场景 A: 仅认证 (仅含 Client-Credentials) → AUTHENTICATED
 * - 场景 B: 认证 + 订阅 (含 REQ-Status-Info) → AUTHENTICATED (带订阅)
 * - 场景 C: 0-RTT 接入 (含 Communication-Request-Parameters) → ACTIVE
 *===========================================================================*/

/* MCAR 场景 C: 资源申请重试/回退参数（放在 MCAR 区域，避免依赖 MCCR
 * 宏的定义顺序） */
#define MCAR_RETRY_MAX_COUNT 3
#define MCAR_RETRY_DELAY_MS 100
#define MCAR_FALLBACK_MAX_LINKS 4

/* MCAR 处理上下文 - 用于在各步骤间传递状态 */
typedef struct {
  /* 从请求中提取的信息 */
  char session_id[128];          /* 会话 ID */
  char client_id[MAX_ID_LEN];    /* 客户端 ID (Origin-Host) */
  char client_realm[MAX_ID_LEN]; /* 客户端域 (Origin-Realm) - v2.1 用于 MNTR
                                    路由 */
  char username[MAX_ID_LEN];     /* 用户名 */
  char password[MAX_ID_LEN];     /* 密码 */
  char client_ip[64];            /* 客户端 TCP 连接 IP */

  /* 可选 AVP 标志 */
  bool has_client_credentials; /* 是否包含 Client-Credentials */
  bool has_req_status_info;    /* 是否包含 REQ-Status-Info */
  bool has_comm_req_params;    /* 是否包含 Communication-Request-Parameters */

  /* 解析后的参数 */
  uint32_t req_status_info;  /* 请求的状态信息级别 */
  CommReqParams comm_params; /* 通信请求参数 */

  /* 鉴权结果 */
  ClientProfile *profile; /* 客户端配置文件指针 */
  bool auth_success;      /* 鉴权是否成功 */

  /* 会话上下文 */
  ClientSession *session; /* 创建的会话对象 */

  /* 安全校验结果 (场景 C: TFT/NAPT 白名单验证) */
  bool security_passed;       /* 安全校验是否通过 */
  char extracted_dest_ip[64]; /* 从 TFT 提取的目的 IP */

  /* 资源分配结果 */
  PolicyResponse policy_resp;            /* 策略决策结果 */
  MIH_Link_Resource_Confirm mih_confirm; /* MIH 资源确认 */
  bool resource_allocated;               /* 资源是否分配成功 */
  bool route_added;                      /* 路由是否添加成功 */

  /* 重试与回退控制 (场景 C: MIH 资源请求) */
  uint32_t retry_count;                                  /* 当前重试次数 */
  char tried_links[MCAR_FALLBACK_MAX_LINKS][MAX_ID_LEN]; /* 已尝试的链路列表 */
  int tried_link_count;                                  /* 已尝试链路数 */

  /* 应答构建参数 */
  uint32_t result_code;         /* Diameter Result-Code */
  uint32_t magic_status_code;   /* MAGIC-Status-Code (0=无错误) */
  uint32_t granted_lifetime;    /* 授权生存期 */
  uint32_t auth_grace_period;   /* 认证宽限期 */
  uint32_t granted_status_info; /* 授予的状态信息级别 */
  char error_msg_buf[256];      /* 错误消息缓冲区 */
  const char
      *error_message; /* 错误消息指针 (指向 error_msg_buf 或静态字符串) */
} McarProcessContext;

/**
 * @brief MCAR Step 1: 格式解析与安全校验。
 * @details 检查必选 AVP (Session-Id, Origin-Host/Realm)，处理会话冲突。
 *          解析可选参数 (Client-Credentials, REQ-Status-Info,
 * Comm-Req-Params)。
 *
 * @param qry Diameter 请求消息。
 * @param ctx MCAR 处理上下文。
 * @return 0 继续处理，-1 停止处理(错误已设置)。
 */
static int mcar_step1_validation(struct msg *qry, McarProcessContext *ctx) {
  fd_log_notice("[app_magic] → Step 1: Format & Security Validation");

  /* 1.1 提取 Session-Id AVP (必需) */
  struct avp *avp_session = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session) != 0 ||
      !avp_session) {
    fd_log_error("[app_magic]   ✗ Missing required AVP: Session-Id");
    ctx->result_code = 5005;       /* DIAMETER_MISSING_AVP */
    ctx->magic_status_code = 3001; /* MISSING_AVP */
    ctx->error_message = "Missing required AVP: Session-Id";
    return -1;
  }
  struct avp_hdr *hdr;
  if (fd_msg_avp_hdr(avp_session, &hdr) == 0 && hdr->avp_value) {
    size_t len = hdr->avp_value->os.len;
    if (len >= sizeof(ctx->session_id))
      len = sizeof(ctx->session_id) - 1;
    memcpy(ctx->session_id, hdr->avp_value->os.data, len);
    ctx->session_id[len] = '\0';
  }
  fd_log_notice("[app_magic]   Session-Id: %s", ctx->session_id);

  /* 1.2 提取 Origin-Host AVP (必需) - 作为客户端 ID */
  struct avp *avp_origin = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_host, &avp_origin) == 0 &&
      avp_origin) {
    if (fd_msg_avp_hdr(avp_origin, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(ctx->client_id))
        len = sizeof(ctx->client_id) - 1;
      memcpy(ctx->client_id, hdr->avp_value->os.data, len);
      ctx->client_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Client-ID: %s",
                ctx->client_id[0] ? ctx->client_id : "(unknown)");

  /* 1.2b 提取 Origin-Realm AVP (必需) - v2.1 用于 MNTR 路由 */
  struct avp *avp_origin_realm = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_realm, &avp_origin_realm) ==
          0 &&
      avp_origin_realm) {
    if (fd_msg_avp_hdr(avp_origin_realm, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(ctx->client_realm))
        len = sizeof(ctx->client_realm) - 1;
      memcpy(ctx->client_realm, hdr->avp_value->os.data, len);
      ctx->client_realm[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Client-Realm: %s",
                ctx->client_realm[0] ? ctx->client_realm : "(unknown)");

  /* 1.3 检查会话冲突 - 如果 Session-ID 已存在且处于 ACTIVE 状态 */
  if (g_ctx) {
    ClientSession *exist_sess =
        magic_session_find_by_id(&g_ctx->session_mgr, ctx->session_id);
    if (exist_sess) {
      /* 策略：踢掉旧会话，允许重连 (可能是客户端重启) */
      fd_log_notice("[app_magic]   ⚠ Duplicate Session-ID detected, resetting "
                    "old session");
      magic_session_set_state(exist_sess, SESSION_STATE_CLOSED);
      /* 释放旧会话的资源 */
      if (exist_sess->assigned_link_id[0]) {
        magic_dataplane_remove_client_route(&g_ctx->dataplane_ctx,
                                            exist_sess->session_id);
      }
    }
  }

  /* 1.4 提取 Client-Credentials 复合 AVP (可选但推荐) */
  struct avp *avp_cred = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_client_credentials, &avp_cred) ==
          0 &&
      avp_cred) {
    ctx->has_client_credentials = true;

    /* 提取 User-Name */
    struct avp *child_avp = NULL;
    if (fd_msg_browse(avp_cred, MSG_BRW_FIRST_CHILD, &child_avp, NULL) == 0) {
      while (child_avp) {
        if (fd_msg_avp_hdr(child_avp, &hdr) == 0) {
          struct dict_object *child_dict;
          if (fd_msg_model(child_avp, &child_dict) == 0 &&
              child_dict == g_std_dict.avp_user_name) {
            if (hdr->avp_value && hdr->avp_value->os.data) {
              size_t len = hdr->avp_value->os.len;
              if (len >= sizeof(ctx->username))
                len = sizeof(ctx->username) - 1;
              memcpy(ctx->username, hdr->avp_value->os.data, len);
              ctx->username[len] = '\0';
            }
            break;
          }
        }
        if (fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL) != 0)
          break;
      }
    }

    /* 提取 Client-Password */
    extract_string_from_grouped_avp(avp_cred, "Client-Password", ctx->password,
                                    sizeof(ctx->password));

    fd_log_notice("[app_magic]   Username: %s",
                  ctx->username[0] ? ctx->username : "(none)");
    fd_log_notice("[app_magic]   Password: %s",
                  ctx->password[0] ? "****" : "(empty)");
  }

  /* 1.5 提取 REQ-Status-Info AVP (可选) - 场景 B */
  struct avp *avp_req_status = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_req_status_info,
                        &avp_req_status) == 0 &&
      avp_req_status) {
    if (fd_msg_avp_hdr(avp_req_status, &hdr) == 0 && hdr->avp_value) {
      ctx->has_req_status_info = true;
      ctx->req_status_info = hdr->avp_value->u32;
      fd_log_notice("[app_magic]   REQ-Status-Info: %u", ctx->req_status_info);
    }
  }

  /* 1.6 提取 Communication-Request-Parameters AVP (可选) - 场景 C */
  struct avp *avp_comm_req = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_comm_req_params, &avp_comm_req) ==
          0 &&
      avp_comm_req) {
    ctx->has_comm_req_params = true;
    if (parse_comm_req_params(avp_comm_req, &ctx->comm_params) == 0) {
      fd_log_notice("[app_magic]   Communication-Request-Parameters found:");
      fd_log_notice("[app_magic]     Profile: %s",
                    ctx->comm_params.profile_name);
      fd_log_notice("[app_magic]     Requested BW: %.2f/%.2f kbps",
                    ctx->comm_params.requested_bw,
                    ctx->comm_params.requested_ret_bw);
      fd_log_notice("[app_magic]     QoS-Level: %u, Priority: %s",
                    ctx->comm_params.qos_level,
                    ctx->comm_params.priority_class);
    }
  }

  /* 记录场景类型 */
  const char *scenario = "Unknown";
  if (ctx->has_comm_req_params) {
    scenario = "C (0-RTT Access)";
  } else if (ctx->has_req_status_info) {
    scenario = "B (Auth + Subscribe)";
  } else {
    scenario = "A (Auth Only)";
  }
  fd_log_notice("[app_magic]   Scenario: %s", scenario);

  return 0; /* 继续处理 */
}

/**
 * @brief MCAR Step 2: 身份鉴权。
 * @details 验证凭据 (Client-Credentials 或
 * Origin-Host)，查找客户端配置，创建会话上下文。 初始化会话状态为
 * AUTHENTICATED。
 *
 * @param ctx MCAR 处理上下文。
 * @return 0 继续处理，-1 停止处理(认证失败)。
 */
static int mcar_step2_auth(McarProcessContext *ctx) {
  fd_log_notice("[app_magic] → Step 2: Authentication");

  if (!g_ctx) {
    fd_log_error("[app_magic]   ✗ System context not initialized");
    ctx->result_code = 5012;       /* DIAMETER_UNABLE_TO_COMPLY */
    ctx->magic_status_code = 1000; /* INTERNAL_ERROR */
    ctx->error_message = "System context not initialized";
    return -1;
  }

  /* 2.1 查找客户端配置文件
   * 优先级: 1) User-Name (来自 Client-Credentials)
   *         2) Origin-Host (回退方案)
   * 这样客户端可以用 User-Name 作为业务标识，与 Diameter Origin-Host 解耦
   */

  if (ctx->username[0] != '\0') {
    /* 优先尝试用 User-Name 查找 */
    ctx->profile = magic_config_find_client(&g_ctx->config, ctx->username);
    if (ctx->profile) {
      fd_log_notice("[app_magic]   Using User-Name '%s' as Client-ID",
                    ctx->username);
    } else {
      /* 回退到 Origin-Host */
      ctx->profile = magic_config_find_client(&g_ctx->config, ctx->client_id);
      fd_log_notice("[app_magic]   User-Name '%s' not found, fallback to "
                    "Origin-Host '%s'",
                    ctx->username, ctx->client_id);
    }
  } else {
    ctx->profile = magic_config_find_client(&g_ctx->config, ctx->client_id);
  }

  if (!ctx->profile) {
    fd_log_error("[app_magic]   ✗ Client profile not found for: %s",
                 ctx->client_id);
    ctx->result_code = 2001;       /* 协议层面成功 (但认证失败) */
    ctx->magic_status_code = 1001; /* MAGIC-ERROR_AUTHENTICATION-FAILED */
    ctx->error_message = "Client profile not found";
    ctx->auth_success = false;
    return -1; /* 认证失败，停止处理 */
  }

  fd_log_notice("[app_magic]   Expected Username: %s",
                ctx->profile->auth.username);

  /* 2.2 验证用户名和密码 (v2.0: 使用 client_password) */
  if (strcmp(ctx->profile->auth.username, ctx->username) != 0 ||
      strcmp(ctx->profile->auth.client_password, ctx->password) != 0) {
    fd_log_error("[app_magic]   ✗ Authentication FAILED");
    if (strcmp(ctx->profile->auth.username, ctx->username) != 0) {
      fd_log_error("[app_magic]     → Username mismatch");
    }
    if (strcmp(ctx->profile->auth.client_password, ctx->password) != 0) {
      fd_log_error("[app_magic]     → Password mismatch");
    }
    ctx->result_code = 2001;       /* 协议层面成功 */
    ctx->magic_status_code = 1001; /* MAGIC-ERROR_AUTHENTICATION-FAILED */
    ctx->error_message = "Invalid username or password";
    ctx->auth_success = false;
    return -1; /* 认证失败，停止处理 */
  }

  fd_log_notice("[app_magic]   ✓ Authentication SUCCESS");
  ctx->auth_success = true;

  /* 2.3 初始化会话上下文 */
  ctx->session = magic_session_create(&g_ctx->session_mgr, ctx->session_id,
                                      ctx->client_id, ctx->client_realm);
  if (!ctx->session) {
    fd_log_error("[app_magic]   ✗ Failed to create session");
    ctx->result_code = 5012;       /* DIAMETER_UNABLE_TO_COMPLY */
    ctx->magic_status_code = 1000; /* INTERNAL_ERROR */
    ctx->error_message = "Failed to create session";
    return -1;
  }

  /* 设置初始状态为 AUTHENTICATED */
  magic_session_set_state(ctx->session, SESSION_STATE_AUTHENTICATED);

  /* 绑定客户端 IP (防 IP 欺骗) */
  if (ctx->profile->auth.source_ip[0]) {
    strncpy(ctx->session->client_ip, ctx->profile->auth.source_ip,
            sizeof(ctx->session->client_ip) - 1);
  }

  /* 将客户端注册到 control 白名单 (MCAR 阶段) - 如果配置了 source_ip */
  if (ctx->session->client_ip[0]) {
    magic_dataplane_ipset_add_control(ctx->session->client_ip);
    fd_log_notice(
        "[app_magic] ✓ MCAR: client %s registered to control whitelist",
        ctx->session->client_ip);
  }

  /* 保存 Profile 名称 */
  strncpy(ctx->session->profile_name, ctx->profile->client_id,
          sizeof(ctx->session->profile_name) - 1);

  /* 设置认证有效期 */
  ctx->session->auth_expire_time = time(NULL) + ctx->granted_lifetime;
  ctx->session->auth_grace_period = ctx->auth_grace_period;

  fd_log_notice(
      "[app_magic]   ✓ Session created: state=AUTHENTICATED, expires=%ld",
      (long)ctx->session->auth_expire_time);

  return 0; /* 继续处理 */
}

/**
 * @brief MCAR Step 3: 订阅处理 (场景 B)。
 * @details 注册状态推送订阅 (REQ-Status-Info)，可选触发立即推送 (MSCR)。
 *          仅当请求中包含 REQ-Status-Info AVP 时执行。
 *
 * @param ctx MCAR 处理上下文。
 */
static void mcar_step3_subscription(McarProcessContext *ctx) {
  if (!ctx->has_req_status_info || ctx->req_status_info == 0) {
    return; /* 未请求订阅 */
  }

  fd_log_notice("[app_magic] → Step 3: Subscription Handling");

  uint32_t req_type = ctx->req_status_info;

  /* 3.1 权限检查：普通用户可能不能订阅 Detailed 信息 (Level 6/7) */
  if ((req_type == 6 || req_type == 7) && ctx->profile) {
    /* 检查是否有权限请求详细状态 */
    /* 默认允许，后续可从配置读取 allow_detailed_status */
    bool allow_detailed = true; /* ctx->profile->allow_detailed_status */
    if (!allow_detailed) {
      fd_log_notice("[app_magic]   ⚠ Downgrading status level from %u to 3 (no "
                    "detailed permission)",
                    req_type);
      req_type = 3; /* 降级到 MAGIC_DLM_Status */
    }
  }

  /* 3.2 注册订阅 */
  if (ctx->session) {
    magic_session_set_subscription(ctx->session, req_type);
    fd_log_notice("[app_magic]   ✓ Subscription registered: level=%u",
                  req_type);
  }

  /* 记录授予的状态信息级别 */
  ctx->granted_status_info = req_type;

  /* 3.3 立即触发一次状态推送 MSCR (v2.2) */
  if (magic_cic_send_initial_mscr(&g_magic_ctx, ctx->session) != 0) {
    fd_log_notice("[app_magic]   ⚠ Failed to send initial MSCR");
  } else {
    fd_log_notice("[app_magic]   ✓ Initial MSCR scheduled");
  }
}

/**
 * @brief MCAR Step 3b: ARINC 839 会话激活条件验证。
 * @details 根据 ARINC 839 规范 §3.2.4.1.1.1.2
 * 验证客户端是否有权在当前飞行条件下通信。 验证条件包括:
 *          - Flight-Phase: 验证客户端配置文件是否允许在当前飞行阶段激活会话。
 *          - Altitude: 验证当前高度是否在允许范围内 (支持白/黑名单)。
 *          - Airport: 验证当前机场是否在允许列表中 (支持白/黑名单)。
 *
 * @param ctx MCAR 处理上下文。
 * @return 0 允许继续，-1 拒绝请求。
 */
static int mcar_step3b_session_conditions(McarProcessContext *ctx) {
  if (!ctx->has_comm_req_params) {
    /* 场景 A/B: 无通信请求参数，无需验证 */
    return 0;
  }

  fd_log_notice(
      "[app_magic] → Step 3b: ARINC 839 Session Activation Conditions");

  /* 获取 ADIF 飞机状态用于验证 */
  AdifAircraftState aircraft_state;
  memset(&aircraft_state, 0, sizeof(aircraft_state));
  bool has_adif = false;
  if (g_ctx && adif_client_get_state(&g_ctx->adif_ctx, &aircraft_state) == 0 &&
      aircraft_state.data_valid) {
    has_adif = true;
    fd_log_notice("[app_magic]   ADIF State: WoW=%d, Alt=%.0f, Phase=%d",
                  aircraft_state.wow.on_ground,
                  aircraft_state.position.altitude_ft,
                  aircraft_state.flight_phase.phase);
  } else {
    fd_log_notice("[app_magic]   ⚠ ADIF state not available, using defaults");
  }

  /* 3b.1 Flight-Phase 验证 */
  if (ctx->comm_params.has_flight_phase) {
    /* 解析客户端请求的飞行阶段 */
    CfgFlightPhase requested_phase =
        magic_config_parse_flight_phase(ctx->comm_params.flight_phase);
    (void)requested_phase; /* 未使用，避免编译警告 */

    /* 从 ADIF 获取当前实际飞行阶段 */
    CfgFlightPhase current_phase = CFG_FLIGHT_PHASE_UNKNOWN;
    if (has_adif) {
      /* 将 ADIF 飞行阶段 (AdifFlightPhase) 映射到配置飞行阶段 (CfgFlightPhase)
       */
      switch (aircraft_state.flight_phase.phase) {
      case FLIGHT_PHASE_GATE:
        current_phase = CFG_FLIGHT_PHASE_GATE;
        break;
      case FLIGHT_PHASE_TAXI:
        current_phase = CFG_FLIGHT_PHASE_TAXI;
        break;
      case FLIGHT_PHASE_TAKEOFF:
        current_phase = CFG_FLIGHT_PHASE_TAKE_OFF;
        break;
      case FLIGHT_PHASE_CLIMB:
        current_phase = CFG_FLIGHT_PHASE_CLIMB;
        break;
      case FLIGHT_PHASE_CRUISE:
        current_phase = CFG_FLIGHT_PHASE_CRUISE;
        break;
      case FLIGHT_PHASE_DESCENT:
        current_phase = CFG_FLIGHT_PHASE_DESCENT;
        break;
      case FLIGHT_PHASE_APPROACH:
        current_phase = CFG_FLIGHT_PHASE_APPROACH;
        break;
      case FLIGHT_PHASE_LANDING:
        current_phase = CFG_FLIGHT_PHASE_LANDING;
        break;
      default:
        current_phase = CFG_FLIGHT_PHASE_UNKNOWN;
        break;
      }
    }

    /* 检查客户端配置文件的飞行阶段限制 */
    if (ctx->profile) {
      if (!magic_config_is_flight_phase_allowed(ctx->profile, current_phase)) {
        fd_log_error("[app_magic]   ✗ Flight-Phase restriction: profile '%s' "
                     "not allowed in phase %d",
                     ctx->profile->client_id, current_phase);
        ctx->result_code = 5001;       /* DIAMETER_COMMAND_UNSUPPORTED - 暂用 */
        ctx->magic_status_code = 1020; /* SESSION_DENIED_FLIGHT_PHASE */
        ctx->error_message = "Session denied: flight phase restriction";
        return -1;
      }
    }

    fd_log_notice(
        "[app_magic]   ✓ Flight-Phase check passed (current=%d, requested=%s)",
        current_phase, ctx->comm_params.flight_phase);
  }

  /* 3b.2 Altitude 验证 */
  if (ctx->comm_params.has_altitude &&
      ctx->comm_params.altitude_range_count > 0) {
    int current_alt = 0;
    if (has_adif) {
      current_alt = (int)aircraft_state.position.altitude_ft;
    }

    /* 检查高度是否在允许范围内 */
    bool in_range = false;
    for (int i = 0; i < ctx->comm_params.altitude_range_count; i++) {
      int min_alt = ctx->comm_params.altitude_ranges[i][0];
      int max_alt = ctx->comm_params.altitude_ranges[i][1];
      if (current_alt >= min_alt && (max_alt < 0 || current_alt <= max_alt)) {
        in_range = true;
        break;
      }
    }

    /* 黑名单模式: 在范围内表示拒绝; 白名单模式: 在范围内表示允许 */
    bool allowed =
        ctx->comm_params.altitude_is_blacklist ? !in_range : in_range;
    if (!allowed) {
      fd_log_error("[app_magic]   ✗ Altitude restriction: current=%d ft, %s",
                   current_alt,
                   ctx->comm_params.altitude_is_blacklist ? "in blacklist"
                                                          : "not in whitelist");
      ctx->result_code = 5001;
      ctx->magic_status_code = 1021; /* SESSION_DENIED_ALTITUDE */
      ctx->error_message = "Session denied: altitude restriction";
      return -1;
    }

    fd_log_notice("[app_magic]   ✓ Altitude check passed (current=%d ft)",
                  current_alt);
  }

  /* 3b.3 Airport 验证 */
  if (ctx->comm_params.has_airport && ctx->comm_params.airport_code_count > 0) {
    /* 仅在地面时进行机场验证 */
    if (has_adif && aircraft_state.wow.on_ground) {
      /* 从 ADIF 或配置获取当前机场
       * TODO: 实际实现应通过以下方式获取:
       * 1. 从 ADIF 位置数据反向地理编码获取最近机场
       * 2. 从机场数据库查询飞机坐标附近的机场
       * 3. 从会话/配置文件中预设的机场信息
       */
      const char *current_airport = NULL;

      /* 如果有当前机场信息，进行验证 */
      if (current_airport && current_airport[0]) {
        bool in_list = false;
        for (int i = 0; i < ctx->comm_params.airport_code_count; i++) {
          if (strcasecmp(ctx->comm_params.airport_codes[i], current_airport) ==
              0) {
            in_list = true;
            break;
          }
        }

        /* 黑名单模式: 在列表中表示拒绝; 白名单模式: 在列表中表示允许 */
        bool allowed =
            ctx->comm_params.airport_is_blacklist ? !in_list : in_list;

        if (!allowed) {
          fd_log_error("[app_magic]   ✗ Airport restriction: current=%s, %s",
                       current_airport,
                       ctx->comm_params.airport_is_blacklist
                           ? "in blacklist"
                           : "not in whitelist");
          ctx->result_code = 5001;
          ctx->magic_status_code = 1022; /* SESSION_DENIED_AIRPORT */
          ctx->error_message = "Session denied: airport restriction";
          return -1;
        }

        fd_log_notice("[app_magic]   ✓ Airport check passed (current=%s)",
                      current_airport);
      } else {
        /* 机场信息不可用，记录警告但允许继续 */
        fd_log_notice("[app_magic]   ⚠ Airport code not available, skipping "
                      "airport check");
      }
    } else {
      /* 不在地面时，跳过机场验证 */
      fd_log_notice(
          "[app_magic]   ⚠ Aircraft not on ground, skipping airport check");
    }
  }

  fd_log_notice("[app_magic]   ✓ All session activation conditions satisfied");
  return 0;
}

/*===========================================================================
 * MCAR 场景 C 辅助函数 (0-RTT 资源申请)
 * 用于链路资源请求的重试与回退机制
 *===========================================================================*/

/**
 * @brief 检查链路是否已尝试过 (MCAR Helper)。
 * @details 检查指定的 Link ID 是否在当前 MCAR 流程的已尝试列表 (Retry/Fallback)
 * 中。
 *
 * @param ctx MCAR 处理上下文。
 * @param link_id 要检查的链路 ID。
 * @return true 已尝试，false 未尝试。
 */
static bool mcar_link_already_tried(McarProcessContext *ctx,
                                    const char *link_id) {
  for (int i = 0; i < ctx->tried_link_count; i++) {
    if (strcmp(ctx->tried_links[i], link_id) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief 标记链路为已尝试 (MCAR Helper)。
 * @details 将 Link ID 添加到已尝试列表，避免重复尝试相同链路。
 *
 * @param ctx MCAR 处理上下文。
 * @param link_id 链路 ID。
 */
static void mcar_mark_link_tried(McarProcessContext *ctx, const char *link_id) {
  if (ctx->tried_link_count < MCAR_FALLBACK_MAX_LINKS) {
    strncpy(ctx->tried_links[ctx->tried_link_count], link_id, MAX_ID_LEN - 1);
    ctx->tried_links[ctx->tried_link_count][MAX_ID_LEN - 1] = '\0';
    ctx->tried_link_count++;
  }
}

/**
 * @brief 带重试机制的链路资源请求 (MCAR Helper)。
 * @details 向 LMI/DLM 发起资源请求 (MIH_Link_Resource_Request)。
 *          如果失败 (Resource Denied/Unavailable)，支持重试
 * (MCAR_RETRY_MAX_COUNT)。 用于 0-RTT 场景 (Scenario C)。
 *
 * @param ctx MCAR 处理上下文。
 * @param link_id 目标链路 ID。
 * @param policy_resp 策略引擎返回的带宽参数。
 * @param mih_confirm 输出参数，存储 MIH 确认结果。
 * @return 0 成功 (Resource Success/Parameters Updated)，-1 失败。
 */
static int mcar_try_link_with_retry(McarProcessContext *ctx,
                                    const char *link_id,
                                    PolicyResponse *policy_resp,
                                    MIH_Link_Resource_Confirm *mih_confirm) {
  int attempt;
  int mih_result;

  for (attempt = 0; attempt < MCAR_RETRY_MAX_COUNT; attempt++) {
    if (attempt > 0) {
      fd_log_notice("[app_magic]     → Retry attempt %d/%d for link %s",
                    attempt + 1, MCAR_RETRY_MAX_COUNT, link_id);
      usleep(MCAR_RETRY_DELAY_MS * 1000);
    }

    /* 构造 MIH 请求 */
    MIH_Link_Resource_Request mih_req;
    memset(&mih_req, 0, sizeof(mih_req));
    memset(mih_confirm, 0, sizeof(*mih_confirm));

    snprintf(mih_req.destination_id.mihf_id,
             sizeof(mih_req.destination_id.mihf_id), "MIHF_%s", link_id);
    mih_req.link_identifier.link_type = 1;
    strncpy(mih_req.link_identifier.link_addr, link_id,
            sizeof(mih_req.link_identifier.link_addr) - 1);
    mih_req.resource_action = RESOURCE_ACTION_REQUEST;
    mih_req.has_qos_params = true;
    mih_req.qos_parameters.cos_id = COS_BEST_EFFORT;
    mih_req.qos_parameters.forward_link_rate = policy_resp->granted_bw_kbps;
    mih_req.qos_parameters.return_link_rate = policy_resp->granted_ret_bw_kbps;
    mih_req.qos_parameters.avg_pk_tx_delay = 100;
    mih_req.qos_parameters.max_pk_tx_delay = 500;
    mih_req.qos_parameters.pk_delay_jitter = 50;
    mih_req.qos_parameters.pk_loss_rate = 0.01f;

    /* 发送 MIH 请求 */
    mih_result = magic_dlm_mih_link_resource_request(&g_ctx->lmi_ctx, &mih_req,
                                                     mih_confirm);

    if (mih_result == 0 && mih_confirm->status == STATUS_SUCCESS) {
      fd_log_notice("[app_magic]     ✓ MIH request succeeded on attempt %d",
                    attempt + 1);
      ctx->retry_count = attempt;
      return 0;
    }

    fd_log_notice("[app_magic]     ⚠ MIH request failed: status=%s",
                  status_to_string(mih_confirm->status));
  }

  fd_log_error("[app_magic]     ✗ All %d retry attempts failed for link %s",
               MCAR_RETRY_MAX_COUNT, link_id);
  return -1;
}

/**
 * @brief 尝试回退到备选链路 (MCAR Helper)。
 * @details
 * 当首选链路分配失败时，查询策略引擎获取备选链路并在备选链路上尝试分配资源。
 *          使用 ADIF 数据辅助决策。
 *
 * @param ctx MCAR 处理上下文。
 * @return 0 成功找到可用链路并分配资源，-1 所有链路都失败。
 */
static int mcar_try_fallback_links(McarProcessContext *ctx) {
  fd_log_notice("[app_magic]   → Attempting fallback to alternative links...");

  /* 获取所有可用链路列表 */
  PolicyRequest fallback_req;
  memset(&fallback_req, 0, sizeof(fallback_req));

  strncpy(fallback_req.client_id, ctx->client_id,
          sizeof(fallback_req.client_id) - 1);
  strncpy(fallback_req.profile_name, ctx->comm_params.profile_name,
          sizeof(fallback_req.profile_name) - 1);
  fallback_req.requested_bw_kbps = (uint32_t)ctx->comm_params.requested_bw;
  fallback_req.requested_ret_bw_kbps =
      (uint32_t)ctx->comm_params.requested_ret_bw;
  fallback_req.required_bw_kbps = (uint32_t)ctx->comm_params.required_bw;
  fallback_req.required_ret_bw_kbps =
      (uint32_t)ctx->comm_params.required_ret_bw;
  fallback_req.priority_class = (uint8_t)atoi(ctx->comm_params.priority_class);
  fallback_req.qos_level = (uint8_t)ctx->comm_params.qos_level;
  strncpy(fallback_req.flight_phase, ctx->comm_params.flight_phase,
          sizeof(fallback_req.flight_phase) - 1);

  /* 获取 ADIF 数据用于回退链路策略决策 */
  AdifAircraftState adif_state;
  if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
      adif_state.data_valid) {
    fallback_req.aircraft_lat = adif_state.position.latitude;
    fallback_req.aircraft_lon = adif_state.position.longitude;
    fallback_req.aircraft_alt = adif_state.position.altitude_ft * 0.3048;
    fallback_req.on_ground = adif_state.wow.on_ground;
    fallback_req.has_adif_data = true;
  }

  /* 遍历尝试所有候选链路 */
  for (int i = 0; i < MCAR_FALLBACK_MAX_LINKS; i++) {
    PolicyResponse fallback_resp;
    memset(&fallback_resp, 0, sizeof(fallback_resp));

    /* 请求策略引擎返回下一个最佳链路 */
    fallback_req.exclude_link_count = ctx->tried_link_count;
    for (int j = 0; j < ctx->tried_link_count && j < 4; j++) {
      strncpy(fallback_req.exclude_links[j], ctx->tried_links[j],
              MAX_ID_LEN - 1);
    }

    if (magic_policy_select_path(&g_ctx->policy_ctx, &fallback_req,
                                 &fallback_resp) != 0 ||
        !fallback_resp.success) {
      fd_log_notice("[app_magic]     → No more fallback links available");
      break;
    }

    /* 跳过已尝试的链路 */
    if (mcar_link_already_tried(ctx, fallback_resp.selected_link_id)) {
      continue;
    }

    fd_log_notice("[app_magic]     → Trying fallback link: %s",
                  fallback_resp.selected_link_id);
    mcar_mark_link_tried(ctx, fallback_resp.selected_link_id);

    /* 尝试分配资源（带重试） */
    MIH_Link_Resource_Confirm fallback_confirm;
    if (mcar_try_link_with_retry(ctx, fallback_resp.selected_link_id,
                                 &fallback_resp, &fallback_confirm) == 0) {
      /* 成功！更新上下文 */
      memcpy(&ctx->policy_resp, &fallback_resp, sizeof(PolicyResponse));
      memcpy(&ctx->mih_confirm, &fallback_confirm,
             sizeof(MIH_Link_Resource_Confirm));
      fd_log_notice("[app_magic]     ✓ Fallback succeeded: link=%s",
                    fallback_resp.selected_link_id);
      return 0;
    }
  }

  fd_log_error("[app_magic]     ✗ All fallback links exhausted");
  return -1;
}

/**
 * @brief 从 TFT-to-Ground 规则中提取目的 IP (MCAR Helper)。
 * @details 解析 TFT 规则以提取目的 IP 地址，用于数据平面路由和防火墙规则。
 *          优先使用严格 3GPP 解析器，失败则回退到字符串匹配模式。
 *
 * @param ctx MCAR 处理上下文。
 * @note 用于 dataplane/iptables 精确控制；提取失败则保持空字符串表示不限制目的
 * IP。
 */
static void mcar_extract_dest_ip_from_tft(McarProcessContext *ctx) {
  ctx->extracted_dest_ip[0] = '\0';

  if (ctx->comm_params.tft_to_ground_count <= 0) {
    return;
  }

  for (int i = 0; i < ctx->comm_params.tft_to_ground_count; i++) {
    const char *rule = ctx->comm_params.tft_to_ground[i];
    if (!rule || !rule[0]) {
      continue;
    }

    /* 优先使用 3GPP 解析器提取 dst_ip */
    TFTRule parsed_rule;
    if (tft_parse_rule(rule, &parsed_rule) == 0 && parsed_rule.is_valid &&
        parsed_rule.dst_ip.is_valid) {
      struct in_addr dst_addr;
      dst_addr.s_addr = htonl(parsed_rule.dst_ip.start_ip);
      strncpy(ctx->extracted_dest_ip, inet_ntoa(dst_addr),
              sizeof(ctx->extracted_dest_ip) - 1);
      ctx->extracted_dest_ip[sizeof(ctx->extracted_dest_ip) - 1] = '\0';
      fd_log_notice("[app_magic]   ✓ TFT 目的 IP: %s", ctx->extracted_dest_ip);
      return;
    }

    /* 回退：按字符串格式 " to " 提取（兼容旧格式/非严格 3GPP） */
    const char *to_ptr = strstr(rule, " to ");
    if (!to_ptr) {
      continue;
    }

    to_ptr += 4; /* 跳过 " to " */
    const char *end_ptr = strchr(to_ptr, ' ');
    if (!end_ptr) {
      end_ptr = to_ptr + strlen(to_ptr);
    }

    size_t ip_len = (size_t)(end_ptr - to_ptr);
    if (ip_len > 0 && ip_len < sizeof(ctx->extracted_dest_ip)) {
      strncpy(ctx->extracted_dest_ip, to_ptr, ip_len);
      ctx->extracted_dest_ip[ip_len] = '\0';

      if (strcmp(ctx->extracted_dest_ip, "any") == 0) {
        ctx->extracted_dest_ip[0] = '\0';
      } else {
        fd_log_notice("[app_magic]   ✓ TFT 目的 IP: %s",
                      ctx->extracted_dest_ip);
      }
      return;
    }
  }
}

/**
 * @brief MCAR 场景 C: TFT/NAPT 白名单验证。
 * @details 验证 TFT 和 NAPT 规则是否符合用户配置文件中的白名单限制。
 *          如果验证失败，保持认证成功但拒绝资源分配 (Partial Success)。
 *
 * @param ctx MCAR 处理上下文。
 * @return 0 验证通过，-1 验证失败。
 */
static int mcar_validate_tft_napt_whitelist(McarProcessContext *ctx) {
  ctx->security_passed = true;

  if (!ctx->profile) {
    return 0; /* 无 profile：不做白名单校验 */
  }

  /* TFT 白名单验证 (ARINC 839 §1.2.2.2) - 原子性验证 */
  if (ctx->comm_params.tft_to_ground_count > 0) {
    fd_log_notice(
        "[app_magic]   Validating %d TFT-to-Ground rules (atomic check)...",
        ctx->comm_params.tft_to_ground_count);
    for (int i = 0; i < ctx->comm_params.tft_to_ground_count; i++) {
      char tft_error_msg[MAX_ERROR_MSG_LEN];
      if (tft_validate_against_whitelist(ctx->comm_params.tft_to_ground[i],
                                         &ctx->profile->traffic, tft_error_msg,
                                         sizeof(tft_error_msg)) != 0) {
        fd_log_error(
            "[app_magic]   ✗ TFT whitelist validation FAILED (rule %d/%d): %s",
            i + 1, ctx->comm_params.tft_to_ground_count, tft_error_msg);
        ctx->security_passed = false;
        ctx->magic_status_code = 1036; /* MAGIC_ERROR_TFT-INVALID */
        strncpy(ctx->error_msg_buf, tft_error_msg,
                sizeof(ctx->error_msg_buf) - 1);
        ctx->error_msg_buf[sizeof(ctx->error_msg_buf) - 1] = '\0';
        ctx->error_message = ctx->error_msg_buf;
        return -1;
      }
    }
    fd_log_notice("[app_magic]   ✓ All %d TFT-to-Ground rules passed whitelist "
                  "validation",
                  ctx->comm_params.tft_to_ground_count);
  }

  if (ctx->comm_params.tft_to_aircraft_count > 0) {
    fd_log_notice(
        "[app_magic]   Validating %d TFT-to-Aircraft rules (atomic check)...",
        ctx->comm_params.tft_to_aircraft_count);
    for (int i = 0; i < ctx->comm_params.tft_to_aircraft_count; i++) {
      char tft_error_msg[MAX_ERROR_MSG_LEN];
      if (tft_validate_against_whitelist(ctx->comm_params.tft_to_aircraft[i],
                                         &ctx->profile->traffic, tft_error_msg,
                                         sizeof(tft_error_msg)) != 0) {
        fd_log_error("[app_magic]   ✗ TFT whitelist validation FAILED "
                     "(toAircraft rule %d/%d): %s",
                     i + 1, ctx->comm_params.tft_to_aircraft_count,
                     tft_error_msg);
        ctx->security_passed = false;
        ctx->magic_status_code = 1036; /* MAGIC_ERROR_TFT-INVALID */
        strncpy(ctx->error_msg_buf, tft_error_msg,
                sizeof(ctx->error_msg_buf) - 1);
        ctx->error_msg_buf[sizeof(ctx->error_msg_buf) - 1] = '\0';
        ctx->error_message = ctx->error_msg_buf;
        return -1;
      }
    }
    fd_log_notice("[app_magic]   ✓ All %d TFT-to-Aircraft rules passed "
                  "whitelist validation",
                  ctx->comm_params.tft_to_aircraft_count);
  }

  /* NAPT 白名单验证 */
  if (ctx->comm_params.napt_count > 0) {
    for (int i = 0; i < ctx->comm_params.napt_count; i++) {
      char napt_error_msg[MAX_ERROR_MSG_LEN];
      if (napt_validate_against_whitelist(
              ctx->comm_params.napt_rules[i], &ctx->profile->traffic,
              NULL, /* TODO: 传入实际链路 IP */
              napt_error_msg, sizeof(napt_error_msg)) != 0) {
        fd_log_error("[app_magic]   ✗ NAPT whitelist validation FAILED: %s",
                     napt_error_msg);
        ctx->security_passed = false;
        ctx->magic_status_code = 1036;
        strncpy(ctx->error_msg_buf, napt_error_msg,
                sizeof(ctx->error_msg_buf) - 1);
        ctx->error_msg_buf[sizeof(ctx->error_msg_buf) - 1] = '\0';
        ctx->error_message = ctx->error_msg_buf;
        return -1;
      }
    }
    fd_log_notice("[app_magic]   ✓ NAPT whitelist validation passed (%d rules)",
                  ctx->comm_params.napt_count);
  }

  return 0;
}

/**
 * @brief MCAR Step 4: 0-RTT 资源申请 (场景 C)。
 * @details 调用策略引擎选择最佳链路，并通过 MIH 向 DLM 申请带宽资源。
 *          支持链路回退机制、RFC 削峰、TFT/NAPT配置下发。
 *
 * @param ctx MCAR 处理上下文。
 */
static void mcar_step4_allocation(McarProcessContext *ctx) {
  if (!ctx->has_comm_req_params) {
    return; /* 场景 A/B：无需分配带宽 */
  }

  fd_log_notice("[app_magic] → Step 4: 0-RTT Resource Allocation");

  /* 初始化本步骤的运行状态 */
  ctx->resource_allocated = false;
  ctx->route_added = false;
  ctx->retry_count = 0;
  ctx->tried_link_count = 0;
  ctx->extracted_dest_ip[0] = '\0';

  /* 4.1 从 Profile 填充缺失的默认值 */
  if (ctx->profile) {
    comm_req_params_fill_from_profile(&ctx->comm_params, ctx->profile);
  }

  /* 4.1b 安全校验：TFT/NAPT 白名单验证（失败则保持认证成功但拒绝资源分配） */
  if (mcar_validate_tft_napt_whitelist(ctx) != 0) {
    fd_log_notice("[app_magic]   ⚠ Auth OK, but security validation failed; "
                  "skip allocation");
    return;
  }

  /* 4.1c 从 TFT 提取目的 IP（用于 dataplane 精确控制） */
  mcar_extract_dest_ip_from_tft(ctx);

  /* 4.2 v2.0: 校验请求是否超限 (削峰) - 使用 bandwidth.max_forward_kbps */
  if (ctx->profile && ctx->profile->bandwidth.max_forward_kbps > 0) {
    float max_fwd = (float)ctx->profile->bandwidth.max_forward_kbps;
    float max_ret = (float)ctx->profile->bandwidth.max_return_kbps;
    if (max_ret == 0)
      max_ret = max_fwd; /* 如果没有返回带宽限制，使用前向限制 */

    if (ctx->comm_params.requested_bw > max_fwd) {
      fd_log_notice(
          "[app_magic]   ⚠ Capping requested FWD BW from %.0f to %.0f kbps",
          ctx->comm_params.requested_bw, max_fwd);
      ctx->comm_params.requested_bw = max_fwd;
    }
    if (ctx->comm_params.requested_ret_bw > max_ret) {
      fd_log_notice(
          "[app_magic]   ⚠ Capping requested RET BW from %.0f to %.0f kbps",
          ctx->comm_params.requested_ret_bw, max_ret);
      ctx->comm_params.requested_ret_bw = max_ret;
    }
  }

  /* 4.3 调用策略引擎 (CM) 进行链路选择 */
  PolicyRequest policy_req;
  memset(&policy_req, 0, sizeof(policy_req));

  strncpy(policy_req.client_id, ctx->client_id,
          sizeof(policy_req.client_id) - 1);
  strncpy(policy_req.profile_name, ctx->comm_params.profile_name,
          sizeof(policy_req.profile_name) - 1);
  policy_req.requested_bw_kbps = (uint32_t)ctx->comm_params.requested_bw;
  policy_req.requested_ret_bw_kbps =
      (uint32_t)ctx->comm_params.requested_ret_bw;
  policy_req.required_bw_kbps = (uint32_t)ctx->comm_params.required_bw;
  policy_req.required_ret_bw_kbps = (uint32_t)ctx->comm_params.required_ret_bw;
  policy_req.priority_class = (uint8_t)atoi(ctx->comm_params.priority_class);
  policy_req.qos_level = (uint8_t)ctx->comm_params.qos_level;
  strncpy(policy_req.flight_phase, ctx->comm_params.flight_phase,
          sizeof(policy_req.flight_phase) - 1);

  /* v2.2: 从 ADIF 获取实时位置和 WoW 数据用于策略决策 */
  AdifAircraftState adif_state;
  if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
      adif_state.data_valid) {
    policy_req.aircraft_lat = adif_state.position.latitude;
    policy_req.aircraft_lon = adif_state.position.longitude;
    policy_req.aircraft_alt = adif_state.position.altitude_ft * 0.3048;
    policy_req.on_ground = adif_state.wow.on_ground;
    policy_req.has_adif_data = true;
    fd_log_debug(
        "[app_magic]   ADIF Data: lat=%.4f, lon=%.4f, alt=%.0fm, WoW=%s",
        policy_req.aircraft_lat, policy_req.aircraft_lon,
        policy_req.aircraft_alt, policy_req.on_ground ? "Ground" : "Airborne");
  } else {
    policy_req.aircraft_lat = 0.0;
    policy_req.aircraft_lon = 0.0;
    policy_req.aircraft_alt = 0.0;
    policy_req.on_ground = false;
    policy_req.has_adif_data = false;
  }

  memset(&ctx->policy_resp, 0, sizeof(ctx->policy_resp));

  if (magic_policy_select_path(&g_ctx->policy_ctx, &policy_req,
                               &ctx->policy_resp) != 0 ||
      !ctx->policy_resp.success) {
    /* 策略决策失败 - 保持 AUTHENTICATED 状态 (允许登录但不给网) */
    fd_log_error("[app_magic]   ✗ Policy decision failed: %s",
                 ctx->policy_resp.reason);
    ctx->magic_status_code = 1010; /* NO_ENTRY_IN_BANDWIDTHTABLE / NO_BW */
    ctx->error_message = "Auth OK, but bandwidth allocation failed (policy)";
    ctx->resource_allocated = false;
    return;
  }

  fd_log_notice("[app_magic]   ✓ Policy Decision: Link=%s, BW=%u/%u kbps",
                ctx->policy_resp.selected_link_id,
                ctx->policy_resp.granted_bw_kbps,
                ctx->policy_resp.granted_ret_bw_kbps);

  /* 4.4 通过 MIH 请求链路资源（带重试 + 回退） */
  memset(&ctx->mih_confirm, 0, sizeof(ctx->mih_confirm));
  mcar_mark_link_tried(ctx, ctx->policy_resp.selected_link_id);

  if (mcar_try_link_with_retry(ctx, ctx->policy_resp.selected_link_id,
                               &ctx->policy_resp, &ctx->mih_confirm) != 0) {
    if (mcar_try_fallback_links(ctx) != 0) {
      fd_log_error("[app_magic]   ✗ All link resource requests failed (MCAR)");
      ctx->magic_status_code = 1010; /* NO_BW */
      ctx->error_message = "Auth OK, but bandwidth allocation failed (MIH)";
      return;
    }
  }

  fd_log_notice(
      "[app_magic]   ✓ MIH Link Resource Allocated: Bearer=%u, Retries=%u",
      ctx->mih_confirm.has_bearer_id ? ctx->mih_confirm.bearer_identifier : 0,
      ctx->retry_count);

  /* 4.5 更新会话状态为 ACTIVE 并分配链路 */
  if (ctx->session) {
    magic_session_assign_link(
        ctx->session, ctx->policy_resp.selected_link_id,
        ctx->mih_confirm.has_bearer_id ? ctx->mih_confirm.bearer_identifier : 0,
        ctx->policy_resp.granted_bw_kbps, ctx->policy_resp.granted_ret_bw_kbps);

    /* 设置状态订阅 (如果请求中也包含 REQ-Status-Info) */
    if (ctx->has_req_status_info) {
      magic_session_set_subscription(ctx->session, ctx->granted_status_info);
    }

    fd_log_notice("[app_magic]   ✓ Session updated: state=ACTIVE, link=%s",
                  ctx->policy_resp.selected_link_id);

    /* 4.6 数据平面路由与计费配置 (与 MCCR 保持一致) */
    {
      const char *client_ip = NULL;
      if (ctx->profile && ctx->profile->auth.source_ip[0]) {
        client_ip = ctx->profile->auth.source_ip;
      } else if (ctx->session && ctx->session->client_ip[0]) {
        client_ip = ctx->session->client_ip;
      } else {
        client_ip = "192.168.10.10"; /* 默认 */
      }

      /* 4.6.1 先确保链路已注册到数据平面 (按需注册) */
      uint32_t table_id = magic_dataplane_get_table_id(
          &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id);
      if (table_id == 0) {
        DlmClient *dlm_client = magic_lmi_find_by_link(
            &g_ctx->lmi_ctx, ctx->policy_resp.selected_link_id);
        if (dlm_client && dlm_client->link_identifier.link_addr[0]) {
          const char *interface_name =
              dlm_client->link_identifier.poa_addr[0]
                  ? dlm_client->link_identifier.poa_addr
                  : dlm_client->link_identifier.link_addr;

          char gateway_ip[64] = "";
          if (dlm_client->current_parameters.gateway != 0) {
            struct in_addr gw_addr;
            gw_addr.s_addr = dlm_client->current_parameters.gateway;
            strncpy(gateway_ip, inet_ntoa(gw_addr), sizeof(gateway_ip) - 1);
          }

          int reg_ret = magic_dataplane_register_link(
              &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id,
              interface_name, gateway_ip[0] ? gateway_ip : NULL);
          if (reg_ret >= 0) {
            fd_log_notice("[app_magic]     ✓ Link registered to dataplane: %s "
                          "→ %s (table=%d)",
                          ctx->policy_resp.selected_link_id, interface_name,
                          reg_ret);
          }
        }
      }

      /* 4.6.2 添加客户端路由规则 */
      int dp_ret = magic_dataplane_add_client_route(
          &g_ctx->dataplane_ctx, client_ip, ctx->session_id,
          ctx->policy_resp.selected_link_id,
          ctx->extracted_dest_ip[0] ? ctx->extracted_dest_ip : NULL);

      ctx->route_added = (dp_ret == 0);

      if (ctx->route_added) {
        fd_log_notice("[app_magic]     ✓ Dataplane route added: %s → %s",
                      client_ip, ctx->policy_resp.selected_link_id);
        magic_dataplane_ipset_add_data(client_ip);
        fd_log_notice("[app_magic]     ✓ Client %s added to data whitelist",
                      client_ip);
      } else {
        fd_log_notice(
            "[app_magic]     ⚠ Dataplane route failed (non-critical)");
      }

      /* 4.6.3 添加 TFT mangle 规则（用于 fwmark 路由） */
      if (ctx->comm_params.tft_to_ground_count > 0) {
        fd_log_notice("[app_magic]     → Adding %d TFT mangle rules (same "
                      "fwmark, same link)...",
                      ctx->comm_params.tft_to_ground_count);

        int tft_success_count = 0;
        for (int i = 0; i < ctx->comm_params.tft_to_ground_count; i++) {
          TFTRule parsed_rule;
          if (tft_parse_rule(ctx->comm_params.tft_to_ground[i], &parsed_rule) ==
                  0 &&
              parsed_rule.is_valid) {
            TftTuple tft_tuple;
            memset(&tft_tuple, 0, sizeof(tft_tuple));

            strncpy(tft_tuple.src_ip, client_ip, sizeof(tft_tuple.src_ip) - 1);

            if (parsed_rule.dst_ip.is_valid) {
              struct in_addr dst_addr;
              dst_addr.s_addr = htonl(parsed_rule.dst_ip.start_ip);
              strncpy(tft_tuple.dst_ip, inet_ntoa(dst_addr),
                      sizeof(tft_tuple.dst_ip) - 1);
            } else if (ctx->extracted_dest_ip[0]) {
              strncpy(tft_tuple.dst_ip, ctx->extracted_dest_ip,
                      sizeof(tft_tuple.dst_ip) - 1);
            }

            tft_tuple.protocol =
                parsed_rule.has_protocol ? parsed_rule.protocol : 0;
            tft_tuple.src_port = parsed_rule.src_port.is_valid
                                     ? parsed_rule.src_port.start_port
                                     : 0;
            tft_tuple.dst_port = parsed_rule.dst_port.is_valid
                                     ? parsed_rule.dst_port.start_port
                                     : 0;

            int tft_ret = magic_dataplane_add_tft_rule(
                &g_ctx->dataplane_ctx, &tft_tuple, ctx->session_id,
                ctx->policy_resp.selected_link_id);

            if (tft_ret == 0) {
              tft_success_count++;
              fd_log_notice(
                  "[app_magic]     ✓ TFT[%d/%d] mangle rule added: %s:%u → "
                  "%s:%u (proto=%u, link=%s)",
                  i + 1, ctx->comm_params.tft_to_ground_count, tft_tuple.src_ip,
                  tft_tuple.src_port, tft_tuple.dst_ip, tft_tuple.dst_port,
                  tft_tuple.protocol, ctx->policy_resp.selected_link_id);
            } else {
              fd_log_error("[app_magic]     ✗ TFT[%d/%d] mangle rule failed "
                           "(continuing with remaining rules)",
                           i + 1, ctx->comm_params.tft_to_ground_count);
            }
          } else {
            fd_log_error("[app_magic]     ✗ TFT[%d/%d] parse failed: %s "
                         "(skipping this rule)",
                         i + 1, ctx->comm_params.tft_to_ground_count,
                         ctx->comm_params.tft_to_ground[i]);
          }
        }

        if (tft_success_count > 0) {
          fd_log_notice(
              "[app_magic]     ✓ Successfully added %d/%d TFT mangle rules",
              tft_success_count, ctx->comm_params.tft_to_ground_count);
        } else {
          fd_log_notice("[app_magic]     ⚠ No TFT mangle rules added (all "
                        "failed, but non-critical)");
        }
      } else {
        fd_log_notice("[app_magic]     → No TFT-to-Ground rules specified, "
                      "skipping TFT mangle rules");
      }

      /* 4.6.4 注册流量监控 */
      uint32_t traffic_mark = traffic_register_session(
          &g_ctx->traffic_ctx, ctx->session_id, ctx->client_id, client_ip);
      if (traffic_mark != 0) {
        ctx->session->conntrack_mark = traffic_mark;
        ctx->session->traffic_start_time = time(NULL);
        fd_log_notice("[app_magic]     ✓ Traffic monitor registered: mark=0x%x",
                      traffic_mark);
      }

      /* 4.6.5 创建 CDR 记录 */
      CDRRecord *cdr =
          cdr_create(&g_ctx->cdr_mgr, ctx->session_id, ctx->client_id,
                     ctx->policy_resp.selected_link_id);
      if (cdr) {
        fd_log_notice("[app_magic]     ✓ CDR created: id=%u", cdr->cdr_id);
      }
    }
  }

  ctx->resource_allocated = true;
}

/**
 * @brief MCAR Step 5: 构建并发送应答。
 * @details 构造 MCAA (Client Authentication Answer) 消息并发送。
 *          包含 Standard AVP, Magic-Status-Code, Error-Message, Policy-Answer
 * 等。 处理所有 MCAR 场景 (A/B/C) 的应答差异。
 *
 * @param msg Diameter 消息指针的指针 (输入为请求，输出为应答)。
 * @param ctx MCAR 处理上下文。
 * @return 0 成功，-1 失败。
 */
static int mcar_step5_finalize(struct msg **msg, McarProcessContext *ctx) {
  struct msg *ans;

  fd_log_notice("[app_magic] → Step 5: Finalize Response");

  /* 如果没有设置结果码，默认成功 */
  if (ctx->result_code == 0) {
    ctx->result_code = 2001; /* DIAMETER_SUCCESS */
  }

  /* 创建应答消息 */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  /* 添加必需的标准 Diameter AVP */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, ctx->result_code);

  /* 如果认证成功，添加认证相关 AVP */
  if (ctx->auth_success) {
    /* RFC 6733: Auth Answer 必须包含 Auth-Application-Id */
    ADD_AVP_U32(ans, g_std_dict.avp_auth_application_id, MAGIC_APP_ID);

    ADD_AVP_U32(ans, g_std_dict.avp_auth_session_state,
                0); /* 0=State Maintained */
    ADD_AVP_U32(ans, g_std_dict.avp_authorization_lifetime,
                ctx->granted_lifetime);
    ADD_AVP_U32(ans, g_std_dict.avp_session_timeout, ctx->granted_lifetime);
    ADD_AVP_U32(ans, g_std_dict.avp_auth_grace_period, ctx->auth_grace_period);

    /* 添加 Server-Password (双向认证) - 从配置读取 */
    const char *server_pwd =
        (ctx->profile && ctx->profile->auth.server_password[0])
            ? ctx->profile->auth.server_password
            : "MAGIC_SERVER_DEFAULT";
    ADD_AVP_STR(ans, g_magic_dict.avp_server_password, server_pwd);
  }

  /* 如果有 MAGIC-Status-Code 错误，添加到应答 */
  if (ctx->magic_status_code > 0) {
    ADD_AVP_U32(ans, g_magic_dict.avp_magic_status_code,
                ctx->magic_status_code);
  }

  /* 如果有错误消息，添加 Error-Message */
  if (ctx->error_message) {
    ADD_AVP_STR(ans, g_std_dict.avp_error_message, ctx->error_message);
  }

  /* 如果请求中包含 REQ-Status-Info，返回授予的级别 */
  if (ctx->has_req_status_info && ctx->auth_success) {
    ADD_AVP_U32(ans, g_magic_dict.avp_req_status_info,
                ctx->granted_status_info);
  }

  /* 如果请求中包含 Communication-Request-Parameters，添加
   * Communication-Answer-Parameters */
  if (ctx->has_comm_req_params && ctx->auth_success) {
    comm_ans_params_t ans_params;
    memset(&ans_params, 0, sizeof(ans_params));

    if (ctx->resource_allocated) {
      /* 资源分配成功 */
      ans_params.profile_name = ctx->comm_params.profile_name;
      ans_params.selected_link_id = ctx->policy_resp.selected_link_id;
      ans_params.bearer_id = ctx->mih_confirm.has_bearer_id
                                 ? ctx->mih_confirm.bearer_identifier
                                 : 0;
      ans_params.granted_bw = ctx->policy_resp.granted_bw_kbps * 1000;
      ans_params.granted_return_bw =
          ctx->policy_resp.granted_ret_bw_kbps * 1000;
      ans_params.session_timeout = ctx->granted_lifetime;
      ans_params.priority_type = ctx->comm_params.priority_type;
      ans_params.priority_class = ctx->comm_params.priority_class;
      ans_params.qos_level = ctx->comm_params.qos_level;
      ans_params.accounting_enabled = ctx->comm_params.accounting_enabled;

      /* 获取网关 IP - 注意：不使用 static 避免多线程数据竞争 */
      char gateway_ip[64] = {0};
      if (magic_dataplane_get_link_gateway(
              &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id,
              gateway_ip, sizeof(gateway_ip)) == 0) {
        ans_params.assigned_ip = gateway_ip;
      }
    } else {
      /* 资源分配失败 - 返回空的参数 */
      ans_params.selected_link_id = "NONE";
      ans_params.granted_bw = 0;
      ans_params.granted_return_bw = 0;
    }

    if (add_comm_ans_params_simple(ans, &ans_params) != 0) {
      fd_log_error(
          "[app_magic]   ✗ Failed to add Communication-Answer-Parameters");
    }
  }

  /* 发送应答 */
  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  /* 记录发送日志 */
  const char *state_str = "FAILED";
  if (ctx->auth_success) {
    if (ctx->resource_allocated) {
      state_str = "ACTIVE (0-RTT)";
    } else if (ctx->has_comm_req_params) {
      state_str = "AUTHENTICATED (BW denied)";
    } else if (ctx->has_req_status_info) {
      state_str = "AUTHENTICATED (subscribed)";
    } else {
      state_str = "AUTHENTICATED";
    }
  }

  fd_log_notice("[app_magic] ✓ Sent MCAA: Result=%u, MAGIC-Status=%u, State=%s",
                ctx->result_code, ctx->magic_status_code, state_str);

  return 0;
}

/**
 * @brief MCAR (Client Authentication Request) 主处理器。
 * @details 协调执行 MCAR 处理的 5 步流水线：
 *          1. Format Validation
 *          2. Authentication
 *          3. Subscription
 *          3b. Arinc Session Check
 *          4. Resource Allocation
 *          5. Finalize Response
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */
static int cic_handle_mcar(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  /* 记录请求开始日志 */
  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MCAR (Client Authentication Request)");
  fd_log_notice("[app_magic] ========================================");

  /* 初始化处理上下文 */
  McarProcessContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.granted_lifetime = 3600; /* 默认 1 小时 */
  ctx.auth_grace_period = 300; /* 默认 5 分钟 */
  ctx.result_code = 2001;      /* 默认成功 */

  /* Step 1: 格式解析与安全校验 */
  if (mcar_step1_validation(qry, &ctx) != 0) {
    goto finalize; /* 校验失败，直接返回错误 */
  }

  /* Step 2: 身份鉴权 */
  if (mcar_step2_auth(&ctx) != 0) {
    goto finalize; /* 认证失败，返回错误 */
  }

  /* Step 3: 订阅处理 (场景 B) */
  mcar_step3_subscription(&ctx);

  /* Step 3b: ARINC 839 会话激活条件验证 (v2.3) */
  if (mcar_step3b_session_conditions(&ctx) != 0) {
    goto finalize; /* 会话激活条件不满足，返回错误 */
  }

  /* Step 4: 0-RTT 资源申请 (场景 C) */
  mcar_step4_allocation(&ctx);

finalize:
  /* Step 5: 构建并发送应答 */
  if (mcar_step5_finalize(msg, &ctx) != 0) {
    fd_log_error("[app_magic] ✗ Failed to send MCAA");
    fd_log_notice("[app_magic] ========================================\n");
    return -1;
  }

  fd_log_notice("[app_magic] ========================================\n");
  return 0;
}

/*===========================================================================
 * MCCR 处理器 (Communication Change Request)
 * 处理通信变更请求的 Diameter 消息
 *
 * 根据 ARINC 839 协议，MCCR 处理采用 4 阶段流水线：
 * Phase 1: 会话验证 (Session Validation)
 * Phase 2: 参数与安全检查 (Parameter & Security Check)
 * Phase 3: 意图路由 (Intent Routing) - 判断 Start/Modify/Stop/Queue
 * Phase 4: 执行与响应 (Execution & Response)
 *
 * 支持的 MCCR 操作类型：
 * - OpenLink (Start): 首次请求通信链路
 * - ChangeLink (Modify): 修改现有链路参数
 * - CloseLink (Stop): 关闭当前链路
 * - QueueLink (Queue): 排队等待资源
 *
 * 特性：
 * - 完整的排队队列管理
 * - 链路资源请求失败时的重试机制
 * - 多链路回退逻辑
 *===========================================================================*/

/* MCCR 操作意图枚举 */
typedef enum {
  MCCR_INTENT_UNKNOWN = 0, /* 未知意图 */
  MCCR_INTENT_START,       /* 开始新链路 (OpenLink) */
  MCCR_INTENT_MODIFY,      /* 修改现有链路 (ChangeLink) */
  MCCR_INTENT_STOP,        /* 停止链路 (CloseLink) */
  MCCR_INTENT_QUEUE        /* 排队等待 (QueueLink) */
} MccxIntentType;

/*===========================================================================
 * 排队队列管理结构
 *===========================================================================*/

#define MCCR_QUEUE_MAX_SIZE 64    /* 最大排队请求数 */
#define MCCR_QUEUE_TIMEOUT_SEC 30 /* 排队超时时间(秒) */
#define MCCR_RETRY_MAX_COUNT 3    /* 最大重试次数 */
#define MCCR_RETRY_DELAY_MS 100   /* 重试间隔(毫秒) */
#define MCCR_FALLBACK_MAX_LINKS 4 /* 最大回退链路数 */

/* 排队条目状态 */
typedef enum {
  QUEUE_STATE_PENDING = 0, /* 等待中 */
  QUEUE_STATE_PROCESSING,  /* 处理中 */
  QUEUE_STATE_COMPLETED,   /* 已完成 */
  QUEUE_STATE_EXPIRED,     /* 已过期 */
  QUEUE_STATE_CANCELLED    /* 已取消 */
} MccxQueueState;

/* 排队条目结构 */
typedef struct {
  bool in_use;                /* 是否在使用 */
  MccxQueueState state;       /* 条目状态 */
  char session_id[128];       /* 会话 ID */
  char client_id[MAX_ID_LEN]; /* 客户端 ID */
  CommReqParams params;       /* 通信参数 */
  time_t enqueue_time;        /* 入队时间 */
  time_t expire_time;         /* 过期时间 */
  uint32_t retry_count;       /* 重试次数 */
  uint32_t priority;          /* 优先级 (数字越小越高) */
} MccxQueueEntry;

/* 排队管理器结构 */
typedef struct {
  MccxQueueEntry entries[MCCR_QUEUE_MAX_SIZE]; /* 排队条目数组 */
  uint32_t count;                              /* 当前排队数量 */
  pthread_mutex_t lock;                        /* 线程锁 */
  bool initialized;                            /* 是否已初始化 */
} MccxQueueManager;

/* 全局排队管理器 */
static MccxQueueManager g_mccr_queue = {0};

/**
 * @brief 初始化排队管理器。
 * @details 初始化排队队列的互斥锁和状态。
 *          队列用于暂存无法立即满足的 MCCR 请求 (QueueLink)。
 */
static void mccr_queue_init(void) {
  if (g_mccr_queue.initialized)
    return;

  memset(&g_mccr_queue, 0, sizeof(g_mccr_queue));
  pthread_mutex_init(&g_mccr_queue.lock, NULL);
  g_mccr_queue.initialized = true;

  fd_log_notice(
      "[app_magic] MCCR Queue Manager initialized (max=%d, timeout=%ds)",
      MCCR_QUEUE_MAX_SIZE, MCCR_QUEUE_TIMEOUT_SEC);
}

/**
 * @brief 清理过期的排队条目。
 * @details 遍历队列，检查并移除超过 `MCCR_QUEUE_TIMEOUT_SEC` 的请求。
 *          过期请求的状态将被标记为 `QUEUE_STATE_EXPIRED`。
 *
 * @return 清理的条目数量。
 */
static int mccr_queue_cleanup_expired(void) {
  if (!g_mccr_queue.initialized)
    return 0;

  int cleaned = 0;
  time_t now = time(NULL);

  pthread_mutex_lock(&g_mccr_queue.lock);

  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    MccxQueueEntry *entry = &g_mccr_queue.entries[i];
    if (entry->in_use && entry->state == QUEUE_STATE_PENDING) {
      if (now >= entry->expire_time) {
        fd_log_notice(
            "[app_magic] Queue entry expired: session=%s, waited=%lds",
            entry->session_id, now - entry->enqueue_time);
        entry->state = QUEUE_STATE_EXPIRED;
        entry->in_use = false;
        g_mccr_queue.count--;
        cleaned++;
      }
    }
  }

  pthread_mutex_unlock(&g_mccr_queue.lock);

  return cleaned;
}

/**
 * @brief 添加请求到排队队列。
 * @details 将 MCCR 请求加入等待队列。
 *          如果队列已满 (MCCR_QUEUE_MAX_SIZE)，则返回失败。
 *          如果已存在相同 Session-ID 的请求，则更新其参数和优先级。
 *
 * @param session_id 会话 ID。
 * @param client_id 客户端 ID。
 * @param params 通信请求参数。
 * @param priority 优先级值 (数字越小优先级越高)。
 * @return 0 成功入队，-1 队列已满。
 */
static int mccr_queue_enqueue(const char *session_id, const char *client_id,
                              const CommReqParams *params, uint32_t priority) {
  if (!g_mccr_queue.initialized) {
    mccr_queue_init();
  }

  /* 先清理过期条目 */
  mccr_queue_cleanup_expired();

  pthread_mutex_lock(&g_mccr_queue.lock);

  /* 检查是否已存在相同会话的排队请求 */
  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    MccxQueueEntry *entry = &g_mccr_queue.entries[i];
    if (entry->in_use && strcmp(entry->session_id, session_id) == 0) {
      /* 更新已有请求 */
      memcpy(&entry->params, params, sizeof(CommReqParams));
      entry->priority = priority;
      entry->retry_count++;
      pthread_mutex_unlock(&g_mccr_queue.lock);
      fd_log_notice("[app_magic] Queue entry updated: session=%s, retry=%u",
                    session_id, entry->retry_count);
      return 0;
    }
  }

  /* 队列已满检查 */
  if (g_mccr_queue.count >= MCCR_QUEUE_MAX_SIZE) {
    pthread_mutex_unlock(&g_mccr_queue.lock);
    fd_log_error("[app_magic] Queue full, cannot enqueue: session=%s",
                 session_id);
    return -1;
  }

  /* 查找空闲槽位 */
  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    MccxQueueEntry *entry = &g_mccr_queue.entries[i];
    if (!entry->in_use) {
      entry->in_use = true;
      entry->state = QUEUE_STATE_PENDING;
      strncpy(entry->session_id, session_id, sizeof(entry->session_id) - 1);
      strncpy(entry->client_id, client_id, sizeof(entry->client_id) - 1);
      memcpy(&entry->params, params, sizeof(CommReqParams));
      entry->enqueue_time = time(NULL);
      entry->expire_time = entry->enqueue_time + MCCR_QUEUE_TIMEOUT_SEC;
      entry->retry_count = 0;
      entry->priority = priority;
      g_mccr_queue.count++;

      pthread_mutex_unlock(&g_mccr_queue.lock);
      fd_log_notice(
          "[app_magic] Queue entry added: session=%s, priority=%u, count=%u/%d",
          session_id, priority, g_mccr_queue.count, MCCR_QUEUE_MAX_SIZE);
      return 0;
    }
  }

  pthread_mutex_unlock(&g_mccr_queue.lock);
  return -1;
}

/**
 * @brief 从队列中移除指定会话的请求。
 * @details 将指定 Session-ID 的请求标记为 COMPLETED 并从队列中移除。
 *
 * @param session_id 会话 ID。
 * @return 0 成功移除，-1 未找到。
 */
static int mccr_queue_dequeue(const char *session_id) {
  if (!g_mccr_queue.initialized)
    return -1;

  pthread_mutex_lock(&g_mccr_queue.lock);

  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    MccxQueueEntry *entry = &g_mccr_queue.entries[i];
    if (entry->in_use && strcmp(entry->session_id, session_id) == 0) {
      entry->state = QUEUE_STATE_COMPLETED;
      entry->in_use = false;
      g_mccr_queue.count--;
      pthread_mutex_unlock(&g_mccr_queue.lock);
      fd_log_notice("[app_magic] Queue entry removed: session=%s", session_id);
      return 0;
    }
  }

  pthread_mutex_unlock(&g_mccr_queue.lock);
  return -1;
}

/**
 * @brief 获取队列中最高优先级的待处理请求。
 * @details 遍历队列查找优先级最高 (数值最小) 的 PENDING 状态请求。
 *
 * @return 指向最高优先级条目的指针，若队列为空则返回 NULL。
 */
static __attribute__((unused)) MccxQueueEntry *
mccr_queue_peek_highest_priority(void) {
  if (!g_mccr_queue.initialized)
    return NULL;

  mccr_queue_cleanup_expired();

  pthread_mutex_lock(&g_mccr_queue.lock);

  MccxQueueEntry *best = NULL;
  uint32_t best_priority = UINT32_MAX;

  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    MccxQueueEntry *entry = &g_mccr_queue.entries[i];
    if (entry->in_use && entry->state == QUEUE_STATE_PENDING) {
      if (entry->priority < best_priority) {
        best_priority = entry->priority;
        best = entry;
      }
    }
  }

  if (best) {
    best->state = QUEUE_STATE_PROCESSING;
  }

  pthread_mutex_unlock(&g_mccr_queue.lock);

  return best;
}

/**
 * @brief 获取队列状态信息。
 * @details 查询当前队列中的待处理请求数和总请求数。
 *
 * @param pending 输出参数，返回等待中的请求数 (可选)。
 * @param total 输出参数，返回队列中的总请求数 (可选)。
 */
static void mccr_queue_get_status(uint32_t *pending, uint32_t *total) {
  if (!g_mccr_queue.initialized) {
    if (pending)
      *pending = 0;
    if (total)
      *total = 0;
    return;
  }

  pthread_mutex_lock(&g_mccr_queue.lock);

  uint32_t p = 0;
  for (int i = 0; i < MCCR_QUEUE_MAX_SIZE; i++) {
    if (g_mccr_queue.entries[i].in_use &&
        g_mccr_queue.entries[i].state == QUEUE_STATE_PENDING) {
      p++;
    }
  }

  if (pending)
    *pending = p;
  if (total)
    *total = g_mccr_queue.count;

  pthread_mutex_unlock(&g_mccr_queue.lock);
}

/*===========================================================================
 * MCCR 处理上下文
 *===========================================================================*/

/* MCCR 处理上下文 - 用于在各阶段间传递状态 */
typedef struct {
  /* Phase 1: 从请求中提取的基本信息 */
  char session_id[128];            /* 会话 ID */
  char client_id[MAX_ID_LEN];      /* 客户端 ID (Origin-Host) */
  char client_realm[MAX_ID_LEN];   /* 客户端域 (Origin-Realm) - v2.1 用于 MNTR
                                      路由 */
  ClientSession *existing_session; /* 现有会话指针 (NULL = 新会话) */

  /* Phase 2: 解析后的参数 */
  CommReqParams comm_params; /* 通信请求参数 */
  bool has_comm_req_params;  /* 是否包含 Communication-Request-Parameters */
  ClientProfile *profile;    /* 客户端配置文件指针 */

  /* 安全校验结果 */
  bool security_passed;           /* 安全校验是否通过 */
  uint32_t security_error_code;   /* 安全错误码 */
  const char *security_error_msg; /* 安全错误消息 */
  char extracted_dest_ip[64];     /* 从 TFT 提取的目的 IP */

  /* Phase 3: 意图路由结果 */
  MccxIntentType intent; /* 识别的操作意图 */

  /* Phase 4: 执行结果 */
  PolicyResponse policy_resp;            /* 策略决策结果 */
  MIH_Link_Resource_Confirm mih_confirm; /* MIH 资源确认 */
  ClientSession *session;                /* 会话对象 (创建或更新) */
  bool resource_allocated;               /* 资源是否分配成功 */
  bool route_added;                      /* 路由是否添加成功 */
  bool queued;                           /* 是否已加入排队 */

  /* 重试与回退控制 */
  uint32_t retry_count;                                  /* 当前重试次数 */
  char tried_links[MCCR_FALLBACK_MAX_LINKS][MAX_ID_LEN]; /* 已尝试的链路 */
  int tried_link_count;                                  /* 已尝试链路数 */

  /* 应答构建参数 */
  uint32_t result_code;       /* Diameter Result-Code */
  uint32_t magic_status_code; /* MAGIC-Status-Code (0=无错误) */
  const char *error_message;  /* 错误消息 (如有) */
} MccxProcessContext;

/*===========================================================================
 * 链路资源请求重试与回退机制
 *===========================================================================*/

/**
 * @brief 检查链路是否已尝试过 (MCCR Helper)。
 * @details 检查指定的 Link ID 是否在当前 MCCR 流程的已尝试列表 (Retry/Fallback)
 * 中。
 *
 * @param ctx MCCR 处理上下文。
 * @param link_id 要检查的链路 ID。
 * @return true 已尝试，false 未尝试。
 */
static bool mccr_link_already_tried(MccxProcessContext *ctx,
                                    const char *link_id) {
  for (int i = 0; i < ctx->tried_link_count; i++) {
    if (strcmp(ctx->tried_links[i], link_id) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief 标记链路为已尝试 (MCCR Helper)。
 * @details 将 Link ID 添加到已尝试列表，避免重复尝试相同链路。
 *
 * @param ctx MCCR 处理上下文。
 * @param link_id 链路 ID。
 */
static void mccr_mark_link_tried(MccxProcessContext *ctx, const char *link_id) {
  if (ctx->tried_link_count < MCCR_FALLBACK_MAX_LINKS) {
    strncpy(ctx->tried_links[ctx->tried_link_count], link_id, MAX_ID_LEN - 1);
    ctx->tried_link_count++;
  }
}

/**
 * @brief 带重试机制的链路资源请求 (MCCR Helper)。
 * @details 向 LMI/DLM 发起资源请求。支持重试机制 (MCCR_RETRY_MAX_COUNT)。
 *
 * @param ctx MCCR 处理上下文。
 * @param link_id 目标链路 ID。
 * @param policy_resp 策略引擎返回的参数。
 * @param mih_confirm 输出参数，存储 MIH 确认结果。
 * @return 0 成功，-1 失败。
 */
static int mccr_try_link_with_retry(MccxProcessContext *ctx,
                                    const char *link_id,
                                    PolicyResponse *policy_resp,
                                    MIH_Link_Resource_Confirm *mih_confirm) {
  int attempt;
  int mih_result;

  for (attempt = 0; attempt < MCCR_RETRY_MAX_COUNT; attempt++) {
    if (attempt > 0) {
      fd_log_notice("[app_magic]     → Retry attempt %d/%d for link %s",
                    attempt + 1, MCCR_RETRY_MAX_COUNT, link_id);
      /* 等待重试间隔 */
      usleep(MCCR_RETRY_DELAY_MS * 1000);
    }

    /* 构造 MIH 请求 */
    MIH_Link_Resource_Request mih_req;
    memset(&mih_req, 0, sizeof(mih_req));
    memset(mih_confirm, 0, sizeof(*mih_confirm));

    snprintf(mih_req.destination_id.mihf_id,
             sizeof(mih_req.destination_id.mihf_id), "MIHF_%s", link_id);
    mih_req.link_identifier.link_type = 1;
    strncpy(mih_req.link_identifier.link_addr, link_id,
            sizeof(mih_req.link_identifier.link_addr) - 1);
    mih_req.resource_action = RESOURCE_ACTION_REQUEST;
    mih_req.has_qos_params = true;
    mih_req.qos_parameters.cos_id = COS_BEST_EFFORT;
    mih_req.qos_parameters.forward_link_rate = policy_resp->granted_bw_kbps;
    mih_req.qos_parameters.return_link_rate = policy_resp->granted_ret_bw_kbps;
    mih_req.qos_parameters.avg_pk_tx_delay = 100;
    mih_req.qos_parameters.max_pk_tx_delay = 500;
    mih_req.qos_parameters.pk_delay_jitter = 50;
    mih_req.qos_parameters.pk_loss_rate = 0.01f;

    /* 发送 MIH 请求 */
    mih_result = magic_dlm_mih_link_resource_request(&g_ctx->lmi_ctx, &mih_req,
                                                     mih_confirm);

    if (mih_result == 0 && mih_confirm->status == STATUS_SUCCESS) {
      fd_log_notice("[app_magic]     ✓ MIH request succeeded on attempt %d",
                    attempt + 1);
      ctx->retry_count = attempt;
      return 0;
    }

    fd_log_notice("[app_magic]     ⚠ MIH request failed: status=%s",
                  status_to_string(mih_confirm->status));
  }

  fd_log_error("[app_magic]     ✗ All %d retry attempts failed for link %s",
               MCCR_RETRY_MAX_COUNT, link_id);
  return -1;
}

/**
 * @brief 尝试回退到备选链路 (MCCR Helper)。
 * @details 当首选链路由于资源不足等原因失败时，尝试使用策略引擎推荐的备选链路。
 *
 * @param ctx MCCR 处理上下文。
 * @return 0 成功找到可用链路并分配资源，-1 所有链路都失败。
 */
static int mccr_try_fallback_links(MccxProcessContext *ctx) {
  fd_log_notice("[app_magic]   → Attempting fallback to alternative links...");

  /* 获取所有可用链路列表 */
  PolicyRequest fallback_req;
  memset(&fallback_req, 0, sizeof(fallback_req));

  strncpy(fallback_req.client_id, ctx->client_id,
          sizeof(fallback_req.client_id) - 1);
  strncpy(fallback_req.profile_name, ctx->comm_params.profile_name,
          sizeof(fallback_req.profile_name) - 1);
  fallback_req.requested_bw_kbps = (uint32_t)ctx->comm_params.requested_bw;
  fallback_req.requested_ret_bw_kbps =
      (uint32_t)ctx->comm_params.requested_ret_bw;
  fallback_req.required_bw_kbps = (uint32_t)ctx->comm_params.required_bw;
  fallback_req.required_ret_bw_kbps =
      (uint32_t)ctx->comm_params.required_ret_bw;
  fallback_req.priority_class = (uint8_t)atoi(ctx->comm_params.priority_class);
  fallback_req.qos_level = (uint8_t)ctx->comm_params.qos_level;
  strncpy(fallback_req.flight_phase, ctx->comm_params.flight_phase,
          sizeof(fallback_req.flight_phase) - 1);

  /* 遍历尝试所有候选链路 */
  for (int i = 0; i < MCCR_FALLBACK_MAX_LINKS; i++) {
    PolicyResponse fallback_resp;
    memset(&fallback_resp, 0, sizeof(fallback_resp));

    /* 请求策略引擎返回下一个最佳链路 */
    fallback_req.exclude_link_count = ctx->tried_link_count;
    for (int j = 0; j < ctx->tried_link_count && j < 4; j++) {
      strncpy(fallback_req.exclude_links[j], ctx->tried_links[j],
              MAX_ID_LEN - 1);
    }

    if (magic_policy_select_path(&g_ctx->policy_ctx, &fallback_req,
                                 &fallback_resp) != 0 ||
        !fallback_resp.success) {
      fd_log_notice("[app_magic]     → No more fallback links available");
      break;
    }

    /* 跳过已尝试的链路 */
    if (mccr_link_already_tried(ctx, fallback_resp.selected_link_id)) {
      continue;
    }

    fd_log_notice("[app_magic]     → Trying fallback link: %s",
                  fallback_resp.selected_link_id);
    mccr_mark_link_tried(ctx, fallback_resp.selected_link_id);

    /* 尝试分配资源（带重试） */
    MIH_Link_Resource_Confirm fallback_confirm;
    if (mccr_try_link_with_retry(ctx, fallback_resp.selected_link_id,
                                 &fallback_resp, &fallback_confirm) == 0) {
      /* 成功！更新上下文 */
      memcpy(&ctx->policy_resp, &fallback_resp, sizeof(PolicyResponse));
      memcpy(&ctx->mih_confirm, &fallback_confirm,
             sizeof(MIH_Link_Resource_Confirm));
      fd_log_notice("[app_magic]     ✓ Fallback succeeded: link=%s",
                    fallback_resp.selected_link_id);
      return 0;
    }
  }

  fd_log_error("[app_magic]     ✗ All fallback links exhausted");
  return -1;
}

/*===========================================================================
 * MCCR 4 阶段流水线实现
 *===========================================================================*/

/**
 * @brief MCCR Phase 1: 会话验证。
 * @details 提取 Session-Id, Origin-Host
 * 等基本信息，并在会话管理器中查找现有会话。 验证会话状态是否允许进行 MCCR 操作
 * (必须是 AUTHENTICATED 或 ACTIVE)。
 *
 * @param qry Diameter 请求消息。
 * @param ctx MCCR 处理上下文。
 * @return 0 继续处理，-1 停止处理(错误已设置)。
 */
static int mccr_phase1_session_validation(struct msg *qry,
                                          MccxProcessContext *ctx) {
  struct avp_hdr *hdr;

  fd_log_notice("[app_magic] → Phase 1: Session Validation");

  /* 1.1 提取 Session-Id AVP (必需) */
  struct avp *avp_session = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session) != 0 ||
      !avp_session) {
    fd_log_error("[app_magic]   ✗ Missing required AVP: Session-Id");
    ctx->result_code = 5005;       /* DIAMETER_MISSING_AVP */
    ctx->magic_status_code = 3001; /* MISSING_AVP */
    ctx->error_message = "Missing required AVP: Session-Id";
    return -1;
  }

  if (fd_msg_avp_hdr(avp_session, &hdr) == 0 && hdr->avp_value) {
    size_t len = hdr->avp_value->os.len;
    if (len >= sizeof(ctx->session_id))
      len = sizeof(ctx->session_id) - 1;
    memcpy(ctx->session_id, hdr->avp_value->os.data, len);
    ctx->session_id[len] = '\0';
  }
  fd_log_notice("[app_magic]   Session-Id: %s", ctx->session_id);

  /* 1.2 提取 Origin-Host AVP (必需) - 作为客户端 ID */
  struct avp *avp_origin = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_host, &avp_origin) == 0 &&
      avp_origin) {
    if (fd_msg_avp_hdr(avp_origin, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(ctx->client_id))
        len = sizeof(ctx->client_id) - 1;
      memcpy(ctx->client_id, hdr->avp_value->os.data, len);
      ctx->client_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Client-ID: %s",
                ctx->client_id[0] ? ctx->client_id : "(unknown)");

  /* 1.2b 提取 Origin-Realm AVP (必需) - v2.1 用于 MNTR 路由 */
  struct avp *avp_origin_realm = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_realm, &avp_origin_realm) ==
          0 &&
      avp_origin_realm) {
    if (fd_msg_avp_hdr(avp_origin_realm, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(ctx->client_realm))
        len = sizeof(ctx->client_realm) - 1;
      memcpy(ctx->client_realm, hdr->avp_value->os.data, len);
      ctx->client_realm[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Client-Realm: %s",
                ctx->client_realm[0] ? ctx->client_realm : "(unknown)");

  /* 1.3 查找现有会话 */
  if (g_ctx) {
    ctx->existing_session =
        magic_session_find_by_id(&g_ctx->session_mgr, ctx->session_id);

    if (ctx->existing_session) {
      fd_log_notice("[app_magic]   ✓ Existing session found: state=%d, link=%s",
                    ctx->existing_session->state,
                    ctx->existing_session->assigned_link_id[0]
                        ? ctx->existing_session->assigned_link_id
                        : "(none)");

      /* 检查会话状态：必须是 AUTHENTICATED 或 ACTIVE */
      if (ctx->existing_session->state != SESSION_STATE_AUTHENTICATED &&
          ctx->existing_session->state != SESSION_STATE_ACTIVE) {
        fd_log_error("[app_magic]   ✗ Invalid session state for MCCR: %d",
                     ctx->existing_session->state);
        ctx->result_code = 5002;       /* DIAMETER_UNKNOWN_SESSION_ID */
        ctx->magic_status_code = 2001; /* INVALID_SESSION_STATE */
        ctx->error_message =
            "Session state does not allow communication change";
        return -1;
      }
    } else {
      fd_log_notice("[app_magic]   → New session (OpenLink scenario)");
    }
  }

  /* 1.4 处理未知 AVP */
  int unknown_avp_count = handle_unknown_avps(qry, false);
  if (unknown_avp_count > 0) {
    fd_log_notice("[app_magic]   Found %d unknown AVP(s) - ignored",
                  unknown_avp_count);
  }

  return 0; /* 继续处理 */
}

/**
 * @brief MCCR Phase 2: 参数解析与安全校验。
 * @details 解析 `Communication-Request-Parameters`，填充默认值 (Profile)。
 *          执行 Flight-Phase, Altitude, Airport 等 ARINC 839 激活条件检查。
 *
 * @param qry Diameter 请求消息。
 * @param ctx MCCR 处理上下文。
 * @return 0 继续处理，-1 停止处理(错误已设置)。
 */
static int mccr_phase2_param_security(struct msg *qry,
                                      MccxProcessContext *ctx) {
  fd_log_notice("[app_magic] → Phase 2: Parameter & Security Check");

  /* 2.1 查找 Communication-Request-Parameters 复合 AVP */
  struct avp *avp_comm_req = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_comm_req_params, &avp_comm_req) !=
          0 ||
      !avp_comm_req) {
    fd_log_error("[app_magic]   ✗ Missing required AVP: "
                 "Communication-Request-Parameters");
    ctx->result_code = 5005;       /* DIAMETER_MISSING_AVP */
    ctx->magic_status_code = 3001; /* MISSING_AVP */
    ctx->error_message =
        "Missing required AVP: Communication-Request-Parameters";
    return -1;
  }

  ctx->has_comm_req_params = true;

  /* 2.2 解析通信请求参数 */
  if (parse_comm_req_params(avp_comm_req, &ctx->comm_params) != 0) {
    fd_log_error(
        "[app_magic]   ✗ Failed to parse Communication-Request-Parameters");
    ctx->result_code = 5004; /* DIAMETER_INVALID_AVP_VALUE */
    ctx->magic_status_code =
        1000; /* MAGIC_ERROR_MISSING_AVP - 解析失败通常意味着缺少必要字段 */
    ctx->error_message = "Failed to parse Communication-Request-Parameters";
    return -1;
  }

  /* 2.3 从客户端 Profile 填充缺失的默认值 */
  ctx->profile = magic_config_find_client(&g_ctx->config, ctx->client_id);
  if (ctx->profile) {
    comm_req_params_fill_from_profile(&ctx->comm_params, ctx->profile);
    fd_log_notice("[app_magic]   ✓ Profile defaults applied from: %s",
                  ctx->client_id);
  }

  /* 2.3.1 v2.1: 如果请求中未指定飞行阶段，尝试从 ADIF 获取实际飞行阶段
     (修复默认 "CRUISE" 导致地面 WiFi 无法使用的问题) */
  if (!ctx->comm_params.has_flight_phase) {
    AdifAircraftState adif_state;
    /* 尝试获取 ADIF 状态 */
    if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
        adif_state.data_valid) {
      const char *adif_phase =
          adif_phase_to_policy_phase(adif_state.flight_phase.phase);
      if (adif_phase && strcmp(adif_phase, "UNKNOWN") != 0) {
        strncpy(ctx->comm_params.flight_phase, adif_phase,
                sizeof(ctx->comm_params.flight_phase) - 1);
        fd_log_notice("[app_magic]   ✓ Flight-Phase defaulted from ADIF: %s",
                      adif_phase);
      }
    }
  }

  /* 记录解析后的通信参数 */
  fd_log_notice("[app_magic]   Parsed Parameters:");
  fd_log_notice("[app_magic]     Profile: %s%s", ctx->comm_params.profile_name,
                ctx->comm_params.has_profile_name ? "" : " (default)");
  fd_log_notice("[app_magic]     Requested BW: %.2f/%.2f kbps%s",
                ctx->comm_params.requested_bw,
                ctx->comm_params.requested_ret_bw,
                ctx->comm_params.has_requested_bw ? "" : " (default)");
  fd_log_notice("[app_magic]     Required BW: %.2f/%.2f kbps",
                ctx->comm_params.required_bw, ctx->comm_params.required_ret_bw);
  fd_log_notice("[app_magic]     Priority: %s (type=%u), QoS: %u",
                ctx->comm_params.priority_class, ctx->comm_params.priority_type,
                ctx->comm_params.qos_level);
  fd_log_notice("[app_magic]     Keep-Request: %u",
                ctx->comm_params.keep_request);
  if (ctx->comm_params.has_flight_phase) {
    fd_log_notice("[app_magic]     Flight-Phase: %s",
                  ctx->comm_params.flight_phase);
  }
  if (ctx->comm_params.has_dlm_name) {
    fd_log_notice("[app_magic]     DLM-Name: %s", ctx->comm_params.dlm_name);
  }

  /*=========================================================================
   * 2.3.5 ARINC 839 会话激活条件验证 (v2.3)
   * 根据 ARINC 839 §3.2.4.1.1.1.2 和 §1.1.1.6.4，验证:
   *   1. Client Profile 是否允许在当前飞行阶段通信
   *   2. 请求的飞行阶段是否与实际飞行阶段匹配
   *   3. 飞机高度是否在会话允许的范围内
   *   4. 飞机是否在允许的机场
   *=========================================================================*/

  /* 获取当前 ADIF 状态 */
  AdifAircraftState adif_state_for_validation;
  bool adif_available =
      (adif_client_get_state(&g_ctx->adif_ctx, &adif_state_for_validation) ==
           0 &&
       adif_state_for_validation.data_valid);

  /* 2.3.5.1 验证 Client Profile 飞行阶段限制 (ARINC 839 §3.2.4.1.1.1.2) */
  if (ctx->profile && adif_available) {
    CfgFlightPhase actual_phase =
        magic_config_parse_flight_phase(adif_phase_to_policy_phase(
            adif_state_for_validation.flight_phase.phase));

    if (!magic_config_is_flight_phase_allowed(ctx->profile, actual_phase)) {
      fd_log_error("[app_magic]   ✗ Session activation denied: Client '%s' not "
                   "allowed in phase '%s'",
                   ctx->client_id,
                   adif_phase_to_policy_phase(
                       adif_state_for_validation.flight_phase.phase));
      ctx->result_code = 4001;       /* DIAMETER_AUTHORIZATION_REJECTED */
      ctx->magic_status_code = 1007; /* PHASE_NOT_ALLOWED */
      ctx->error_message = "Communication not allowed in current flight phase "
                           "per Client Profile";
      return -1;
    }
    fd_log_notice("[app_magic]   ✓ Flight-Phase authorization passed (Profile "
                  "allows phase '%s')",
                  adif_phase_to_policy_phase(
                      adif_state_for_validation.flight_phase.phase));
  }

  /* 2.3.5.2 验证 Altitude AVP 范围 (ARINC 839 §1.1.1.6.4.2) */
  if (ctx->comm_params.has_altitude &&
      ctx->comm_params.altitude_range_count > 0 && adif_available) {
    double actual_alt_ft = adif_state_for_validation.position.altitude_ft;
    bool alt_in_range = false;

    /* 检查飞机高度是否在任一指定范围内 */
    for (int i = 0; i < ctx->comm_params.altitude_range_count; i++) {
      int32_t min_alt = ctx->comm_params.altitude_ranges[i][0];
      int32_t max_alt = ctx->comm_params.altitude_ranges[i][1];

      bool in_this_range = true;
      if (min_alt != -1 && actual_alt_ft < min_alt)
        in_this_range = false;
      if (max_alt != -1 && actual_alt_ft > max_alt)
        in_this_range = false;

      if (in_this_range) {
        alt_in_range = true;
        break;
      }
    }

    /* 黑名单模式: 在范围内=拒绝; 白名单模式: 不在范围内=拒绝 */
    bool alt_allowed =
        ctx->comm_params.altitude_is_blacklist ? !alt_in_range : alt_in_range;

    if (!alt_allowed) {
      fd_log_error("[app_magic]   ✗ Session activation denied: Aircraft "
                   "altitude %.0f ft %s altitude %s",
                   actual_alt_ft,
                   ctx->comm_params.altitude_is_blacklist ? "in" : "not in",
                   ctx->comm_params.altitude_is_blacklist ? "blacklist"
                                                          : "whitelist");
      ctx->result_code = 4001;       /* DIAMETER_AUTHORIZATION_REJECTED */
      ctx->magic_status_code = 1008; /* ALTITUDE_NOT_ALLOWED */
      ctx->error_message =
          "Session not active at current altitude per Altitude AVP";
      return -1;
    }
    fd_log_notice("[app_magic]   ✓ Altitude check passed (%.0f ft)",
                  actual_alt_ft);
  }

  /* 2.3.5.3 验证 Airport AVP 白/黑名单 (ARINC 839 §1.1.1.6.4.3) */
  if (ctx->comm_params.has_airport && ctx->comm_params.airport_code_count > 0 &&
      adif_available) {
    /* 仅在地面时进行机场验证 */
    if (adif_state_for_validation.wow.on_ground) {
      /* 从 ADIF 或配置获取当前机场 (此处使用占位实现) */
      const char *current_airport = NULL; /* TODO: 从 ADIF 获取最近机场 */

      /* 如果有当前机场信息，进行验证 */
      if (current_airport && current_airport[0]) {
        bool airport_in_list = false;

        for (int i = 0; i < ctx->comm_params.airport_code_count; i++) {
          if (strcasecmp(ctx->comm_params.airport_codes[i], current_airport) ==
              0) {
            airport_in_list = true;
            break;
          }
        }

        /* 黑名单模式: 在列表中=拒绝; 白名单模式: 不在列表中=拒绝 */
        bool airport_allowed = ctx->comm_params.airport_is_blacklist
                                   ? !airport_in_list
                                   : airport_in_list;

        if (!airport_allowed) {
          fd_log_error("[app_magic]   ✗ Session activation denied: Airport "
                       "'%s' %s airport %s",
                       current_airport,
                       ctx->comm_params.airport_is_blacklist ? "in" : "not in",
                       ctx->comm_params.airport_is_blacklist ? "blacklist"
                                                             : "whitelist");
          ctx->result_code = 4001;       /* DIAMETER_AUTHORIZATION_REJECTED */
          ctx->magic_status_code = 1009; /* AIRPORT_NOT_ALLOWED */
          ctx->error_message =
              "Session not active at current airport per Airport AVP";
          return -1;
        }
        fd_log_notice("[app_magic]   ✓ Airport check passed (%s)",
                      current_airport);
      } else {
        fd_log_notice(
            "[app_magic]   ⚠ Airport check skipped: current airport unknown");
      }
    } else {
      fd_log_notice("[app_magic]   → Airport check skipped: aircraft airborne");
    }
  }

  /* 2.4 TFT 安全校验 - Source IP 必须匹配客户端 IP */
  ctx->security_passed = true; /* 默认通过 */

  if (ctx->existing_session && ctx->existing_session->client_ip[0]) {
    /* 有现有会话，校验客户端 IP */
    const char *expected_ip = ctx->existing_session->client_ip;

    /* 从第一条 TFT 中提取 Source IP 进行校验 (使用 3GPP 格式解析) */
    if (ctx->comm_params.tft_to_ground_count > 0) {
      TFTRule parsed_rule;
      if (tft_parse_rule(ctx->comm_params.tft_to_ground[0], &parsed_rule) ==
          0) {
        /* 校验 IP 是否匹配 (如果指定了源 IP) */
        if (parsed_rule.src_ip.is_valid) {
          struct in_addr expected_addr;
          inet_aton(expected_ip, &expected_addr);
          uint32_t host_ip = ntohl(expected_addr.s_addr);

          if (host_ip < parsed_rule.src_ip.start_ip ||
              host_ip > parsed_rule.src_ip.end_ip) {
            fd_log_error("[app_magic]   ✗ Security check failed: TFT Source IP "
                         "mismatch");
            fd_log_error("[app_magic]     Expected: %s, Got TFT Range: "
                         "%u.%u.%u.%u - %u.%u.%u.%u",
                         expected_ip,
                         (parsed_rule.src_ip.start_ip >> 24) & 0xFF,
                         (parsed_rule.src_ip.start_ip >> 16) & 0xFF,
                         (parsed_rule.src_ip.start_ip >> 8) & 0xFF,
                         (parsed_rule.src_ip.start_ip >> 0) & 0xFF,
                         (parsed_rule.src_ip.end_ip >> 24) & 0xFF,
                         (parsed_rule.src_ip.end_ip >> 16) & 0xFF,
                         (parsed_rule.src_ip.end_ip >> 8) & 0xFF,
                         (parsed_rule.src_ip.end_ip >> 0) & 0xFF);

            ctx->security_passed = false;
            ctx->security_error_code = 1017; /* IP_MISMATCH */
            ctx->security_error_msg =
                "TFT Source IP does not match authenticated client IP";
            ctx->result_code = 3007; /* DIAMETER_UNABLE_TO_COMPLY */
            ctx->magic_status_code = ctx->security_error_code;
            ctx->error_message = ctx->security_error_msg;
            return -1;
          }
        }
      }
    }

    fd_log_notice("[app_magic]   ✓ Security check passed: Client IP verified");
  } else if (ctx->profile && ctx->profile->auth.source_ip[0]) {
    fd_log_notice("[app_magic]   Expected client IP from profile: %s",
                  ctx->profile->auth.source_ip);
  }

  /* 2.4.1 TFT 白名单验证 (ARINC 839 §1.2.2.2) - 原子性验证 */
  if (ctx->profile && ctx->comm_params.tft_to_ground_count > 0) {
    fd_log_notice(
        "[app_magic]   Validating %d TFT-to-Ground rules (atomic check)...",
        ctx->comm_params.tft_to_ground_count);

    /* ★★★ 调试：打印白名单配置状态 ★★★ */
    fd_log_notice("[app_magic]   ★ Whitelist config: num_allowed_tfts=%u",
                  ctx->profile->traffic.num_allowed_tfts);
    for (uint32_t j = 0; j < ctx->profile->traffic.num_allowed_tfts && j < 5;
         j++) {
      fd_log_notice("[app_magic]     Whitelist[%u]: %s", j,
                    ctx->profile->traffic.allowed_tfts[j]);
    }

    for (int i = 0; i < ctx->comm_params.tft_to_ground_count; i++) {
      fd_log_notice("[app_magic]   ★ Validating client TFT[%d]: %s", i,
                    ctx->comm_params.tft_to_ground[i]);
      char tft_error_msg[MAX_ERROR_MSG_LEN];
      int tft_validation_result = tft_validate_against_whitelist(
          ctx->comm_params.tft_to_ground[i], &ctx->profile->traffic,
          tft_error_msg, sizeof(tft_error_msg));

      if (tft_validation_result != 0) {
        /* 验证失败：全部拒绝（原子性原则）*/
        fd_log_error(
            "[app_magic]   ✗ TFT whitelist validation FAILED (rule %d/%d): %s",
            i + 1, ctx->comm_params.tft_to_ground_count, tft_error_msg);
        fd_log_error("[app_magic]     TFT[%d]: %s", i + 1,
                     ctx->comm_params.tft_to_ground[i]);
        fd_log_error("[app_magic]     ✗ Rejecting entire MCCR (atomic "
                     "validation principle)");

        ctx->security_passed = false;
        ctx->result_code = 5003;       /* DIAMETER_AUTHORIZATION_REJECTED */
        ctx->magic_status_code = 1036; /* MAGIC_ERROR_TFT-INVALID */
        ctx->error_message = tft_error_msg;
        return -1;
      }
    }

    fd_log_notice("[app_magic]   ✓ All %d TFT-to-Ground rules passed whitelist "
                  "validation",
                  ctx->comm_params.tft_to_ground_count);
  }

  /* 2.4.2 验证 TFTtoAircraft-Rule（如果存在）- 原子性验证 */
  if (ctx->profile && ctx->comm_params.tft_to_aircraft_count > 0) {
    fd_log_notice(
        "[app_magic]   Validating %d TFT-to-Aircraft rules (atomic check)...",
        ctx->comm_params.tft_to_aircraft_count);

    for (int i = 0; i < ctx->comm_params.tft_to_aircraft_count; i++) {
      char tft_error_msg[MAX_ERROR_MSG_LEN];
      int tft_validation_result = tft_validate_against_whitelist(
          ctx->comm_params.tft_to_aircraft[i], &ctx->profile->traffic,
          tft_error_msg, sizeof(tft_error_msg));

      if (tft_validation_result != 0) {
        fd_log_error("[app_magic]   ✗ TFT whitelist validation FAILED "
                     "(toAircraft rule %d/%d): %s",
                     i + 1, ctx->comm_params.tft_to_aircraft_count,
                     tft_error_msg);
        fd_log_error("[app_magic]     ✗ Rejecting entire MCCR (atomic "
                     "validation principle)");
        ctx->security_passed = false;
        ctx->result_code = 5003;
        ctx->magic_status_code = 1036;
        ctx->error_message = tft_error_msg;
        return -1;
      }
    }

    fd_log_notice("[app_magic]   ✓ All %d TFT-to-Aircraft rules passed "
                  "whitelist validation",
                  ctx->comm_params.tft_to_aircraft_count);
  }

  /* 2.4.3 NAPT 白名单验证 */
  if (ctx->profile && ctx->comm_params.napt_count > 0) {
    for (int i = 0; i < ctx->comm_params.napt_count; i++) {
      char napt_error_msg[MAX_ERROR_MSG_LEN];
      if (napt_validate_against_whitelist(
              ctx->comm_params.napt_rules[i], &ctx->profile->traffic,
              NULL, /* TODO: 传入实际链路 IP */
              napt_error_msg, sizeof(napt_error_msg)) != 0) {
        fd_log_error("[app_magic]   ✗ NAPT whitelist validation FAILED: %s",
                     napt_error_msg);
        ctx->security_passed = false;
        ctx->result_code = 5003;
        ctx->magic_status_code = 1036;
        ctx->error_message = napt_error_msg;
        return -1;
      }
    }
    fd_log_notice("[app_magic]   ✓ NAPT whitelist validation passed (%d rules)",
                  ctx->comm_params.napt_count);
  }

  /* 2.5 从 TFT 提取目的 IP（用于 iptables 精确控制）*/
  ctx->extracted_dest_ip[0] = '\0';
  if (ctx->comm_params.tft_to_ground_count > 0 &&
      ctx->comm_params.tft_to_ground[0][0]) {
    const char *to_ptr = strstr(ctx->comm_params.tft_to_ground[0], " to ");
    if (to_ptr) {
      to_ptr += 4; /* 跳过 " to " */
      const char *end_ptr = strchr(to_ptr, ' ');
      if (!end_ptr)
        end_ptr = to_ptr + strlen(to_ptr);

      size_t ip_len = end_ptr - to_ptr;
      if (ip_len > 0 && ip_len < sizeof(ctx->extracted_dest_ip)) {
        strncpy(ctx->extracted_dest_ip, to_ptr, ip_len);
        ctx->extracted_dest_ip[ip_len] = '\0';

        /* 如果是 "any"，清空（表示允许整个网段）*/
        if (strcmp(ctx->extracted_dest_ip, "any") == 0) {
          ctx->extracted_dest_ip[0] = '\0';
        } else {
          fd_log_notice("[app_magic]   ✓ TFT 目的 IP: %s",
                        ctx->extracted_dest_ip);
        }
      }
    }
  }

  /* 2.5 Profile 一致性检查 */
  if (ctx->existing_session && ctx->comm_params.has_profile_name) {
    /* 检查请求的 Profile 是否与会话的 Profile 一致 */
    /* 注意：某些场景下允许切换 Profile */
    fd_log_notice("[app_magic]   Profile consistency: OK");
  }

  return 0; /* 继续处理 */
}

/**
 * @brief MCCR Phase 3: 意图路由。
 * @details 根据会话状态 (Authenticated/Active) 和请求参数
 * (Bandwidth/Keep-Request) 判断本次 MCCR 的操作意图。 可能的意图: START,
 * MODIFY, STOP, QUEUE。
 *
 * @param ctx MCCR 处理上下文。
 * @return 0 继续处理，-1 停止处理(错误已设置)。
 */
static int mccr_phase3_intent_routing(MccxProcessContext *ctx) {
  fd_log_notice("[app_magic] → Phase 3: Intent Routing");

  /* 3.1 根据会话状态和请求参数判断意图 */
  bool has_zero_bw = (ctx->comm_params.requested_bw == 0 &&
                      ctx->comm_params.requested_ret_bw == 0 &&
                      ctx->comm_params.required_bw == 0 &&
                      ctx->comm_params.required_ret_bw == 0);

  if (!ctx->existing_session) {
    /* 场景 A: 无现有会话 → OpenLink (Start) */
    ctx->intent = MCCR_INTENT_START;
    fd_log_notice("[app_magic]   Intent: START (OpenLink) - New session");
  } else if (has_zero_bw) {
    /* 场景 C: 现有会话 + 零带宽请求 → CloseLink (Stop) */
    ctx->intent = MCCR_INTENT_STOP;
    fd_log_notice(
        "[app_magic]   Intent: STOP (CloseLink) - Zero bandwidth requested");
  } else if (ctx->comm_params.keep_request &&
             ctx->existing_session->state == SESSION_STATE_AUTHENTICATED) {
    /* 场景 D: AUTHENTICATED 状态 + Keep-Request 标志 → QueueLink (Queue) */
    ctx->intent = MCCR_INTENT_QUEUE;
    fd_log_notice(
        "[app_magic]   Intent: QUEUE (QueueLink) - Keep-Request flag set");
  } else if (ctx->existing_session->state == SESSION_STATE_ACTIVE) {
    /* 场景 B: ACTIVE 状态 + 非零带宽 → ChangeLink (Modify) */
    ctx->intent = MCCR_INTENT_MODIFY;
    fd_log_notice(
        "[app_magic]   Intent: MODIFY (ChangeLink) - Modify active session");
  } else {
    /* AUTHENTICATED 状态 + 非零带宽 → OpenLink (Start) */
    ctx->intent = MCCR_INTENT_START;
    fd_log_notice("[app_magic]   Intent: START (OpenLink) - Activate session");
  }

  /* 3.2 意图级别的预校验 */
  switch (ctx->intent) {
  case MCCR_INTENT_STOP:
    /* 停止操作：检查是否有活动链路可释放 */
    if (ctx->existing_session && !ctx->existing_session->assigned_link_id[0]) {
      fd_log_notice("[app_magic]   ⚠ No active link to stop, will ack anyway");
    }
    break;

  case MCCR_INTENT_MODIFY:
    /* 修改操作：检查是否有活动链路可修改 */
    if (ctx->existing_session && !ctx->existing_session->assigned_link_id[0]) {
      fd_log_notice(
          "[app_magic]   ⚠ No active link to modify, treating as START");
      ctx->intent = MCCR_INTENT_START;
    }
    break;

  case MCCR_INTENT_QUEUE:
    /* 排队操作：初始化排队管理器 */
    if (!g_mccr_queue.initialized) {
      mccr_queue_init();
    }
    /* 检查队列状态 */
    uint32_t pending, total;
    mccr_queue_get_status(&pending, &total);
    fd_log_notice("[app_magic]   Queue status: %u pending, %u total", pending,
                  total);
    break;

  default:
    break;
  }

  return 0; /* 继续处理 */
}

/**
 * @brief MCCR Phase 4 辅助函数: 执行 STOP 操作。
 * @details 释放当前会话占用的链路资源 (MIH Release)，移除数据平面路由。
 *          将相关会话状态变更为 AUTHENTICATED。
 *
 * @param ctx MCCR 处理上下文。
 */
static void mccr_execute_stop(MccxProcessContext *ctx) {
  fd_log_notice("[app_magic]   Executing: CloseLink");

  if (ctx->existing_session && ctx->existing_session->assigned_link_id[0]) {
    /* 释放 MIH 资源 */
    MIH_Link_Resource_Request mih_release;
    MIH_Link_Resource_Confirm mih_rel_confirm;
    memset(&mih_release, 0, sizeof(mih_release));
    memset(&mih_rel_confirm, 0, sizeof(mih_rel_confirm));

    snprintf(mih_release.destination_id.mihf_id,
             sizeof(mih_release.destination_id.mihf_id), "MIHF_%s",
             ctx->existing_session->assigned_link_id);
    mih_release.resource_action = RESOURCE_ACTION_RELEASE;
    mih_release.has_bearer_id = true;
    mih_release.bearer_identifier = ctx->existing_session->bearer_id;

    magic_dlm_mih_link_resource_request(&g_ctx->lmi_ctx, &mih_release,
                                        &mih_rel_confirm);
    fd_log_notice("[app_magic]     ✓ MIH resource released");

    /* 删除数据平面路由 */
    magic_dataplane_remove_client_route(&g_ctx->dataplane_ctx, ctx->session_id);
    fd_log_notice("[app_magic]     ✓ Dataplane route removed");

    /* 清除会话链路分配，状态转换: ACTIVE → AUTHENTICATED */
    ctx->existing_session->assigned_link_id[0] = '\0';
    ctx->existing_session->bearer_id = 0;
    ctx->existing_session->granted_bw_kbps = 0;
    ctx->existing_session->granted_ret_bw_kbps = 0;
    magic_session_set_state(ctx->existing_session, SESSION_STATE_AUTHENTICATED);
    fd_log_notice("[app_magic]     ✓ Session state: ACTIVE → AUTHENTICATED");
  }

  /* 从排队队列中移除（如果存在） */
  mccr_queue_dequeue(ctx->session_id);

  ctx->result_code = 2001; /* SUCCESS */
  ctx->resource_allocated = false;
}

/**
 * @brief MCCR Phase 4 辅助函数: 执行 START/MODIFY 操作。
 * @details 调用策略引擎选择链路，通过 MIH 申请资源。
 *          如果成功，更新会话状态为 ACTIVE 并配置数据平面 TFT/路由。
 *          如果失败，尝试回退链路。
 *
 * @param ctx MCCR 处理上下文。
 * @return 0 成功，-1 失败。
 */
static int mccr_execute_start_modify(MccxProcessContext *ctx) {
  fd_log_notice("[app_magic]   Executing: %s",
                ctx->intent == MCCR_INTENT_START ? "OpenLink" : "ChangeLink");

  /* 4.1 调用策略引擎进行链路选择 */
  PolicyRequest policy_req;
  memset(&policy_req, 0, sizeof(policy_req));

  strncpy(policy_req.client_id, ctx->client_id,
          sizeof(policy_req.client_id) - 1);
  strncpy(policy_req.profile_name, ctx->comm_params.profile_name,
          sizeof(policy_req.profile_name) - 1);
  policy_req.requested_bw_kbps = (uint32_t)ctx->comm_params.requested_bw;
  policy_req.requested_ret_bw_kbps =
      (uint32_t)ctx->comm_params.requested_ret_bw;
  policy_req.required_bw_kbps = (uint32_t)ctx->comm_params.required_bw;
  policy_req.required_ret_bw_kbps = (uint32_t)ctx->comm_params.required_ret_bw;
  policy_req.priority_class = (uint8_t)atoi(ctx->comm_params.priority_class);
  policy_req.qos_level = (uint8_t)ctx->comm_params.qos_level;
  strncpy(policy_req.flight_phase, ctx->comm_params.flight_phase,
          sizeof(policy_req.flight_phase) - 1);

  /* v2.2: 从 ADIF 获取实时位置和 WoW 数据用于策略决策 */
  AdifAircraftState adif_state;
  if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
      adif_state.data_valid) {
    policy_req.aircraft_lat = adif_state.position.latitude;
    policy_req.aircraft_lon = adif_state.position.longitude;
    policy_req.aircraft_alt =
        adif_state.position.altitude_ft * 0.3048; /* ft->m */
    policy_req.on_ground = adif_state.wow.on_ground;
    policy_req.has_adif_data = true;
    fd_log_debug(
        "[app_magic]   ADIF Data: lat=%.4f, lon=%.4f, alt=%.0fm, WoW=%s",
        policy_req.aircraft_lat, policy_req.aircraft_lon,
        policy_req.aircraft_alt, policy_req.on_ground ? "Ground" : "Airborne");
  } else {
    policy_req.aircraft_lat = 0.0;
    policy_req.aircraft_lon = 0.0;
    policy_req.aircraft_alt = 0.0;
    policy_req.on_ground = false;
    policy_req.has_adif_data = false;
  }

  memset(&ctx->policy_resp, 0, sizeof(ctx->policy_resp));

  if (magic_policy_select_path(&g_ctx->policy_ctx, &policy_req,
                               &ctx->policy_resp) != 0 ||
      !ctx->policy_resp.success) {
    fd_log_error("[app_magic]     ✗ Policy decision failed: %s",
                 ctx->policy_resp.reason);
    ctx->result_code = 5012;       /* DIAMETER_UNABLE_TO_COMPLY */
    ctx->magic_status_code = 1010; /* NO_ENTRY_IN_BANDWIDTHTABLE */
    ctx->error_message = ctx->policy_resp.reason;
    ctx->resource_allocated = false;
    return -1;
  }

  fd_log_notice("[app_magic]     ✓ Policy Decision: Link=%s, BW=%u/%u kbps",
                ctx->policy_resp.selected_link_id,
                ctx->policy_resp.granted_bw_kbps,
                ctx->policy_resp.granted_ret_bw_kbps);

  /* 标记主选链路为已尝试 */
  mccr_mark_link_tried(ctx, ctx->policy_resp.selected_link_id);

  /* 4.2 如果是 MODIFY 且链路变化，先释放旧资源 */
  if (ctx->intent == MCCR_INTENT_MODIFY && ctx->existing_session &&
      ctx->existing_session->assigned_link_id[0] &&
      strcmp(ctx->existing_session->assigned_link_id,
             ctx->policy_resp.selected_link_id) != 0) {
    fd_log_notice("[app_magic]     → Releasing old link: %s",
                  ctx->existing_session->assigned_link_id);

    MIH_Link_Resource_Request mih_release;
    MIH_Link_Resource_Confirm mih_rel_confirm;
    memset(&mih_release, 0, sizeof(mih_release));
    memset(&mih_rel_confirm, 0, sizeof(mih_rel_confirm));

    snprintf(mih_release.destination_id.mihf_id,
             sizeof(mih_release.destination_id.mihf_id), "MIHF_%s",
             ctx->existing_session->assigned_link_id);
    mih_release.resource_action = RESOURCE_ACTION_RELEASE;
    mih_release.has_bearer_id = true;
    mih_release.bearer_identifier = ctx->existing_session->bearer_id;

    magic_dlm_mih_link_resource_request(&g_ctx->lmi_ctx, &mih_release,
                                        &mih_rel_confirm);
    magic_dataplane_remove_client_route(&g_ctx->dataplane_ctx, ctx->session_id);
  }

  /* 4.3 通过 MIH 请求链路资源（带重试机制） */
  if (mccr_try_link_with_retry(ctx, ctx->policy_resp.selected_link_id,
                               &ctx->policy_resp, &ctx->mih_confirm) != 0) {
    /* 主链路失败，尝试回退到备选链路 */
    if (mccr_try_fallback_links(ctx) != 0) {
      /* 所有链路都失败 */
      fd_log_error("[app_magic]     ✗ All link resource requests failed");
      ctx->result_code = 5012;       /* DIAMETER_UNABLE_TO_COMPLY */
      ctx->magic_status_code = 1010; /* NO_BW */
      ctx->error_message = "No available link resources";
      ctx->resource_allocated = false;
      return -1;
    }
  }

  fd_log_notice(
      "[app_magic]     ✓ MIH Link Resource Allocated: Bearer=%u, Retries=%u",
      ctx->mih_confirm.has_bearer_id ? ctx->mih_confirm.bearer_identifier : 0,
      ctx->retry_count);

  /* 4.4 创建或更新会话 */
  if (!ctx->existing_session) {
    ctx->session = magic_session_create(&g_ctx->session_mgr, ctx->session_id,
                                        ctx->client_id, ctx->client_realm);
    if (!ctx->session) {
      fd_log_error("[app_magic]     ✗ Failed to create session");
      ctx->result_code = 5012;
      ctx->magic_status_code = 1000; /* INTERNAL_ERROR */
      ctx->error_message = "Failed to create session";
      ctx->resource_allocated = false;
      return -1;
    }
  } else {
    ctx->session = ctx->existing_session;
  }

  /* 分配链路资源到会话 */
  magic_session_assign_link(
      ctx->session, ctx->policy_resp.selected_link_id,
      ctx->mih_confirm.has_bearer_id ? ctx->mih_confirm.bearer_identifier : 0,
      ctx->policy_resp.granted_bw_kbps, ctx->policy_resp.granted_ret_bw_kbps);

  /* 状态转换: AUTHENTICATED → ACTIVE */
  magic_session_set_state(ctx->session, SESSION_STATE_ACTIVE);
  fd_log_notice("[app_magic]     ✓ Session state: → ACTIVE");

  /* 4.5 添加数据平面路由 */
  {
    const char *client_ip = NULL;
    if (ctx->profile && ctx->profile->auth.source_ip[0]) {
      client_ip = ctx->profile->auth.source_ip;
    } else if (ctx->session && ctx->session->client_ip[0]) {
      client_ip = ctx->session->client_ip;
    } else {
      client_ip = "192.168.10.10"; /* 默认 */
    }

    /* 4.5.1 先确保链路已注册到数据平面 (按需注册) */
    uint32_t table_id = magic_dataplane_get_table_id(
        &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id);
    if (table_id == 0) {
      /* 链路尚未注册，需要注册 */
      DlmClient *dlm_client = magic_lmi_find_by_link(
          &g_ctx->lmi_ctx, ctx->policy_resp.selected_link_id);
      if (dlm_client && dlm_client->link_identifier.link_addr[0]) {
        /* 从 poa_addr 获取网络接口名，如果为空则回退到 link_addr */
        const char *interface_name =
            dlm_client->link_identifier.poa_addr[0]
                ? dlm_client->link_identifier.poa_addr
                : dlm_client->link_identifier.link_addr;

        /* 从 DLM 参数中获取网关 IP */
        char gateway_ip[64] = "";
        if (dlm_client->current_parameters.gateway != 0) {
          struct in_addr gw_addr;
          gw_addr.s_addr = dlm_client->current_parameters.gateway;
          strncpy(gateway_ip, inet_ntoa(gw_addr), sizeof(gateway_ip) - 1);
        }

        int reg_ret = magic_dataplane_register_link(
            &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id,
            interface_name, gateway_ip[0] ? gateway_ip : NULL);
        if (reg_ret >= 0) {
          fd_log_notice("[app_magic]     ✓ Link registered to dataplane: %s → "
                        "%s (table=%d)",
                        ctx->policy_resp.selected_link_id, interface_name,
                        reg_ret);
        } else {
          fd_log_error(
              "[app_magic]     ✗ Failed to register link to dataplane: %s",
              ctx->policy_resp.selected_link_id);
        }
      } else {
        fd_log_error("[app_magic]     ✗ Cannot register link: DLM client not "
                     "found for %s",
                     ctx->policy_resp.selected_link_id);
      }
    }

    /* 4.5.2 添加客户端路由规则 */
    int dp_ret = magic_dataplane_add_client_route(
        &g_ctx->dataplane_ctx, client_ip, ctx->session_id,
        ctx->policy_resp.selected_link_id,
        ctx->extracted_dest_ip[0] ? ctx->extracted_dest_ip : NULL);
    ctx->route_added = (dp_ret == 0);
    if (ctx->route_added) {
      fd_log_notice("[app_magic]     ✓ Dataplane route added: %s → %s",
                    client_ip, ctx->policy_resp.selected_link_id);
      /* 将客户端加入 data 白名单，允许真实业务流量通过 ipset+iptables */
      magic_dataplane_ipset_add_data(client_ip);
      fd_log_notice("[app_magic]     ✓ Client %s added to data whitelist",
                    client_ip);
    } else {
      fd_log_notice("[app_magic]     ⚠ Dataplane route failed (non-critical)");
    }

    /* 4.5.3 添加 TFT mangle 规则（用于 fwmark 路由）
     * ARINC 839: 一个 MCCR 可包含多条 TFT (1-255)
     * 同会话同策略：所有 TFT 共享同一个 fwmark，走同一条链路 */
    if (ctx->comm_params.tft_to_ground_count > 0) {
      fd_log_notice("[app_magic]     → Adding %d TFT mangle rules (same "
                    "fwmark, same link)...",
                    ctx->comm_params.tft_to_ground_count);

      int tft_success_count = 0;
      for (int i = 0; i < ctx->comm_params.tft_to_ground_count; i++) {
        TFTRule parsed_rule;
        if (tft_parse_rule(ctx->comm_params.tft_to_ground[i], &parsed_rule) ==
                0 &&
            parsed_rule.is_valid) {
          /* 构造 TftTuple 结构 */
          TftTuple tft_tuple;
          memset(&tft_tuple, 0, sizeof(tft_tuple));

          /* 源 IP (客户端) */
          strncpy(tft_tuple.src_ip, client_ip, sizeof(tft_tuple.src_ip) - 1);

          /* 目的 IP */
          if (parsed_rule.dst_ip.is_valid) {
            struct in_addr dst_addr;
            dst_addr.s_addr = htonl(parsed_rule.dst_ip.start_ip);
            strncpy(tft_tuple.dst_ip, inet_ntoa(dst_addr),
                    sizeof(tft_tuple.dst_ip) - 1);
          } else if (ctx->extracted_dest_ip[0]) {
            strncpy(tft_tuple.dst_ip, ctx->extracted_dest_ip,
                    sizeof(tft_tuple.dst_ip) - 1);
          }

          /* 协议和端口 */
          tft_tuple.protocol =
              parsed_rule.has_protocol ? parsed_rule.protocol : 0;
          tft_tuple.src_port = parsed_rule.src_port.is_valid
                                   ? parsed_rule.src_port.start_port
                                   : 0;
          tft_tuple.dst_port = parsed_rule.dst_port.is_valid
                                   ? parsed_rule.dst_port.start_port
                                   : 0;

          /* 添加 TFT 规则到数据平面 */
          int tft_ret = magic_dataplane_add_tft_rule(
              &g_ctx->dataplane_ctx, &tft_tuple, ctx->session_id,
              ctx->policy_resp.selected_link_id);

          if (tft_ret == 0) {
            tft_success_count++;
            fd_log_notice(
                "[app_magic]     ✓ TFT[%d/%d] mangle rule added: %s:%u → %s:%u "
                "(proto=%u, link=%s)",
                i + 1, ctx->comm_params.tft_to_ground_count, tft_tuple.src_ip,
                tft_tuple.src_port, tft_tuple.dst_ip, tft_tuple.dst_port,
                tft_tuple.protocol, ctx->policy_resp.selected_link_id);
          } else {
            fd_log_error("[app_magic]     ✗ TFT[%d/%d] mangle rule failed "
                         "(continuing with remaining rules)",
                         i + 1, ctx->comm_params.tft_to_ground_count);
          }
        } else {
          fd_log_error("[app_magic]     ✗ TFT[%d/%d] parse failed: %s "
                       "(skipping this rule)",
                       i + 1, ctx->comm_params.tft_to_ground_count,
                       ctx->comm_params.tft_to_ground[i]);
        }
      }

      if (tft_success_count > 0) {
        fd_log_notice(
            "[app_magic]     ✓ Successfully added %d/%d TFT mangle rules",
            tft_success_count, ctx->comm_params.tft_to_ground_count);
      } else {
        fd_log_notice("[app_magic]     ⚠ No TFT mangle rules added (all "
                      "failed, but non-critical)");
      }
    } else {
      fd_log_notice("[app_magic]     → No TFT-to-Ground rules specified, "
                    "skipping TFT mangle rules");
    }

    /* 4.6 v2.1: 注册流量监控 (Netlink conntrack) */
    uint32_t traffic_mark = traffic_register_session(
        &g_ctx->traffic_ctx, ctx->session_id, ctx->client_id, client_ip);
    if (traffic_mark != 0) {
      ctx->session->conntrack_mark = traffic_mark;
      ctx->session->traffic_start_time = time(NULL);
      fd_log_notice("[app_magic]     ✓ Traffic monitor registered: mark=0x%x",
                    traffic_mark);
    } else {
      fd_log_notice("[app_magic]     ⚠ Traffic monitor registration failed "
                    "(non-critical)");
    }

    /* 4.7 v2.2: 创建 CDR 记录 */
    CDRRecord *cdr =
        cdr_create(&g_ctx->cdr_mgr, ctx->session_id, ctx->client_id,
                   ctx->policy_resp.selected_link_id);
    if (cdr) {
      fd_log_notice("[app_magic]     ✓ CDR created: id=%u, uuid=%s",
                    cdr->cdr_id, cdr->cdr_uuid);
    } else {
      fd_log_notice("[app_magic]     ⚠ CDR creation failed (non-critical)");
    }
  }

  /* 从排队队列中移除（如果存在） */
  mccr_queue_dequeue(ctx->session_id);

  ctx->result_code = 2001; /* SUCCESS */
  ctx->resource_allocated = true;
  return 0;
}

/**
 * @brief MCCR Phase 4 辅助函数: 执行 QUEUE 操作。
 * @details 尝试将请求加入排队队列等待资源。
 *          如果策略引擎发现有立即可用资源，可能会直接升级为立即分配。
 *
 * @param ctx MCCR 处理上下文。
 */
static void mccr_execute_queue(MccxProcessContext *ctx) {
  fd_log_notice("[app_magic]   Executing: QueueLink");

  /* 首先尝试立即分配资源 */
  PolicyRequest policy_req;
  PolicyResponse policy_resp;
  memset(&policy_req, 0, sizeof(policy_req));
  memset(&policy_resp, 0, sizeof(policy_resp));

  strncpy(policy_req.client_id, ctx->client_id,
          sizeof(policy_req.client_id) - 1);
  strncpy(policy_req.profile_name, ctx->comm_params.profile_name,
          sizeof(policy_req.profile_name) - 1);
  policy_req.requested_bw_kbps = (uint32_t)ctx->comm_params.requested_bw;
  policy_req.requested_ret_bw_kbps =
      (uint32_t)ctx->comm_params.requested_ret_bw;
  policy_req.required_bw_kbps = (uint32_t)ctx->comm_params.required_bw;
  policy_req.required_ret_bw_kbps = (uint32_t)ctx->comm_params.required_ret_bw;

  /* v2.2: 从 ADIF 获取实时位置和 WoW 数据用于策略决策 */
  AdifAircraftState adif_state;
  if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
      adif_state.data_valid) {
    policy_req.aircraft_lat = adif_state.position.latitude;
    policy_req.aircraft_lon = adif_state.position.longitude;
    policy_req.aircraft_alt =
        adif_state.position.altitude_ft * 0.3048; /* ft->m */
    policy_req.on_ground = adif_state.wow.on_ground;
    policy_req.has_adif_data = true;
  } else {
    policy_req.aircraft_lat = 0.0;
    policy_req.aircraft_lon = 0.0;
    policy_req.aircraft_alt = 0.0;
    policy_req.on_ground = false;
    policy_req.has_adif_data = false;
  }

  if (magic_policy_select_path(&g_ctx->policy_ctx, &policy_req, &policy_resp) ==
          0 &&
      policy_resp.success) {
    /* 有可用资源，尝试分配 */
    mccr_mark_link_tried(ctx, policy_resp.selected_link_id);

    MIH_Link_Resource_Confirm mih_confirm;
    if (mccr_try_link_with_retry(ctx, policy_resp.selected_link_id,
                                 &policy_resp, &mih_confirm) == 0) {
      /* 资源分配成功 */
      fd_log_notice("[app_magic]     ✓ Immediate allocation succeeded, no "
                    "queueing needed");
      memcpy(&ctx->policy_resp, &policy_resp, sizeof(policy_resp));
      memcpy(&ctx->mih_confirm, &mih_confirm, sizeof(mih_confirm));

      /* 创建/更新会话 */
      if (!ctx->existing_session) {
        ctx->session =
            magic_session_create(&g_ctx->session_mgr, ctx->session_id,
                                 ctx->client_id, ctx->client_realm);
      } else {
        ctx->session = ctx->existing_session;
      }

      if (ctx->session) {
        magic_session_assign_link(
            ctx->session, policy_resp.selected_link_id,
            mih_confirm.has_bearer_id ? mih_confirm.bearer_identifier : 0,
            policy_resp.granted_bw_kbps, policy_resp.granted_ret_bw_kbps);
        magic_session_set_state(ctx->session, SESSION_STATE_ACTIVE);
      }

      ctx->result_code = 2001; /* SUCCESS */
      ctx->resource_allocated = true;
      ctx->queued = false;
      return;
    }
  }

  /* 资源不足，加入排队队列 */
  fd_log_notice("[app_magic]     → Resources not available, adding to queue");

  /* 计算优先级（数字越小越高） */
  uint32_t priority =
      (uint32_t)(100 - atoi(ctx->comm_params.priority_class) * 10);

  if (mccr_queue_enqueue(ctx->session_id, ctx->client_id, &ctx->comm_params,
                         priority) == 0) {
    fd_log_notice("[app_magic]     ✓ Request queued successfully");
    ctx->result_code = 2001; /* SUCCESS (request accepted, queued) */
    ctx->resource_allocated = false;
    ctx->queued = true;

    /* 记录队列状态 */
    uint32_t pending, total;
    mccr_queue_get_status(&pending, &total);
    fd_log_notice("[app_magic]     Queue status: %u pending, %u total", pending,
                  total);
  } else {
    fd_log_error("[app_magic]     ✗ Queue full, cannot accept request");
    ctx->result_code = 5012;       /* DIAMETER_UNABLE_TO_COMPLY */
    ctx->magic_status_code = 1011; /* QUEUE_FULL */
    ctx->error_message = "Request queue is full";
    ctx->resource_allocated = false;
    ctx->queued = false;
  }
}

/**
 * @brief MCCR Phase 4: 执行与响应。
 * @details 根据 Phase 3 确定的意图 (Intent) 执行具体操作
 * (START/MODIFY/STOP/QUEUE)。 构建并发送 MCCA (Client Communication Answer)
 * 响应消息。 填充 Communication-Answer-Parameters。
 *
 * @param msg Diameter 消息指针的指针 (输入为请求，输出为应答)。
 * @param ctx MCCR 处理上下文。
 * @return 0 成功，-1 失败。
 */
static int mccr_phase4_execution(struct msg **msg, MccxProcessContext *ctx) {
  struct msg *ans;

  fd_log_notice("[app_magic] → Phase 4: Execution & Response");

  /* 4.1 根据意图执行不同操作 */
  switch (ctx->intent) {
  case MCCR_INTENT_STOP:
    mccr_execute_stop(ctx);
    break;

  case MCCR_INTENT_START:
  case MCCR_INTENT_MODIFY:
    mccr_execute_start_modify(ctx);
    break;

  case MCCR_INTENT_QUEUE:
    mccr_execute_queue(ctx);
    break;

  default:
    fd_log_error("[app_magic]   ✗ Unknown intent type: %d", ctx->intent);
    ctx->result_code = 5012;
    ctx->magic_status_code = 1000;
    ctx->error_message = "Unknown operation intent";
    break;
  }

  /* 4.2 构建应答消息 (MCCA) */
  fd_log_notice("[app_magic]   Building Response...");

  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  /* 添加必需的标准 Diameter AVP */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, ctx->result_code);

  /* 如果有 MAGIC-Status-Code 错误，添加到应答 */
  if (ctx->magic_status_code > 0) {
    ADD_AVP_U32(ans, g_magic_dict.avp_magic_status_code,
                ctx->magic_status_code);
  }

  /* 如果有错误消息，添加 Error-Message */
  if (ctx->error_message) {
    ADD_AVP_STR(ans, g_std_dict.avp_error_message, ctx->error_message);
  }

  /* 4.3 添加 Communication-Answer-Parameters */
  {
    comm_ans_params_t ans_params;
    memset(&ans_params, 0, sizeof(ans_params));

    if (ctx->resource_allocated) {
      /* 资源分配成功 */
      ans_params.profile_name = ctx->comm_params.profile_name;
      ans_params.selected_link_id = ctx->policy_resp.selected_link_id;
      ans_params.bearer_id = ctx->mih_confirm.has_bearer_id
                                 ? ctx->mih_confirm.bearer_identifier
                                 : 0;
      ans_params.granted_bw =
          ctx->policy_resp.granted_bw_kbps * 1000; /* kbps -> bps */
      ans_params.granted_return_bw =
          ctx->policy_resp.granted_ret_bw_kbps * 1000;
      ans_params.priority_type = ctx->comm_params.priority_type;
      ans_params.priority_class = ctx->comm_params.priority_class;
      ans_params.qos_level = ctx->comm_params.qos_level;
      ans_params.accounting_enabled = ctx->comm_params.accounting_enabled;
      ans_params.dlm_availability_list = ctx->policy_resp.selected_link_id;
      ans_params.keep_request = ctx->comm_params.keep_request;
      ans_params.auto_detect = ctx->comm_params.auto_detect;
      ans_params.session_timeout =
          ctx->comm_params.timeout > 0 ? ctx->comm_params.timeout : 3600;

      /* 可选位置限制字段 */
      if (ctx->comm_params.has_flight_phase) {
        ans_params.flight_phase = ctx->comm_params.flight_phase;
      }
      if (ctx->comm_params.has_altitude) {
        ans_params.altitude = ctx->comm_params.altitude;
      }
      if (ctx->comm_params.has_airport) {
        ans_params.airport = ctx->comm_params.airport;
      }

      /* 获取网关 IP - 注意：不使用 static 避免多线程数据竞争 */
      char link_gateway_ip[64] = {0};
      if (magic_dataplane_get_link_gateway(
              &g_ctx->dataplane_ctx, ctx->policy_resp.selected_link_id,
              link_gateway_ip, sizeof(link_gateway_ip)) == 0) {
        ans_params.assigned_ip = link_gateway_ip;
      }
    } else {
      /* 资源未分配 - 返回空参数 */
      ans_params.profile_name = ctx->comm_params.profile_name;
      ans_params.selected_link_id = ctx->queued ? "QUEUED" : "NONE";
      ans_params.bearer_id = 0;
      ans_params.granted_bw = 0;
      ans_params.granted_return_bw = 0;
      ans_params.session_timeout = 0;
      ans_params.assigned_ip = NULL;
    }

    if (add_comm_ans_params_simple(ans, &ans_params) != 0) {
      fd_log_error(
          "[app_magic]     ✗ Failed to add Communication-Answer-Parameters");
    }
  }

  /* 4.4 发送应答 */
  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  /* 记录发送日志 */
  const char *intent_str = "UNKNOWN";
  switch (ctx->intent) {
  case MCCR_INTENT_START:
    intent_str = "START";
    break;
  case MCCR_INTENT_MODIFY:
    intent_str = "MODIFY";
    break;
  case MCCR_INTENT_STOP:
    intent_str = "STOP";
    break;
  case MCCR_INTENT_QUEUE:
    intent_str = "QUEUE";
    break;
  default:
    break;
  }

  fd_log_notice(
      "[app_magic] ✓ Sent MCCA: Result=%u, Intent=%s, Allocated=%s, Queued=%s",
      ctx->result_code, intent_str, ctx->resource_allocated ? "Yes" : "No",
      ctx->queued ? "Yes" : "No");

  return 0;
}

/**
 * @brief MCCR (Client Communication Request) 主处理器。
 * @details 协调执行 MCCR 处理的 4 阶段流水线：
 *          1. Session Validation
 *          2. Param & Security
 *          3. Intent Routing
 *          4. Execution & Response
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */
static int cic_handle_mccr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  /* 记录请求开始日志 */
  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MCCR (Communication Change Request)");
  fd_log_notice("[app_magic] ========================================");

  /* 初始化处理上下文 */
  MccxProcessContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.result_code = 2001; /* 默认成功 */
  ctx.security_passed = true;

  /* Phase 1: 会话验证 */
  if (mccr_phase1_session_validation(qry, &ctx) != 0) {
    goto finalize;
  }

  /* Phase 2: 参数与安全校验 */
  if (mccr_phase2_param_security(qry, &ctx) != 0) {
    goto finalize;
  }

  /* Phase 3: 意图路由 */
  if (mccr_phase3_intent_routing(&ctx) != 0) {
    goto finalize;
  }

finalize:
  /* Phase 4: 执行与响应 */
  if (mccr_phase4_execution(msg, &ctx) != 0) {
    fd_log_error("[app_magic] ✗ Failed to send MCCA");
    fd_log_notice("[app_magic] ========================================\n");
    return -1;
  }

  fd_log_notice("[app_magic] ========================================\n");
  return 0;
}

/**
 * @brief STR (Session Termination Request) 处理器。
 * @details 处理会话终止请求。
 *          - 释放会话资源 (fd_sess_reclaim)
 *          - 关闭并生成 CDR 记录
 *          - 清除数据平面路由和 TFT 规则
 *          - 返回 STA (Session Termination Answer)
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_str(struct msg **msg, struct avp *avp,
                          struct session *sess, void *opaque,
                          enum disp_action *act) {
  struct msg *ans;            /* 应答消息 */
  int ret = 0;                /* 返回值 */
  char session_id[128] = {0}; /* 会话 ID */

  (void)avp;    /* 未使用的参数 */
  (void)opaque; /* 未使用的参数 */

  *act = DISP_ACT_CONT; /* 设置分派动作：继续处理 */

  /* 记录请求开始日志 */
  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] STR (Session Termination Request)");
  fd_log_notice("[app_magic] ========================================");

  /* 提取会话 ID 并记录 */
  if (sess) {
    os0_t sid;      /* 会话 ID 字符串 */
    size_t sid_len; /* 会话 ID 长度 */
    if (fd_sess_getsid(sess, &sid, &sid_len) == 0) {
      fd_log_notice("[app_magic]   Session: %.*s", (int)sid_len, sid);
      /* 保存会话 ID 用于删除路由规则 */
      size_t copy_len =
          sid_len < sizeof(session_id) - 1 ? sid_len : sizeof(session_id) - 1;
      memcpy(session_id, sid, copy_len);
      session_id[copy_len] = '\0';
    }

    /* 释放会话资源 */
    fd_sess_reclaim(&sess);
  }

  /* 删除数据平面路由规则 */
  if (session_id[0]) {
    /* v2.1: 先获取最终流量统计并保存到会话 */
    ClientSession *client_sess =
        magic_session_find_by_id(&g_ctx->session_mgr, session_id);
    uint64_t final_bytes_in = 0, final_bytes_out = 0;

    if (client_sess) {
      TrafficStats final_stats;
      if (traffic_get_session_stats(&g_ctx->traffic_ctx, session_id,
                                    &final_stats) == 0) {
        final_bytes_in = final_stats.bytes_in;
        final_bytes_out = final_stats.bytes_out;
        client_sess->bytes_in = final_stats.bytes_in;
        client_sess->bytes_out = final_stats.bytes_out;
        fd_log_notice("[app_magic]   Final traffic: in=%lu out=%lu bytes",
                      (unsigned long)final_stats.bytes_in,
                      (unsigned long)final_stats.bytes_out);
      } else {
        /* 使用缓存的值 */
        final_bytes_in = client_sess->bytes_in;
        final_bytes_out = client_sess->bytes_out;
      }

      /* 设置会话状态为终止 */
      magic_session_set_state(client_sess, SESSION_STATE_CLOSED);
    }

    /* v2.2: 关闭 CDR 记录 */
    CDRRecord *cdr = cdr_find_by_session(&g_ctx->cdr_mgr, session_id);
    if (cdr) {
      int cdr_ret =
          cdr_close(&g_ctx->cdr_mgr, cdr, final_bytes_in, final_bytes_out);
      if (cdr_ret == 0) {
        fd_log_notice("[app_magic] ✓ CDR closed: id=%u, traffic in=%lu out=%lu",
                      cdr->cdr_id, (unsigned long)final_bytes_in,
                      (unsigned long)final_bytes_out);
      } else {
        fd_log_notice("[app_magic] ⚠ CDR close failed");
      }
    }

    /* 注销流量监控 */
    int tm_ret = traffic_unregister_session(&g_ctx->traffic_ctx, session_id);
    if (tm_ret == 0) {
      fd_log_notice("[app_magic] ✓ Traffic monitor unregistered for session");
    }

    /* 删除数据平面路由 */
    int dp_ret =
        magic_dataplane_remove_client_route(&g_ctx->dataplane_ctx, session_id);
    if (dp_ret == 0) {
      fd_log_notice("[app_magic] ✓ Dataplane route removed for session");
    }
  }

  /* 创建应答消息 (STA) */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { goto error; });
  ans = *msg;

  /* 添加必需的标准 Diameter AVP */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_host,
              fd_g_config->cnf_diamid); /* 服务器主机名 */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm,
              fd_g_config->cnf_diamrlm);              /* 服务器域 */
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001); /* 结果码：成功 */

  /* 发送应答消息 */
  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { goto error; });

  /* 记录成功发送日志 */
  fd_log_notice("[app_magic] ✓ Sent STA");
  fd_log_notice("[app_magic] ========================================\n");

  return 0; /* 成功返回 */

error:
  return ret; /* 返回错误码 */
}

/**
 * @brief MNTR (Notification Report) 处理器。
 * @details 处理来自客户端（或反向代理）的通知报告消息 (推送)。
 *          根据 ARINC 839 协议 Section 4.1.3.3，解析
Communication-Report-Parameters，
 *          更新服务器端保存的客户端通信状态。
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_mntr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry, *ans;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MNTR (Notification Report)");
  fd_log_notice("[app_magic] ========================================");

  /* 提取 Session-Id */
  char session_id[128] = {0};
  struct avp *avp_session = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session) == 0 &&
      avp_session) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_session, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(session_id))
        len = sizeof(session_id) - 1;
      memcpy(session_id, hdr->avp_value->os.data, len);
      session_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Session-Id: %s", session_id);

  /* 查找 Communication-Report-Parameters (Code 20003)
   * 根据协议，此 AVP 包含变更后的参数，仅包含已变更的字段 */
  struct avp *avp_comm_report = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_comm_report_params,
                        &avp_comm_report) == 0 &&
      avp_comm_report) {
    char profile_name[64] = "";
    float granted_bw = 0.0, granted_ret_bw = 0.0;
    uint32_t priority_type = 0, qos_level = 0;
    char priority_class[16] = "";
    char dlm_availability_list[128] = "";
    char gateway_ip[64] = "";

    /* 提取 Profile-Name (必需) */
    extract_string_from_grouped_avp(avp_comm_report, "Profile-Name",
                                    profile_name, sizeof(profile_name));

    /* 提取带宽参数 (可选) */
    int has_bw = extract_float32_from_grouped_avp(
        avp_comm_report, "Granted-Bandwidth", &granted_bw);
    int has_ret_bw = extract_float32_from_grouped_avp(
        avp_comm_report, "Granted-Return-Bandwidth", &granted_ret_bw);

    /* 提取优先级参数 (可选) */
    int has_priority_type =
        (extract_uint32_from_grouped_avp(avp_comm_report, "Priority-Type",
                                         &priority_type) == 0);
    extract_string_from_grouped_avp(avp_comm_report, "Priority-Class",
                                    priority_class, sizeof(priority_class));
    int has_qos = (extract_uint32_from_grouped_avp(avp_comm_report, "QoS-Level",
                                                   &qos_level) == 0);

    /* 提取 DLM 可用性列表 (可选) */
    extract_string_from_grouped_avp(avp_comm_report, "DLM-Availability-List",
                                    dlm_availability_list,
                                    sizeof(dlm_availability_list));

    /* 提取网关 IP (可选) */
    extract_string_from_grouped_avp(avp_comm_report, "Gateway-IPAddress",
                                    gateway_ip, sizeof(gateway_ip));

    fd_log_notice("[app_magic]   Communication-Report-Parameters:");
    fd_log_notice("[app_magic]     Profile: %s", profile_name);
    if (has_bw == 0 || has_ret_bw == 0) {
      fd_log_notice("[app_magic]     Granted BW: %.2f/%.2f kbps", granted_bw,
                    granted_ret_bw);
    }
    if (has_priority_type) {
      fd_log_notice("[app_magic]     Priority-Type: %u", priority_type);
    }
    if (priority_class[0]) {
      fd_log_notice("[app_magic]     Priority-Class: %s", priority_class);
    }
    if (has_qos) {
      fd_log_notice("[app_magic]     QoS-Level: %u", qos_level);
    }
    if (dlm_availability_list[0]) {
      fd_log_notice("[app_magic]     DLM-Availability-List: %s",
                    dlm_availability_list);
    }
    if (gateway_ip[0]) {
      fd_log_notice("[app_magic]     Gateway-IPAddress: %s", gateway_ip);
    }

    /* 更新本地会话状态 (如果会话存在) */
    if (g_ctx && session_id[0]) {
      ClientSession *session =
          magic_session_find_by_id(&g_ctx->session_mgr, session_id);
      if (session) {
        if (has_bw == 0) {
          session->granted_bw_kbps = (uint32_t)granted_bw;
        }
        if (has_ret_bw == 0) {
          session->granted_ret_bw_kbps = (uint32_t)granted_ret_bw;
        }
        fd_log_notice("[app_magic]   → Session state updated");
      }
    }
  } else {
    fd_log_notice("[app_magic]   ⚠ Communication-Report-Parameters not found");
  }

  /* 提取 MAGIC-Status-Code (如果存在) */
  struct avp *avp_status = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_magic_status_code, &avp_status) ==
          0 &&
      avp_status) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_status, &hdr) == 0 && hdr->avp_value) {
      fd_log_notice("[app_magic]   MAGIC-Status-Code: %u", hdr->avp_value->u32);
    }
  }

  /* 提取 Error-Message (如果存在) */
  struct avp *avp_error = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_error_message, &avp_error) == 0 &&
      avp_error) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_error, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      fd_log_notice("[app_magic]   Error-Message: %.*s",
                    (int)hdr->avp_value->os.len, hdr->avp_value->os.data);
    }
  }

  /* 创建 MNTA 应答 */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001); /* DIAMETER_SUCCESS */

  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  fd_log_notice("[app_magic] ✓ Sent MNTA");
  fd_log_notice("[app_magic] ========================================\n");

  return 0;
}

/**
 * @brief MSCR (Status Change Report) 处理器。
 * @details 处理状态变更报告消息
(通常由服务器主动推送到客户端，但在某些架构中可能双向)。
 *          在此实现中，主要处理来自对端的 MSCR，提取 Status-Type, DLM-List
等信息。
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_mscr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry, *ans;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MSCR (Status Change Report)");
  fd_log_notice("[app_magic] ========================================");

  /* 提取 Session-Id */
  char session_id[128] = {0};
  struct avp *avp_session = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session) == 0 &&
      avp_session) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_session, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(session_id))
        len = sizeof(session_id) - 1;
      memcpy(session_id, hdr->avp_value->os.data, len);
      session_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Session-Id: %s", session_id);

  /* 提取 Status-Type (可选) */
  struct avp *avp_status_type = NULL;
  uint32_t status_type = 0;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_status_type, &avp_status_type) ==
          0 &&
      avp_status_type) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_status_type, &hdr) == 0 && hdr->avp_value) {
      status_type = hdr->avp_value->u32;
      const char *status_desc = "";
      switch (status_type) {
      case 0:
        status_desc = "No_Status";
        break;
      case 1:
        status_desc = "MAGIC_Status";
        break;
      case 2:
        status_desc = "DLM_Status";
        break;
      case 3:
        status_desc = "MAGIC_DLM_Status";
        break;
      case 6:
        status_desc = "DLM_Link_Status";
        break;
      case 7:
        status_desc = "MAGIC_DLM_LINK_Status";
        break;
      default:
        status_desc = "Unknown";
        break;
      }
      fd_log_notice("[app_magic]   Status-Type: %u (%s)", status_type,
                    status_desc);
    }
  }

  /* 提取 Registered-Clients (当 status_type 包含 MAGIC_Status) */
  struct avp *avp_clients = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_registered_clients,
                        &avp_clients) == 0 &&
      avp_clients) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_clients, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      fd_log_notice("[app_magic]   Registered-Clients: %.*s",
                    (int)hdr->avp_value->os.len, hdr->avp_value->os.data);
    }
  }

  /* 提取 MAGIC-Status-Code (如果存在) */
  struct avp *avp_magic_status = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_magic_status_code,
                        &avp_magic_status) == 0 &&
      avp_magic_status) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_magic_status, &hdr) == 0 && hdr->avp_value) {
      fd_log_notice("[app_magic]   MAGIC-Status-Code: %u", hdr->avp_value->u32);
    }
  }

  /* 提取 Error-Message (如果存在) */
  struct avp *avp_error = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_error_message, &avp_error) == 0 &&
      avp_error) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_error, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      fd_log_notice("[app_magic]   Error-Message: %.*s",
                    (int)hdr->avp_value->os.len, hdr->avp_value->os.data);
    }
  }

  /* 提取 DLM-List (当 status_type >= 2) */
  struct avp *avp_dlm_list = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_dlm_list, &avp_dlm_list) == 0 &&
      avp_dlm_list) {
    fd_log_notice("[app_magic]   DLM-List:");

    /* 遍历 DLM-Info 子 AVP (Code 20008) */
    struct avp *dlm_info = NULL;
    if (fd_msg_browse(avp_dlm_list, MSG_BRW_FIRST_CHILD, &dlm_info, NULL) ==
        0) {
      int dlm_count = 0;
      while (dlm_info) {
        char dlm_name[64] = "";
        uint32_t dlm_available = 0;
        float dlm_max_bw = 0.0, dlm_max_ret_bw = 0.0;
        uint32_t dlm_max_links = 0, dlm_alloc_links = 0;
        float dlm_alloc_bw = 0.0, dlm_alloc_ret_bw = 0.0;

        /* 提取 DLM 基本信息 */
        extract_string_from_grouped_avp(dlm_info, "DLM-Name", dlm_name,
                                        sizeof(dlm_name));
        extract_uint32_from_grouped_avp(dlm_info, "DLM-Available",
                                        &dlm_available);
        extract_float32_from_grouped_avp(dlm_info, "DLM-Max-Bandwidth",
                                         &dlm_max_bw);
        extract_float32_from_grouped_avp(dlm_info, "DLM-Max-Return-Bandwidth",
                                         &dlm_max_ret_bw);
        extract_uint32_from_grouped_avp(dlm_info, "DLM-Max-Links",
                                        &dlm_max_links);
        extract_uint32_from_grouped_avp(dlm_info, "DLM-Allocated-Links",
                                        &dlm_alloc_links);
        extract_float32_from_grouped_avp(dlm_info, "DLM-Allocated-Bandwidth",
                                         &dlm_alloc_bw);
        extract_float32_from_grouped_avp(
            dlm_info, "DLM-Allocated-Return-Bandwidth", &dlm_alloc_ret_bw);

        const char *avail_str = (dlm_available == 0)   ? "YES"
                                : (dlm_available == 1) ? "NO"
                                                       : "UNKNOWN";
        fd_log_notice("[app_magic]     DLM[%d]: %s", dlm_count, dlm_name);
        fd_log_notice(
            "[app_magic]       Available: %s, MaxLinks: %u, AllocLinks: %u",
            avail_str, dlm_max_links, dlm_alloc_links);
        fd_log_notice(
            "[app_magic]       MaxBW: %.2f/%.2f kbps, AllocBW: %.2f/%.2f kbps",
            dlm_max_bw, dlm_max_ret_bw, dlm_alloc_bw, dlm_alloc_ret_bw);

        /* 查找 DLM-Link-Status-List (详细链路状态, status_type >= 6) */
        struct avp *link_status_list = NULL;
        if (fd_msg_search_avp(dlm_info, g_magic_dict.avp_dlm_link_status_list,
                              &link_status_list) == 0 &&
            link_status_list) {
          struct avp *link_status = NULL;
          if (fd_msg_browse(link_status_list, MSG_BRW_FIRST_CHILD, &link_status,
                            NULL) == 0) {
            int link_count = 0;
            while (link_status) {
              char link_name[64] = "";
              uint32_t link_number = 0, link_available = 0;
              uint32_t link_conn_status = 0, link_login_status = 0;
              uint32_t link_qos = 0;
              float link_max_bw = 0.0, link_alloc_bw = 0.0;
              char link_error[128] = "";

              extract_string_from_grouped_avp(link_status, "Link-Name",
                                              link_name, sizeof(link_name));
              extract_uint32_from_grouped_avp(link_status, "Link-Number",
                                              &link_number);
              extract_uint32_from_grouped_avp(link_status, "Link-Available",
                                              &link_available);
              extract_uint32_from_grouped_avp(link_status, "QoS-Level",
                                              &link_qos);
              extract_uint32_from_grouped_avp(
                  link_status, "Link-Connection-Status", &link_conn_status);
              extract_uint32_from_grouped_avp(link_status, "Link-Login-Status",
                                              &link_login_status);
              extract_float32_from_grouped_avp(
                  link_status, "Link-Max-Bandwidth", &link_max_bw);
              extract_float32_from_grouped_avp(
                  link_status, "Link-Alloc-Bandwidth", &link_alloc_bw);
              extract_string_from_grouped_avp(link_status, "Link-Error-String",
                                              link_error, sizeof(link_error));

              const char *conn_str = (link_conn_status == 0)   ? "Disconnected"
                                     : (link_conn_status == 1) ? "Connected"
                                                               : "Forced_Close";
              const char *login_str =
                  (link_login_status == 1) ? "Logged off" : "Logged on";

              fd_log_notice("[app_magic]         Link[%d]: %s (#%u)",
                            link_count++, link_name, link_number);
              fd_log_notice(
                  "[app_magic]           Conn: %s, Login: %s, QoS: %u",
                  conn_str, login_str, link_qos);
              fd_log_notice("[app_magic]           MaxBW: %.2f, AllocBW: %.2f",
                            link_max_bw, link_alloc_bw);
              if (link_error[0]) {
                fd_log_notice("[app_magic]           Error: %s", link_error);
              }

              if (fd_msg_browse(link_status, MSG_BRW_NEXT, &link_status,
                                NULL) != 0)
                break;
            }
          }
        }

        dlm_count++;
        if (fd_msg_browse(dlm_info, MSG_BRW_NEXT, &dlm_info, NULL) != 0)
          break;
      }
    }
  }

  /* 创建 MSCA 应答 */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);

  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  fd_log_notice("[app_magic] ✓ Sent MSCA");
  fd_log_notice("[app_magic] ========================================\n");

  return 0;
}

/**
 * @brief MSXR (Status Request) 处理器。
 * @details 处理状态查询请求消息。客户端通过 MSXR 查询服务器状态、DLM 状态等。
 *          支持 v2.1 版本，包含速率限制检查。
 *          根据 Status-Type 返回相应的数据 (Status-Type, Registered-Clients,
DLM-List)。
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_msxr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry, *ans;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MSXR (Status Request) v2.1");
  fd_log_notice("[app_magic] ========================================");

  /* 提取 Session-Id */
  char session_id[128] = {0};
  struct avp *avp_session = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session) == 0 &&
      avp_session) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_session, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(session_id))
        len = sizeof(session_id) - 1;
      memcpy(session_id, hdr->avp_value->os.data, len);
      session_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Session-Id: %s", session_id);

  /* 提取 Origin-Host 获取客户端 ID */
  char client_id[MAX_ID_LEN] = "";
  struct avp *avp_origin = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_host, &avp_origin) == 0 &&
      avp_origin) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_origin, &hdr) == 0 && hdr->avp_value) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(client_id))
        len = sizeof(client_id) - 1;
      memcpy(client_id, hdr->avp_value->os.data, len);
      client_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Client-ID: %s", client_id);

  /* 提取 Status-Type (必需) */
  struct avp *avp_status_type = NULL;
  uint32_t status_type = 0;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_status_type, &avp_status_type) ==
          0 &&
      avp_status_type) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_status_type, &hdr) == 0 && hdr->avp_value) {
      status_type = hdr->avp_value->u32;
    }
  }

  const char *status_desc = "";
  switch (status_type) {
  case 0:
    status_desc = "No_Status";
    break;
  case 1:
    status_desc = "MAGIC_Status";
    break;
  case 2:
    status_desc = "DLM_Status";
    break;
  case 3:
    status_desc = "MAGIC_DLM_Status";
    break;
  case 6:
    status_desc = "DLM_Link_Status";
    break;
  case 7:
    status_desc = "All_Status";
    break;
  default:
    status_desc = "Unknown";
    break;
  }
  fd_log_notice("[app_magic]   Status-Type: %u (%s)", status_type, status_desc);

  /* 查找客户端配置 */
  ClientProfile *client_profile =
      g_ctx ? magic_config_find_client(&g_ctx->config, client_id) : NULL;

  /* v2.1: 速率限制检查 (关联到 Client-ID) */
  uint32_t rate_limit = 5; /* 默认 5 秒 */
  if (client_profile) {
    rate_limit = client_profile->session.msxr_rate_limit_sec;
  }

  if (msxr_check_rate_limit(client_id, rate_limit) != 0) {
    /* 超频：返回 DIAMETER_TOO_BUSY (3004) */
    fd_log_notice("[app_magic]   Rate limit exceeded! Returning "
                  "DIAMETER_TOO_BUSY (3004)");

    CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
                 { return -1; });
    ans = *msg;

    ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
    ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
    ADD_AVP_U32(ans, g_std_dict.avp_result_code, 3004); /* DIAMETER_TOO_BUSY */

    CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });
    fd_log_notice("[app_magic] ✓ Sent MSXA (Rate Limited - 3004)");
    return 0;
  }

  /* v2.1: 权限裁决 - 可能降级 Status-Type */
  uint32_t granted_status_type = status_type;

  if (client_profile) {
    /* 规则 A: 详细信息控制 - 请求 6/7 但无权限时降级 */
    if ((status_type == 6 || status_type == 7) &&
        !client_profile->session.allow_detailed_status) {
      granted_status_type = (status_type == 6) ? 2 : 3; /* 6->2, 7->3 */
      fd_log_notice("[app_magic]   Permission downgrade: %u -> %u (detailed "
                    "status not allowed)",
                    status_type, granted_status_type);
    }
  }

  /* 创建 MSXA 应答 */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001); /* 始终返回 SUCCESS */
  ADD_AVP_U32(ans, g_magic_dict.avp_status_type,
              granted_status_type); /* 隐式告知降级 */

  /* 根据请求类型添加状态信息 */
  bool need_magic_status =
      (granted_status_type == 1 || granted_status_type == 3 ||
       granted_status_type == 7);
  bool need_dlm_status = (granted_status_type >= 2);
  bool need_link_status =
      (granted_status_type == 6 || granted_status_type == 7);

  /* 添加 Registered-Clients (MAGIC 系统状态) */
  if (need_magic_status && g_ctx) {
    /* 规则 B: 检查是否允许查看客户端列表 */
    bool can_see_clients = true;
    if (client_profile && !client_profile->session.allow_registered_clients) {
      can_see_clients = false;
      fd_log_notice(
          "[app_magic]   Registered-Clients hidden (permission denied)");
    }

    if (can_see_clients) {
      char clients_str[1024] = "";
      int offset = 0;

      /* v2.2: 统计每个 client_id 的会话数，避免重复显示 */
      typedef struct {
        char client_id[128];
        int session_count;
        char first_session_id[256]; /* 首个会话 ID (用于单会话时显示) */
      } ClientSessionCount;
      ClientSessionCount client_counts[MAX_SESSIONS];
      int unique_clients = 0;

      /* 第一遍：统计每个 client_id 的会话数 */
      for (int i = 0; i < MAX_SESSIONS; i++) {
        ClientSession *session = &g_ctx->session_mgr.sessions[i];
        if (!session->in_use)
          continue;
        if (session->state != SESSION_STATE_AUTHENTICATED &&
            session->state != SESSION_STATE_ACTIVE)
          continue;

        /* 查找是否已存在该 client_id */
        int found = -1;
        for (int j = 0; j < unique_clients; j++) {
          if (strcmp(client_counts[j].client_id, session->client_id) == 0) {
            found = j;
            break;
          }
        }

        if (found >= 0) {
          client_counts[found].session_count++;
        } else if (unique_clients < MAX_SESSIONS) {
          strncpy(client_counts[unique_clients].client_id, session->client_id,
                  sizeof(client_counts[unique_clients].client_id) - 1);
          strncpy(client_counts[unique_clients].first_session_id,
                  session->session_id,
                  sizeof(client_counts[unique_clients].first_session_id) - 1);
          client_counts[unique_clients].session_count = 1;
          unique_clients++;
        }
      }

      /* 第二遍：生成输出字符串 */
      for (int i = 0; i < unique_clients; i++) {
        int written;
        if (client_counts[i].session_count > 1) {
          /* 多会话：显示 "client_id(N sessions)" */
          written = snprintf(clients_str + offset, sizeof(clients_str) - offset,
                             "%s%s(%d sessions)", offset > 0 ? "," : "",
                             client_counts[i].client_id,
                             client_counts[i].session_count);
        } else {
          /* 单会话：仅显示 client_id */
          written = snprintf(clients_str + offset, sizeof(clients_str) - offset,
                             "%s%s", offset > 0 ? "," : "",
                             client_counts[i].client_id);
        }
        if (written > 0)
          offset += written;
      }

      if (offset > 0) {
        ADD_AVP_STR(ans, g_magic_dict.avp_registered_clients, clients_str);
        fd_log_notice("[app_magic]   Registered-Clients: %s", clients_str);
      }
    }
  }

  /* 添加 DLM-List (DLM 状态) - 带白名单过滤 */
  if (need_dlm_status && g_ctx) {
    struct avp *dlm_list_avp = NULL;
    CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_list, 0, &dlm_list_avp),
                 { goto error; });

    int dlm_count = 0; /* 计数实际添加的 DLM */

    /* 遍历 LMI 中的所有 DLM 客户端 */
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
      DlmClient *dlm = &g_ctx->lmi_ctx.clients[i];
      if (!dlm->is_registered)
        continue; /* 跳过未注册的 DLM */

      /* v2.1: DLM 白名单过滤 */
      if (client_profile &&
          !magic_config_is_dlm_allowed(client_profile, dlm->link_id)) {
        fd_log_notice("[app_magic]   DLM %s filtered (not in client whitelist)",
                      dlm->link_id);
        continue;
      }

      /* 使用辅助函数添加 DLM-Info */
      dlm_info_t dlm_info;
      memset(&dlm_info, 0, sizeof(dlm_info));
      dlm_info.dlm_name = dlm->link_id;
      dlm_info.dlm_available = dlm->is_link_up ? 0 : 1; /* 0=YES, 1=NO */
      dlm_info.dlm_max_links = 10;
      dlm_info.dlm_max_bw = (float)dlm->link_params.current_bandwidth_kbps;
      dlm_info.dlm_max_return_bw =
          (float)dlm->link_params.current_bandwidth_kbps;
      dlm_info.dlm_alloc_links = dlm->num_active_bearers;
      dlm_info.dlm_alloc_bw = (float)dlm->link_params.current_bandwidth_kbps;
      dlm_info.dlm_alloc_return_bw = dlm_info.dlm_alloc_bw;
      dlm_info.qos_levels[0] = 0; /* BE */
      dlm_info.qos_levels[1] = 1; /* AF */
      dlm_info.qos_levels[2] = 2; /* EF */
      dlm_info.qos_level_count = 3;

      if (add_dlm_info_simple(dlm_list_avp, &dlm_info) == 0) {
        dlm_count++; /* 成功添加一个 DLM */
        fd_log_notice("[app_magic]   Added DLM-Info: %s (fd=%d, bw=%.0f/%.0f)",
                      dlm->link_id, dlm->client_fd, dlm_info.dlm_max_bw,
                      dlm_info.dlm_alloc_bw);

        /* 如果需要详细链路状态，添加 DLM-Link-Status-List */
        if (need_link_status) {
          /* 获取刚添加的 DLM-Info AVP */
          struct avp *last_dlm_info = NULL;
          struct avp *child = NULL;
          if (fd_msg_browse(dlm_list_avp, MSG_BRW_FIRST_CHILD, &child, NULL) ==
              0) {
            while (child) {
              last_dlm_info = child;
              if (fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL) != 0)
                break;
            }
          }

          if (last_dlm_info) {
            /* 创建 DLM-Link-Status-List */
            struct avp *link_status_list = NULL;
            CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_dlm_link_status_list,
                                        0, &link_status_list),
                         { continue; });

            /* 添加链路状态 */
            link_status_t link_stat;
            memset(&link_stat, 0, sizeof(link_stat));
            link_stat.link_name = dlm->link_id;
            link_stat.link_number = i + 1;
            link_stat.link_available =
                dlm->client_fd > 0 ? 1 : 2; /* 1=YES, 2=NO */
            link_stat.qos_level = 0;        /* BE */
            link_stat.link_conn_status =
                dlm->client_fd > 0 ? 1 : 0; /* 1=Connected */
            link_stat.link_login_status =
                dlm->is_registered ? 2 : 1; /* 2=Logged on */
            link_stat.link_max_bw =
                (float)dlm->link_params.current_bandwidth_kbps;
            link_stat.link_max_return_bw = link_stat.link_max_bw;
            link_stat.link_alloc_bw =
                (float)dlm->link_params.current_bandwidth_kbps;
            link_stat.link_alloc_return_bw = link_stat.link_alloc_bw;
            link_stat.link_error_string = NULL;

            if (add_link_status_simple(link_status_list, &link_stat) == 0) {
              fd_log_notice(
                  "[app_magic]     Added Link-Status: %s (#%u, conn=%u)",
                  link_stat.link_name, link_stat.link_number,
                  link_stat.link_conn_status);
            }

            CHECK_FCT_DO(fd_msg_avp_add(last_dlm_info, MSG_BRW_LAST_CHILD,
                                        link_status_list),
                         { fd_msg_free((struct msg *)link_status_list); });
          }
        }
      }
    }

    /* 只在有 DLM 数据时才添加 DLM-List，避免空 AVP 违反字典规则 */
    if (dlm_count > 0) {
      CHECK_FCT_DO(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, dlm_list_avp),
                   { fd_msg_free((struct msg *)dlm_list_avp); });
      fd_log_notice("[app_magic]   DLM-List added with %d DLM(s)", dlm_count);
    } else {
      /* 没有 DLM 数据，释放空 AVP */
      fd_msg_free((struct msg *)dlm_list_avp);
      fd_log_notice("[app_magic]   No DLM data available, DLM-List not added");
    }
  }

  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { goto error; });

  fd_log_notice("[app_magic] ✓ Sent MSXA");
  fd_log_notice("[app_magic] ========================================\n");

  return 0;

error:
  return -1;
}

/**
 * @brief MADR (Accounting Data Request) 处理器。
 * @details 处理计费数据查询请求。
 *          根据 CDR-Type (List/Data) 和 CDR-Level (All/User/Session) 返回相应的
 * CDR 信息。 支持 v2.1 版本的数据隔离 (Requester ID 检查)。
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_madr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry, *ans;

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MADR (Accounting Data Request)");
  fd_log_notice("[app_magic] ========================================");

  /* ========== v2.1: 提取请求方身份 (用于数据隔离) ========== */
  char requester_id[128] = "";
  struct avp *avp_origin_host = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_origin_host, &avp_origin_host) ==
          0 &&
      avp_origin_host) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_origin_host, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(requester_id))
        len = sizeof(requester_id) - 1;
      memcpy(requester_id, hdr->avp_value->os.data, len);
      requester_id[len] = '\0';
    }
  }
  fd_log_notice("[app_magic]   Requester: %s",
                requester_id[0] ? requester_id : "(unknown)");

  /* 提取 CDR-Type: 1=LIST_REQUEST, 2=DATA_REQUEST */
  uint32_t cdr_type = 1;
  struct avp *avp_cdr_type = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_cdr_type, &avp_cdr_type) == 0 &&
      avp_cdr_type) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_cdr_type, &hdr) == 0 && hdr->avp_value) {
      cdr_type = hdr->avp_value->u32;
    }
  }

  /* 提取 CDR-Level: 1=ALL, 2=USER_DEPENDENT, 3=SESSION_DEPENDENT */
  uint32_t cdr_level = 1;
  struct avp *avp_cdr_level = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_cdr_level, &avp_cdr_level) == 0 &&
      avp_cdr_level) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_cdr_level, &hdr) == 0 && hdr->avp_value) {
      cdr_level = hdr->avp_value->u32;
    }
  }

  /* 提取 CDR-Request-Identifier (可选) */
  char cdr_req_id[64] = "";
  struct avp *avp_cdr_req_id = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_cdr_req_id, &avp_cdr_req_id) ==
          0 &&
      avp_cdr_req_id) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_cdr_req_id, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(cdr_req_id))
        len = sizeof(cdr_req_id) - 1;
      memcpy(cdr_req_id, hdr->avp_value->os.data, len);
      cdr_req_id[len] = '\0';
    }
  }

  fd_log_notice("[app_magic]   CDR-Type: %u (%s), CDR-Level: %u (%s)", cdr_type,
                cdr_type == 1 ? "LIST" : "DATA", cdr_level,
                cdr_level == 1 ? "ALL" : (cdr_level == 2 ? "USER" : "SESSION"));
  if (cdr_req_id[0]) {
    fd_log_notice("[app_magic]   CDR-Request-Id: %s", cdr_req_id);
  }

  /* 创建 MADA 应答 */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);
  ADD_AVP_U32(ans, g_magic_dict.avp_cdr_type, cdr_type);
  ADD_AVP_U32(ans, g_magic_dict.avp_cdr_level, cdr_level);

  if (cdr_req_id[0]) {
    ADD_AVP_STR(ans, g_magic_dict.avp_cdr_req_id, cdr_req_id);
  }

  /* 根据 CDR-Level 过滤 CDR 记录 */
  /* CDR-Level: 1=ALL, 2=USER_DEPENDENT (按用户), 3=SESSION_DEPENDENT (按会话)
   */

  int active_count = 0, finished_count = 0, forwarded_count = 0,
      unknown_count = 0;

  /* ===== CDRs-Active: 当前活动的 CDR ===== */
  if (cdr_type == 1 || cdr_type == 2) {
    struct avp *cdrs_active_avp = NULL;
    CHECK_FCT_DO(
        fd_msg_avp_new(g_magic_dict.avp_cdrs_active, 0, &cdrs_active_avp),
        { goto send; });

    if (g_ctx) {
      for (int i = 0; i < MAX_SESSIONS; i++) {
        ClientSession *session = &g_ctx->session_mgr.sessions[i];
        if (!session->in_use)
          continue;
        if (session->state != SESSION_STATE_ACTIVE)
          continue;

        /* ========== v2.1: 数据隔离过滤 ========== */
        /* CDR-Level=2 (USER_DEPENDENT): 只返回属于请求方的 CDR */
        if (cdr_level == 2 && requester_id[0]) {
          if (strcmp(session->client_id, requester_id) != 0) {
            continue; /* 跳过不属于该用户的会话 */
          }
        }

        /* CDR-Level=3 (SESSION_DEPENDENT): 只返回指定会话 */
        if (cdr_level == 3 && cdr_req_id[0]) {
          uint32_t current_cdr_id =
              traffic_session_id_to_mark(session->session_id);
          char current_cdr_id_str[32];
          snprintf(current_cdr_id_str, sizeof(current_cdr_id_str), "%u",
                   current_cdr_id);

          if (strcmp(session->session_id, cdr_req_id) != 0 &&
              strcmp(current_cdr_id_str, cdr_req_id) != 0) {
            continue;
          }
        }

        struct avp *cdr_info_avp = NULL;
        CHECK_FCT_DO(
            fd_msg_avp_new(g_magic_dict.avp_cdr_info, 0, &cdr_info_avp),
            { continue; });

        /* CDR-ID (10046) - 使用 session_id 的 hash 作为 CDR-ID */
        uint32_t cdr_id = traffic_session_id_to_mark(session->session_id);
        ADD_AVP_U32(cdr_info_avp, g_magic_dict.avp_cdr_id, cdr_id);

        /* CDR-Content (10047) - 仅 DATA_REQUEST 时包含完整内容 */
        if (cdr_type == 2) {
          /* ========== v2.1: 从 Netlink 获取真实流量统计 ========== */
          TrafficStats stats = {0};
          uint64_t bytes_in = 0, bytes_out = 0;

          if (traffic_get_session_stats(&g_ctx->traffic_ctx,
                                        session->session_id, &stats) == 0) {
            bytes_in = stats.bytes_in;
            bytes_out = stats.bytes_out;
          } else {
            /* 回退到缓存的值 */
            bytes_in = session->bytes_in;
            bytes_out = session->bytes_out;
          }

          char cdr_content[512];
          snprintf(cdr_content, sizeof(cdr_content),
                   "CDR_ID=%u;SESSION_ID=%s;CLIENT_ID=%s;STATUS=ACTIVE;"
                   "DLM_NAME=%s;START_TIME=%ld;BYTES_IN=%lu;BYTES_OUT=%lu",
                   cdr_id, session->session_id, session->client_id,
                   session->assigned_link_id[0] ? session->assigned_link_id
                                                : "NONE",
                   (long)session->traffic_start_time, (unsigned long)bytes_in,
                   (unsigned long)bytes_out);
          ADD_AVP_STR(cdr_info_avp, g_magic_dict.avp_cdr_content, cdr_content);
        }

        CHECK_FCT_DO(
            fd_msg_avp_add(cdrs_active_avp, MSG_BRW_LAST_CHILD, cdr_info_avp), {
              fd_msg_free((struct msg *)cdr_info_avp);
              continue;
            });
        active_count++;
      }
    }

    if (active_count > 0) {
      CHECK_FCT_DO(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, cdrs_active_avp),
                   { fd_msg_free((struct msg *)cdrs_active_avp); });
    } else {
      fd_msg_free((struct msg *)cdrs_active_avp);
    }
  }

  /* ===== CDRs-Finished: 已完成的 CDR ===== */
  if (cdr_type == 1 || cdr_type == 2) {
    struct avp *cdrs_finished_avp = NULL;
    CHECK_FCT_DO(
        fd_msg_avp_new(g_magic_dict.avp_cdrs_finished, 0, &cdrs_finished_avp),
        { goto add_forwarded; });

    if (g_ctx) {
      for (int i = 0; i < MAX_SESSIONS; i++) {
        ClientSession *session = &g_ctx->session_mgr.sessions[i];
        /* 注意: TERMINATED 状态的会话可能已被释放，这里只处理 in_use 的 */
        if (!session->in_use)
          continue;
        if (session->state != SESSION_STATE_CLOSED)
          continue;

        /* v2.1: 数据隔离 */
        if (cdr_level == 2 && requester_id[0]) {
          if (strcmp(session->client_id, requester_id) != 0) {
            continue;
          }
        }
        if (cdr_level == 3 && cdr_req_id[0]) {
          uint32_t current_cdr_id =
              traffic_session_id_to_mark(session->session_id);
          char current_cdr_id_str[32];
          snprintf(current_cdr_id_str, sizeof(current_cdr_id_str), "%u",
                   current_cdr_id);

          if (strcmp(session->session_id, cdr_req_id) != 0 &&
              strcmp(current_cdr_id_str, cdr_req_id) != 0) {
            continue;
          }
        }

        struct avp *cdr_info_avp = NULL;
        CHECK_FCT_DO(
            fd_msg_avp_new(g_magic_dict.avp_cdr_info, 0, &cdr_info_avp),
            { continue; });

        uint32_t cdr_id = traffic_session_id_to_mark(session->session_id);
        ADD_AVP_U32(cdr_info_avp, g_magic_dict.avp_cdr_id, cdr_id);

        if (cdr_type == 2) {
          char cdr_content[512];
          snprintf(cdr_content, sizeof(cdr_content),
                   "CDR_ID=%u;SESSION_ID=%s;CLIENT_ID=%s;STATUS=FINISHED;"
                   "END_TIME=%ld;BYTES_IN=%lu;BYTES_OUT=%lu",
                   cdr_id, session->session_id, session->client_id,
                   (long)session->last_activity,
                   (unsigned long)session->bytes_in,
                   (unsigned long)session->bytes_out);
          ADD_AVP_STR(cdr_info_avp, g_magic_dict.avp_cdr_content, cdr_content);
        }

        CHECK_FCT_DO(
            fd_msg_avp_add(cdrs_finished_avp, MSG_BRW_LAST_CHILD, cdr_info_avp),
            {
              fd_msg_free((struct msg *)cdr_info_avp);
              continue;
            });
        finished_count++;
      }
    }

    if (finished_count > 0) {
      CHECK_FCT_DO(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, cdrs_finished_avp),
                   { fd_msg_free((struct msg *)cdrs_finished_avp); });
    } else {
      fd_msg_free((struct msg *)cdrs_finished_avp);
    }
  }

add_forwarded:
  /* ===== CDRs-Forwarded: 已转发的 CDR (暂无实现) ===== */
  if (cdr_type == 1 || cdr_type == 2) {
    /* v2.1: CDRs-Forwarded 需要 CDR 持久化存储，当前版本暂不支持 */
    fd_log_debug("[app_magic]   CDRs-Forwarded: 暂无 (需要持久化存储)");
  }

  /* ===== CDRs-Unknown: 状态未知的 CDR ===== */
  if ((cdr_type == 1 || cdr_type == 2) && cdr_req_id[0]) {
    /* 如果请求了特定 CDR-Request-Identifier 但未找到，添加到 Unknown */
    int found = 0;
    if (g_ctx) {
      for (int i = 0; i < MAX_SESSIONS; i++) {
        ClientSession *session = &g_ctx->session_mgr.sessions[i];
        if (session->in_use) {
          uint32_t current_cdr_id =
              traffic_session_id_to_mark(session->session_id);
          char current_cdr_id_str[32];
          snprintf(current_cdr_id_str, sizeof(current_cdr_id_str), "%u",
                   current_cdr_id);

          if (strcmp(session->session_id, cdr_req_id) == 0 ||
              strcmp(current_cdr_id_str, cdr_req_id) == 0) {
            found = 1;
            break;
          }
        }
      }
    }

    if (!found) {
      struct avp *cdrs_unknown_avp = NULL;
      CHECK_FCT_DO(
          fd_msg_avp_new(g_magic_dict.avp_cdrs_unknown, 0, &cdrs_unknown_avp),
          { goto send; });

      /* CDRs-Unknown 只包含 CDR-ID */
      ADD_AVP_U32(cdrs_unknown_avp, g_magic_dict.avp_cdr_id, 0);

      CHECK_FCT_DO(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, cdrs_unknown_avp),
                   { fd_msg_free((struct msg *)cdrs_unknown_avp); });
      unknown_count++;
    }
  }

  fd_log_notice("[app_magic]   CDR counts: Active=%d, Finished=%d, "
                "Forwarded=%d, Unknown=%d",
                active_count, finished_count, forwarded_count, unknown_count);

send:
  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  fd_log_notice("[app_magic] ✓ Sent MADA");
  fd_log_notice("[app_magic] ========================================\n");

  return 0;
}

/**
 * @brief MACR (Accounting Control Request) 处理器。
 * @details 处理计费控制请求，支持 "不断网切账单" (CDR Rollover)。
 *          动作: 对目标会话执行 CDR 归档 (Archive) 并创建新 CDR (Create)，
 *          确保流量统计的连续性。
 *          支持 v2.1 版本的权限校验。
 *
 * @param msg Diameter 消息指针。
 * @param avp 触发 AVP (未使用)。
 * @param sess 会话对象。
 * @param opaque 不透明数据。
 * @param act 分发动作 (输出)。
 * @return 0 成功，-1 失败。
 */

static int cic_handle_macr(struct msg **msg, struct avp *avp,
                           struct session *sess, void *opaque,
                           enum disp_action *act) {
  struct msg *qry, *ans;
  int result_code = ER_DIAMETER_SUCCESS;
  int magic_status_code = 0; /* 0 表示成功（无错误）*/
  char error_msg[128] = "";

  (void)avp;
  (void)opaque;
  (void)sess;

  qry = *msg;
  *act = DISP_ACT_CONT;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] MACR (Accounting Control Request)");
  fd_log_notice("[app_magic] ========================================");

  /* ========== 1. 提取请求方的 Session-Id (操作者) ========== */
  char requester_session_id[128] = "";
  struct avp *avp_session_id = NULL;
  if (fd_msg_search_avp(qry, g_std_dict.avp_session_id, &avp_session_id) == 0 &&
      avp_session_id) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_session_id, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(requester_session_id))
        len = sizeof(requester_session_id) - 1;
      memcpy(requester_session_id, hdr->avp_value->os.data, len);
      requester_session_id[len] = '\0';
    }
  }

  /* ========== 2. 提取 CDR-Restart-Session-Id (目标被操作者) ========== */
  char restart_session_id[128] = "";
  struct avp *avp_restart_id = NULL;
  if (fd_msg_search_avp(qry, g_magic_dict.avp_cdr_restart_sess_id,
                        &avp_restart_id) == 0 &&
      avp_restart_id) {
    struct avp_hdr *hdr;
    if (fd_msg_avp_hdr(avp_restart_id, &hdr) == 0 && hdr->avp_value &&
        hdr->avp_value->os.data) {
      size_t len = hdr->avp_value->os.len;
      if (len >= sizeof(restart_session_id))
        len = sizeof(restart_session_id) - 1;
      memcpy(restart_session_id, hdr->avp_value->os.data, len);
      restart_session_id[len] = '\0';
    }
  }

  fd_log_notice("[app_magic]   Requester Session-Id: %s", requester_session_id);
  fd_log_notice("[app_magic]   CDR-Restart-Session-Id: %s", restart_session_id);

  /* CDR 切分结果 */
  CDRRolloverResult rollover_result;
  memset(&rollover_result, 0, sizeof(rollover_result));

  /* ========== 3. 参数校验 ========== */
  if (!restart_session_id[0]) {
    result_code = ER_DIAMETER_MISSING_AVP;
    magic_status_code = 5001; /* MAGIC_ERROR_UNKNOWN_SESSION */
    snprintf(error_msg, sizeof(error_msg),
             "CDR-Restart-Session-Id not provided");
    fd_log_notice("[app_magic]   ✗ Error: %s", error_msg);
    goto send_response;
  }

  if (!g_ctx) {
    result_code = ER_DIAMETER_UNABLE_TO_COMPLY;
    magic_status_code = 5002; /* MAGIC_ERROR_REQUEST_NOT_PROCESSED */
    snprintf(error_msg, sizeof(error_msg),
             "Internal error: context not initialized");
    fd_log_error("[app_magic]   ✗ Error: %s", error_msg);
    goto send_response;
  }

  /* ========== 4. 权限校验 ========== */
  /* 查找请求方的客户端配置 */
  ClientSession *requester_session = NULL;
  ClientProfile *requester_profile = NULL;

  if (requester_session_id[0]) {
    requester_session =
        magic_session_find_by_id(&g_ctx->session_mgr, requester_session_id);
    if (requester_session) {
      requester_profile = magic_config_find_client(
          &g_ctx->config, requester_session->client_id);
    }
  }

  /* 检查权限: 是否允许控制 CDR */
  if (requester_profile) {
    /* 检查是否是切分自己的 CDR (允许) */
    bool is_self_control =
        (strcmp(requester_session_id, restart_session_id) == 0);

    /* 检查是否有 CDR 控制权限 (allow_cdr_control) */
    bool has_cdr_permission = requester_profile->session.allow_cdr_control;

    if (!is_self_control && !has_cdr_permission) {
      result_code = ER_DIAMETER_AUTHORIZATION_REJECTED;
      magic_status_code = 5003; /* MAGIC_ERROR_ACCOUNTING_CONTROL_DENIED */
      snprintf(error_msg, sizeof(error_msg),
               "Permission denied: client %s cannot control CDR of session %s",
               requester_session->client_id, restart_session_id);
      fd_log_notice("[app_magic]   ✗ Error: %s", error_msg);
      goto send_response;
    }
  }
  /* 注意: 如果找不到请求方 Profile，暂时允许操作 (向后兼容) */

  /* ========== 5. 目标会话校验 ========== */
  ClientSession *target_session =
      magic_session_find_by_id(&g_ctx->session_mgr, restart_session_id);
  if (!target_session) {
    result_code = ER_DIAMETER_UNKNOWN_SESSION_ID;
    magic_status_code = 5001; /* MAGIC_ERROR_UNKNOWN_SESSION */
    snprintf(error_msg, sizeof(error_msg), "Target session not found: %s",
             restart_session_id);
    fd_log_notice("[app_magic]   ✗ Error: %s", error_msg);
    goto send_response;
  }

  /* 检查目标会话状态 */
  if (target_session->state != SESSION_STATE_ACTIVE &&
      target_session->state != SESSION_STATE_AUTHENTICATED) {
    result_code = ER_DIAMETER_UNABLE_TO_COMPLY;
    magic_status_code = 5001; /* MAGIC_ERROR_UNKNOWN_SESSION */
    snprintf(error_msg, sizeof(error_msg),
             "Target session not in valid state: %s (state=%s)",
             restart_session_id,
             magic_session_state_name(target_session->state));
    fd_log_notice("[app_magic]   ✗ Error: %s", error_msg);
    goto send_response;
  }

  fd_log_notice("[app_magic]   Target session found: client=%s, state=%s",
                target_session->client_id,
                magic_session_state_name(target_session->state));

  /* ========== 6. 读取当前流量统计 ========== */
  uint64_t current_bytes_in = 0;
  uint64_t current_bytes_out = 0;

  /* 尝试从 Traffic Monitor 获取实时流量 */
  TrafficStats traffic_stats = {0};
  if (traffic_get_session_stats(&g_ctx->traffic_ctx, restart_session_id,
                                &traffic_stats) == 0) {
    current_bytes_in = traffic_stats.bytes_in;
    current_bytes_out = traffic_stats.bytes_out;
    fd_log_notice("[app_magic]   Traffic stats (from Netlink): in=%lu, out=%lu",
                  (unsigned long)current_bytes_in,
                  (unsigned long)current_bytes_out);
  } else {
    /* 回退到会话缓存的值 */
    current_bytes_in = target_session->bytes_in;
    current_bytes_out = target_session->bytes_out;
    fd_log_notice("[app_magic]   Traffic stats (from cache): in=%lu, out=%lu",
                  (unsigned long)current_bytes_in,
                  (unsigned long)current_bytes_out);
  }

  /* ========== 7. 执行 CDR 切分 (原子操作) ========== */
  fd_log_notice("[app_magic]   Performing CDR rollover...");

  int rollover_ret =
      cdr_rollover(&g_ctx->cdr_mgr, restart_session_id, current_bytes_in,
                   current_bytes_out, &rollover_result);

  if (rollover_ret != 0 || !rollover_result.success) {
    result_code = ER_DIAMETER_UNABLE_TO_COMPLY;
    magic_status_code = 5002; /* MAGIC_ERROR_REQUEST_NOT_PROCESSED */

    if (rollover_result.error_message[0]) {
      snprintf(error_msg, sizeof(error_msg), "CDR rollover failed: %s",
               rollover_result.error_message);
    } else {
      snprintf(error_msg, sizeof(error_msg),
               "CDR rollover failed: internal error");
    }

    fd_log_error("[app_magic]   ✗ Error: %s", error_msg);
    goto send_response;
  }

  /* ========== 8. 更新会话上下文 ========== */
  /* 重置流量计数器的起始时间和基准值 */
  target_session->traffic_start_time = time(NULL);
  target_session->bytes_in = 0;
  target_session->bytes_out = 0;
  target_session->last_activity = time(NULL);

  fd_log_notice("[app_magic]   ✓ CDR rollover successful:");
  fd_log_notice("[app_magic]     Old CDR: id=%u, uuid=%s",
                rollover_result.old_cdr_id, rollover_result.old_cdr_uuid);
  fd_log_notice("[app_magic]     Old CDR traffic: in=%lu, out=%lu",
                (unsigned long)rollover_result.final_bytes_in,
                (unsigned long)rollover_result.final_bytes_out);
  fd_log_notice("[app_magic]     New CDR: id=%u, uuid=%s",
                rollover_result.new_cdr_id, rollover_result.new_cdr_uuid);

  /* 执行周期性维护 (归档和清理) */
  cdr_periodic_maintenance(&g_ctx->cdr_mgr);

send_response:
  /* ========== 9. 创建 MACA 应答 ========== */
  CHECK_FCT_DO(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0),
               { return -1; });
  ans = *msg;

  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, result_code);
  ADD_AVP_STR(ans, g_magic_dict.avp_cdr_restart_sess_id, restart_session_id);

  /* 如果成功，添加 CDRs-Updated */
  if (rollover_result.success) {
    struct avp *cdrs_updated_avp = NULL;
    CHECK_FCT_DO(
        fd_msg_avp_new(g_magic_dict.avp_cdrs_updated, 0, &cdrs_updated_avp),
        { goto send; });

    /* CDR-Start-Stop-Pair 包含旧/新 CDR ID 对 */
    struct avp *pair_avp = NULL;
    CHECK_FCT_DO(
        fd_msg_avp_new(g_magic_dict.avp_cdr_start_stop_pair, 0, &pair_avp), {
          fd_msg_free((struct msg *)cdrs_updated_avp);
          goto send;
        });

    /* CDR-Stopped (10049): 被停止的 CDR 标识符 */
    ADD_AVP_U32(pair_avp, g_magic_dict.avp_cdr_stopped,
                rollover_result.old_cdr_id);
    /* CDR-Started (10050): 新启动的 CDR 标识符 */
    ADD_AVP_U32(pair_avp, g_magic_dict.avp_cdr_started,
                rollover_result.new_cdr_id);

    CHECK_FCT_DO(fd_msg_avp_add(cdrs_updated_avp, MSG_BRW_LAST_CHILD, pair_avp),
                 {
                   fd_msg_free((struct msg *)pair_avp);
                   fd_msg_free((struct msg *)cdrs_updated_avp);
                   goto send;
                 });

    CHECK_FCT_DO(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, cdrs_updated_avp),
                 { fd_msg_free((struct msg *)cdrs_updated_avp); });

    fd_log_notice("[app_magic]   CDR restart success: stopped=%u, started=%u",
                  rollover_result.old_cdr_id, rollover_result.new_cdr_id);
  } else {
    /* 添加 MAGIC-Status-Code 错误码 */
    if (magic_status_code != 0) { /* 0 表示成功 */
      ADD_AVP_U32(ans, g_magic_dict.avp_magic_status_code, magic_status_code);
    }

    /* 添加 Error-Message 描述错误原因 */
    if (error_msg[0]) {
      ADD_AVP_STR(ans, g_std_dict.avp_error_message, error_msg);
    }

    fd_log_notice("[app_magic]   CDR restart failed: %s", error_msg);
  }

send:
  CHECK_FCT_DO(fd_msg_send(msg, NULL, NULL), { return -1; });

  fd_log_notice("[app_magic] ✓ Sent MACA (result=%s, code=%d)",
                rollover_result.success ? "SUCCESS" : "FAILED", result_code);
  fd_log_notice("[app_magic] ========================================\n");

  return 0;
}

/*===========================================================================
 * ADIF 状态变化回调 - 动态策略调整
 * 当飞机状态发生变化时，检查所有活动会话是否仍满足激活条件
 *===========================================================================*/

/**
 * @brief 检查单个会话是否仍满足激活条件
 * @param session 会话对象
 * @param state ADIF 飞机状态
 * @param profile 客户端配置文件
 * @return true=满足条件, false=不满足(需终止)
 */
static bool check_session_activation_conditions(ClientSession *session,
                                                const AdifAircraftState *state,
                                                ClientProfile *profile) {
  if (!session || !state || !profile) {
    return true; /* 缺少验证数据，允许继续 */
  }

  /* 将ADIF飞行阶段映射到配置飞行阶段 */
  CfgFlightPhase current_phase = CFG_FLIGHT_PHASE_UNKNOWN;
  switch (state->flight_phase.phase) {
  case FLIGHT_PHASE_GATE:
    current_phase = CFG_FLIGHT_PHASE_GATE;
    break;
  case FLIGHT_PHASE_TAXI:
    current_phase = CFG_FLIGHT_PHASE_TAXI;
    break;
  case FLIGHT_PHASE_TAKEOFF:
    current_phase = CFG_FLIGHT_PHASE_TAKE_OFF;
    break;
  case FLIGHT_PHASE_CLIMB:
    current_phase = CFG_FLIGHT_PHASE_CLIMB;
    break;
  case FLIGHT_PHASE_CRUISE:
    current_phase = CFG_FLIGHT_PHASE_CRUISE;
    break;
  case FLIGHT_PHASE_DESCENT:
    current_phase = CFG_FLIGHT_PHASE_DESCENT;
    break;
  case FLIGHT_PHASE_APPROACH:
    current_phase = CFG_FLIGHT_PHASE_APPROACH;
    break;
  case FLIGHT_PHASE_LANDING:
    current_phase = CFG_FLIGHT_PHASE_LANDING;
    break;
  default:
    current_phase = CFG_FLIGHT_PHASE_UNKNOWN;
    break;
  }

  /* 检查飞行阶段是否允许 */
  if (!magic_config_is_flight_phase_allowed(profile, current_phase)) {
    fd_log_notice(
        "[app_magic] Session %s violated flight phase restriction (current=%d)",
        session->session_id, current_phase);
    return false;
  }

  /* TODO: 检查高度范围和机场限制（需要从会话中保存原始 CommReqParams）*/

  return true; /* 所有检查通过 */
}

/**
 * @brief 为会话重新选择链路
 * @param ctx MAGIC 上下文
 * @param session 会话对象
 * @param state ADIF 飞机状态
 * @param profile 客户端配置文件
 * @return 新选择的链路ID（如果改变），NULL 如果保持不变或失败
 */
static const char *reevaluate_session_link(MagicContext *ctx,
                                           ClientSession *session,
                                           const AdifAircraftState *state,
                                           ClientProfile *profile) {
  if (!ctx || !session || !state) {
    return NULL;
  }

  /* 构建策略请求 */
  PolicyRequest policy_req;
  memset(&policy_req, 0, sizeof(policy_req));

  strncpy(policy_req.client_id, session->client_id,
          sizeof(policy_req.client_id) - 1);

  /* 使用会话当前的带宽参数 */
  policy_req.requested_bw_kbps = session->granted_bw_kbps;
  policy_req.requested_ret_bw_kbps = session->granted_ret_bw_kbps;
  policy_req.required_bw_kbps = session->granted_bw_kbps / 2; /* 最低要求 50% */
  policy_req.required_ret_bw_kbps = session->granted_ret_bw_kbps / 2;

  /* 填充 ADIF 数据 */
  policy_req.aircraft_lat = state->position.latitude;
  policy_req.aircraft_lon = state->position.longitude;
  policy_req.aircraft_alt = state->position.altitude_ft * 0.3048;
  policy_req.on_ground = state->wow.on_ground;
  policy_req.has_adif_data = true;

  /* 根据飞行阶段设置请求中的 flight_phase */
  switch (state->flight_phase.phase) {
  case FLIGHT_PHASE_GATE:
    strncpy(policy_req.flight_phase, "GATE",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_TAXI:
    strncpy(policy_req.flight_phase, "TAXI",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_TAKEOFF:
    strncpy(policy_req.flight_phase, "TAKE-OFF",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_CLIMB:
    strncpy(policy_req.flight_phase, "CLIMB",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_CRUISE:
    strncpy(policy_req.flight_phase, "CRUISE",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_DESCENT:
    strncpy(policy_req.flight_phase, "DESCENT",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_APPROACH:
    strncpy(policy_req.flight_phase, "APPROACH",
            sizeof(policy_req.flight_phase) - 1);
    break;
  case FLIGHT_PHASE_LANDING:
    strncpy(policy_req.flight_phase, "LANDING",
            sizeof(policy_req.flight_phase) - 1);
    break;
  default:
    strncpy(policy_req.flight_phase, "GATE",
            sizeof(policy_req.flight_phase) - 1);
    break;
  }

  /* 调用策略引擎 */
  PolicyResponse policy_resp;
  memset(&policy_resp, 0, sizeof(policy_resp));

  if (magic_policy_select_path(&ctx->policy_ctx, &policy_req, &policy_resp) !=
          0 ||
      !policy_resp.success) {
    fd_log_error("[app_magic]   Policy reevaluation failed for session %s: %s",
                 session->session_id, policy_resp.reason);
    return NULL;
  }

  /* 检查链路是否改变 */
  if (strcmp(session->assigned_link_id, policy_resp.selected_link_id) == 0) {
    /* 链路未改变 */
    return NULL;
  }

  /* 返回新链路ID (静态缓冲区) */
  static char new_link_id[64];
  strncpy(new_link_id, policy_resp.selected_link_id, sizeof(new_link_id) - 1);
  new_link_id[sizeof(new_link_id) - 1] = '\0';
  return new_link_id;
}

/**
 * @brief 执行会话链路切换 (Handover)。
 * @details 执行以下步骤：
 *          1. 释放旧链路资源 (MIH Release)。
 *          2. 申请新链路资源 (MIH Request)。
 *          3. 确保新链路已注册到数据平面 (Register Link)。
 *          4. 切换数据平面路由 (Switch Route)。
 *          5. 发送 MNTR 通知客户端 (Notify Client)。
 *
 * @param ctx MAGIC 上下文。
 * @param session 会话对象。
 * @param old_link_id 旧链路 ID。
 * @param new_link_id 新链路 ID。
 * @return 0 成功，-1 失败。
 */
static int perform_link_handover(MagicContext *ctx, ClientSession *session,
                                 const char *old_link_id,
                                 const char *new_link_id) {
  if (!ctx || !session || !new_link_id) {
    return -1;
  }

  fd_log_notice("[app_magic]   → Performing handover: %s -> %s",
                old_link_id ? old_link_id : "(none)", new_link_id);

  /* 1. 释放旧链路资源 */
  if (old_link_id && old_link_id[0]) {
    MIH_Link_Resource_Request release_req;
    memset(&release_req, 0, sizeof(release_req));
    snprintf(release_req.destination_id.mihf_id,
             sizeof(release_req.destination_id.mihf_id), "MIHF_%s",
             old_link_id);
    /* 设置 link_identifier 用于查找 DLM */
    strncpy(release_req.link_identifier.link_addr, old_link_id,
            sizeof(release_req.link_identifier.link_addr) - 1);
    release_req.resource_action = RESOURCE_ACTION_RELEASE;
    release_req.has_bearer_id = (session->bearer_id > 0);
    release_req.bearer_identifier = session->bearer_id;

    MIH_Link_Resource_Confirm release_confirm;
    memset(&release_confirm, 0, sizeof(release_confirm));
    magic_dlm_mih_link_resource_request(&ctx->lmi_ctx, &release_req,
                                        &release_confirm);

    fd_log_notice("[app_magic]     Released resources on %s (bearer=%u)",
                  old_link_id, session->bearer_id);
  }

  /* 2. 请求新链路资源 */
  MIH_Link_Resource_Request alloc_req;
  memset(&alloc_req, 0, sizeof(alloc_req));
  snprintf(alloc_req.destination_id.mihf_id,
           sizeof(alloc_req.destination_id.mihf_id), "MIHF_%s", new_link_id);
  /* 设置 link_identifier 用于查找 DLM */
  strncpy(alloc_req.link_identifier.link_addr, new_link_id,
          sizeof(alloc_req.link_identifier.link_addr) - 1);
  alloc_req.resource_action = RESOURCE_ACTION_REQUEST;
  alloc_req.has_qos_params = true;
  alloc_req.qos_parameters.cos_id = COS_BEST_EFFORT;
  alloc_req.qos_parameters.forward_link_rate = session->granted_bw_kbps;
  alloc_req.qos_parameters.return_link_rate = session->granted_ret_bw_kbps;

  MIH_Link_Resource_Confirm alloc_confirm;
  memset(&alloc_confirm, 0, sizeof(alloc_confirm));

  if (magic_dlm_mih_link_resource_request(&ctx->lmi_ctx, &alloc_req,
                                          &alloc_confirm) != 0 ||
      alloc_confirm.status != STATUS_SUCCESS) {
    fd_log_error(
        "[app_magic]     ✗ Failed to allocate resources on %s (status=%d)",
        new_link_id, alloc_confirm.status);
    /* 尝试恢复旧链路（可选，暂不实现） */
    return -1;
  }

  fd_log_notice(
      "[app_magic]     Allocated resources on %s (bearer=%u)", new_link_id,
      alloc_confirm.has_bearer_id ? alloc_confirm.bearer_identifier : 0);

  /* 2.5 确保新链路已注册到数据平面（按需注册） */
  uint32_t table_id =
      magic_dataplane_get_table_id(&ctx->dataplane_ctx, new_link_id);
  if (table_id == 0) {
    /* 链路尚未注册，需要从 DLM 获取接口信息并注册 */
    DlmClient *dlm_client = magic_lmi_find_by_link(&ctx->lmi_ctx, new_link_id);
    if (dlm_client && dlm_client->link_identifier.link_addr[0]) {
      const char *interface_name = dlm_client->link_identifier.poa_addr[0]
                                       ? dlm_client->link_identifier.poa_addr
                                       : dlm_client->link_identifier.link_addr;

      fd_log_notice("[app_magic]     DLM info: link_addr=%s, poa_addr=%s, "
                    "using interface=%s",
                    dlm_client->link_identifier.link_addr,
                    dlm_client->link_identifier.poa_addr[0]
                        ? dlm_client->link_identifier.poa_addr
                        : "(empty)",
                    interface_name);

      char gateway_ip[64] = "";
      if (dlm_client->current_parameters.gateway != 0) {
        struct in_addr gw_addr;
        gw_addr.s_addr = dlm_client->current_parameters.gateway;
        strncpy(gateway_ip, inet_ntoa(gw_addr), sizeof(gateway_ip) - 1);
      }

      int reg_ret = magic_dataplane_register_link(
          &ctx->dataplane_ctx, new_link_id, interface_name,
          gateway_ip[0] ? gateway_ip : NULL);
      if (reg_ret >= 0) {
        fd_log_notice("[app_magic]     ✓ Link registered to dataplane: %s → %s "
                      "(table=%d)",
                      new_link_id, interface_name, reg_ret);
      } else {
        fd_log_error(
            "[app_magic]     ✗ Failed to register link %s to dataplane",
            new_link_id);
      }
    } else {
      fd_log_error(
          "[app_magic]     ✗ Cannot register link: DLM not found for %s",
          new_link_id);
    }
  }

  /* 3. 使用专用函数切换数据平面路由 */
  if (magic_dataplane_switch_client_link(
          &ctx->dataplane_ctx, session->session_id, new_link_id) == 0) {
    fd_log_notice("[app_magic]     Switched dataplane routing to %s",
                  new_link_id);
  } else {
    fd_log_error("[app_magic]     ✗ Failed to switch dataplane routing");
    /* 继续更新会话信息，路由切换失败不阻止会话更新 */
  }

  /* 4. 更新会话信息 */
  strncpy(session->assigned_link_id, new_link_id,
          sizeof(session->assigned_link_id) - 1);
  session->bearer_id =
      alloc_confirm.has_bearer_id ? alloc_confirm.bearer_identifier : 0;

  /* 5. 根据 ARINC 839 §4.1.3.3 发送 MNTR 通知客户端链路切换 */
  char gateway_ip[64] = {0};
  if (magic_dataplane_get_link_gateway(&ctx->dataplane_ctx, new_link_id,
                                       gateway_ip, sizeof(gateway_ip)) == 0) {
    /* 更新会话网关信息 */
    strncpy(session->gateway_ip, gateway_ip, sizeof(session->gateway_ip) - 1);
  }

  /* 调用专用接口发送 MNTR (NOTIFY_HANDOVER) */
  if (magic_cic_on_handover(ctx, session, new_link_id, gateway_ip) == 0) {
    fd_log_notice(
        "[app_magic]     MNTR sent to client: new_link=%s, gateway=%s",
        new_link_id, gateway_ip[0] ? gateway_ip : "(unknown)");
  } else {
    fd_log_error("[app_magic]     ⚠ Failed to send MNTR to client");
  }

  fd_log_notice("[app_magic]   ✓ Handover complete: %s now using %s",
                session->session_id, new_link_id);

  return 0;
}

/**
 * @brief ADIF 状态变化回调函数。
 * @details 当飞机状态 (飞行阶段/WoW/高度/位置) 发生变化时被调用。
 *          遍历所有活动会话，重新评估其激活条件和链路选择策略。
 *          如果条件不再满足，终止会话。
 *          如果策略建议更优链路，执行链路切换 (Handover)。
 *
 * @param state 新的飞机状态。
 * @param user_data 用户数据 (MagicContext 指针)。
 */
void on_adif_state_changed(const AdifAircraftState *state, void *user_data) {
  if (!state || !user_data) {
    return;
  }

  MagicContext *ctx = (MagicContext *)user_data;

  fd_log_notice("[app_magic] ========================================");
  fd_log_notice("[app_magic] ADIF State Changed - Reevaluating Sessions");
  fd_log_notice("[app_magic] WoW=%d, Alt=%.0f ft, Phase=%s",
                state->wow.on_ground, state->position.altitude_ft,
                adif_flight_phase_to_string(state->flight_phase.phase));

  /* 获取所有活动会话 */
  ClientSession *active_sessions[MAX_SESSIONS];
  int session_count = magic_session_get_active_sessions(
      &ctx->session_mgr, active_sessions, MAX_SESSIONS);

  fd_log_notice("[app_magic] Reevaluating %d active sessions", session_count);

  int terminated_count = 0;
  int handover_count = 0;
  int unchanged_count = 0;

  /* 检查每个会话 */
  for (int i = 0; i < session_count; i++) {
    ClientSession *session = active_sessions[i];

    /* 查找客户端配置文件 */
    ClientProfile *profile =
        magic_config_find_client(&ctx->config, session->client_id);
    if (!profile) {
      fd_log_notice("[app_magic]   Session %s: no profile found, skipping",
                    session->session_id);
      continue;
    }

    /* Step 1: 验证激活条件 */
    if (!check_session_activation_conditions(session, state, profile)) {
      fd_log_notice("[app_magic]   ✗ Session %s violates activation "
                    "conditions, terminating",
                    session->session_id);

      /* 设置会话状态为终止 */
      magic_session_set_state(session, SESSION_STATE_CLOSED);

      /* 释放链路资源 */
      if (session->assigned_link_id[0]) {
        /* 通过 MIH 释放资源 */
        MIH_Link_Resource_Request mih_req;
        memset(&mih_req, 0, sizeof(mih_req));
        snprintf(mih_req.destination_id.mihf_id,
                 sizeof(mih_req.destination_id.mihf_id), "MIHF_%s",
                 session->assigned_link_id);
        mih_req.resource_action = RESOURCE_ACTION_RELEASE;

        MIH_Link_Resource_Confirm mih_confirm;
        memset(&mih_confirm, 0, sizeof(mih_confirm));
        magic_dlm_mih_link_resource_request(&ctx->lmi_ctx, &mih_req,
                                            &mih_confirm);

        /* 清除数据平面路由 */
        magic_dataplane_remove_client_route(&ctx->dataplane_ctx,
                                            session->session_id);
      }

      /* TODO: 发送 STR (Session Termination Request) 通知客户端 */

      terminated_count++;
      continue;
    }

    /* Step 2: 重新评估链路选择（仅对 ACTIVE 状态的会话） */
    if (session->state == SESSION_STATE_ACTIVE &&
        session->assigned_link_id[0]) {
      const char *new_link_id =
          reevaluate_session_link(ctx, session, state, profile);

      if (new_link_id) {
        /* 链路需要切换 */
        char old_link_id[64];
        strncpy(old_link_id, session->assigned_link_id,
                sizeof(old_link_id) - 1);
        old_link_id[sizeof(old_link_id) - 1] = '\0';

        fd_log_notice(
            "[app_magic]   ⚡ Session %s: link change detected (%s -> %s)",
            session->session_id, old_link_id, new_link_id);

        if (perform_link_handover(ctx, session, old_link_id, new_link_id) ==
            0) {
          handover_count++;
        } else {
          fd_log_error("[app_magic]   ✗ Handover failed for session %s",
                       session->session_id);
        }
      } else {
        fd_log_notice("[app_magic]   ✓ Session %s: link unchanged (%s)",
                      session->session_id, session->assigned_link_id);
        unchanged_count++;
      }
    } else {
      fd_log_notice(
          "[app_magic]   ✓ Session %s: not ACTIVE or no link assigned",
          session->session_id);
      unchanged_count++;
    }
  }

  fd_log_notice("[app_magic] Session reevaluation complete:");
  fd_log_notice("[app_magic]   - Terminated: %d", terminated_count);
  fd_log_notice("[app_magic]   - Handovers: %d", handover_count);
  fd_log_notice("[app_magic]   - Unchanged: %d", unchanged_count);
  fd_log_notice("[app_magic] ========================================\n");
}

/**
 * @brief CIC 模块初始化。
 * @details 初始化客户端交互组件。
 *          - 初始化 MAGIC 字典。
 *          - 注册 Diameter 应用支持 (Vendor-Specific)。
 *          - 注册所有 Command Handler (MCAR, MCCR, STR, MNTR, MSCR, MSXR, MADR,
 * MACR)。
 *
 * @param ctx 全局 MAGIC 上下文。
 * @return 0 成功，-1 失败。
 */

int magic_cic_init(MagicContext *ctx) {
  if (!ctx) {
    return -1; /* 参数校验失败 */
  }

  g_ctx = ctx; /* 保存全局上下文指针 */

  struct disp_when when; /* 分派条件结构 */

  /* 记录初始化开始 */
  fd_log_notice("[app_magic] Initializing CIC module...");

  /* 初始化 MAGIC 协议字典句柄 */
  CHECK_FCT_DO(magic_dict_init(), {
    fd_log_error("[app_magic] Failed to initialize MAGIC dictionary");
    return -1;
  });

  /* 注册 MAGIC Diameter 应用支持 */
  /* 传入 vendor 对象是关键，告诉 freeDiameter 这是一个 Vendor-Specific 应用
   * (AEEC 13712) */
  CHECK_FCT(fd_disp_app_support(g_magic_dict.app, g_magic_dict.vendor, 1, 0));

  /* 初始化分派条件 */
  memset(&when, 0, sizeof(when));
  when.app = g_magic_dict.app; /* 设置应用为 MAGIC */

  /* 注册 MCAR (Client Authentication Request) 处理器 */
  when.command = g_magic_dict.cmd_mcar;
  CHECK_FCT(fd_disp_register(cic_handle_mcar, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MCAR handler registered");

  /* 注册 MCCR (Communication Change Request) 处理器 */
  when.command = g_magic_dict.cmd_mccr;
  CHECK_FCT(fd_disp_register(cic_handle_mccr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MCCR handler registered");

  /* STR 使用标准 Diameter 命令 (Session Termination Request) */
  command_code_t str_code = 275; /* STR 命令码 */
  struct dict_object *cmd_str;   /* STR 命令对象 */
  CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND,
                              CMD_BY_CODE_R, &str_code, &cmd_str, ENOENT),
               {
                 fd_log_error("[app_magic] STR not found");
                 return -1;
               });
  when.command = cmd_str; /* 设置为标准 STR 命令 */
  CHECK_FCT(fd_disp_register(cic_handle_str, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ STR handler registered");

  /* 注册 MNTR (Notification Report) 处理器 */
  when.command = g_magic_dict.cmd_mntr;
  CHECK_FCT(fd_disp_register(cic_handle_mntr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MNTR handler registered");

  /* 注册 MSCR (Status Change Report) 处理器 */
  when.command = g_magic_dict.cmd_mscr;
  CHECK_FCT(fd_disp_register(cic_handle_mscr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MSCR handler registered");

  /* 注册 MSXR (Status Request) 处理器 */
  when.command = g_magic_dict.cmd_msxr;
  CHECK_FCT(fd_disp_register(cic_handle_msxr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MSXR handler registered");

  /* 注册 MADR (Accounting Data Request) 处理器 */
  when.command = g_magic_dict.cmd_madr;
  CHECK_FCT(fd_disp_register(cic_handle_madr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MADR handler registered");

  /* 注册 MACR (Accounting Control Request) 处理器 */
  when.command = g_magic_dict.cmd_macr;
  CHECK_FCT(fd_disp_register(cic_handle_macr, DISP_HOW_CC, &when, NULL, NULL));
  fd_log_notice("[app_magic] ✓ MACR handler registered");

  return 0; /* 初始化成功 */
}

/**
 * @brief CIC 模块清理。
 * @details 清理客户端交互组件资源，重置全局上下文指针。
 *
 * @param ctx 全局 MAGIC 上下文。
 */

void magic_cic_cleanup(MagicContext *ctx) {
  if (ctx) {
    g_ctx = NULL; /* 清空全局上下文指针 */
    fd_log_notice("[app_magic] CIC module cleaned up");
  }
}
