/**
 * @file mih_protocol.h
 * @brief MIH (Media Independent Handover) 协议定义文件
 * @description ARINC 839-2014 附件2 - IEEE 802.21 修改版
 *
 * 本文件定义内容:
 * - IEEE 802.21 标准原语 (MAGIC系统使用的子集)
 * - MAGIC 专用 Link_Resource 原语 (替代标准的 Link_Action)
 * - ARINC 839 附件2 定义的数据类型
 *
 * 文件结构:
 * 1. MIH 原语类型码定义
 * 2. LINK_PARAM_TYPE 链路参数类型枚举
 * 3. MIH 数据类型定义
 * 4. MIH SAP 原语结构体
 * 5. IEEE 802.21 标准原语实现
 * 6. 辅助函数
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MIH_PROTOCOL_H /* 头文件保护宏开始 */
#define MIH_PROTOCOL_H /* 防止重复包含 */

#include <stdbool.h> /* 布尔类型: bool, true, false */
#include <stdint.h>  /* 标准整数类型: uint8_t, uint16_t, uint32_t 等 */

/*===========================================================================
 * MIH 原语类型码定义 (IEEE 802.21 标准 + ARINC 839 扩展)
 *
 * 编码规则:
 * - 0x01xx: 请求/确认原语 (Request/Confirm)
 * - 0x02xx: 指示原语 (Indication) - 异步事件通知
 * - 0x03xx: ARINC 839 扩展原语
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * IEEE 802.21 标准原语码 - 请求/确认类 (0x01xx)
 * 这些原语用于 CM Core 与 DLM 之间的同步请求-响应通信
 *---------------------------------------------------------------------------*/
#define MIH_LINK_CAPABILITY_DISCOVER_REQ                                       \
  0x0101 /* 链路能力发现请求 - 查询链路支持的功能 */
#define MIH_LINK_CAPABILITY_DISCOVER_CNF                                       \
  0x0102 /* 链路能力发现确认 - 返回链路能力信息 */
#define MIH_LINK_EVENT_SUBSCRIBE_REQ                                           \
  0x0103 /* 事件订阅请求 - 订阅链路事件通知 */
#define MIH_LINK_EVENT_SUBSCRIBE_CNF 0x0104 /* 事件订阅确认 - 返回订阅结果 */
#define MIH_LINK_EVENT_UNSUBSCRIBE_REQ                                         \
  0x0105 /* 取消事件订阅请求 - 取消已订阅的事件 */
#define MIH_LINK_EVENT_UNSUBSCRIBE_CNF                                         \
  0x0106 /* 取消事件订阅确认 - 返回取消结果 */
#define MIH_LINK_GET_PARAMETERS_REQ                                            \
  0x0107 /* 获取链路参数请求 - 主动查询链路状态 */
#define MIH_LINK_GET_PARAMETERS_CNF                                            \
  0x0108 /* 获取链路参数确认 - 返回当前参数值 */
#define MIH_LINK_CONFIGURE_THRESHOLDS_REQ                                      \
  0x0109 /* 配置阈值请求 - 设置事件触发阈值 */
#define MIH_LINK_CONFIGURE_THRESHOLDS_CNF                                      \
  0x010A /* 配置阈值确认 - 返回配置结果 */

/*---------------------------------------------------------------------------
 * IEEE 802.21 标准原语码 - 指示类 (0x02xx)
 * 这些原语用于 DLM 向 CM Core 发送异步事件通知
 *---------------------------------------------------------------------------*/
#define MIH_LINK_DETECTED_IND 0x0201   /* 链路检测指示 - 发现新的可用链路 */
#define MIH_LINK_UP_IND 0x0202         /* 链路上线指示 - 链路变为可用状态 */
#define MIH_LINK_DOWN_IND 0x0203       /* 链路下线指示 - 链路变为不可用 */
#define MIH_LINK_GOING_DOWN_IND 0x0204 /* 链路即将断开指示 - 提前预警 */
#define MIH_LINK_PARAMETERS_REPORT_IND                                         \
  0x0205 /* 链路参数报告指示 - 参数变化通知 */

/*---------------------------------------------------------------------------
 * ARINC 839 自定义原语码 (0x03xx)
 * Link_Resource 原语替代 IEEE 802.21 的 Link_Action
 * 用于资源分配和释放
 *---------------------------------------------------------------------------*/
#define MIH_LINK_RESOURCE_REQ 0x0301 /* 链路资源请求 - 请求或释放链路资源 */
#define MIH_LINK_RESOURCE_CNF 0x0302 /* 链路资源确认 - 返回资源操作结果 */

/*===========================================================================
 * LINK_PARAM_TYPE - 链路参数类型枚举 (ARINC 839 扩展 IEEE 802.21)
 *
 * 本枚举定义了 MAGIC 系统支持的所有链路技术类型
 * 包括:
 * - IEEE 802.x 系列标准
 * - 3GPP 蜂窝网络 (3G/4G/5G)
 * - 卫星通信 (L/Ku/Ka 波段)
 * - 航空专用数据链路 (VDL/HFDL/LDACS)
 *
 * 编码范围:
 * - 0x00-0x0F: IEEE 802.x 标准类型
 * - 0x10-0x1F: 3GPP 蜂窝网络类型
 * - 0x20-0x2F: 卫星通信类型
 * - 0x30-0x3F: 航空专用类型
 * - 0x80-0xFF: 供应商扩展范围
 *===========================================================================*/

typedef enum {
  /*-----------------------------------------------------------------------
   * IEEE 802.x 标准链路类型 (0x00-0x0F)
   * 这些是 IEEE 802.21 原生支持的链路类型
   *-----------------------------------------------------------------------*/
  LINK_PARAM_TYPE_GENERIC = 0x00, /* 通用链路 - 未分类的链路类型 */
  LINK_PARAM_TYPE_ETH = 0x01,     /* 以太网 (IEEE 802.3) - 有线网络 */
  LINK_PARAM_TYPE_802_11 = 0x02,  /* Wi-Fi (IEEE 802.11) - 无线局域网 */
  LINK_PARAM_TYPE_802_16 = 0x03,  /* WiMAX (IEEE 802.16) - 城域网 */
  LINK_PARAM_TYPE_802_20 = 0x04,  /* IEEE 802.20 - 移动宽带无线接入 */
  LINK_PARAM_TYPE_802_22 = 0x05,  /* IEEE 802.22 - 区域网 (TV 白频) */

  /*-----------------------------------------------------------------------
   * 3GPP 蜂窝网络类型 (0x10-0x1F) - ARINC 839 扩展
   * 用于航空器与地面网络的蜂窝通信
   *-----------------------------------------------------------------------*/
  LINK_PARAM_TYPE_UMTS = 0x10,    /* 3G UMTS - 第三代移动通信 */
  LINK_PARAM_TYPE_C2K = 0x11,     /* CDMA2000 - 码分多址2000标准 */
  LINK_PARAM_TYPE_FDD_LTE = 0x12, /* 4G LTE FDD - 频分双工长期演进 */
  LINK_PARAM_TYPE_TDD_LTE = 0x13, /* 4G LTE TDD - 时分双工长期演进 */
  LINK_PARAM_TYPE_HRPD = 0x14,    /* CDMA2000 HRPD (EV-DO) - 高速数据 */
  LINK_PARAM_TYPE_5G_NR = 0x15,   /* 5G NR - 第五代新空口 */

  /*-----------------------------------------------------------------------
   * 卫星通信类型 (0x20-0x2F) - ARINC 839 航空扩展
   * 航空通信中最重要的远程链路类型
   *-----------------------------------------------------------------------*/
  LINK_PARAM_TYPE_INMARSAT = 0x20,  /* Inmarsat - 国际海事卫星组织 */
  LINK_PARAM_TYPE_SATCOM_L = 0x21,  /* L-Band 卫星 - 低频段卫星通信 */
  LINK_PARAM_TYPE_SATCOM_KU = 0x22, /* Ku-Band 卫星 - 中频段卫星 (12-18 GHz) */
  LINK_PARAM_TYPE_SATCOM_KA =
      0x23,                       /* Ka-Band 卫星 - 高频段卫星 (26.5-40 GHz) */
  LINK_PARAM_TYPE_IRIDIUM = 0x24, /* Iridium 卫星 - 铱星系统 (低轨道) */
  LINK_PARAM_TYPE_VSAT = 0x25,    /* VSAT - 甚小口径终端卫星系统 */

  /*-----------------------------------------------------------------------
   * 航空专用链路类型 (0x30-0x3F)
   * 航空特定的数据链路标准
   *-----------------------------------------------------------------------*/
  LINK_PARAM_TYPE_VDL2 = 0x30,     /* VDL Mode 2 - 甚高频数据链路模式2 */
  LINK_PARAM_TYPE_VDL3 = 0x31,     /* VDL Mode 3 - 甚高频数据链路模式3 */
  LINK_PARAM_TYPE_VDL4 = 0x32,     /* VDL Mode 4 - 甚高频数据链路模式4 */
  LINK_PARAM_TYPE_HFDL = 0x33,     /* HFDL - 高频数据链路 (远程通信) */
  LINK_PARAM_TYPE_AeroMACS = 0x34, /* AeroMACS - 航空移动机场通信系统 */
  LINK_PARAM_TYPE_LDACS = 0x35,    /* L-DACS - L波段数字航空通信系统 */
  LINK_PARAM_TYPE_ATG = 0x36,      /* ATG - 空对地直接通信 */

  /*-----------------------------------------------------------------------
   * 供应商扩展范围 (0x80-0xFF)
   * 保留给设备厂商定义的私有链路类型
   *-----------------------------------------------------------------------*/
  LINK_PARAM_TYPE_VENDOR_START = 0x80, /* 供应商扩展起始值 */
  LINK_PARAM_TYPE_VENDOR_END = 0xFF    /* 供应商扩展结束值 */
} LINK_PARAM_TYPE;

/*===========================================================================
 * MIH 数据类型定义 (ARINC 839 Section 2.3 & 2.4)
 *
 * 本节定义 MIH 协议使用的基本数据类型和结构体
 * 包括标识符、Bearer、QoS参数、状态码等
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * MIHF_ID - MIHF 标识符结构体
 * 用于唯一标识一个 MIH Function 实体
 * 在 MAGIC 系统中用于标识 CM Core 或 DLM
 *---------------------------------------------------------------------------*/
typedef struct {
  char mihf_id[64]; /* MIHF 标识符字符串, 如 "CM_CORE_1" 或 "DLM_SATCOM" */
} MIHF_ID;

/*---------------------------------------------------------------------------
 * LINK_TUPLE_ID - 链路元组标识符
 * 唯一标识一条通信链路
 * 包含链路类型、本地地址和接入点地址
 *---------------------------------------------------------------------------*/
typedef struct {
  uint8_t link_type;  /* 链路类型 (LINK_PARAM_TYPE 枚举值) */
  char link_addr[32]; /* 链路地址 (MAC地址、IP地址等) */
  char poa_addr[32];  /* 接入点地址 (Point of Attachment, 可选) */
} LINK_TUPLE_ID;

/*---------------------------------------------------------------------------
 * BEARER_ID - 承载标识符
 * UNSIGNED INT 1 类型, 支持 0-255 共 256 种不同的承载
 * 承载是在链路上建立的逻辑通道, 每个承载可以有不同的 QoS 参数
 *---------------------------------------------------------------------------*/
typedef uint8_t BEARER_ID;

/*---------------------------------------------------------------------------
 * LINK_DATA_RATE_FL/RL - 链路数据速率
 * 单位: kbps (千比特每秒)
 * FL = Forward Link (前向链路: 地面→飞机)
 * RL = Return Link (反向链路: 飞机→地面)
 *---------------------------------------------------------------------------*/
typedef uint32_t LINK_DATA_RATE_FL; /* 前向链路速率 (地面到飞机) */
typedef uint32_t LINK_DATA_RATE_RL; /* 反向链路速率 (飞机到地面) */

/*---------------------------------------------------------------------------
 * COS_ID - 服务等级标识枚举
 * 定义 8 种 QoS 服务等级, 对应 DSCP 标记
 * 用于流量分类和优先级调度
 *---------------------------------------------------------------------------*/
/**
 * @brief 服务等级 (CoS) 标识枚举。
 * @details 定义了 8 种 QoS 服务等级，对应 Diameter 核心 AVP `QoS-Level`
 * 以及底层的 DSCP 标记。 用于策略引擎进行流量分类和优先级调度。
 */
typedef enum {
  COS_BEST_EFFORT = 0, ///< 尽力而为 - 默认等级，无 QoS 保证。
  COS_BACKGROUND = 1,  ///< 后台流量 - 最低优先级（如批量下载、系统备份）。
  COS_VIDEO = 2,       ///< 视频流量 - 需要稳定带宽和持续流控。
  COS_VOICE = 3,       ///< 语音流量 - 实时通信，需要极低延迟和低抖动。
  COS_INTERACTIVE =
      4, ///< 交互式流量 - 关键业务操作，需要快速响应（如遥测、控制台）。
  COS_SIGNALING = 5,           ///< 信令流量 - 协议交互控制消息。
  COS_NETWORK_CONTROL = 6,     ///< 网络控制 - 路由发现及核心控制。
  COS_EXPEDITED_FORWARDING = 7 ///< 加速转发 - 特级保障业务。
} COS_ID;

/*---------------------------------------------------------------------------
 * QOS_PARAM - QoS 参数结构体
 * 定义链路的服务质量要求
 * 包括带宽、延迟、抖动、丢包率等指标
 *---------------------------------------------------------------------------*/
/**
 * @brief QoS 参数结构体。
 * @details 详细描述了链路的各维度服务质量指标，用于 CM Core 与 DLM
 * 之间的资源请求磋商。
 */
typedef struct {
  COS_ID cos_id; ///< 服务等级 ID。
  LINK_DATA_RATE_FL
      forward_link_rate; ///< 前向链路 (地面到飞机) 目标速率 (kbps)。
  LINK_DATA_RATE_RL
      return_link_rate; ///< 反向链路 (飞机到地面) 目标速率 (kbps)。

  /* 可选延迟参数 (单位: 毫秒) */
  uint32_t min_pk_tx_delay; ///< 最小数据包传输延迟。
  uint32_t avg_pk_tx_delay; ///< 平均数据包传输延迟。
  uint32_t max_pk_tx_delay; ///< 最大数据包传输延迟。
  uint32_t pk_delay_jitter; ///< 数据包延迟抖动指标。
  float pk_loss_rate;       ///< 丢包率目标值 (0.00 ~ 1.00)。
} QOS_PARAM;

/*---------------------------------------------------------------------------
 * RESOURCE_ACTION_TYPE - 资源操作类型枚举
 * 定义对链路资源的操作类型
 *---------------------------------------------------------------------------*/
typedef enum {
  RESOURCE_ACTION_REQUEST = 0, /* 请求资源 - 分配新的 Bearer */
  RESOURCE_ACTION_RELEASE = 1  /* 释放资源 - 释放已有的 Bearer */
} RESOURCE_ACTION_TYPE;

/*---------------------------------------------------------------------------
 * STATUS - 操作状态码枚举
 * 定义 MIH 操作的返回状态
 *---------------------------------------------------------------------------*/
typedef enum {
  STATUS_SUCCESS = 0,                /* 成功 */
  STATUS_FAILURE = 1,                /* 一般失败 */
  STATUS_INSUFFICIENT_RESOURCES = 2, /* 资源不足 */
  STATUS_INVALID_BEARER = 3,         /* 无效的 Bearer ID */
  STATUS_LINK_NOT_AVAILABLE = 4,     /* 链路不可用 */
  STATUS_QOS_NOT_SUPPORTED = 5       /* 不支持请求的 QoS */
} STATUS;

/*---------------------------------------------------------------------------
 * HARDWARE_HEALTH - 硬件健康状态
 * 用于报告 DLM 硬件的健康信息
 * 最大长度 253 字节, 非空字符结尾
 *---------------------------------------------------------------------------*/
typedef struct {
  char health_info[254]; /* 健康状态信息 (非 NULL 结尾字符串) */
  uint8_t length;        /* 实际内容长度 */
} HARDWARE_HEALTH;

/*---------------------------------------------------------------------------
 * DEV_STATES_REQ - 设备状态请求位图
 * 用于指定要查询的设备状态类型
 *---------------------------------------------------------------------------*/
typedef enum {
  DEV_STATE_DEVICE_INFO = (1 << 0),    /* 位0: 设备基本信息 */
  DEV_STATE_BATT_LEVEL = (1 << 1),     /* 位1: 电池电量 */
  DEV_STATE_HARDWARE_HEALTH = (1 << 2) /* 位2: 硬件健康状态 */
                                       /* 位 3-15: 保留供将来使用 */
} DEV_STATES_REQ;

/*===========================================================================
 * MIH SAP 原语定义 (Section 2.1)
 *
 * SAP = Service Access Point (服务访问点)
 * MIH SAP 原语用于 MIH User (如 CM Core) 与 MIHF 之间的通信
 *
 * ARINC 839 使用 Link_Resource 原语替代 IEEE 802.21 的 Link_Action
 * 这是为了更好地适应航空通信的资源管理需求
 *===========================================================================*/

/**
 * @brief MIH_Link_Resource_Request - MIH 链路资源请求结构体
 * @description 用于 MIH User 向 MIHF 请求或释放链路资源
 *
 * 使用场景:
 * - REQUEST: 在链路上建立新的 Bearer, 分配 QoS 资源
 * - RELEASE: 释放已有的 Bearer, 归还资源
 */
typedef struct {
  MIHF_ID destination_id;               /* 目标 MIHF 标识符 (本地或远程) */
  LINK_TUPLE_ID link_identifier;        /* 目标链路标识符 */
  RESOURCE_ACTION_TYPE resource_action; /* 操作类型: REQUEST 或 RELEASE */

  /* 可选字段 - Bearer 标识 */
  bool has_bearer_id;          /* 是否包含 Bearer ID */
  BEARER_ID bearer_identifier; /* Bearer 标识符 (用于已存在的 Bearer) */

  /* 可选字段 - QoS 参数 (REQUEST 操作必需) */
  bool has_qos_params;      /* 是否包含 QoS 参数 */
  QOS_PARAM qos_parameters; /* QoS 参数 (REQUEST 时必填) */
} MIH_Link_Resource_Request;

/**
 * @brief MIH_Link_Resource_Confirm - MIH 链路资源确认结构体
 * @description MIH_Link_Resource.Request 的响应消息
 *
 * 返回资源操作的结果和分配的 Bearer ID
 */
typedef struct {
  MIHF_ID source_identifier;     /* 源 MIHF 标识符 */
  LINK_TUPLE_ID link_identifier; /* 链路标识符 */
  STATUS status;                 /* 操作状态码 */

  /* 可选字段 - 分配的 Bearer ID (成功时) */
  bool has_bearer_id;          /* 是否包含 Bearer ID */
  BEARER_ID bearer_identifier; /* 分配的 Bearer ID */
} MIH_Link_Resource_Confirm;

/*===========================================================================
 * MIH LINK SAP 原语定义 (Section 2.2)
 *
 * LINK SAP 原语用于 MIHF 与链路层 (DLM) 之间的通信
 * 这是内部接口, 不包含 MIHF ID 等上层信息
 *===========================================================================*/

/**
 * @brief LINK_Resource_Request - 链路层资源请求结构体
 * @description 用于 MIHF 向链路层请求或释放资源
 *
 * 与 MIH_Link_Resource_Request 类似, 但不包含 MIHF ID 和链路标识
 * 因为链路已经确定 (通过 IPC 连接)
 */
typedef struct {
  RESOURCE_ACTION_TYPE resource_action; /* 操作类型: REQUEST 或 RELEASE */

  /* 可选字段 - Bearer 标识 */
  bool has_bearer_id;          /* 是否包含 Bearer ID */
  BEARER_ID bearer_identifier; /* Bearer 标识符 */

  /* 可选字段 - QoS 参数 */
  bool has_qos_params;      /* 是否包含 QoS 参数 */
  QOS_PARAM qos_parameters; /* QoS 参数 */
} LINK_Resource_Request;

/**
 * @brief LINK_Resource_Confirm - 链路层资源确认结构体
 * @description LINK_Resource.Request 的响应消息
 */
typedef struct {
  STATUS status; /* 操作状态码 */

  /* 可选字段 - 分配的 Bearer ID */
  bool has_bearer_id;          /* 是否包含 Bearer ID */
  BEARER_ID bearer_identifier; /* 分配的 Bearer ID */
} LINK_Resource_Confirm;

/*===========================================================================
 * 辅助函数 - 枚举值到字符串转换
 *
 * 这些内联函数用于将枚举值转换为可读的字符串
 * 主要用于日志输出和调试
 *===========================================================================*/

/**
 * @brief 将资源操作类型转换为字符串
 * @param action 资源操作类型枚举值
 * @return 对应的字符串描述
 */
static inline const char *
resource_action_to_string(RESOURCE_ACTION_TYPE action) {
  switch (action) {
  case RESOURCE_ACTION_REQUEST:
    return "REQUEST"; /* 请求资源 */
  case RESOURCE_ACTION_RELEASE:
    return "RELEASE"; /* 释放资源 */
  default:
    return "UNKNOWN"; /* 未知类型 */
  }
}

/**
 * @brief 将状态码转换为字符串
 * @param status 状态码枚举值
 * @return 对应的字符串描述
 */
static inline const char *status_to_string(STATUS status) {
  switch (status) {
  case STATUS_SUCCESS:
    return "SUCCESS"; /* 成功 */
  case STATUS_FAILURE:
    return "FAILURE"; /* 失败 */
  case STATUS_INSUFFICIENT_RESOURCES:
    return "INSUFFICIENT_RESOURCES"; /* 资源不足 */
  case STATUS_INVALID_BEARER:
    return "INVALID_BEARER"; /* 无效 Bearer */
  case STATUS_LINK_NOT_AVAILABLE:
    return "LINK_NOT_AVAILABLE"; /* 链路不可用 */
  case STATUS_QOS_NOT_SUPPORTED:
    return "QOS_NOT_SUPPORTED"; /* 不支持的 QoS */
  default:
    return "UNKNOWN"; /* 未知状态 */
  }
}

/**
 * @brief 将服务等级 ID 转换为字符串
 * @param cos 服务等级枚举值
 * @return 对应的字符串描述
 */
static inline const char *cos_id_to_string(COS_ID cos) {
  switch (cos) {
  case COS_BEST_EFFORT:
    return "BEST_EFFORT"; /* 尽力而为 */
  case COS_BACKGROUND:
    return "BACKGROUND"; /* 后台流量 */
  case COS_VIDEO:
    return "VIDEO"; /* 视频流量 */
  case COS_VOICE:
    return "VOICE"; /* 语音流量 */
  case COS_INTERACTIVE:
    return "INTERACTIVE"; /* 交互式 */
  case COS_SIGNALING:
    return "SIGNALING"; /* 信令 */
  case COS_NETWORK_CONTROL:
    return "NETWORK_CONTROL"; /* 网络控制 */
  case COS_EXPEDITED_FORWARDING:
    return "EXPEDITED_FORWARDING"; /* 加速转发 */
  default:
    return "UNKNOWN"; /* 未知等级 */
  }
}

/**
 * @brief 验证 QoS 参数的有效性
 * @param qos 指向 QoS 参数结构体的指针
 * @return true = 参数有效, false = 参数无效
 *
 * 验证规则:
 * 1. 参数指针不能为 NULL
 * 2. 前向和反向链路速率至少有一个非零
 * 3. 丢包率必须在 0.0-1.0 范围内
 */
static inline bool validate_qos_params(const QOS_PARAM *qos) {
  /* 检查指针有效性 */
  if (!qos)
    return false;

  /* 验证数据速率: 至少一个方向有带宽 */
  if (qos->forward_link_rate == 0 && qos->return_link_rate == 0) {
    return false; /* 前向和反向速率不能同时为零 */
  }

  /* 验证丢包率范围: 0.0 到 1.0 */
  if (qos->pk_loss_rate < 0.0 || qos->pk_loss_rate > 1.0) {
    return false; /* 丢包率超出有效范围 */
  }

  return true; /* 所有验证通过 */
}

/**
 * @brief 将链路参数类型转换为字符串
 * @param type 链路参数类型枚举值
 * @return 对应的字符串描述
 *
 * 支持所有 ARINC 839 定义的链路类型
 */
static inline const char *link_param_type_to_string(LINK_PARAM_TYPE type) {
  switch (type) {
  /* IEEE 802.x 标准类型 */
  case LINK_PARAM_TYPE_GENERIC:
    return "GENERIC"; /* 通用链路 */
  case LINK_PARAM_TYPE_ETH:
    return "ETHERNET"; /* 以太网 */
  case LINK_PARAM_TYPE_802_11:
    return "802.11/Wi-Fi"; /* Wi-Fi */
  case LINK_PARAM_TYPE_802_16:
    return "802.16/WiMAX"; /* WiMAX */

  /* 3GPP 蜂窝网络类型 */
  case LINK_PARAM_TYPE_UMTS:
    return "3G/UMTS"; /* 3G */
  case LINK_PARAM_TYPE_C2K:
    return "CDMA2000"; /* CDMA2000 */
  case LINK_PARAM_TYPE_FDD_LTE:
    return "4G/LTE-FDD"; /* LTE FDD */
  case LINK_PARAM_TYPE_TDD_LTE:
    return "4G/LTE-TDD"; /* LTE TDD */
  case LINK_PARAM_TYPE_HRPD:
    return "CDMA2000/HRPD"; /* HRPD */
  case LINK_PARAM_TYPE_5G_NR:
    return "5G/NR"; /* 5G 新空口 */

  /* 卫星通信类型 */
  case LINK_PARAM_TYPE_INMARSAT:
    return "INMARSAT"; /* 海事卫星 */
  case LINK_PARAM_TYPE_SATCOM_L:
    return "SATCOM/L-Band"; /* L波段卫星 */
  case LINK_PARAM_TYPE_SATCOM_KU:
    return "SATCOM/Ku-Band"; /* Ku波段卫星 */
  case LINK_PARAM_TYPE_SATCOM_KA:
    return "SATCOM/Ka-Band"; /* Ka波段卫星 */
  case LINK_PARAM_TYPE_IRIDIUM:
    return "IRIDIUM"; /* 铱星 */
  case LINK_PARAM_TYPE_VSAT:
    return "VSAT"; /* VSAT */

  /* 航空专用类型 */
  case LINK_PARAM_TYPE_VDL2:
    return "VDL-Mode2"; /* VDL 模式2 */
  case LINK_PARAM_TYPE_VDL3:
    return "VDL-Mode3"; /* VDL 模式3 */
  case LINK_PARAM_TYPE_VDL4:
    return "VDL-Mode4"; /* VDL 模式4 */
  case LINK_PARAM_TYPE_HFDL:
    return "HFDL"; /* 高频数据链路 */
  case LINK_PARAM_TYPE_AeroMACS:
    return "AeroMACS"; /* 机场 WiMAX */
  case LINK_PARAM_TYPE_LDACS:
    return "L-DACS"; /* L波段数字航空通信 */
  case LINK_PARAM_TYPE_ATG:
    return "Air-to-Ground"; /* 空对地 */

  default:
    /* 检查是否为供应商扩展类型 */
    if (type >= LINK_PARAM_TYPE_VENDOR_START &&
        type <= LINK_PARAM_TYPE_VENDOR_END) {
      return "VENDOR_SPECIFIC"; /* 供应商特定类型 */
    }
    return "UNKNOWN"; /* 未知类型 */
  }
}

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Capability_Discover
 *
 * 功能: 发现链路的能力信息
 * 用途: CM Core 查询 DLM 管理的链路支持哪些功能
 *
 * 原语流程:
 * 1. CM Core 发送 Link_Capability_Discover.request
 * 2. DLM 返回 Link_Capability_Discover.confirm (包含能力信息)
 *===========================================================================*/

/**
 * @brief LINK_CAPABILITY - 链路能力结构体
 * 描述链路支持的各种能力和特性
 */
typedef struct {
  uint32_t supported_events;   /* 支持的事件位图 (LINK_EVENT_TYPE) */
  uint32_t supported_commands; /* 支持的命令位图 */
  uint32_t max_bandwidth_kbps; /* 链路最大带宽 (kbps) */
  uint32_t typical_latency_ms; /* 典型延迟 (毫秒) */
  uint8_t link_type;           /* 链路类型 (LINK_PARAM_TYPE 值) */
  uint8_t security_level;      /* 安全等级 (1-5, 5 最高) */
  uint16_t mtu;                /* 最大传输单元 (字节) */
  bool is_asymmetric;          /* 是否为非对称链路 (上下行速率不同) */
  uint8_t reserved[3];         /* 保留字段, 用于对齐和未来扩展 */
} LINK_CAPABILITY;

/**
 * @brief LINK_Capability_Discover_Request - 链路能力发现请求
 * 由 CM Core 发送到 DLM
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 要查询能力的目标链路 */
} LINK_Capability_Discover_Request;

/**
 * @brief LINK_Capability_Discover_Confirm - 链路能力发现确认
 * 由 DLM 返回给 CM Core
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 链路标识符 (回显) */
  STATUS status;                 /* 操作状态: SUCCESS 或 失败原因 */
  bool has_capability;           /* 是否包含能力信息 */
  LINK_CAPABILITY capability;    /* 链路能力详情 (status=SUCCESS 时有效) */
} LINK_Capability_Discover_Confirm;

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Event_Subscribe / Unsubscribe
 *
 * 功能: 订阅或取消订阅链路事件通知
 * 用途: CM Core 注册感兴趣的链路事件, DLM 在事件发生时通知 CM Core
 *
 * 支持的事件类型:
 * - Link_Detected: 发现新链路
 * - Link_Up: 链路上线
 * - Link_Down: 链路下线
 * - Link_Going_Down: 链路即将断开 (提前预警)
 * - Link_Parameters_Report: 链路参数变化报告
 * - Handover 相关事件
 *===========================================================================*/

/**
 * @brief LINK_EVENT_TYPE - 链路事件类型位图枚举
 * 使用位图方式, 可以同时订阅多个事件
 * 例如: LINK_EVENT_UP | LINK_EVENT_DOWN 同时订阅上线和下线事件
 */
typedef enum {
  LINK_EVENT_DETECTED = (1 << 0),   /* 位0: Link_Detected - 发现新链路 */
  LINK_EVENT_UP = (1 << 1),         /* 位1: Link_Up - 链路变为可用 */
  LINK_EVENT_DOWN = (1 << 2),       /* 位2: Link_Down - 链路变为不可用 */
  LINK_EVENT_GOING_DOWN = (1 << 3), /* 位3: Link_Going_Down - 链路即将断开 */
  LINK_EVENT_PARAM_REPORT =
      (1 << 4), /* 位4: Link_Parameters_Report - 参数变化 */
  LINK_EVENT_HANDOVER_IMMINENT =
      (1 << 5), /* 位5: Handover_Imminent - 即将切换 */
  LINK_EVENT_HANDOVER_COMPLETE =
      (1 << 6), /* 位6: Handover_Complete - 切换完成 */
  /* 位 7-15: 保留供将来使用 */
  LINK_EVENT_ALL = 0xFFFF /* 所有事件 (用于一次订阅全部) */
} LINK_EVENT_TYPE;

/**
 * @brief LINK_Event_Subscribe_Request - 事件订阅请求
 * 由 CM Core 发送, 订阅指定链路的事件
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 目标链路标识符 */
  uint16_t event_list;           /* 要订阅的事件位图 (LINK_EVENT_TYPE 组合) */
} LINK_Event_Subscribe_Request;

/**
 * @brief LINK_Event_Subscribe_Confirm - 事件订阅确认
 * 由 DLM 返回, 确认订阅结果
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 链路标识符 (回显) */
  STATUS status;                 /* 操作状态 */
  uint16_t subscribed_events;    /* 实际成功订阅的事件位图 */
} LINK_Event_Subscribe_Confirm;

/**
 * @brief LINK_Event_Unsubscribe_Request - 取消事件订阅请求
 * 由 CM Core 发送, 取消订阅指定事件
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 目标链路标识符 */
  uint16_t event_list;           /* 要取消订阅的事件位图 */
} LINK_Event_Unsubscribe_Request;

/**
 * @brief LINK_Event_Unsubscribe_Confirm - 取消事件订阅确认
 * 由 DLM 返回, 确认取消结果
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 链路标识符 (回显) */
  STATUS status;                 /* 操作状态 */
  uint16_t remaining_events;     /* 取消后仍然订阅的事件位图 */
} LINK_Event_Unsubscribe_Confirm;

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Get_Parameters
 *
 * 功能: 主动查询链路当前参数
 * 用途: CM Core 获取链路的实时状态信息
 *
 * 与 Link_Parameters_Report 的区别:
 * - Link_Get_Parameters: 主动查询 (pull 模式)
 * - Link_Parameters_Report: 被动接收 (push 模式, 由 DLM 主动推送)
 *===========================================================================*/

/**
 * @brief LINK_PARAM_QUERY_TYPE - 参数查询类型位图枚举
 * 指定要查询的参数类型, 可以组合多个
 */
typedef enum {
  LINK_PARAM_QUERY_DATA_RATE = (1 << 0),       /* 位0: 数据速率 (TX/RX) */
  LINK_PARAM_QUERY_SIGNAL_STRENGTH = (1 << 1), /* 位1: 信号强度 (dBm) */
  LINK_PARAM_QUERY_SINR = (1 << 2),            /* 位2: 信噪比 (dB) */
  LINK_PARAM_QUERY_PACKET_LOSS = (1 << 3),     /* 位3: 丢包率 */
  LINK_PARAM_QUERY_LINK_QUALITY = (1 << 4),    /* 位4: 链路质量 (0-100) */
  LINK_PARAM_QUERY_LATENCY = (1 << 5),         /* 位5: 延迟 (ms) */
  LINK_PARAM_QUERY_JITTER = (1 << 6),          /* 位6: 抖动 (ms) */
  LINK_PARAM_QUERY_AVAILABLE_BW = (1 << 7),    /* 位7: 可用带宽 */
  LINK_PARAM_QUERY_IP_CONFIG = (1 << 8),       /* 位8: IP 配置信息 */
  LINK_PARAM_QUERY_ALL = 0xFFFF                /* 查询所有参数 */
} LINK_PARAM_QUERY_TYPE;

/**
 * @brief LINK_PARAMETERS - 链路参数值结构体
 * 包含链路的所有可查询参数
 */
typedef struct {
  /*-----------------------------------------------------------------------
   * 基本传输参数
   *-----------------------------------------------------------------------*/
  uint32_t current_tx_rate_kbps; /* 当前发送速率 (kbps) */
  uint32_t current_rx_rate_kbps; /* 当前接收速率 (kbps) */
  int32_t signal_strength_dbm;   /* 信号强度 (dBm, 负值) */
  uint8_t signal_quality;        /* 信号质量 (0-100, 100 最好) */
  int16_t sinr_db;               /* 信噪比 (dB * 10, 以保留精度) */
  uint8_t reserved1;             /* 保留, 用于对齐 */

  /*-----------------------------------------------------------------------
   * QoS 相关参数
   *-----------------------------------------------------------------------*/
  uint32_t current_latency_ms;       /* 当前往返延迟 (毫秒) */
  uint32_t current_jitter_ms;        /* 当前延迟抖动 (毫秒) */
  float packet_loss_rate;            /* 丢包率 (0.0-1.0) */
  uint32_t available_bandwidth_kbps; /* 可用带宽 (kbps) */

  /*-----------------------------------------------------------------------
   * 网络配置信息 (IP 层)
   *-----------------------------------------------------------------------*/
  uint32_t ip_address;    /* IP 地址 (网络字节序, 大端) */
  uint32_t netmask;       /* 子网掩码 (网络字节序) */
  uint32_t gateway;       /* 默认网关 (网络字节序) */
  uint32_t dns_primary;   /* 主 DNS 服务器 (网络字节序) */
  uint32_t dns_secondary; /* 备 DNS 服务器 (网络字节序) */

  /*-----------------------------------------------------------------------
   * 链路状态信息
   *-----------------------------------------------------------------------*/
  uint8_t link_state;      /* 链路状态: 0=Down, 1=Up, 2=GoingDown */
  uint8_t handover_status; /* 切换状态: 0=None, 1=Preparing, 2=Active */
  uint16_t active_bearers; /* 当前活动的 Bearer 数量 */
} LINK_PARAMETERS;

/**
 * @brief LINK_Get_Parameters_Request - 链路参数查询请求
 * 由 CM Core 发送到 DLM
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 目标链路标识符 */
  uint16_t param_type_list;      /* 要查询的参数类型位图 */
} LINK_Get_Parameters_Request;

/**
 * @brief LINK_Get_Parameters_Confirm - 链路参数查询确认
 * 由 DLM 返回, 包含请求的参数值
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 链路标识符 (回显) */
  STATUS status;                 /* 操作状态 */
  uint16_t returned_params;      /* 实际返回的参数位图 */
  LINK_PARAMETERS parameters;    /* 参数值 (仅对应位有效) */
} LINK_Get_Parameters_Confirm;

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Parameters_Report
 *
 * 功能: 链路参数变化报告 (由 DLM 主动发送)
 * 用途: 当链路参数发生显著变化时, DLM 主动通知 CM Core
 *
 * 触发条件:
 * - 信号强度变化超过阈值
 * - 链路速率显著变化
 * - QoS 参数变化
 * - 网络配置变更
 *
 * 注意: 这是 Indication 类型原语, 没有对应的 Request/Confirm
 *===========================================================================*/

/**
 * @brief LINK_Parameters_Report_Indication - 链路参数报告指示
 * 由 DLM 主动推送到 CM Core
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 报告链路的标识符 */
  uint16_t changed_params;    /* 发生变化的参数位图 (LINK_PARAM_QUERY_TYPE) */
  LINK_PARAMETERS parameters; /* 当前参数值 */
  uint32_t report_timestamp;  /* 报告时间戳 (Unix 时间戳) */
} LINK_Parameters_Report_Indication;

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Detected
 *
 * 功能: 新链路检测指示
 * 用途: 当 DLM 检测到新的可用链路时, 通知 CM Core
 *
 * 触发场景:
 * - 飞机进入新基站覆盖区域
 * - 卫星链路变为可用
 * - Wi-Fi 热点检测
 *
 * 注意: 这是 Indication 类型原语
 *===========================================================================*/

/**
 * @brief LINK_Detected_Indication - 链路检测指示
 * 由 DLM 发送到 CM Core, 报告发现的新链路
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 新检测到的链路标识符 */
  LINK_PARAM_TYPE link_type;     /* 链路类型 */
  uint32_t max_bandwidth_kbps;   /* 预估最大带宽 (kbps) */
  int32_t signal_strength_dbm;   /* 信号强度 (dBm) */
  uint8_t signal_quality;        /* 信号质量 (0-100) */
  uint8_t security_supported;    /* 支持的安全等级 (1-5) */
  uint16_t reserved;             /* 保留字段 */
  uint32_t detection_timestamp;  /* 检测时间戳 (Unix 时间戳) */
} LINK_Detected_Indication;

/*===========================================================================
 * IEEE 802.21 标准原语 - Link_Going_Down / Link_Down / Link_Up
 *
 * 功能: 链路状态变化指示
 *
 * Link_Going_Down: 链路即将断开的提前预警
 * - 给 CM Core 时间准备链路切换
 * - 包含预计断开时间和原因
 *
 * Link_Down: 链路已经断开
 * - 链路变为不可用状态
 * - 需要立即切换到备用链路
 *
 * Link_Up: 链路上线
 * - 链路变为可用状态
 * - 包含初始参数信息
 *===========================================================================*/

/**
 * @brief LINK_DOWN_REASON - 链路断开原因枚举
 * 描述链路断开的具体原因
 */
typedef enum {
  LINK_DOWN_REASON_EXPLICIT = 0,      /* 显式断开 - 主动断开链路 */
  LINK_DOWN_REASON_SIGNAL_LOSS = 1,   /* 信号丢失 - 信号低于阈值 */
  LINK_DOWN_REASON_HANDOVER = 2,      /* 切换导致 - 正常切换断开 */
  LINK_DOWN_REASON_FAILURE = 3,       /* 链路故障 - 硬件或协议故障 */
  LINK_DOWN_REASON_POWER_OFF = 4,     /* 设备关闭 - 设备被关闭 */
  LINK_DOWN_REASON_LOW_BATTERY = 5,   /* 电池电量低 - 需要省电 */
  LINK_DOWN_REASON_TIMEOUT = 6,       /* 超时 - 无响应超时 */
  LINK_DOWN_REASON_COVERAGE_LOST = 7, /* 覆盖丢失 - 飞出覆盖区域 */
  LINK_DOWN_REASON_UNKNOWN = 255      /* 未知原因 */
} LINK_DOWN_REASON;

/**
 * @brief LINK_Going_Down_Indication - 链路即将断开指示
 * 由 DLM 发送, 提前预警链路即将断开
 * CM Core 应开始准备链路切换
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 即将断开的链路标识符 */
  uint32_t time_to_down_ms;      /* 预计断开时间 (毫秒后) */
  uint8_t reason_code;           /* 断开原因 (LINK_DOWN_REASON 值) */
  uint8_t confidence;            /* 预测置信度 (0-100, 100 最确定) */
  uint16_t reserved;             /* 保留字段 */
  char reason_text[64];          /* 原因描述文本 (可读信息) */
} LINK_Going_Down_Indication;

/**
 * @brief LINK_Down_Indication - 链路断开指示
 * 由 DLM 发送, 通知链路已经断开
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 已断开的链路标识符 */
  uint8_t reason_code;           /* 断开原因 (LINK_DOWN_REASON 值) */
  uint8_t reserved[3];           /* 保留字段, 用于对齐 */
  char reason_text[64];          /* 原因描述文本 */
  uint32_t down_timestamp;       /* 断开时间戳 (Unix 时间戳) */
} LINK_Down_Indication;

/**
 * @brief LINK_Up_Indication - 链路上线指示
 * 由 DLM 发送, 通知链路已变为可用
 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* 上线的链路标识符 */
  LINK_PARAMETERS parameters;    /* 链路初始参数 */
  uint32_t up_timestamp;         /* 上线时间戳 (Unix 时间戳) */
} LINK_Up_Indication;

/*===========================================================================
 * 辅助函数 - 事件类型和原因码转换
 *
 * 这些函数用于将事件类型和原因码转换为可读字符串
 * 便于日志输出和调试
 *===========================================================================*/

/**
 * @brief 将链路事件类型转换为字符串
 * @param event 链路事件类型枚举值
 * @return 对应的字符串描述
 *
 * 注意: 此函数处理单个事件值, 不处理组合位图
 */
static inline const char *link_event_type_to_string(LINK_EVENT_TYPE event) {
  switch (event) {
  case LINK_EVENT_DETECTED:
    return "LINK_DETECTED"; /* 发现新链路 */
  case LINK_EVENT_UP:
    return "LINK_UP"; /* 链路上线 */
  case LINK_EVENT_DOWN:
    return "LINK_DOWN"; /* 链路下线 */
  case LINK_EVENT_GOING_DOWN:
    return "LINK_GOING_DOWN"; /* 链路即将断开 */
  case LINK_EVENT_PARAM_REPORT:
    return "LINK_PARAMETERS_REPORT"; /* 参数报告 */
  case LINK_EVENT_HANDOVER_IMMINENT:
    return "HANDOVER_IMMINENT"; /* 即将切换 */
  case LINK_EVENT_HANDOVER_COMPLETE:
    return "HANDOVER_COMPLETE"; /* 切换完成 */
  default:
    return "UNKNOWN"; /* 未知事件 */
  }
}

/**
 * @brief 将链路断开原因转换为字符串
 * @param reason 断开原因枚举值
 * @return 对应的字符串描述
 */
static inline const char *link_down_reason_to_string(LINK_DOWN_REASON reason) {
  switch (reason) {
  case LINK_DOWN_REASON_EXPLICIT:
    return "EXPLICIT_DISCONNECT"; /* 显式断开 */
  case LINK_DOWN_REASON_SIGNAL_LOSS:
    return "SIGNAL_LOSS"; /* 信号丢失 */
  case LINK_DOWN_REASON_HANDOVER:
    return "HANDOVER"; /* 切换导致 */
  case LINK_DOWN_REASON_FAILURE:
    return "LINK_FAILURE"; /* 链路故障 */
  case LINK_DOWN_REASON_POWER_OFF:
    return "POWER_OFF"; /* 设备关闭 */
  case LINK_DOWN_REASON_LOW_BATTERY:
    return "LOW_BATTERY"; /* 电池电量低 */
  case LINK_DOWN_REASON_TIMEOUT:
    return "TIMEOUT"; /* 超时 */
  case LINK_DOWN_REASON_COVERAGE_LOST:
    return "COVERAGE_LOST"; /* 覆盖丢失 */
  default:
    return "UNKNOWN"; /* 未知原因 */
  }
}

#endif /* MIH_PROTOCOL_H */ /* 头文件保护宏结束 */
