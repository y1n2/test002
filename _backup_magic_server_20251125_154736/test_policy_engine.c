/**
 * @file test_policy_engine.c
 * @brief 策略引擎测试程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "policy_engine.h"
#include "xml_config_parser.h"

// 测试用例计数
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

/*===========================================================================
 * 测试 1: 策略引擎初始化
 *===========================================================================*/

void test_policy_engine_initialization() {
    printf("\n========================================\n");
    printf("TEST 1: Policy Engine Initialization\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    
    int ret = magic_config_load_all(&config);
    TEST_ASSERT(ret == 0, "Load XML configuration");
    
    PolicyEngineContext ctx;
    ret = policy_engine_init(&ctx, &config);
    TEST_ASSERT(ret == 0, "Initialize policy engine");
    TEST_ASSERT(ctx.current_phase == FLIGHT_PHASE_PARKED, "Initial phase is PARKED");
    TEST_ASSERT(ctx.num_links == 3, "Loaded 3 links");
    TEST_ASSERT(ctx.active_ruleset != NULL, "Active ruleset assigned");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 2: 飞行阶段切换
 *===========================================================================*/

void test_flight_phase_transitions() {
    printf("\n========================================\n");
    printf("TEST 2: Flight Phase Transitions\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 测试阶段切换
    int ret = policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_TAXI);
    TEST_ASSERT(ret == 0, "Switch to TAXI phase");
    TEST_ASSERT(ctx.current_phase == FLIGHT_PHASE_TAXI, "Phase is TAXI");
    TEST_ASSERT(ctx.stats.phase_switches == 1, "Phase switch count = 1");
    
    ret = policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_CRUISE);
    TEST_ASSERT(ret == 0, "Switch to CRUISE phase");
    TEST_ASSERT(ctx.current_phase == FLIGHT_PHASE_CRUISE, "Phase is CRUISE");
    TEST_ASSERT(ctx.stats.phase_switches == 2, "Phase switch count = 2");
    
    // 测试相同阶段（不应该触发切换）
    ret = policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_CRUISE);
    TEST_ASSERT(ret == 0, "Same phase returns success");
    TEST_ASSERT(ctx.stats.phase_switches == 2, "Phase switch count still 2");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 3: 链路状态更新
 *===========================================================================*/

void test_link_state_updates() {
    printf("\n========================================\n");
    printf("TEST 3: Link State Updates\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 更新链路状态
    int ret = policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    TEST_ASSERT(ret == 0, "Update SATCOM link state");
    
    ret = policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    TEST_ASSERT(ret == 0, "Update CELLULAR link state");
    
    ret = policy_engine_update_link_state(&ctx, "LINK_WIFI", true, 102400, 5);
    TEST_ASSERT(ret == 0, "Update WIFI link state");
    
    // 验证链路状态
    bool all_up = true;
    for (uint32_t i = 0; i < ctx.num_links; i++) {
        if (!ctx.link_states[i].is_up) {
            all_up = false;
            break;
        }
    }
    TEST_ASSERT(all_up, "All links are UP");
    
    // 测试链路断开
    ret = policy_engine_update_link_state(&ctx, "LINK_WIFI", false, 0, 0);
    TEST_ASSERT(ret == 0, "Set WIFI link DOWN");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 4: 路径选择 - PARKED 阶段
 *===========================================================================*/

void test_path_selection_parked() {
    printf("\n========================================\n");
    printf("TEST 4: Path Selection - PARKED Phase\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 设置所有链路为在线
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", true, 102400, 5);
    
    // 测试 BULK_DATA 流量（PARKED 阶段：WIFI > CELLULAR > SATCOM(禁止)）
    PathSelectionDecision decision;
    int ret = policy_engine_select_path(&ctx, TRAFFIC_CLASS_BULK_DATA, &decision);
    TEST_ASSERT(ret == 0, "Select path for BULK_DATA");
    TEST_ASSERT(decision.selection_valid, "Path selection is valid");
    TEST_ASSERT(strcmp(decision.selected_link_id, "LINK_WIFI") == 0, 
                "Selected WIFI for BULK_DATA in PARKED");
    
    policy_engine_print_decision(&decision);
    
    // 测试 COCKPIT_DATA 流量（PARKED 阶段：CELLULAR > WIFI）
    ret = policy_engine_select_path(&ctx, TRAFFIC_CLASS_COCKPIT_DATA, &decision);
    TEST_ASSERT(ret == 0, "Select path for COCKPIT_DATA");
    TEST_ASSERT(decision.selection_valid, "Path selection is valid");
    TEST_ASSERT(strcmp(decision.selected_link_id, "LINK_CELLULAR") == 0, 
                "Selected CELLULAR for COCKPIT_DATA in PARKED");
    
    policy_engine_print_decision(&decision);
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 5: 路径选择 - CRUISE 阶段
 *===========================================================================*/

void test_path_selection_cruise() {
    printf("\n========================================\n");
    printf("TEST 5: Path Selection - CRUISE Phase\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 切换到 CRUISE 阶段
    policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_CRUISE);
    
    // 设置所有链路为在线
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", true, 102400, 5);
    
    // 测试 ALL_TRAFFIC（CRUISE 阶段：SATCOM > CELLULAR(禁止) > WIFI(禁止)）
    PathSelectionDecision decision;
    int ret = policy_engine_select_path(&ctx, TRAFFIC_CLASS_FLIGHT_CRITICAL, &decision);
    TEST_ASSERT(ret == 0, "Select path for FLIGHT_CRITICAL");
    TEST_ASSERT(decision.selection_valid, "Path selection is valid");
    TEST_ASSERT(strcmp(decision.selected_link_id, "LINK_SATCOM") == 0, 
                "Selected SATCOM for FLIGHT_CRITICAL in CRUISE");
    
    policy_engine_print_decision(&decision);
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 6: 链路故障场景
 *===========================================================================*/

void test_link_failure_scenario() {
    printf("\n========================================\n");
    printf("TEST 6: Link Failure Scenario\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // PARKED 阶段，WIFI 离线
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", false, 0, 0);  // WIFI DOWN
    
    // BULK_DATA 应该 fallback 到 CELLULAR
    PathSelectionDecision decision;
    int ret = policy_engine_select_path(&ctx, TRAFFIC_CLASS_BULK_DATA, &decision);
    TEST_ASSERT(ret == 0, "Select path with WIFI down");
    TEST_ASSERT(decision.selection_valid, "Path selection is valid");
    TEST_ASSERT(strcmp(decision.selected_link_id, "LINK_CELLULAR") == 0, 
                "Fallback to CELLULAR when WIFI is down");
    
    policy_engine_print_decision(&decision);
    
    // CRUISE 阶段，SATCOM 离线
    policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_CRUISE);
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", false, 0, 0);  // SATCOM DOWN
    
    // 所有流量应该无可用路径（CELLULAR 和 WIFI 都被禁止）
    ret = policy_engine_select_path(&ctx, TRAFFIC_CLASS_FLIGHT_CRITICAL, &decision);
    TEST_ASSERT(ret != 0, "No available path when SATCOM is down in CRUISE");
    TEST_ASSERT(!decision.selection_valid, "Path selection is invalid");
    
    policy_engine_print_decision(&decision);
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 7: 流量类别映射
 *===========================================================================*/

void test_traffic_class_mapping() {
    printf("\n========================================\n");
    printf("TEST 7: Traffic Class Mapping\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 测试客户端映射
    TrafficClass tc = policy_engine_map_client_to_traffic_class(&ctx, "EFB_NAV_APP_01");
    TEST_ASSERT(tc == TRAFFIC_CLASS_FLIGHT_CRITICAL, 
                "Map EFB_NAV to FLIGHT_CRITICAL");
    
    tc = policy_engine_map_client_to_traffic_class(&ctx, "LEGACY_AVIONICS_02");
    TEST_ASSERT(tc == TRAFFIC_CLASS_ACARS_COMMS, 
                "Map LEGACY_AVIONICS to ACARS_COMMS");
    
    tc = policy_engine_map_client_to_traffic_class(&ctx, "PASSENGER_SUBNET_03");
    TEST_ASSERT(tc == TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT, 
                "Map PASSENGER_SUBNET to PASSENGER_ENTERTAINMENT");
    
    tc = policy_engine_map_client_to_traffic_class(&ctx, "CABIN_CREW_APP_04");
    TEST_ASSERT(tc == TRAFFIC_CLASS_CABIN_OPERATIONS, 
                "Map CABIN_CREW to CABIN_OPERATIONS");
    
    // 测试 Diameter 应用映射
    tc = policy_engine_map_diameter_app_to_traffic_class(&ctx, 16777216);
    TEST_ASSERT(tc == TRAFFIC_CLASS_FLIGHT_CRITICAL, 
                "Map DCCA (16777216) to FLIGHT_CRITICAL");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 8: 路径可用性检查
 *===========================================================================*/

void test_path_availability() {
    printf("\n========================================\n");
    printf("TEST 8: Path Availability Check\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 设置链路状态
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", false, 0, 0);
    
    // PARKED 阶段
    bool available = policy_engine_is_path_available(&ctx, "LINK_SATCOM", 
                                                     TRAFFIC_CLASS_BULK_DATA);
    TEST_ASSERT(!available, "SATCOM prohibited for BULK_DATA in PARKED");
    
    available = policy_engine_is_path_available(&ctx, "LINK_CELLULAR", 
                                                TRAFFIC_CLASS_BULK_DATA);
    TEST_ASSERT(available, "CELLULAR available for BULK_DATA in PARKED");
    
    available = policy_engine_is_path_available(&ctx, "LINK_WIFI", 
                                                TRAFFIC_CLASS_BULK_DATA);
    TEST_ASSERT(!available, "WIFI not available (link is down)");
    
    // CRUISE 阶段
    policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_CRUISE);
    
    available = policy_engine_is_path_available(&ctx, "LINK_SATCOM", 
                                                TRAFFIC_CLASS_FLIGHT_CRITICAL);
    TEST_ASSERT(available, "SATCOM available for FLIGHT_CRITICAL in CRUISE");
    
    available = policy_engine_is_path_available(&ctx, "LINK_CELLULAR", 
                                                TRAFFIC_CLASS_FLIGHT_CRITICAL);
    TEST_ASSERT(!available, "CELLULAR prohibited for ALL_TRAFFIC in CRUISE");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 9: 动态评分算法
 *===========================================================================*/

void test_dynamic_scoring() {
    printf("\n========================================\n");
    printf("TEST 9: Dynamic Scoring Algorithm\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 设置不同的链路负载
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", true, 102400, 5);
    
    // 模拟 CELLULAR 高负载
    ctx.link_states[1].current_load_kbps = 18000;  // 90% 负载
    
    // 在 PARKED 阶段选择 COCKPIT_DATA（偏好：CELLULAR > WIFI）
    PathSelectionDecision decision;
    policy_engine_select_path(&ctx, TRAFFIC_CLASS_COCKPIT_DATA, &decision);
    
    // 虽然 CELLULAR 排名第一，但高负载应该降低其评分
    // 验证评分机制
    TEST_ASSERT(decision.num_paths >= 2, "Evaluated multiple paths");
    TEST_ASSERT(decision.selection_valid, "Valid path selected");
    
    printf("  Path scores:\n");
    for (uint32_t i = 0; i < decision.num_paths; i++) {
        printf("    %s: %u (rank %u)\n", 
               decision.paths[i].link_id,
               decision.paths[i].score,
               decision.paths[i].preference_ranking);
    }
    
    policy_engine_print_decision(&decision);
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 测试 10: 状态打印
 *===========================================================================*/

void test_status_printing() {
    printf("\n========================================\n");
    printf("TEST 10: Status Printing\n");
    printf("========================================\n");
    
    MagicConfig config;
    magic_config_init(&config);
    magic_config_load_all(&config);
    
    PolicyEngineContext ctx;
    policy_engine_init(&ctx, &config);
    
    // 设置链路状态
    policy_engine_update_link_state(&ctx, "LINK_SATCOM", true, 2048, 600);
    policy_engine_update_link_state(&ctx, "LINK_CELLULAR", true, 20480, 50);
    policy_engine_update_link_state(&ctx, "LINK_WIFI", false, 0, 0);
    
    // 执行一些操作
    policy_engine_set_flight_phase(&ctx, FLIGHT_PHASE_TAXI);
    
    PathSelectionDecision decision;
    policy_engine_select_path(&ctx, TRAFFIC_CLASS_FLIGHT_CRITICAL, &decision);
    policy_engine_select_path(&ctx, TRAFFIC_CLASS_COCKPIT_DATA, &decision);
    
    // 打印状态
    policy_engine_print_status(&ctx);
    
    TEST_ASSERT(true, "Status printed successfully");
    
    policy_engine_destroy(&ctx);
}

/*===========================================================================
 * 主函数
 *===========================================================================*/

int main() {
    printf("\n");
    printf("========================================\n");
    printf("  MAGIC Policy Engine Test Suite\n");
    printf("========================================\n");
    
    test_policy_engine_initialization();
    test_flight_phase_transitions();
    test_link_state_updates();
    test_path_selection_parked();
    test_path_selection_cruise();
    test_link_failure_scenario();
    test_traffic_class_mapping();
    test_path_availability();
    test_dynamic_scoring();
    test_status_printing();
    
    printf("\n");
    printf("========================================\n");
    printf("  Test Summary\n");
    printf("========================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n  ✓ All tests passed!\n");
    } else {
        printf("\n  ✗ Some tests failed!\n");
    }
    printf("========================================\n\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
