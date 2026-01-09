/**
 * @file policy_engine.c
 * @brief MAGIC 策略引擎实现
 */

#include "policy_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================
 * 辅助函数：字符串解析和转换
 *===========================================================================*/

const char* policy_engine_get_phase_string(FlightPhase phase) {
    switch (phase) {
        case FLIGHT_PHASE_PARKED:    return "PARKED";
        case FLIGHT_PHASE_TAXI:      return "TAXI";
        case FLIGHT_PHASE_TAKEOFF:   return "TAKEOFF";
        case FLIGHT_PHASE_CLIMB:     return "CLIMB";
        case FLIGHT_PHASE_CRUISE:    return "CRUISE";
        case FLIGHT_PHASE_OCEANIC:   return "OCEANIC";
        case FLIGHT_PHASE_DESCENT:   return "DESCENT";
        case FLIGHT_PHASE_APPROACH:  return "APPROACH";
        case FLIGHT_PHASE_LANDING:   return "LANDING";
        default:                     return "UNKNOWN";
    }
}

const char* policy_engine_get_traffic_class_string(TrafficClass traffic_class) {
    switch (traffic_class) {
        case TRAFFIC_CLASS_FLIGHT_CRITICAL:        return "FLIGHT_CRITICAL";
        case TRAFFIC_CLASS_COCKPIT_DATA:           return "COCKPIT_DATA";
        case TRAFFIC_CLASS_CABIN_OPERATIONS:       return "CABIN_OPERATIONS";
        case TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT: return "PASSENGER_ENTERTAINMENT";
        case TRAFFIC_CLASS_BULK_DATA:              return "BULK_DATA";
        case TRAFFIC_CLASS_ACARS_COMMS:            return "ACARS_COMMS";
        case TRAFFIC_CLASS_ALL_TRAFFIC:            return "ALL_TRAFFIC";
        default:                                   return "UNKNOWN";
    }
}

FlightPhase policy_engine_parse_phase_string(const char* phase_str) {
    if (!phase_str) return FLIGHT_PHASE_UNKNOWN;
    
    if (strcmp(phase_str, "PARKED") == 0)    return FLIGHT_PHASE_PARKED;
    if (strcmp(phase_str, "TAXI") == 0)      return FLIGHT_PHASE_TAXI;
    if (strcmp(phase_str, "TAKEOFF") == 0)   return FLIGHT_PHASE_TAKEOFF;
    if (strcmp(phase_str, "CLIMB") == 0)     return FLIGHT_PHASE_CLIMB;
    if (strcmp(phase_str, "CRUISE") == 0)    return FLIGHT_PHASE_CRUISE;
    if (strcmp(phase_str, "OCEANIC") == 0)   return FLIGHT_PHASE_OCEANIC;
    if (strcmp(phase_str, "DESCENT") == 0)   return FLIGHT_PHASE_DESCENT;
    if (strcmp(phase_str, "APPROACH") == 0)  return FLIGHT_PHASE_APPROACH;
    if (strcmp(phase_str, "LANDING") == 0)   return FLIGHT_PHASE_LANDING;
    
    return FLIGHT_PHASE_UNKNOWN;
}

TrafficClass policy_engine_parse_traffic_class_string(const char* class_str) {
    if (!class_str) return TRAFFIC_CLASS_UNKNOWN;
    
    if (strcmp(class_str, "FLIGHT_CRITICAL") == 0)        return TRAFFIC_CLASS_FLIGHT_CRITICAL;
    if (strcmp(class_str, "COCKPIT_DATA") == 0)           return TRAFFIC_CLASS_COCKPIT_DATA;
    if (strcmp(class_str, "CABIN_OPERATIONS") == 0)       return TRAFFIC_CLASS_CABIN_OPERATIONS;
    if (strcmp(class_str, "PASSENGER_ENTERTAINMENT") == 0) return TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT;
    if (strcmp(class_str, "BULK_DATA") == 0)              return TRAFFIC_CLASS_BULK_DATA;
    if (strcmp(class_str, "ACARS_COMMS") == 0)            return TRAFFIC_CLASS_ACARS_COMMS;
    if (strcmp(class_str, "ALL_TRAFFIC") == 0)            return TRAFFIC_CLASS_ALL_TRAFFIC;
    
    return TRAFFIC_CLASS_UNKNOWN;
}

/*===========================================================================
 * 初始化和销毁
 *===========================================================================*/

int policy_engine_init(PolicyEngineContext* ctx, MagicConfig* config) {
    if (!ctx || !config) {
        fprintf(stderr, "[POLICY] Error: NULL context or config\n");
        return -1;
    }
    
    memset(ctx, 0, sizeof(PolicyEngineContext));
    
    ctx->config = config;
    ctx->current_phase = FLIGHT_PHASE_PARKED;  // 默认停机阶段
    strcpy(ctx->current_phase_str, "PARKED");
    ctx->phase_change_time = time(NULL);
    ctx->stats.engine_start_time = time(NULL);
    
    // 初始化链路状态（从配置加载）
    ctx->num_links = config->num_datalinks;
    for (uint32_t i = 0; i < config->num_datalinks && i < MAX_ACTIVE_LINKS; i++) {
        LinkState* state = &ctx->link_states[i];
        DatalinkProfile* profile = &config->datalinks[i];
        
        strcpy(state->link_id, profile->link_id);
        state->is_up = false;  // 初始为离线
        state->available_bandwidth_kbps = profile->capabilities.max_tx_rate_kbps;
        state->current_load_kbps = 0;
        state->rtt_ms = profile->capabilities.typical_latency_ms;
        state->loss_rate = 0.0f;
        state->last_update = time(NULL);
        state->config = profile;
    }
    
    // 设置初始策略规则集
    ctx->active_ruleset = magic_config_find_ruleset(config, "PARKED");
    
    printf("[POLICY] ✓ Policy Engine Initialized\n");
    printf("[POLICY]   Initial Phase: %s\n", ctx->current_phase_str);
    printf("[POLICY]   Tracked Links: %u\n", ctx->num_links);
    if (ctx->active_ruleset) {
        printf("[POLICY]   Active RuleSet: %s (%u rules)\n", 
               ctx->active_ruleset->ruleset_id, 
               ctx->active_ruleset->num_rules);
    }
    
    return 0;
}

void policy_engine_destroy(PolicyEngineContext* ctx) {
    if (!ctx) return;
    
    printf("[POLICY] Engine destroyed. Stats:\n");
    printf("[POLICY]   Total Decisions: %lu\n", ctx->stats.total_decisions);
    printf("[POLICY]   Phase Switches: %lu\n", ctx->stats.phase_switches);
    printf("[POLICY]   Path Selections: %lu\n", ctx->stats.path_selections);
    
    memset(ctx, 0, sizeof(PolicyEngineContext));
}

/*===========================================================================
 * 飞行阶段管理
 *===========================================================================*/

int policy_engine_set_flight_phase(PolicyEngineContext* ctx, FlightPhase new_phase) {
    if (!ctx) return -1;
    
    if (ctx->current_phase == new_phase) {
        return 0;  // 相同阶段，无需切换
    }
    
    FlightPhase old_phase = ctx->current_phase;
    const char* old_str = policy_engine_get_phase_string(old_phase);
    const char* new_str = policy_engine_get_phase_string(new_phase);
    
    printf("\n");
    printf("[POLICY] ========================================\n");
    printf("[POLICY]  Flight Phase Transition\n");
    printf("[POLICY] ========================================\n");
    printf("[POLICY]   %s → %s\n", old_str, new_str);
    
    // 更新阶段
    ctx->current_phase = new_phase;
    strncpy(ctx->current_phase_str, new_str, sizeof(ctx->current_phase_str) - 1);
    ctx->phase_change_time = time(NULL);
    ctx->stats.phase_switches++;
    
    // 查找对应的策略规则集
    PolicyRuleSet* new_ruleset = magic_config_find_ruleset(ctx->config, new_str);
    
    if (new_ruleset) {
        ctx->active_ruleset = new_ruleset;
        printf("[POLICY]   Active RuleSet: %s (%u rules)\n", 
               new_ruleset->ruleset_id, 
               new_ruleset->num_rules);
        
        // 打印规则摘要
        for (uint32_t i = 0; i < new_ruleset->num_rules; i++) {
            PolicyRule* rule = &new_ruleset->rules[i];
            printf("[POLICY]     Rule %u: %s traffic → ", 
                   i+1, rule->traffic_class);
            
            for (uint32_t j = 0; j < rule->num_preferences; j++) {
                PathPreference* pref = &rule->preferences[j];
                printf("%s", pref->link_id);
                if (pref->action == ACTION_PROHIBIT) {
                    printf("(PROHIBIT)");
                }
                if (j < rule->num_preferences - 1) {
                    printf(" → ");
                }
            }
            printf("\n");
        }
    } else {
        printf("[POLICY]   Warning: No ruleset found for phase %s\n", new_str);
        ctx->active_ruleset = NULL;
    }
    
    printf("[POLICY] ========================================\n\n");
    
    return 0;
}

/*===========================================================================
 * 链路状态更新
 *===========================================================================*/

int policy_engine_update_link_state(PolicyEngineContext* ctx,
                                    const char* link_id,
                                    bool is_up,
                                    uint32_t bandwidth_kbps,
                                    uint32_t rtt_ms) {
    if (!ctx || !link_id) return -1;
    
    // 查找链路状态
    LinkState* state = NULL;
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        if (strcmp(ctx->link_states[i].link_id, link_id) == 0) {
            state = &ctx->link_states[i];
            break;
        }
    }
    
    if (!state) {
        fprintf(stderr, "[POLICY] Error: Unknown link %s\n", link_id);
        return -1;
    }
    
    // 检测状态变化
    bool state_changed = (state->is_up != is_up);
    
    // 更新状态
    state->is_up = is_up;
    state->available_bandwidth_kbps = bandwidth_kbps;
    state->rtt_ms = rtt_ms;
    state->last_update = time(NULL);
    
    if (state_changed) {
        printf("[POLICY] Link %s: %s\n", 
               link_id, 
               is_up ? "UP ✓" : "DOWN ✗");
        printf("[POLICY]   Bandwidth: %u kbps\n", bandwidth_kbps);
        printf("[POLICY]   RTT: %u ms\n", rtt_ms);
    }
    
    return 0;
}

/*===========================================================================
 * 路径选择算法
 *===========================================================================*/

/**
 * @brief 计算路径评分（用于动态选择）
 */
static uint32_t calculate_path_score(LinkState* link_state, 
                                     PathPreference* preference) {
    if (!link_state || !link_state->is_up) {
        return 0;  // 链路不可用，评分为0
    }
    
    uint32_t score = 10000;  // 基础分
    
    // 优先级排名（排名越高，基础分越高）
    score += (10 - preference->ranking) * 2000;
    
    // 带宽评分（可用带宽越高越好）
    score += (link_state->available_bandwidth_kbps / 1000);
    
    // 延迟评分（延迟越低越好）
    if (link_state->rtt_ms > 0) {
        score += (1000 - link_state->rtt_ms);
    }
    
    // 成本评分（成本越低越好）
    if (link_state->config) {
        score += (100 - link_state->config->policy_attrs.cost_index) * 50;
    }
    
    // 负载评分（当前负载越低越好）
    uint32_t load_percent = 0;
    if (link_state->available_bandwidth_kbps > 0) {
        load_percent = (link_state->current_load_kbps * 100) / 
                       link_state->available_bandwidth_kbps;
    }
    score += (100 - load_percent) * 20;
    
    // 可靠性评分（丢包率越低越好）
    score += (uint32_t)((1.0f - link_state->loss_rate) * 1000);
    
    return score;
}

int policy_engine_select_path(PolicyEngineContext* ctx,
                              TrafficClass traffic_class,
                              PathSelectionDecision* decision) {
    if (!ctx || !decision) return -1;
    
    memset(decision, 0, sizeof(PathSelectionDecision));
    decision->traffic_class = traffic_class;
    decision->selection_time = time(NULL);
    
    ctx->stats.total_decisions++;
    
    // 检查当前是否有激活的策略规则集
    if (!ctx->active_ruleset) {
        snprintf(decision->reason, sizeof(decision->reason),
                "No active policy ruleset for phase %s", 
                ctx->current_phase_str);
        return -1;
    }
    
    // 查找匹配的策略规则
    PolicyRule* matching_rule = NULL;
    const char* traffic_str = policy_engine_get_traffic_class_string(traffic_class);
    
    for (uint32_t i = 0; i < ctx->active_ruleset->num_rules; i++) {
        PolicyRule* rule = &ctx->active_ruleset->rules[i];
        
        // 精确匹配或 ALL_TRAFFIC 匹配
        if (strcmp(rule->traffic_class, traffic_str) == 0 ||
            strcmp(rule->traffic_class, "ALL_TRAFFIC") == 0) {
            matching_rule = rule;
            break;
        }
    }
    
    if (!matching_rule) {
        snprintf(decision->reason, sizeof(decision->reason),
                "No policy rule for traffic class %s in phase %s",
                traffic_str, ctx->current_phase_str);
        return -1;
    }
    
    // 评估所有路径偏好
    decision->num_paths = 0;
    PathSelectionResult* best_path = NULL;
    uint32_t best_score = 0;
    
    for (uint32_t i = 0; i < matching_rule->num_preferences && 
                        i < MAX_SELECTED_PATHS; i++) {
        PathPreference* pref = &matching_rule->preferences[i];
        PathSelectionResult* result = &decision->paths[decision->num_paths];
        
        // 填充基本信息
        strcpy(result->link_id, pref->link_id);
        result->preference_ranking = pref->ranking;
        result->action = pref->action;
        
        // 查找链路状态
        LinkState* link_state = NULL;
        for (uint32_t j = 0; j < ctx->num_links; j++) {
            if (strcmp(ctx->link_states[j].link_id, pref->link_id) == 0) {
                link_state = &ctx->link_states[j];
                break;
            }
        }
        
        // 检查链路可用性
        result->is_available = false;
        if (link_state && link_state->is_up && pref->action != ACTION_PROHIBIT) {
            result->is_available = true;
        }
        
        // 计算评分
        if (result->is_available && link_state) {
            result->score = calculate_path_score(link_state, pref);
            
            // 更新最佳路径
            if (result->score > best_score) {
                best_score = result->score;
                best_path = result;
            }
        } else {
            result->score = 0;
        }
        
        decision->num_paths++;
    }
    
    // 选择最佳路径
    if (best_path) {
        strcpy(decision->selected_link_id, best_path->link_id);
        decision->selection_valid = true;
        ctx->stats.path_selections++;
        
        snprintf(decision->reason, sizeof(decision->reason),
                "Selected %s (score: %u, rank: %u) for %s in phase %s",
                best_path->link_id,
                best_path->score,
                best_path->preference_ranking,
                traffic_str,
                ctx->current_phase_str);
    } else {
        decision->selection_valid = false;
        snprintf(decision->reason, sizeof(decision->reason),
                "No available path for %s in phase %s",
                traffic_str,
                ctx->current_phase_str);
    }
    
    return decision->selection_valid ? 0 : -1;
}

/*===========================================================================
 * 流量类别映射
 *===========================================================================*/

TrafficClass policy_engine_map_client_to_traffic_class(PolicyEngineContext* ctx,
                                                       const char* client_id) {
    if (!ctx || !client_id) return TRAFFIC_CLASS_UNKNOWN;
    
    // 从配置中查找客户端
    ClientProfile* client = magic_config_find_client(ctx->config, client_id);
    if (!client) {
        return TRAFFIC_CLASS_UNKNOWN;
    }
    
    // 根据 system_role 映射到流量类别
    const char* role = client->metadata.system_role;
    
    if (strstr(role, "FLIGHT_CRITICAL")) {
        return TRAFFIC_CLASS_FLIGHT_CRITICAL;
    } else if (strstr(role, "ACARS")) {
        return TRAFFIC_CLASS_ACARS_COMMS;
    } else if (strstr(role, "CABIN_OPERATIONS")) {
        return TRAFFIC_CLASS_CABIN_OPERATIONS;
    } else if (strstr(role, "PASSENGER")) {
        return TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT;
    } else if (strstr(role, "BULK") || strstr(role, "DATA")) {
        return TRAFFIC_CLASS_BULK_DATA;
    }
    
    return TRAFFIC_CLASS_UNKNOWN;
}

TrafficClass policy_engine_map_diameter_app_to_traffic_class(PolicyEngineContext* ctx,
                                                             uint32_t app_id) {
    (void)ctx;  // 未使用
    
    // Diameter 应用 ID 映射
    // 参考: RFC 6733, 3GPP TS 29.272
    switch (app_id) {
        case 16777216:  // Diameter Credit Control (DCCA)
            return TRAFFIC_CLASS_FLIGHT_CRITICAL;
        case 16777251:  // S6a/S6d (LTE Authentication)
            return TRAFFIC_CLASS_FLIGHT_CRITICAL;
        case 16777238:  // Gx (Policy Control)
            return TRAFFIC_CLASS_COCKPIT_DATA;
        case 16777302:  // Sy (Policy)
            return TRAFFIC_CLASS_CABIN_OPERATIONS;
        default:
            return TRAFFIC_CLASS_ALL_TRAFFIC;
    }
}

/*===========================================================================
 * 路径可用性检查
 *===========================================================================*/

bool policy_engine_is_path_available(PolicyEngineContext* ctx,
                                    const char* link_id,
                                    TrafficClass traffic_class) {
    if (!ctx || !link_id) return false;
    
    // 检查链路是否在线
    LinkState* state = NULL;
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        if (strcmp(ctx->link_states[i].link_id, link_id) == 0) {
            state = &ctx->link_states[i];
            break;
        }
    }
    
    if (!state || !state->is_up) {
        return false;
    }
    
    // 检查策略是否允许
    if (!ctx->active_ruleset) {
        return false;
    }
    
    const char* traffic_str = policy_engine_get_traffic_class_string(traffic_class);
    
    for (uint32_t i = 0; i < ctx->active_ruleset->num_rules; i++) {
        PolicyRule* rule = &ctx->active_ruleset->rules[i];
        
        if (strcmp(rule->traffic_class, traffic_str) == 0 ||
            strcmp(rule->traffic_class, "ALL_TRAFFIC") == 0) {
            
            // 检查该链路是否在路径偏好中且未被禁止
            for (uint32_t j = 0; j < rule->num_preferences; j++) {
                PathPreference* pref = &rule->preferences[j];
                if (strcmp(pref->link_id, link_id) == 0) {
                    return (pref->action != ACTION_PROHIBIT);
                }
            }
        }
    }
    
    return false;
}

/*===========================================================================
 * 状态打印
 *===========================================================================*/

void policy_engine_print_status(const PolicyEngineContext* ctx) {
    if (!ctx) return;
    
    printf("\n");
    printf("========================================\n");
    printf("  POLICY ENGINE STATUS\n");
    printf("========================================\n");
    
    printf("Flight Phase: %s\n", ctx->current_phase_str);
    
    if (ctx->active_ruleset) {
        printf("Active RuleSet: %s (%u rules)\n", 
               ctx->active_ruleset->ruleset_id,
               ctx->active_ruleset->num_rules);
    } else {
        printf("Active RuleSet: None\n");
    }
    
    printf("\nLink States:\n");
    for (uint32_t i = 0; i < ctx->num_links; i++) {
        const LinkState* state = &ctx->link_states[i];
        printf("  [%u] %s: %s\n", 
               i+1, 
               state->link_id, 
               state->is_up ? "UP" : "DOWN");
        if (state->is_up) {
            printf("      Bandwidth: %u kbps (load: %u kbps)\n",
                   state->available_bandwidth_kbps,
                   state->current_load_kbps);
            printf("      RTT: %u ms, Loss: %.2f%%\n",
                   state->rtt_ms,
                   state->loss_rate * 100);
        }
    }
    
    printf("\nStatistics:\n");
    printf("  Total Decisions: %lu\n", ctx->stats.total_decisions);
    printf("  Phase Switches: %lu\n", ctx->stats.phase_switches);
    printf("  Path Selections: %lu\n", ctx->stats.path_selections);
    
    time_t uptime = time(NULL) - ctx->stats.engine_start_time;
    printf("  Uptime: %ld seconds\n", uptime);
    
    printf("========================================\n\n");
}

void policy_engine_print_decision(const PathSelectionDecision* decision) {
    if (!decision) return;
    
    const char* traffic_str = policy_engine_get_traffic_class_string(decision->traffic_class);
    
    printf("\n[POLICY] ========================================\n");
    printf("[POLICY]  Path Selection Decision\n");
    printf("[POLICY] ========================================\n");
    printf("[POLICY]   Traffic Class: %s\n", traffic_str);
    printf("[POLICY]   Evaluated Paths: %u\n", decision->num_paths);
    
    for (uint32_t i = 0; i < decision->num_paths; i++) {
        const PathSelectionResult* path = &decision->paths[i];
        printf("[POLICY]     [%u] %s (rank %u): ", 
               i+1, path->link_id, path->preference_ranking);
        
        if (path->action == ACTION_PROHIBIT) {
            printf("PROHIBIT ✗\n");
        } else if (!path->is_available) {
            printf("UNAVAILABLE ✗\n");
        } else {
            printf("Available (score: %u)", path->score);
            if (strcmp(path->link_id, decision->selected_link_id) == 0) {
                printf(" ← SELECTED ✓");
            }
            printf("\n");
        }
    }
    
    printf("[POLICY]\n");
    if (decision->selection_valid) {
        printf("[POLICY]   ✓ Selected Link: %s\n", decision->selected_link_id);
    } else {
        printf("[POLICY]   ✗ No Available Path\n");
    }
    printf("[POLICY]   Reason: %s\n", decision->reason);
    printf("[POLICY] ========================================\n\n");
}

/*===========================================================================
 * 扩展接口
 *===========================================================================*/

void policy_engine_register_custom_evaluator(
    PolicyEngineContext* ctx,
    CustomPolicyEvaluator evaluator) {
    if (!ctx) return;
    
    // TODO: 将来实现自定义评估器支持
    (void)evaluator;  // 暂时未使用
    printf("[POLICY] Custom evaluator registered\n");
}
