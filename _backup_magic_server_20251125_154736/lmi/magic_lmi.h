/**
 * @file magic_lmi.h
 * @brief MAGIC Link Management Interface (LMI) - 链路管理接口定义
 * 
 * 基于 ARINC 839-2014 Section 4.2 & Attachment 2
 * 实现 IEEE 802.21 MIH (Media Independent Handover) 修改版
 * 
 * LMI 是 MAGIC 系统的"通用语言"，定义了中央管理模块(CM)和数据链路模块(DLM)之间的标准接口。
 * 所有物理链路（卫星、蜂窝网络、WiFi）都通过 DLM 驱动统一到此接口，实现介质无关的链路管理。
 */

#ifndef MAGIC_LMI_H
#define MAGIC_LMI_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* ========================================================================
 * 1. 基础类型定义 (Basic Types)
 * ======================================================================== */

/**
 * 链路 ID 类型 - 全局唯一标识符
 * 格式: LINK_<TYPE> (如 LINK_SATCOM, LINK_CELLULAR)
 */
typedef char lmi_link_id_t[64];

/**
 * DLM 驱动 ID - 标识具体的 DLM 实现
 */
typedef char lmi_driver_id_t[64];

/**
 * 会话 ID - 用于追踪资源分配会话
 */
typedef uint32_t lmi_session_id_t;

/**
 * 带宽单位: bits per second (bps)
 */
typedef uint64_t lmi_bandwidth_t;

/**
 * 时延单位: 毫秒 (ms)
 */
typedef uint32_t lmi_latency_t;

/* ========================================================================
 * 2. 枚举类型定义 (Enumerations)
 * ======================================================================== */

/**
 * 链路类型 (Link Type)
 * ARINC 839 定义的物理链路分类
 */
typedef enum {
    LMI_LINK_TYPE_UNKNOWN = 0,
    LMI_LINK_TYPE_SATELLITE,    /**< 卫星链路 (SATCOM) */
    LMI_LINK_TYPE_CELLULAR,     /**< 蜂窝网络 (4G/5G ATG) */
    LMI_LINK_TYPE_GATELINK,     /**< 地面WiFi (Airport Gatelink) */
    LMI_LINK_TYPE_VHF,          /**< VHF 数据链 (预留) */
    LMI_LINK_TYPE_HFDL,         /**< HF 数据链 (预留) */
} lmi_link_type_e;

/**
 * 链路状态 (Link State)
 * 基于 ARINC 839 Section 4.2.2.2 定义的状态机
 */
typedef enum {
    LMI_STATE_UNAVAILABLE = 0,  /**< 硬件未初始化或物理断开 */
    LMI_STATE_AVAILABLE,        /**< 已注册但未激活（待命） */
    LMI_STATE_ACTIVATING,       /**< 正在建立连接 */
    LMI_STATE_ACTIVE,           /**< 已激活，可传输数据 */
    LMI_STATE_SUSPENDING,       /**< 正在暂停 */
    LMI_STATE_SUSPENDED,        /**< 已暂停，保留资源但不传输 */
    LMI_STATE_ERROR,            /**< 硬件故障或协议错误 */
} lmi_link_state_e;

/**
 * 资源操作类型 (Resource Action)
 */
typedef enum {
    LMI_RESOURCE_ALLOCATE = 1,  /**< 分配新资源 */
    LMI_RESOURCE_MODIFY,        /**< 修改现有资源 */
    LMI_RESOURCE_RELEASE,       /**< 释放资源 */
} lmi_resource_action_e;

/**
 * 链路事件类型 (Link Event)
 * 用于 DLM 向 CM 上报异步事件
 */
typedef enum {
    LMI_EVENT_LINK_UP = 1,      /**< 链路已建立 */
    LMI_EVENT_LINK_DOWN,        /**< 链路已断开 */
    LMI_EVENT_CAPABILITY_CHANGE,/**< 能力参数变化（如信号强度） */
    LMI_EVENT_HANDOVER_RECOMMEND,/**< 建议切换到其他链路 */
    LMI_EVENT_RESOURCE_EXHAUSTED,/**< 资源耗尽 */
    LMI_EVENT_ERROR,            /**< 硬件错误 */
} lmi_event_type_e;

/**
 * 安全等级 (Security Level)
 */
typedef enum {
    LMI_SECURITY_NONE = 0,
    LMI_SECURITY_LOW,           /**< 基础加密 */
    LMI_SECURITY_MEDIUM,        /**< TLS 1.2+ */
    LMI_SECURITY_HIGH,          /**< IPsec + 端到端加密 */
} lmi_security_level_e;

/**
 * 覆盖范围 (Coverage Type)
 */
typedef enum {
    LMI_COVERAGE_UNKNOWN = 0,
    LMI_COVERAGE_GLOBAL,        /**< 全球覆盖（卫星） */
    LMI_COVERAGE_TERRESTRIAL,   /**< 陆地覆盖（蜂窝网络） */
    LMI_COVERAGE_GATE_ONLY,     /**< 仅机场停机坪 */
} lmi_coverage_e;

/* ========================================================================
 * 3. 数据结构定义 (Data Structures)
 * ======================================================================== */

/**
 * 链路能力描述 (Link Capabilities)
 * ARINC 839 Section 4.2.3.1 - Link_Capability_Discover
 */
typedef struct {
    lmi_bandwidth_t max_tx_rate;    /**< 最大上行速率 (bps) */
    lmi_bandwidth_t max_rx_rate;    /**< 最大下行速率 (bps) */
    lmi_latency_t typical_latency;  /**< 典型时延 (ms) */
    lmi_latency_t max_latency;      /**< 最大时延 (ms) */
    uint32_t mtu;                   /**< 最大传输单元 (bytes) */
    bool supports_multicast;        /**< 是否支持组播 */
    bool supports_qos;              /**< 是否支持 QoS */
} lmi_link_capability_t;

/**
 * 链路策略属性 (Policy Attributes)
 * 用于 CM 进行链路选择决策
 */
typedef struct {
    uint32_t cost_index;            /**< 成本指数 (0-100，越高越贵) */
    lmi_security_level_e security;  /**< 安全等级 */
    lmi_coverage_e coverage;        /**< 覆盖范围 */
    uint32_t priority;              /**< 优先级 (1-10，越大优先级越高) */
} lmi_policy_attr_t;

/**
 * 资源请求参数 (Resource Request)
 * ARINC 839 Section 4.2.3.2 - Link_Resource
 */
typedef struct {
    lmi_session_id_t session_id;    /**< 会话标识 */
    lmi_resource_action_e action;   /**< 操作类型 */
    
    /* 带宽需求 */
    lmi_bandwidth_t min_tx_rate;    /**< 最小上行速率 */
    lmi_bandwidth_t requested_tx_rate; /**< 期望上行速率 */
    lmi_bandwidth_t min_rx_rate;    /**< 最小下行速率 */
    lmi_bandwidth_t requested_rx_rate; /**< 期望下行速率 */
    
    /* QoS 参数 */
    uint32_t qos_class;             /**< QoS 等级 (0-4) */
    uint32_t max_delay_ms;          /**< 最大可容忍时延 */
    float packet_loss_tolerance;    /**< 可容忍丢包率 (0.0-1.0) */
    
    /* 会话参数 */
    uint32_t timeout_sec;           /**< 会话超时时间 */
    bool persistent;                /**< 是否为持久连接 */
    char client_id[128];            /**< 发起请求的客户端 ID */
} lmi_resource_request_t;

/**
 * 资源响应 (Resource Response)
 */
typedef struct {
    lmi_session_id_t session_id;
    int result_code;                /**< 0=成功, 其他=错误码 */
    char error_message[256];
    
    /* 实际授予的资源 */
    lmi_bandwidth_t granted_tx_rate;
    lmi_bandwidth_t granted_rx_rate;
    uint32_t allocated_qos_class;
    
    /* 连接信息 */
    char local_ip[64];              /**< 分配的本地 IP 地址 */
    char gateway_ip[64];            /**< 网关地址 */
    char dns_primary[64];           /**< 主 DNS */
    char dns_secondary[64];         /**< 备用 DNS */
} lmi_resource_response_t;

/**
 * 链路事件数据 (Link Event Data)
 */
typedef struct {
    lmi_link_id_t link_id;
    lmi_event_type_e event_type;
    lmi_link_state_e new_state;
    lmi_link_state_e old_state;
    
    uint64_t timestamp;             /**< Unix 时间戳 (ms) */
    char message[512];              /**< 事件描述信息 */
    
    /* 扩展数据（可选） */
    union {
        struct {
            int signal_strength;    /**< 信号强度 (dBm) */
            int signal_quality;     /**< 信号质量 (0-100) */
        } quality;
        
        struct {
            char recommended_link[64]; /**< 建议切换的目标链路 */
            int handover_reason;
        } handover;
        
        struct {
            int error_code;
            char error_details[256];
        } error;
    } ext;
} lmi_link_event_t;

/**
 * 链路统计信息 (Link Statistics)
 */
typedef struct {
    uint64_t bytes_transmitted;
    uint64_t bytes_received;
    uint64_t packets_transmitted;
    uint64_t packets_received;
    uint64_t errors_tx;
    uint64_t errors_rx;
    uint64_t drops_tx;
    uint64_t drops_rx;
    uint32_t current_tx_rate_kbps;
    uint32_t current_rx_rate_kbps;
    uint64_t uptime_seconds;
} lmi_link_stats_t;

/**
 * 链路信息结构体 (Link Info)
 * DLM 注册时提供的完整链路描述
 */
typedef struct {
    lmi_link_id_t link_id;          /**< 链路唯一标识 */
    lmi_driver_id_t driver_id;      /**< DLM 驱动 ID */
    char link_name[128];            /**< 人类可读名称 */
    lmi_link_type_e link_type;      /**< 链路类型 */
    char interface_name[32];        /**< 网卡接口名 (如 eth1) */
    
    lmi_link_capability_t capability; /**< 链路能力 */
    lmi_policy_attr_t policy;       /**< 策略属性 */
    lmi_link_state_e state;         /**< 当前状态 */
} lmi_link_info_t;

/* ========================================================================
 * 4. 回调函数类型定义 (Callback Types)
 * ======================================================================== */

/**
 * 链路事件回调函数
 * DLM 通过此回调向 CM 上报异步事件
 * 
 * @param event 事件数据
 * @param user_data 用户自定义数据指针
 */
typedef void (*lmi_event_callback_t)(const lmi_link_event_t *event, void *user_data);

/**
 * 日志回调函数
 * 
 * @param level 日志级别 (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
 * @param message 日志内容
 * @param user_data 用户自定义数据指针
 */
typedef void (*lmi_log_callback_t)(int level, const char *message, void *user_data);

/* ========================================================================
 * 5. LMI 操作接口定义 (LMI Operations Interface)
 * ======================================================================== */

/**
 * LMI 操作函数表 - 这是 DLM 必须实现的标准接口
 * 
 * 每个 DLM 驱动必须提供此结构体的完整实现，CM 通过函数指针调用 DLM 的具体功能。
 * 这种设计模式实现了"依赖倒置"：CM 依赖抽象接口，而不是具体 DLM 实现。
 */
typedef struct lmi_operations {
    /**
     * 初始化 DLM
     * 
     * @param config_file 配置文件路径（从 Datalink_Profile.xml 解析的参数）
     * @param event_cb 事件回调函数
     * @param log_cb 日志回调函数
     * @param user_data 回调函数的用户数据
     * @return 0=成功, <0=错误码
     */
    int (*init)(const char *config_file, 
                lmi_event_callback_t event_cb,
                lmi_log_callback_t log_cb,
                void *user_data);
    
    /**
     * 注册链路到 CM
     * DLM 启动后调用此函数向 CM 注册自己的能力
     * 
     * @param link_info 链路信息结构体
     * @return 0=成功, <0=错误码
     */
    int (*register_link)(lmi_link_info_t *link_info);
    
    /**
     * 发现链路能力（能力协商）
     * ARINC 839 Primitive: Link_Capability_Discover
     * 
     * @param link_id 链路标识
     * @param capability 输出：链路能力参数
     * @return 0=成功, <0=错误码
     */
    int (*discover_capability)(const lmi_link_id_t link_id,
                               lmi_link_capability_t *capability);
    
    /**
     * 资源请求（连接建立/修改/释放）
     * ARINC 839 Primitive: Link_Resource
     * 
     * 这是 LMI 的核心接口，处理所有资源管理操作：
     * - ALLOCATE: 建立新连接（如激活卫星 PDP Context）
     * - MODIFY: 修改带宽或 QoS
     * - RELEASE: 释放连接
     * 
     * @param link_id 链路标识
     * @param request 资源请求参数
     * @param response 输出：资源响应结果
     * @return 0=成功, <0=错误码
     */
    int (*request_resource)(const lmi_link_id_t link_id,
                            const lmi_resource_request_t *request,
                            lmi_resource_response_t *response);
    
    /**
     * 获取链路状态
     * ARINC 839 Primitive: Link_Get_Parameters
     * 
     * @param link_id 链路标识
     * @param state 输出：当前链路状态
     * @return 0=成功, <0=错误码
     */
    int (*get_state)(const lmi_link_id_t link_id, lmi_link_state_e *state);
    
    /**
     * 获取链路统计信息
     * 
     * @param link_id 链路标识
     * @param stats 输出：统计数据
     * @return 0=成功, <0=错误码
     */
    int (*get_statistics)(const lmi_link_id_t link_id, lmi_link_stats_t *stats);
    
    /**
     * 暂停链路（保留资源但停止传输）
     * ARINC 839 Primitive: Link_Action (SUSPEND)
     * 
     * @param link_id 链路标识
     * @return 0=成功, <0=错误码
     */
    int (*suspend_link)(const lmi_link_id_t link_id);
    
    /**
     * 恢复链路
     * ARINC 839 Primitive: Link_Action (RESUME)
     * 
     * @param link_id 链路标识
     * @return 0=成功, <0=错误码
     */
    int (*resume_link)(const lmi_link_id_t link_id);
    
    /**
     * 执行链路健康检查
     * 周期性调用以监控硬件状态
     * 
     * @param link_id 链路标识
     * @return 0=健康, >0=警告, <0=故障
     */
    int (*health_check)(const lmi_link_id_t link_id);
    
    /**
     * 清理并关闭 DLM
     * 
     * @return 0=成功, <0=错误码
     */
    int (*shutdown)(void);
    
    /* 私有数据指针，DLM 可用于存储内部状态 */
    void *private_data;
    
} lmi_operations_t;

/* ========================================================================
 * 6. 错误码定义 (Error Codes)
 * ======================================================================== */

#define LMI_SUCCESS             0       /**< 操作成功 */
#define LMI_ERR_INVALID_PARAM   -1      /**< 无效参数 */
#define LMI_ERR_NOT_INITIALIZED -2      /**< DLM 未初始化 */
#define LMI_ERR_LINK_NOT_FOUND  -3      /**< 链路不存在 */
#define LMI_ERR_RESOURCE_BUSY   -4      /**< 资源已被占用 */
#define LMI_ERR_RESOURCE_UNAVAIL -5     /**< 资源不可用 */
#define LMI_ERR_TIMEOUT         -6      /**< 操作超时 */
#define LMI_ERR_HARDWARE_FAILURE -7     /**< 硬件故障 */
#define LMI_ERR_INSUFFICIENT_BW -8      /**< 带宽不足 */
#define LMI_ERR_AUTH_FAILED     -9      /**< 认证失败 */
#define LMI_ERR_NETWORK_ERROR   -10     /**< 网络错误 */
#define LMI_ERR_NOT_SUPPORTED   -11     /**< 功能不支持 */
#define LMI_ERR_INTERNAL        -99     /**< 内部错误 */

/* ========================================================================
 * 7. 辅助函数声明 (Helper Functions)
 * ======================================================================== */

/**
 * 将链路类型枚举转换为字符串
 */
const char* lmi_link_type_to_string(lmi_link_type_e type);

/**
 * 将链路状态枚举转换为字符串
 */
const char* lmi_link_state_to_string(lmi_link_state_e state);

/**
 * 将事件类型枚举转换为字符串
 */
const char* lmi_event_type_to_string(lmi_event_type_e type);

/**
 * 将错误码转换为人类可读字符串
 */
const char* lmi_error_to_string(int error_code);

/**
 * 生成唯一的会话 ID
 */
lmi_session_id_t lmi_generate_session_id(void);

/**
 * 获取当前时间戳（毫秒）
 */
uint64_t lmi_get_timestamp_ms(void);

#endif /* MAGIC_LMI_H */
