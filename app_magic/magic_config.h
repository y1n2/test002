/**
 * @file magic_config.h
 * @brief MAGIC 配置管理器头文件
 * @description 管理 3 个核心 XML 配置文件的数据结构和 API
 *
 * 本文件定义了 MAGIC 系统配置管理的所有数据结构和接口函数
 * 主要管理以下 3 个 XML 配置文件:
 * 1. Datalink_Profile.xml - 数据链路配置
 * 2. Central_Policy_Profile.xml - 中央策略配置
 * 3. Client_Profile.xml - 客户端配置
 *
 * 配置加载流程:
 * 1. 调用 magic_config_init() 初始化配置结构体
 * 2. 调用相应的加载函数加载各个配置文件
 * 3. 使用查找函数获取特定配置项
 * 4. 使用完毕后调用 magic_config_cleanup() 清理资源
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MAGIC_CONFIG_H /* 头文件保护宏开始，防止重复包含 */
#define MAGIC_CONFIG_H /* 定义头文件保护宏 */

/*===========================================================================
 * 标准头文件包含
 *===========================================================================*/
#include <stdbool.h> /* 包含布尔类型定义，如 bool, true, false */
#include <stdint.h>  /* 包含标准整数类型定义，如 uint32_t, uint8_t 等 */
#include <time.h>    /* 包含时间相关函数和类型定义，如 time_t */

/*===========================================================================
 * 常量定义 - 系统限制和最大值
 *===========================================================================*/

/* 数据链路相关常量 */
#define MAX_LINKS 10   /**< @brief 系统中最多支持的数据链路数量 */
#define MAX_CLIENTS 50 /**< @brief 系统中最多支持的客户端数量 */
#define MAX_POLICY_RULESETS                                                    \
  10 /**< @brief 策略配置文件中最多支持的规则集数量                \
      */
#define MAX_RULES_PER_RULESET 20 /**< @brief 每个规则集中最多支持的规则数量 */
#define MAX_PATH_PREFERENCES 5   /**< @brief 每个规则中最多支持的路径偏好数量 */
#define MAX_TRAFFIC_CLASS_DEFS 10 /**< @brief 流量类别定义的最大数量 */
#define MAX_MATCH_PATTERNS 5      /**< @brief 每个流量类别的最大匹配模式数量 */

/* 字符串长度限制常量 */
#define MAX_ID_LEN 64 /**< @brief ID 字段的最大长度 (如 link_id, client_id) */
#define MAX_NAME_LEN 128     /**< @brief 名称字段的最大长度 (如 link_name) */
#define MAX_IP_STR_LEN 64    /**< @brief IP 地址字符串的最大长度 */
#define MAX_PORTLIST_LEN 256 /**< @brief 端口列表字符串的最大长度 */

/* 客户端配置相关常量 (Client_Profile.xml v2.0) */
#define MAX_ALLOWED_DLMS 10 /**< @brief 每个客户端允许的最大 DLM 数量 */
#define MAX_ALLOWED_QOS_LEVELS                                                 \
  5                           /**< @brief 每个客户端允许的最大 QoS 级别数量 */
#define MAX_ALLOWED_PHASES 10 /**< @brief 每个客户端允许的最大飞行阶段数量 */
#define MAX_IP_RANGE_LEN 128  /**< @brief IP 范围字符串最大长度 */
#define MAX_PROTOCOL_LEN 32   /**< @brief 协议名称最大长度 */

/*===========================================================================
 * 数据链路配置结构体 (Datalink_Profile.xml v2.0)
 *
 * 本节定义数据链路配置相关的数据结构
 * 新版支持 DLMConfig 结构: DLM -> Links -> Link
 * 每个 DLM 仅包含一条物理链路
 *===========================================================================*/

/* DLM 相关常量 */
#define MAX_QOS_LEVELS_PER_DLM 4    /* 每个 DLM 最多支持的 QoS 级别数 */
#define MAX_DLM_SOCKET_PATH_LEN 128 /* DLM Socket 路径最大长度 */

/**
 * @brief DLM 类型枚举 (对应 XML 中的 DLMType)
 */
typedef enum {
  DLM_TYPE_UNKNOWN = 0,   ///< 未知类型
  DLM_TYPE_SATELLITE = 1, ///< 卫星链路 - 全球覆盖
  DLM_TYPE_CELLULAR = 2,  ///< 蜂窝/ATG 链路 - 地面覆盖
  DLM_TYPE_HYBRID = 3     ///< 混合/地面链路 (如 Gatelink Wi-Fi) - 机场覆盖
} DLMType;

/**
 * @brief 负载均衡算法枚举
 */
typedef enum {
  LB_ALGO_UNKNOWN = 0,      ///< 未知
  LB_ALGO_ROUND_ROBIN = 1,  ///< 轮询
  LB_ALGO_LEAST_LOADED = 2, ///< 最小负载
  LB_ALGO_PRIORITY = 3      ///< 优先级
} LoadBalanceAlgorithm;

/**
 * @brief 覆盖范围枚举 (兼容旧代码)
 * @details 定义链路的地理覆盖范围类型。
 */
typedef enum {
  COVERAGE_UNKNOWN = 0,     ///< 未知覆盖范围 - 默认值
  COVERAGE_GLOBAL = 1,      ///< 全球覆盖 - 如卫星链路
  COVERAGE_TERRESTRIAL = 2, ///< 地面覆盖 - 如蜂窝网络
  COVERAGE_GATE_ONLY = 3    ///< 仅机场覆盖 - 如地面 Wi-Fi
} Coverage;

/**
 * @brief 地理覆盖配置结构体
 * @details 定义 DLM 的地理覆盖边界 (用于策略决策时的位置检查)。
 */
typedef struct {
  bool enabled; ///< 是否启用覆盖限制

  /* 经纬度范围 */
  double min_latitude;  ///< 最小纬度 (-90 to 90)
  double max_latitude;  ///< 最大纬度
  double min_longitude; ///< 最小经度 (-180 to 180)
  double max_longitude; ///< 最大经度

  /* 高度范围 (英尺) */
  uint32_t min_altitude_ft; ///< 最小高度
  uint32_t max_altitude_ft; ///< 最大高度
} CoverageConfig;

/**
 * @brief 负载均衡配置结构体
 */
typedef struct {
  LoadBalanceAlgorithm algorithm;     ///< 负载均衡算法
  bool enable_failover;               ///< 是否启用故障转移
  uint32_t health_check_interval_sec; ///< 健康检查间隔 (秒)
} LoadBalanceConfig;

/**
 * @brief DLM 配置结构体 (Datalink_Profile.xml v2.0)
 * @details 完整描述一个数据链路管理器的配置信息。每个 DLM 仅包含一条物理链路。
 */
typedef struct {
  /* 基本信息 */
  char
      dlm_name[MAX_ID_LEN]; ///< DLM 名称/ID (如 "LINK_SATCOM", "LINK_CELLULAR")
  char description[MAX_NAME_LEN]; ///< 描述
  bool enabled;                   ///< 是否启用
  DLMType dlm_type; ///< DLM 类型 (1=Satellite, 2=Cellular, 3=Hybrid)

  /* 物理链路信息 (每个 DLM 仅一条链路) */
  char link_name[MAX_NAME_LEN]; ///< 链路名称 (如 "Satcom-Modem")
  uint32_t link_number;         ///< 链路编号

  /* 带宽容量 (kbps) */
  float max_forward_bw_kbps;    ///< 最大前向带宽 (下行)
  float max_return_bw_kbps;     ///< 最大返回带宽 (上行)
  float oversubscription_ratio; ///< 超售比率 (如 1.2 表示 20% 超售)

  /* 支持的 QoS 级别 */
  uint8_t supported_qos[MAX_QOS_LEVELS_PER_DLM]; ///< 支持的 QoS 级别列表
  uint32_t num_supported_qos;                    ///< 支持的 QoS 级别数量

  /* 物理特性 */
  uint32_t latency_ms;    ///< 典型延迟 (ms)
  uint32_t jitter_ms;     ///< 抖动 (ms)
  float packet_loss_rate; ///< 丢包率 (0.0-1.0)

  /* MIH 接口信息 - 使用 Unix Domain Socket */
  char mihf_id[MAX_ID_LEN]; ///< MIH Function ID (如 "mihf_satcom")
  char dlm_socket_path[MAX_DLM_SOCKET_PATH_LEN]; ///< DLM Socket 路径 (如
                                                 ///< "/tmp/mihf_satcom.sock")

  /* 负载均衡配置 */
  LoadBalanceConfig load_balance; ///< 负载均衡配置

  /* 地理覆盖配置 */
  CoverageConfig coverage; ///< 覆盖范围配置

  /* 运行时状态 */
  bool is_active; ///< DLM 是否激活
} DLMConfig;

/* ========== 兼容性类型别名 (供旧代码过渡使用) ========== */
typedef DLMConfig DatalinkProfile;

/*===========================================================================
 * 中央策略配置结构体 (Central_Policy_Profile.xml)
 *
 * 本节定义中央策略配置相关的数据结构
 * 策略配置决定如何为不同类型的流量选择合适的通信链路
 *===========================================================================*/

/**
 * @brief 策略动作枚举
 * @details 定义策略规则的执行动作。
 */
typedef enum {
  ACTION_PERMIT = 1,   ///< 允许 - 允许使用指定的链路
  ACTION_PROHIBIT = 2, ///< 禁止 - 禁止使用指定的链路
  ACTION_DEFAULT = 0   ///< 默认 - 使用系统默认行为
} PolicyAction;

/**
 * @brief 路径偏好结构体
 * @details 描述对特定链路的偏好设置。
 */
typedef struct {
  uint32_t ranking;           ///< 排名 - 优先级 (1=最高, 数字越大优先级越低)
  char link_id[MAX_ID_LEN];   ///< 链路 ID - 偏好的链路标识符
  PolicyAction action;        ///< 动作 - 对该链路的策略动作
  char security_required[32]; ///< 安全要求 - 所需的安全等级字符串

  /* v2.0 新增: 延迟限制 */
  bool has_max_latency;    ///< 是否设置了延迟限制
  uint32_t max_latency_ms; ///< 最大允许延迟 (毫秒) - 超过此值链路不可选

  /* v2.2 新增: WoW (Weight on Wheels) 感知 */
  bool on_ground_only; ///< 仅地面可用 - true 表示此链路仅在地面 (WoW=true)
                       ///< 时可选
  bool airborne_only;  ///< 仅空中可用 - true 表示此链路仅在空中 (WoW=false)
                       ///< 时可选
} PathPreference;

/**
 * @brief 策略规则结构体
 * @details 定义一类流量的链路选择规则。
 */
typedef struct {
  char traffic_class[MAX_ID_LEN]; ///< 流量类别 - 规则适用的流量类型
  PathPreference preferences[MAX_PATH_PREFERENCES]; ///< 路径偏好列表
  uint32_t num_preferences; ///< 偏好数量 - preferences 数组的有效元素数
} PolicyRule;

/**
 * @brief 策略规则集结构体
 * @details 包含多个策略规则的集合，通常对应特定的飞行阶段。
 */
typedef struct {
  char ruleset_id[MAX_ID_LEN];      ///< 规则集 ID - 规则集的唯一标识符
  char flight_phases[MAX_NAME_LEN]; ///< 飞行阶段 - 适用的飞行阶段 (如 "cruise",
                                    ///< "takeoff")

  PolicyRule rules[MAX_RULES_PER_RULESET]; ///< 策略规则数组
  uint32_t num_rules; ///< 规则数量 - rules 数组的有效元素数
} PolicyRuleSet;

/*===========================================================================
 * 流量分类定义结构体 (Central_Policy_Profile.xml v2.0 新增)
 *
 * 本节定义动态流量分类相关的数据结构
 * 用于将客户端属性 (PriorityClass, QoSLevel, ProfileName) 映射到抽象流量类别
 *===========================================================================*/

/**
 * @brief 流量类别定义结构体
 * @details 描述一个流量类别的匹配规则。
 */
typedef struct {
  char traffic_class_id[MAX_ID_LEN]; ///< 流量类别 ID (如 "COCKPIT_DATA",
                                     ///< "BULK_DATA")

  /* 匹配条件 - 任意一个条件匹配则命中 */
  bool has_priority_class_match; ///< 是否启用优先级类匹配
  uint8_t match_priority_class;  ///< 匹配的优先级类 (1-8)

  bool has_qos_level_match; ///< 是否启用 QoS 级别匹配
  uint8_t match_qos_level;  ///< 匹配的 QoS 级别 (0-3)

  uint32_t num_patterns; ///< Profile 名称模式数量
  char match_patterns[MAX_MATCH_PATTERNS]
                     [MAX_NAME_LEN]; ///< 通配符模式列表 (支持 * 和 ?)

  bool is_default; ///< 是否为默认流量类 (匹配所有剩余流量)
} TrafficClassDefinition;

/**
 * @brief 链路切换策略结构体
 * @details 定义全局链路切换防抖动参数。
 */
typedef struct {
  uint32_t min_dwell_time_sec;    ///< 最小驻留时间 (秒) - 切换后至少保持的时间
  uint32_t hysteresis_percentage; ///< 迟滞百分比 (0-100) -
                                  ///< 新链路需优于当前多少才切换
} SwitchingPolicy;

/**
 * @brief 中央策略配置结构体 (v2.0 增强版)
 * 包含所有策略配置信息
 */
typedef struct {
  char available_links[MAX_LINKS]
                      [MAX_ID_LEN]; /* 可用链路列表 - 系统可用的链路 ID 数组 */
  uint32_t num_links; /* 链路数量 - available_links 数组的有效元素数 */

  /* v2.0 新增: 流量类别定义 */
  TrafficClassDefinition traffic_class_defs[MAX_TRAFFIC_CLASS_DEFS];
  uint32_t num_traffic_class_defs; /* 流量类别定义数量 */

  /* v2.0 新增: 全局链路切换策略 */
  SwitchingPolicy switching_policy; /* 链路切换防抖动参数 */

  PolicyRuleSet rulesets[MAX_POLICY_RULESETS]; /* 规则集数组 - 所有策略规则集 */
  uint32_t num_rulesets; /* 规则集数量 - rulesets 数组的有效元素数 */
} CentralPolicyProfile;

/*===========================================================================
 * 客户端配置结构体 (Client_Profile.xml v2.0)
 *
 * 本节定义客户端配置相关的数据结构 - 7大分区结构
 * 1. Auth - 认证信息
 * 2. Bandwidth - 带宽配置 (单位: kbps)
 * 3. QoS - 服务质量配置
 * 4. LinkPolicy - 链路策略配置
 * 5. Session - 会话策略配置
 * 6. Traffic - 流量安全配置
 * 7. Location - 位置约束配置
 *===========================================================================*/

/**
 * @brief 客户端配置飞行阶段枚举 (Client_Profile.xml v2.0)
 * @details 定义航空器的各个飞行阶段。
 * @note 这与 magic_adif.h 中的 AdifFlightPhase 不同，这里使用 CFG_
 * 前缀避免命名冲突。
 */
typedef enum {
  CFG_FLIGHT_PHASE_GATE = 0,        ///< 停机位 - 登机/下机阶段
  CFG_FLIGHT_PHASE_TAXI = 1,        ///< 滑行 - 地面滑行阶段
  CFG_FLIGHT_PHASE_TAKE_OFF = 2,    ///< 起飞 - 起飞阶段
  CFG_FLIGHT_PHASE_CLIMB = 3,       ///< 爬升 - 爬升阶段
  CFG_FLIGHT_PHASE_CRUISE = 4,      ///< 巡航 - 巡航阶段
  CFG_FLIGHT_PHASE_DESCENT = 5,     ///< 下降 - 下降阶段
  CFG_FLIGHT_PHASE_APPROACH = 6,    ///< 进近 - 进近阶段
  CFG_FLIGHT_PHASE_LANDING = 7,     ///< 着陆 - 着陆阶段
  CFG_FLIGHT_PHASE_MAINTENANCE = 8, ///< 维护 - 维护阶段
  CFG_FLIGHT_PHASE_UNKNOWN = -1     ///< 未知 - 默认值
} CfgFlightPhase;

/**
 * @brief 优先级类型枚举 (QoS 配置)
 * @details 定义优先级处理方式。
 */
typedef enum {
  PRIORITY_TYPE_BLOCKING = 1,   ///< 阻塞式 - 等待资源释放
  PRIORITY_TYPE_PREEMPTION = 2, ///< 抢占式 - 抢占低优先级资源
  PRIORITY_TYPE_UNKNOWN = 0     ///< 未知 - 默认值
} PriorityType;

/**
 * @brief 1. 客户端认证信息结构体 (Auth 分区)
 * @details 包含客户端的认证凭据和源地址。
 */
typedef struct {
  char username[MAX_ID_LEN];        ///< 用户名 - 客户端登录用户名
  char client_password[MAX_ID_LEN]; ///< 客户端密码 - 用于客户端认证
  char server_password[MAX_ID_LEN]; ///< 服务端密码 - 用于双向认证
  char source_ip[MAX_IP_STR_LEN];   ///< 源 IP 地址 - 客户端的 IP 地址
} ClientAuthConfig;

/**
 * @brief 2. 带宽配置结构体 (Bandwidth 分区)
 * @details 包含客户端的带宽限制参数 (所有值单位: kbps)。
 * @note XML 中存储的是 bps，解析时需要除以 1000 转换为 kbps。
 */
typedef struct {
  uint32_t max_forward_kbps; ///< 最大前向带宽 (kbps) - 客户端->服务端最大速率
  uint32_t max_return_kbps;  ///< 最大返回带宽 (kbps) - 服务端->客户端最大速率
  uint32_t
      guaranteed_forward_kbps;     ///< 保证前向带宽 (kbps) - 最低保证的前向速率
  uint32_t guaranteed_return_kbps; ///< 保证返回带宽 (kbps) - 最低保证的返回速率
  uint32_t default_request_kbps;   ///< 默认请求带宽 (kbps) - 未指定时的默认请求
} BandwidthConfig;

/**
 * @brief 3. QoS 配置结构体 (QoS 分区)
 * @details 包含客户端的服务质量参数。
 */
typedef struct {
  PriorityType priority_type; ///< 优先级类型 - 1=阻塞式, 2=抢占式
  uint8_t priority_class;     ///< 优先级类 - 范围 1-9, 数字越小优先级越高
  uint8_t default_level;      ///< 默认 QoS 级别 - 未指定时的默认级别

  uint8_t allowed_levels[MAX_ALLOWED_QOS_LEVELS]; ///< 允许的 QoS 级别列表
  uint32_t num_allowed_levels;                    ///< 允许的级别数量
} QoSConfig;

/**
 * @brief 4. 链路策略配置结构体 (LinkPolicy 分区)
 * 定义客户端可使用的数据链路
 */
typedef struct {
  char allowed_dlms[MAX_ALLOWED_DLMS]
                   [MAX_ID_LEN]; /* 允许的 DLM 列表 (如 LINK_SATCOM,
                                    LINK_CELLULAR) */
  uint32_t num_allowed_dlms;     /* 允许的 DLM 数量 */

  char preferred_dlm[MAX_ID_LEN]; /* 首选 DLM - 优先使用的链路 */
  bool allow_multi_link;          /* 是否允许多链路 - 同时使用多条链路 */
  uint32_t max_concurrent_links;  /* 最大并发链路数 - 同时使用的最大链路数 */
} LinkPolicyConfig;

/**
 * @brief 5. 会话策略配置结构体 (Session 分区)
 * 定义客户端会话相关参数
 */
typedef struct {
  uint32_t max_concurrent_sessions; /* 最大并发会话数 */
  uint32_t session_timeout_sec;     /* 会话超时时间 (秒) */
  bool auto_reconnect;              /* 是否自动重连 */
  uint32_t reconnect_delay_sec;     /* 重连延迟时间 (秒) */

  CfgFlightPhase allowed_phases[MAX_ALLOWED_PHASES]; /* 允许的飞行阶段列表 */
  uint32_t num_allowed_phases;                       /* 允许的阶段数量 */

  /* v2.1: MSXR 权限控制 */
  bool allow_detailed_status; /* 是否允许查询详细链路状态 (Status-Type=6/7) */
  bool allow_registered_clients; /* 是否允许查询在线客户端列表 */
  uint32_t msxr_rate_limit_sec;  /* MSXR 最小请求间隔 (秒), 0=无限制, 默认5 */

  /* v2.2: CDR 控制权限 (MACR) */
  bool allow_cdr_control; /* 是否允许控制他人 CDR (切分/重启) */
} SessionPolicyConfig;

/**
 * @brief 6. 流量安全配置结构体 (Traffic 分区)
 * 定义客户端的流量安全约束
 */
typedef struct {
  bool encryption_required; /* 是否需要加密 */
  char allowed_protocols[5]
                        [MAX_PROTOCOL_LEN]; /* 允许的协议列表 (如 TCP, UDP) */
  uint32_t num_allowed_protocols;           /* 允许的协议数量 */

  /* TFT 白名单 - 允许的流量规则（精确匹配） */
  char allowed_tfts[255][512]; /* 允许的 TFT 规则列表 */
  uint32_t num_allowed_tfts;   /* 允许的 TFT 数量 (1-255) */

  /* TFT 白名单 - 范围验证（ARINC 839 §1.2.2.2） */
  char dest_ip_range[256];     /* 允许的目标IP范围 (如 10.2.2.0/24) */
  char dest_port_range[256];   /* 允许的目标端口范围 (如 80,443,5000-6000) */
  char source_port_range[256]; /* 允许的源端口范围 (如 1024-65535) */

  uint32_t max_packet_size; /* 最大数据包大小 (字节) */
} TrafficSecurityConfig;

/**
 * @brief 7. 位置约束配置结构体 (Location 分区)
 * 定义客户端的地理和覆盖约束
 */
typedef struct {
  bool geo_restriction_enabled; /* 是否启用地理限制 */

  char allowed_regions[5][MAX_NAME_LEN]; /* 允许的区域列表 (如 "DOMESTIC",
                                            "INTERNATIONAL") */
  uint32_t num_allowed_regions;          /* 允许的区域数量 */

  bool require_coverage;      /* 是否要求覆盖检查 */
  Coverage min_coverage_type; ///< 最低覆盖类型要求
} LocationConstraintConfig;

/**
 * @brief 客户端配置结构体 (Client_Profile.xml v2.0)
 * @details 完整描述一个客户端的配置信息 - 7大分区整合。
 */
typedef struct {
  /* 基础信息 */
  char profile_name[MAX_NAME_LEN]; ///< 配置文件名称 - 主查找键
  char client_id[MAX_ID_LEN];      ///< 客户端 ID - 唯一标识符
  char description[MAX_NAME_LEN];  ///< 描述 - 人类可读描述
  bool enabled;                    ///< 是否启用 - 配置是否生效

  /* 7大分区配置 */
  ClientAuthConfig auth;             ///< 1. 认证信息
  BandwidthConfig bandwidth;         ///< 2. 带宽配置 (kbps)
  QoSConfig qos;                     ///< 3. QoS 配置
  LinkPolicyConfig link_policy;      ///< 4. 链路策略
  SessionPolicyConfig session;       ///< 5. 会话策略
  TrafficSecurityConfig traffic;     ///< 6. 流量安全
  LocationConstraintConfig location; ///< 7. 位置约束

  /* 运行时状态 */
  bool is_online; ///< 是否在线 - 客户端当前是否连接
} ClientProfile;

/*===========================================================================
 * 全局配置管理器结构体
 *
 * 本节定义全局配置管理器，整合所有配置信息
 *===========================================================================*/

/**
 * @brief MAGIC 配置管理器结构体
 * @details 全局配置管理器的主结构体，包含所有从 XML 加载的配置信息。
 */
typedef struct {
  DLMConfig
      dlm_configs[MAX_LINKS];  ///< DLM 配置数组 (Datalink_Profile.xml v2.0)
  uint32_t num_dlm_configs;    ///< DLM 数量
  CentralPolicyProfile policy; ///< 中央策略配置
  ClientProfile clients[MAX_CLIENTS]; ///< 客户端配置数组
  uint32_t num_clients;               ///< 客户端数量
  time_t load_time;                   ///< 加载时间 - 配置最后加载的时间戳
  bool is_loaded;                     ///< 是否已加载 - 配置是否成功加载
  bool adif_degraded_mode; ///< ADIF 连接失败时为 true，仅保障核心应用
} MagicConfig;

/*===========================================================================
 * API 函数声明
 *
 * 本节声明配置管理器的所有公共接口函数
 *===========================================================================*/

/**
 * @brief 初始化配置管理器。
 * @param[in,out] config 指向配置管理器结构体的指针。
 * @return void
 */
void magic_config_init(MagicConfig *config);

/**
 * @brief 加载数据链路配置。
 * @param[in,out] config    指向配置管理器结构体的指针。
 * @param[in]     base_path 配置文件基础路径（如 "/etc/magic/config"）。
 * @return int 成功返回 0；失败返回 -1。
 */
int magic_config_load_datalinks(MagicConfig *config, const char *base_path);

/**
 * @brief 加载策略配置。
 * @param[in,out] config    指向配置管理器结构体的指针。
 * @param[in]     base_path 配置文件基础路径。
 * @return int 成功返回 0；失败返回 -1。
 */
int magic_config_load_policy(MagicConfig *config, const char *base_path);

/**
 * @brief 加载客户端配置。
 * @param[in,out] config    指向配置管理器结构体的指针。
 * @param[in]     base_path 配置文件基础路径。
 * @return int 成功返回 0；失败返回 -1。
 */
int magic_config_load_clients(MagicConfig *config, const char *base_path);

/**
 * @brief 查找 DLM 配置 (新版 API - 按 dlm_name 查找)。
 * @param[in]  config   指向配置管理器结构体的指针。
 * @param[in]  dlm_name 要查找的 DLM 名称 (如 "LINK_SATCOM")。
 * @return DLMConfig* 找到的 DLM 配置指针，未找到返回 NULL。
 */
DLMConfig *magic_config_find_dlm(MagicConfig *config, const char *dlm_name);

/**
 * @brief 查找数据链路配置 (兼容旧 API - 内部调用 magic_config_find_dlm)
 * @param config 指向配置管理器结构体的指针
 * @param link_id 要查找的链路 ID
 * @return 找到的链路配置指针，未找到返回 NULL
 */
DatalinkProfile *magic_config_find_datalink(MagicConfig *config,
                                            const char *link_id);

/**
 * @brief 检查飞机位置是否在 DLM 覆盖范围内
 * @param dlm DLM 配置指针
 * @param latitude 当前纬度
 * @param longitude 当前经度
 * @param altitude_ft 当前高度 (英尺)
 * @return true=在覆盖范围内, false=不在范围内
 */
bool magic_config_check_dlm_coverage(const DLMConfig *dlm, double latitude,
                                     double longitude, double altitude_ft);

/**
 * @brief 检查 DLM 是否支持指定的 QoS 级别
 * @param dlm DLM 配置指针
 * @param qos_level 要检查的 QoS 级别
 * @return true=支持, false=不支持
 */
bool magic_config_dlm_supports_qos(const DLMConfig *dlm, uint8_t qos_level);

/**
 * @brief 查找客户端配置 (按 client_id)
 * @param config 指向配置管理器结构体的指针
 * @param client_id 要查找的客户端 ID
 * @return 找到的客户端配置指针，未找到返回 NULL
 */
ClientProfile *magic_config_find_client(MagicConfig *config,
                                        const char *client_id);

/**
 * @brief 查找客户端配置 (按 ProfileName - 主查找键)
 * @param config 指向配置管理器结构体的指针
 * @param profile_name 要查找的配置文件名称
 * @return 找到的客户端配置指针，未找到返回 NULL
 */
ClientProfile *magic_config_find_client_by_profile(MagicConfig *config,
                                                   const char *profile_name);

/**
 * @brief 验证客户端是否允许使用指定的 DLM
 * @param client 客户端配置指针
 * @param dlm_id 要验证的 DLM ID (如 "LINK_SATCOM")
 * @return true=允许使用, false=不允许
 */
bool magic_config_is_dlm_allowed(const ClientProfile *client,
                                 const char *dlm_id);

/**
 * @brief 验证客户端是否允许使用指定的 QoS 级别
 * @param client 客户端配置指针
 * @param qos_level 要验证的 QoS 级别
 * @return true=允许使用, false=不允许
 */
bool magic_config_is_qos_level_allowed(const ClientProfile *client,
                                       uint8_t qos_level);

/**
 * @brief 验证客户端是否允许在指定的飞行阶段使用
 * @param client 客户端配置指针
 * @param phase 要验证的飞行阶段
 * @return true=允许使用, false=不允许
 */
bool magic_config_is_flight_phase_allowed(const ClientProfile *client,
                                          CfgFlightPhase phase);

/**
 * @brief 解析飞行阶段字符串
 * @param phase_str 飞行阶段字符串 (如 "CRUISE", "GATE")
 * @return 对应的 CfgFlightPhase 枚举值
 */
CfgFlightPhase magic_config_parse_flight_phase(const char *phase_str);

/**
 * @brief 查找策略规则集
 * @param config 指向配置管理器结构体的指针
 * @param flight_phase 要查找的飞行阶段
 * @return 找到的规则集指针，未找到返回 NULL
 */
PolicyRuleSet *magic_config_find_ruleset(MagicConfig *config,
                                         const char *flight_phase);

/**
 * @brief 打印配置摘要
 * @param config 指向配置管理器结构体的指针
 */
void magic_config_print_summary(const MagicConfig *config);

/**
 * @brief 清理配置管理器。
 * @details 释放配置结构体中分配的所有内存（如字符串、动态列表）。
 * @param[in,out] config 指向配置管理器结构体的指针。
 * @return void
 */
void magic_config_cleanup(MagicConfig *config);

#endif /* MAGIC_CONFIG_H */ /* 头文件保护宏结束 */
