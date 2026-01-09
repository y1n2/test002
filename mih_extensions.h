/**
 * @file mih_extensions.h
 * @brief MAGIC 系统对 ARINC 839 MIH 协议的扩展定义。
 * @details 本文件定义了 MAGIC
 * 系统所需的非标准原语代码（0x8000+）和增强型业务结构体。
 *          包括链路动态注册、生命周期监控（心跳）以及航空特定的链路参数上报逻辑。
 *
 * @note 标准 ARINC 839 原语定义请参考
 * [mih_protocol.h](file:///home/zhuwuhui/freeDiameter/extensions/app_magic/mih_protocol.h)。
 */

#ifndef MIH_EXTENSIONS_H // 防止头文件被重复包含的保护宏开始
#define MIH_EXTENSIONS_H // 定义头文件保护宏

#include "mih_protocol.h" // 包含MIH协议标准定义头文件，定义基础MIH原语和结构体
#include <sys/types.h>    // 包含系统类型定义头文件，提供pid_t等类型

/*===========================================================================
 * MAGIC Extension Primitive Codes (0x8000+ = Vendor Specific) -
 * MAGIC扩展原语代码（0x8000+表示厂商特定）
 *===========================================================================*/

/* Link Registration (MAGIC Extension - Required for dynamic DLM discovery) -
 * 链路注册（MAGIC扩展 - 用于动态DLM发现） */
#define MIH_EXT_LINK_REGISTER_REQUEST 0x8101 // MIH扩展链路注册请求原语代码
#define MIH_EXT_LINK_REGISTER_CONFIRM 0x8102 // MIH扩展链路注册确认原语代码

/* Lifecycle Management (MAGIC Extension - Required for health monitoring) -
 * 生命周期管理（MAGIC扩展 - 用于健康监控） */
#define MIH_EXT_HEARTBEAT 0x8F01     // MIH扩展心跳原语代码，用于周期性健康检查
#define MIH_EXT_HEARTBEAT_ACK 0x8F02 // MIH扩展心跳确认原语代码

/* Link Status Reports (Standard MIH + MAGIC Extension) - 链路状态报告（标准MIH
 * + MAGIC扩展） */
#define MIH_LINK_UP_INDICATION                                                 \
  0x0202 /* Standard ARINC 839 - 标准ARINC 839链路上报指示 */
#define MIH_LINK_DOWN_INDICATION                                               \
  0x0203 /* Standard ARINC 839 - 标准ARINC 839链路断开指示 */
#define MIH_EXT_LINK_PARAMETERS_REPORT                                         \
  0x8204 /* MAGIC Extension - MAGIC扩展链路参数报告 */

/* Link Type Enumeration (for LINK_TUPLE_ID.link_type) -
 * 链路类型枚举（用于LINK_TUPLE_ID.link_type） */
#ifndef LINK_TYPE_SATCOM     // 条件编译，如果未定义LINK_TYPE_SATCOM则定义以下宏
#define LINK_TYPE_SATCOM 1   // 卫星通信链路类型
#define LINK_TYPE_CELLULAR 2 // 蜂窝通信链路类型
#define LINK_TYPE_WIFI 3     // WiFi无线链路类型
#endif

/* Link State Enumeration - 链路状态枚举 */
#ifndef LINK_STATE_DOWN // 条件编译，如果未定义LINK_STATE_DOWN则定义枚举
typedef enum {
  LINK_STATE_DOWN = 0,       // 链路断开状态
  LINK_STATE_UP = 1,         // 链路连接状态
  LINK_STATE_GOING_DOWN = 2, // 链路正在断开状态
  LINK_STATE_GOING_UP = 3    // 链路正在连接状态
} mih_link_state_t;          // MIH链路状态类型枚举
#endif

/*===========================================================================
 * MIH_EXT_Link_Register (MAGIC Extension) - MIH扩展链路注册（MAGIC扩展）
 * @description Dynamic link registration for DLM discovery -
 * 用于DLM发现的动态链路注册
 * @rationale ARINC 839 assumes static link configuration. MAGIC requires -
 * 理由：ARINC 839假设静态链路配置。MAGIC需要 dynamic DLM registration for
 * aircraft systems. - 飞机系统的动态DLM注册
 *===========================================================================*/

/**
 * @brief 链路能力结构体。
 * @details 用于在链路注册阶段告知 CM 核心该链路的物理特性和业务限制。
 */
typedef struct {
  uint32_t max_bandwidth_kbps; ///< 最大理论带宽 (kbps)。
  uint32_t typical_latency_ms; ///< 典型往返延迟 (ms)。
  uint32_t cost_per_mb;        ///< 单位流量成本（分/MB）。
  uint8_t coverage; ///< 覆盖范围：0=None, 1=Global, 2=Terrestrial, 3=Gate。
  uint8_t security_level; ///< 安全级别：1-5，数值越大越安全。
  uint16_t mtu;           ///< 最大传输单元 (MTU)。
} __attribute__((packed)) MIH_Link_Capabilities;

/* MIH_EXT_Link_Register.request - MIH扩展链路注册请求 */
typedef struct {
  LINK_TUPLE_ID link_identifier;      /* Standard MIH link ID - 标准MIH链路ID */
  MIH_Link_Capabilities capabilities; /* Link capabilities - 链路能力 */
  pid_t dlm_pid;                      /* DLM process ID - DLM进程ID */
  uint32_t reserved; /* Reserved for future use - 保留供将来使用 */
} __attribute__((
    packed)) MIH_EXT_Link_Register_Request; // MIH扩展链路注册请求结构体

/* MIH_EXT_Link_Register.confirm - MIH扩展链路注册确认 */
typedef struct {
  STATUS status; /* SUCCESS or error code - 成功或错误代码 */
  uint32_t
      assigned_id;   /* Numeric ID assigned by CM Core - CM核心分配的数字ID */
  char message[128]; /* Status message - 状态消息 */
} __attribute__((
    packed)) MIH_EXT_Link_Register_Confirm; // MIH扩展链路注册确认结构体

/*===========================================================================
 * MIH_Link_Up/Down Indication (Standard ARINC 839 Section 2.2.2) -
 * MIH链路上报/断开指示（标准ARINC 839第2.2.2节）
 *===========================================================================*/

/* Link Parameters Structure - 链路参数结构体 */
typedef struct {
  uint32_t current_bandwidth_kbps; // 当前带宽，单位kbps
  uint32_t current_latency_ms;     // 当前延迟，单位毫秒
  int32_t signal_strength_dbm;     // 信号强度，单位dBm
  uint32_t ip_address;             /* Network byte order - IP地址，网络字节序 */
  uint32_t netmask;                // 子网掩码
  uint8_t
      link_state; /* mih_link_state_t - 链路状态，使用mih_link_state_t枚举 */
  uint8_t signal_quality; /* 0-100 - 信号质量，0-100百分比 */
  uint16_t reserved;      // 保留字段
} __attribute__((packed)) mih_link_parameters_t; // MIH链路参数结构体

/* MIH_Link_Up.indication - MIH链路上报指示 */
typedef struct {
  LINK_TUPLE_ID link_id;                     // 链路ID
  mih_link_parameters_t link_params;         // 链路参数
} __attribute__((packed)) mih_link_up_ind_t; // MIH链路上报指示结构体

/* MIH_Link_Down.indication - MIH链路断开指示 */
typedef struct {
  LINK_TUPLE_ID link_id;                       // 链路ID
  uint8_t reason_code;                         // 断开原因代码
  uint8_t reserved[3];                         // 保留字段，3字节
  char reason_text[128];                       // 断开原因文本描述
} __attribute__((packed)) mih_link_down_ind_t; // MIH链路断开指示结构体

/*===========================================================================
 * MIH_EXT_Heartbeat (MAGIC Extension) - MIH扩展心跳（MAGIC扩展）
 * @description Periodic health check from DLM to CM Core -
 * 从DLM到CM核心的周期性健康检查
 * @rationale Required for production systems to detect DLM failures -
 * 生产系统需要检测DLM故障
 *===========================================================================*/

/* Health Status Codes - 健康状态代码 */
typedef enum {
  HEALTH_STATUS_OK = 0,      /* Normal operation - 正常运行 */
  HEALTH_STATUS_WARNING = 1, /* Degraded but functional - 降级但仍能运行 */
  HEALTH_STATUS_ERROR = 2    /* Critical error - 严重错误 */
} MIH_Health_Status_Code;    // MIH健康状态代码枚举

/* MIH_EXT_Heartbeat - MIH扩展心跳 */
typedef struct {
  LINK_TUPLE_ID link_identifier; /* Link ID - 链路ID */
  uint8_t health_status;         /* MIH_Health_Status_Code -
                                    健康状态，使用MIH_Health_Status_Code枚举 */
  uint8_t reserved[3];           // 保留字段，3字节
  uint64_t tx_bytes;             /* Total bytes transmitted - 总发送字节数 */
  uint64_t rx_bytes;             /* Total bytes received - 总接收字节数 */
  uint32_t active_bearers;       /* Number of active bearers - 活动承载数量 */
  uint32_t reserved2;            // 保留字段
} __attribute__((packed)) MIH_EXT_Heartbeat; // MIH扩展心跳结构体

/* MIH_EXT_Heartbeat_Ack - MIH扩展心跳确认 */
typedef struct {
  uint8_t
      ack_status; /* 0=OK, 1=Warning received - 确认状态：0=OK，1=收到警告 */
  uint8_t reserved[3];       // 保留字段，3字节
  uint32_t server_timestamp; /* CM Core timestamp - CM核心时间戳 */
} __attribute__((packed)) MIH_EXT_Heartbeat_Ack; // MIH扩展心跳确认结构体

/*===========================================================================
 * MIH_EXT_Link_Parameters_Report (MAGIC Extension) -
 * MIH扩展链路参数报告（MAGIC扩展）
 * @description Enhanced link status report with additional metrics -
 * 带有附加指标的增强链路状态报告
 * @rationale Extends standard MIH with aviation-specific parameters -
 * 使用航空特定参数扩展标准MIH
 *===========================================================================*/

typedef struct {
  LINK_TUPLE_ID link_identifier; // 链路标识符

  /* Dynamic Link Parameters - 动态链路参数 */
  uint32_t current_bandwidth_kbps; // 当前带宽，单位kbps
  uint32_t current_latency_ms;     // 当前延迟，单位毫秒
  int32_t signal_strength_dbm;     // 信号强度，单位dBm
  uint8_t signal_quality; /* 0-100 percentage - 信号质量，0-100百分比 */
  uint8_t link_state;     /* 0=Down, 1=Up, 2=Going Down, 3=Going Up -
                             链路状态：0=断开，1=连接，2=正在断开，3=正在连接 */
  uint16_t reserved;      // 保留字段

  /* Network Configuration - 网络配置 */
  uint32_t ip_address; /* Network byte order - IP地址，网络字节序 */
  uint32_t netmask;    // 子网掩码
  uint32_t gateway;    // 网关地址

  /* Aviation-Specific Metrics - 航空特定指标 */
  uint32_t altitude_meters;  /* Aircraft altitude - 飞机高度，单位米 */
  int32_t latitude_micro;    /* Latitude * 1e6 - 纬度乘以1e6 */
  int32_t longitude_micro;   /* Longitude * 1e6 - 经度乘以1e6 */
  uint16_t ground_speed_kts; /* Ground speed in knots - 地面速度，单位节 */
  uint16_t reserved2;        // 保留字段
} __attribute__((
    packed)) MIH_EXT_Link_Parameters_Report; // MIH扩展链路参数报告结构体

/*===========================================================================
 * Enhanced MIH_Link_Resource (with MAGIC session context) -
 * 增强的MIH链路资源（带有MAGIC会话上下文）
 * @description Extends standard primitive with Diameter session tracking -
 * 使用Diameter会话跟踪扩展标准原语
 *===========================================================================*/

/**
 * @brief Enhanced MIH_Link_Resource.request with session context -
 * 带有会话上下文的增强MIH链路资源请求
 * @description Adds Diameter session ID and client ID for correlation -
 * 添加Diameter会话ID和客户端ID用于关联
 *
 * This structure extends the standard MIH_Link_Resource_Request from -
 * 此结构体从mih_protocol.h扩展标准MIH_Link_Resource_Request mih_protocol.h with
 * additional context needed by MAGIC system. - 带有MAGIC系统需要的附加上下文
 */
typedef struct {
  /* Standard ARINC 839 fields - 标准ARINC 839字段 */
  MIHF_ID destination_id;               // 目标ID
  LINK_TUPLE_ID link_identifier;        // 链路标识符
  RESOURCE_ACTION_TYPE resource_action; // 资源动作类型

  bool has_bearer_id;          // 是否有承载ID
  BEARER_ID bearer_identifier; // 承载标识符

  bool has_qos_params;      // 是否有QoS参数
  QOS_PARAM qos_parameters; // QoS参数

  /* MAGIC Extension: Session Management - MAGIC扩展：会话管理 */
  uint32_t
      diameter_session_id; /* Diameter Session-Id hash - Diameter会话ID哈希 */
  char client_id[64];      /* Aircraft/client identifier - 飞机/客户端标识符 */
  uint8_t flight_phase;    /* 0=Ground, 1=Taxi, 2=Takeoff, etc. -
                              飞行阶段：0=地面，1=滑行，2=起飞等 */
  uint8_t reserved[3];     // 保留字段，3字节
} __attribute__((
    packed)) MAGIC_MIH_Link_Resource_Request; // MAGIC增强MIH链路资源请求结构体

/**
 * @brief Enhanced MIH_Link_Resource.confirm with extended status -
 * 带有扩展状态的增强MIH链路资源确认
 */
typedef struct {
  /* Standard ARINC 839 fields - 标准ARINC 839字段 */
  MIHF_ID source_identifier;     // 源标识符
  LINK_TUPLE_ID link_identifier; // 链路标识符
  STATUS status;                 // 状态

  bool has_bearer_id;          // 是否有承载ID
  BEARER_ID bearer_identifier; // 承载标识符

  /* MAGIC Extension: Granted Parameters - MAGIC扩展：授予的参数 */
  uint32_t granted_fwd_bandwidth_kbps; // 授予的下行带宽，单位kbps
  uint32_t granted_ret_bandwidth_kbps; // 授予的上行带宽，单位kbps
  uint32_t estimated_latency_ms;       // 估计延迟，单位毫秒

  char status_message[128]; // 状态消息
} __attribute__((
    packed)) MAGIC_MIH_Link_Resource_Confirm; // MAGIC增强MIH链路资源确认结构体

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Convert extension primitive type to string -
 * 将扩展原语类型转换为字符串
 */
static inline const char *mih_ext_primitive_to_string(
    uint16_t type) // 内联函数，将原语类型转换为可读字符串
{
  switch (type) { // 根据原语类型值进行分支选择
  case MIH_EXT_LINK_REGISTER_REQUEST:
    return "MIH_EXT_Link_Register.request"; // 链路注册请求
  case MIH_EXT_LINK_REGISTER_CONFIRM:
    return "MIH_EXT_Link_Register.confirm"; // 链路注册确认
  case MIH_EXT_HEARTBEAT:
    return "MIH_EXT_Heartbeat"; // 心跳
  case MIH_EXT_HEARTBEAT_ACK:
    return "MIH_EXT_Heartbeat_Ack"; // 心跳确认
  case MIH_EXT_LINK_PARAMETERS_REPORT:
    return "MIH_EXT_Link_Parameters_Report"; // 链路参数报告
  default:
    return "UNKNOWN_EXTENSION"; // 未知扩展
  }
}

/**
 * @brief Convert health status to string - 将健康状态转换为字符串
 */
static inline const char *mih_health_status_to_string(
    MIH_Health_Status_Code status) // 内联函数，将健康状态转换为可读字符串
{
  switch (status) { // 根据健康状态值进行分支选择
  case HEALTH_STATUS_OK:
    return "OK"; // 正常
  case HEALTH_STATUS_WARNING:
    return "WARNING"; // 警告
  case HEALTH_STATUS_ERROR:
    return "ERROR"; // 错误
  default:
    return "UNKNOWN"; // 未知
  }
}

/**
 * @brief Validate link capabilities - 验证链路能力
 */
static inline bool mih_validate_capabilities(
    const MIH_Link_Capabilities *cap) // 内联函数，验证链路能力结构体的有效性
{
  if (!cap)
    return false; // 检查指针是否为空，如果为空则返回false
  if (cap->max_bandwidth_kbps == 0)
    return false; // 检查最大带宽是否为0，如果是则无效
  if (cap->typical_latency_ms == 0)
    return false; // 检查典型延迟是否为0，如果是则无效
  if (cap->coverage > 3)
    return false; // 检查覆盖范围是否超过3，如果是则无效
  if (cap->security_level == 0 || cap->security_level > 5)
    return false; // 检查安全级别是否在1-5范围内，如果不在则无效
  return true;    // 所有检查通过，返回true表示能力有效
}

#endif /* MIH_EXTENSIONS_H */ // 结束头文件保护宏
