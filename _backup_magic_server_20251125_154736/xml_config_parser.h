/**
 * @file xml_config_parser.h
 * @brief MAGIC XML 配置文件解析器
 * @description 解析三个核心 XML 配置文件：
 *   - Datalink_Profile.xml: 链路配置
 *   - Central_Policy_Profile.xml: 策略规则
 *   - Client_Profile.xml: 客户端配置
 */

#ifndef XML_CONFIG_PARSER_H
#define XML_CONFIG_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*===========================================================================
 * 配置文件路径
 * 说明：支持多种路径查找顺序
 *   1. 当前目录（build/ 目录运行时）
 *   2. ../config/ （从 build/ 访问上级目录）
 *   3. magic_server/config/ （从项目根目录运行时）
 *===========================================================================*/

#ifndef CONFIG_DIR
#define CONFIG_DIR                  "../config"
#endif

#define DATALINK_PROFILE_FILE       CONFIG_DIR "/Datalink_Profile.xml"
#define POLICY_PROFILE_FILE         CONFIG_DIR "/Central_Policy_Profile.xml"
#define CLIENT_PROFILE_FILE         CONFIG_DIR "/Client_Profile.xml"

#define MAX_LINKS                   10
#define MAX_CLIENTS                 50
#define MAX_POLICY_RULESETS         10
#define MAX_RULES_PER_RULESET       20
#define MAX_PATH_PREFERENCES        5

#define MAX_ID_LEN                  64
#define MAX_NAME_LEN                128
#define MAX_IP_STR_LEN              64
#define MAX_PORTLIST_LEN            256

/*===========================================================================
 * 数据链路配置（Datalink_Profile.xml）
 *===========================================================================*/

typedef enum {
    LINK_TYPE_SATELLITE = 1,
    LINK_TYPE_CELLULAR  = 2,
    LINK_TYPE_GATELINK  = 3,
    LINK_TYPE_UNKNOWN   = 0
} LinkType;

typedef enum {
    COVERAGE_GLOBAL      = 1,
    COVERAGE_TERRESTRIAL = 2,
    COVERAGE_GATE_ONLY   = 3,
    COVERAGE_UNKNOWN     = 0
} Coverage;

typedef enum {
    SECURITY_HIGH   = 3,
    SECURITY_MEDIUM = 2,
    SECURITY_LOW    = 1,
    SECURITY_NONE   = 0
} SecurityLevel;

typedef struct {
    uint32_t max_tx_rate_kbps;      // 最大发送速率
    uint32_t typical_latency_ms;    // 典型延迟
} LinkCapabilities;

typedef struct {
    uint32_t cost_index;            // 成本指数 (1-100)
    SecurityLevel security_level;   // 安全级别
    Coverage coverage;              // 覆盖范围
} PolicyAttributes;

typedef struct {
    char link_id[MAX_ID_LEN];           // 链路 ID: "LINK_SATCOM"
    char link_name[MAX_NAME_LEN];       // 链路名称
    char dlm_driver_id[MAX_ID_LEN];     // DLM 驱动 ID
    char interface_name[16];            // 网卡名称: "eth1"
    LinkType type;                      // 链路类型
    
    LinkCapabilities capabilities;      // 链路能力
    PolicyAttributes policy_attrs;      // 策略属性
    
    bool is_active;                     // 运行时状态（非 XML）
} DatalinkProfile;

/*===========================================================================
 * 中央策略配置（Central_Policy_Profile.xml）
 *===========================================================================*/

typedef enum {
    ACTION_PERMIT   = 1,
    ACTION_PROHIBIT = 2,
    ACTION_DEFAULT  = 0
} PolicyAction;

typedef struct {
    uint32_t ranking;                   // 优先级排名 (1, 2, 3...)
    char link_id[MAX_ID_LEN];           // 链路 ID
    PolicyAction action;                // 动作 (PERMIT/PROHIBIT)
    char security_required[32];         // 安全要求 (VPN/TLS)
} PathPreference;

typedef struct {
    char traffic_class[MAX_ID_LEN];     // 流量类别
    PathPreference preferences[MAX_PATH_PREFERENCES];
    uint32_t num_preferences;
} PolicyRule;

typedef struct {
    char ruleset_id[MAX_ID_LEN];        // 规则集 ID: "GROUND_OPS"
    char flight_phases[MAX_NAME_LEN];   // 飞行阶段: "PARKED, TAXI"
    
    PolicyRule rules[MAX_RULES_PER_RULESET];
    uint32_t num_rules;
} PolicyRuleSet;

typedef struct {
    char available_links[MAX_LINKS][MAX_ID_LEN];
    uint32_t num_links;
    
    PolicyRuleSet rulesets[MAX_POLICY_RULESETS];
    uint32_t num_rulesets;
} CentralPolicyProfile;

/*===========================================================================
 * 客户端配置（Client_Profile.xml）
 *===========================================================================*/

typedef enum {
    AUTH_MAGIC_AWARE = 1,   // MAGIC 感知客户端（用户名/密码）
    AUTH_NON_AWARE   = 2,   // 非感知客户端（IP/端口过滤）
    AUTH_UNKNOWN     = 0
} AuthenticationType;

typedef struct {
    AuthenticationType type;
    
    // MAGIC_AWARE 认证
    char username[MAX_ID_LEN];
    char password[MAX_ID_LEN];
    char primary_key[MAX_ID_LEN];
    
    // NON_AWARE IP/端口过滤
    char source_ip[MAX_IP_STR_LEN];
    char dest_ip_port[MAX_IP_STR_LEN];
    char dest_port_list[MAX_PORTLIST_LEN];
} Authentication;

typedef struct {
    char hardware_type[MAX_ID_LEN];
    char system_role[MAX_ID_LEN];
    char aircraft_app_id[MAX_ID_LEN];
} ClientMetadata;

typedef struct {
    uint32_t max_session_bw_kbps;       // 单会话最大带宽
    uint32_t total_client_bw_kbps;      // 客户端总带宽
    uint32_t max_concurrent_sessions;   // 最大并发会话数
} ClientLimits;

typedef struct {
    char client_id[MAX_ID_LEN];         // 客户端 ID: "EFB_NAV_APP_01"
    
    Authentication auth;                // 认证信息
    ClientMetadata metadata;            // 元数据
    ClientLimits limits;                // 限制配置
    
    char traffic_class_id[MAX_ID_LEN]; // 流量类别映射
    
    bool is_online;                     // 运行时状态（非 XML）
} ClientProfile;

/*===========================================================================
 * 全局配置管理器
 *===========================================================================*/

typedef struct {
    // 数据链路配置
    DatalinkProfile datalinks[MAX_LINKS];
    uint32_t num_datalinks;
    
    // 策略配置
    CentralPolicyProfile policy;
    
    // 客户端配置
    ClientProfile clients[MAX_CLIENTS];
    uint32_t num_clients;
    
    // 加载时间戳
    time_t load_time;
    bool is_loaded;
} MagicConfig;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化配置管理器
 */
void magic_config_init(MagicConfig* config);

/**
 * @brief 加载所有 XML 配置文件
 * @param config 配置管理器指针
 * @return 0=成功, -1=失败
 */
int magic_config_load_all(MagicConfig* config);

/**
 * @brief 加载数据链路配置
 * @param config 配置管理器
 * @param filepath XML 文件路径
 * @return 0=成功, -1=失败
 */
int magic_config_load_datalinks(MagicConfig* config, const char* filepath);

/**
 * @brief 加载策略配置
 * @param config 配置管理器
 * @param filepath XML 文件路径
 * @return 0=成功, -1=失败
 */
int magic_config_load_policy(MagicConfig* config, const char* filepath);

/**
 * @brief 加载客户端配置
 * @param config 配置管理器
 * @param filepath XML 文件路径
 * @return 0=成功, -1=失败
 */
int magic_config_load_clients(MagicConfig* config, const char* filepath);

/**
 * @brief 根据 ID 查找数据链路
 */
DatalinkProfile* magic_config_find_datalink(MagicConfig* config, const char* link_id);

/**
 * @brief 根据 ID 查找客户端
 */
ClientProfile* magic_config_find_client(MagicConfig* config, const char* client_id);

/**
 * @brief 根据飞行阶段查找策略规则集
 */
PolicyRuleSet* magic_config_find_ruleset(MagicConfig* config, const char* flight_phase);

/**
 * @brief 打印配置摘要（调试用）
 */
void magic_config_print_summary(const MagicConfig* config);

/**
 * @brief 释放配置资源
 */
void magic_config_cleanup(MagicConfig* config);

#endif /* XML_CONFIG_PARSER_H */
