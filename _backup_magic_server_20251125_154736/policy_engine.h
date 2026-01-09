/**
 * @file policy_engine.h
 * @brief MAGIC 策略引擎 - 基于配置的路径选择和流量管理
 * @description 
 *   - 飞行阶段管理和自动切换
 *   - 基于策略规则的路径选择算法
 *   - 流量类别分类和映射
 *   - 可扩展的策略评估框架
 */

#ifndef POLICY_ENGINE_H
#define POLICY_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "xml_config_parser.h"

/*===========================================================================
 * 常量定义
 *===========================================================================*/

#define MAX_ACTIVE_LINKS            10
#define MAX_SELECTED_PATHS          5
#define MAX_TRAFFIC_CLASSES         20

/*===========================================================================
 * 飞行阶段枚举
 *===========================================================================*/

typedef enum {
    FLIGHT_PHASE_PARKED        = 1,
    FLIGHT_PHASE_TAXI          = 2,
    FLIGHT_PHASE_TAKEOFF       = 3,
    FLIGHT_PHASE_CLIMB         = 4,
    FLIGHT_PHASE_CRUISE        = 5,
    FLIGHT_PHASE_OCEANIC       = 6,
    FLIGHT_PHASE_DESCENT       = 7,
    FLIGHT_PHASE_APPROACH      = 8,
    FLIGHT_PHASE_LANDING       = 9,
    FLIGHT_PHASE_UNKNOWN       = 0
} FlightPhase;

/*===========================================================================
 * 流量类别枚举
 *===========================================================================*/

typedef enum {
    TRAFFIC_CLASS_FLIGHT_CRITICAL     = 1,   // 飞行关键数据
    TRAFFIC_CLASS_COCKPIT_DATA        = 2,   // 驾驶舱数据
    TRAFFIC_CLASS_CABIN_OPERATIONS    = 3,   // 客舱运营数据
    TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT = 4, // 乘客娱乐
    TRAFFIC_CLASS_BULK_DATA           = 5,   // 大容量数据
    TRAFFIC_CLASS_ACARS_COMMS         = 6,   // ACARS 通信
    TRAFFIC_CLASS_ALL_TRAFFIC         = 99,  // 所有流量
    TRAFFIC_CLASS_UNKNOWN             = 0
} TrafficClass;

/*===========================================================================
 * 链路状态信息
 *===========================================================================*/

typedef struct {
    char link_id[MAX_ID_LEN];
    bool is_up;                         // 链路是否在线
    uint32_t available_bandwidth_kbps;  // 可用带宽
    uint32_t current_load_kbps;         // 当前负载
    uint32_t rtt_ms;                    // 往返时延
    float loss_rate;                    // 丢包率 (0.0-1.0)
    time_t last_update;                 // 最后更新时间
    
    // 从配置文件获取的属性
    DatalinkProfile* config;            // 指向配置的指针
} LinkState;

/*===========================================================================
 * 路径选择结果
 *===========================================================================*/

typedef struct {
    char link_id[MAX_ID_LEN];
    uint32_t preference_ranking;        // 优先级排名
    PolicyAction action;                // PERMIT/PROHIBIT
    bool is_available;                  // 链路是否可用
    uint32_t score;                     // 综合评分 (用于动态选择)
    
    // 评分因子
    struct {
        uint32_t bandwidth_score;       // 带宽评分
        uint32_t latency_score;         // 延迟评分
        uint32_t cost_score;            // 成本评分
        uint32_t reliability_score;     // 可靠性评分
    } metrics;
} PathSelectionResult;

typedef struct {
    TrafficClass traffic_class;
    uint32_t num_paths;
    PathSelectionResult paths[MAX_SELECTED_PATHS];
    
    // 选择的最佳路径
    char selected_link_id[MAX_ID_LEN];
    bool selection_valid;
    
    time_t selection_time;
    char reason[128];                   // 选择原因说明
} PathSelectionDecision;

/*===========================================================================
 * 策略引擎上下文
 *===========================================================================*/

typedef struct {
    // 配置引用
    MagicConfig* config;                // XML 配置
    
    // 当前飞行阶段
    FlightPhase current_phase;
    char current_phase_str[32];
    time_t phase_change_time;
    PolicyRuleSet* active_ruleset;      // 当前激活的策略规则集
    
    // 链路状态
    LinkState link_states[MAX_ACTIVE_LINKS];
    uint32_t num_links;
    
    // 统计信息
    struct {
        uint64_t total_decisions;       // 总决策次数
        uint64_t phase_switches;        // 阶段切换次数
        uint64_t path_selections;       // 路径选择次数
        time_t engine_start_time;       // 引擎启动时间
    } stats;
    
} PolicyEngineContext;

// 扩展点：自定义策略评估函数类型
typedef int (*CustomPolicyEvaluator)(PolicyEngineContext* ctx, 
                                     TrafficClass traffic_class,
                                     PathSelectionDecision* decision);

/*===========================================================================
 * 策略引擎 API
 *===========================================================================*/

/**
 * @brief 初始化策略引擎
 * @param ctx 策略引擎上下文
 * @param config XML 配置
 * @return 0=成功, -1=失败
 */
int policy_engine_init(PolicyEngineContext* ctx, MagicConfig* config);

/**
 * @brief 更新飞行阶段
 * @param ctx 策略引擎上下文
 * @param new_phase 新的飞行阶段
 * @return 0=成功, -1=失败
 */
int policy_engine_set_flight_phase(PolicyEngineContext* ctx, FlightPhase new_phase);

/**
 * @brief 更新链路状态
 * @param ctx 策略引擎上下文
 * @param link_id 链路 ID
 * @param is_up 链路是否在线
 * @param bandwidth_kbps 可用带宽
 * @param rtt_ms 往返时延
 * @return 0=成功, -1=失败
 */
int policy_engine_update_link_state(PolicyEngineContext* ctx,
                                    const char* link_id,
                                    bool is_up,
                                    uint32_t bandwidth_kbps,
                                    uint32_t rtt_ms);

/**
 * @brief 根据流量类别选择最佳路径
 * @param ctx 策略引擎上下文
 * @param traffic_class 流量类别
 * @param decision 输出的路径选择决策
 * @return 0=成功, -1=失败
 */
int policy_engine_select_path(PolicyEngineContext* ctx,
                              TrafficClass traffic_class,
                              PathSelectionDecision* decision);

/**
 * @brief 根据客户端 ID 映射流量类别
 * @param ctx 策略引擎上下文
 * @param client_id 客户端 ID
 * @return 流量类别
 */
TrafficClass policy_engine_map_client_to_traffic_class(PolicyEngineContext* ctx,
                                                       const char* client_id);

/**
 * @brief 根据 Diameter 应用 ID 映射流量类别
 * @param ctx 策略引擎上下文
 * @param app_id Diameter 应用 ID
 * @return 流量类别
 */
TrafficClass policy_engine_map_diameter_app_to_traffic_class(PolicyEngineContext* ctx,
                                                             uint32_t app_id);

/**
 * @brief 评估路径是否可用
 * @param ctx 策略引擎上下文
 * @param link_id 链路 ID
 * @param traffic_class 流量类别
 * @return true=可用, false=不可用
 */
bool policy_engine_is_path_available(PolicyEngineContext* ctx,
                                    const char* link_id,
                                    TrafficClass traffic_class);

/**
 * @brief 获取当前飞行阶段字符串
 * @param phase 飞行阶段枚举
 * @return 阶段字符串
 */
const char* policy_engine_get_phase_string(FlightPhase phase);

/**
 * @brief 获取流量类别字符串
 * @param traffic_class 流量类别枚举
 * @return 类别字符串
 */
const char* policy_engine_get_traffic_class_string(TrafficClass traffic_class);

/**
 * @brief 解析飞行阶段字符串
 * @param phase_str 阶段字符串 (如 "PARKED", "CRUISE")
 * @return 飞行阶段枚举
 */
FlightPhase policy_engine_parse_phase_string(const char* phase_str);

/**
 * @brief 解析流量类别字符串
 * @param class_str 类别字符串 (如 "BULK_DATA")
 * @return 流量类别枚举
 */
TrafficClass policy_engine_parse_traffic_class_string(const char* class_str);

/**
 * @brief 打印策略引擎状态
 * @param ctx 策略引擎上下文
 */
void policy_engine_print_status(const PolicyEngineContext* ctx);

/**
 * @brief 打印路径选择决策
 * @param decision 路径选择决策
 */
void policy_engine_print_decision(const PathSelectionDecision* decision);

/**
 * @brief 销毁策略引擎
 * @param ctx 策略引擎上下文
 */
void policy_engine_destroy(PolicyEngineContext* ctx);

/*===========================================================================
 * 扩展接口：自定义策略评估器
 *===========================================================================*/

/**
 * @brief 注册自定义策略评估函数
 * @param ctx 策略引擎上下文
 * @param evaluator 自定义评估函数指针
 */
void policy_engine_register_custom_evaluator(
    PolicyEngineContext* ctx,
    CustomPolicyEvaluator evaluator);

#endif /* POLICY_ENGINE_H */
