/**
 * @file test_xml_parser.c
 * @brief MAGIC XML 配置解析器测试程序
 */

#include "xml_config_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    MagicConfig config;
    
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  MAGIC XML Configuration Parser Test  ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    
    // 初始化配置
    magic_config_init(&config);
    
    // 加载所有配置文件
    if (magic_config_load_all(&config) != 0) {
        fprintf(stderr, "Failed to load configuration\n");
        return 1;
    }
    
    // 打印配置摘要
    magic_config_print_summary(&config);
    
    // 测试查找功能
    printf("========================================\n");
    printf("  Testing Lookup Functions\n");
    printf("========================================\n\n");
    
    // 查找链路
    printf("Looking up LINK_SATCOM...\n");
    DatalinkProfile* satcom = magic_config_find_datalink(&config, "LINK_SATCOM");
    if (satcom) {
        printf("  ✓ Found: %s\n", satcom->link_name);
        printf("    DLM Driver: %s\n", satcom->dlm_driver_id);
        printf("    Interface:  %s\n", satcom->interface_name);
        printf("    Bandwidth:  %u kbps\n", satcom->capabilities.max_tx_rate_kbps);
        printf("    Latency:    %u ms\n", satcom->capabilities.typical_latency_ms);
    } else {
        printf("  ✗ Not found\n");
    }
    printf("\n");
    
    // 查找客户端
    printf("Looking up EFB_NAV_APP_01...\n");
    ClientProfile* efb = magic_config_find_client(&config, "EFB_NAV_APP_01");
    if (efb) {
        printf("  ✓ Found: %s\n", efb->client_id);
        printf("    Role:       %s\n", efb->metadata.system_role);
        printf("    Auth Type:  %s\n", 
               efb->auth.type == AUTH_MAGIC_AWARE ? "MAGIC_AWARE" : "NON_AWARE");
        if (efb->auth.type == AUTH_MAGIC_AWARE) {
            printf("    Username:   %s\n", efb->auth.username);
        }
        printf("    Bandwidth:  %u kbps\n", efb->limits.total_client_bw_kbps);
        printf("    Max Sessions: %u\n", efb->limits.max_concurrent_sessions);
    } else {
        printf("  ✗ Not found\n");
    }
    printf("\n");
    
    // 查找策略规则集
    printf("Looking up ruleset for flight phase 'PARKED'...\n");
    PolicyRuleSet* ground_ops = magic_config_find_ruleset(&config, "PARKED");
    if (ground_ops) {
        printf("  ✓ Found: %s\n", ground_ops->ruleset_id);
        printf("    Flight Phases: %s\n", ground_ops->flight_phases);
        printf("    Number of Rules: %u\n", ground_ops->num_rules);
        
        for (uint32_t i = 0; i < ground_ops->num_rules; i++) {
            PolicyRule* rule = &ground_ops->rules[i];
            printf("\n    Rule %u: %s\n", i + 1, rule->traffic_class);
            printf("      Path Preferences:\n");
            
            for (uint32_t j = 0; j < rule->num_preferences; j++) {
                PathPreference* pref = &rule->preferences[j];
                const char* action_str = "";
                if (pref->action == ACTION_PERMIT) action_str = " (PERMIT)";
                else if (pref->action == ACTION_PROHIBIT) action_str = " (PROHIBIT)";
                
                printf("        %u. %s%s", pref->ranking, pref->link_id, action_str);
                if (pref->security_required[0]) {
                    printf(" [Security: %s]", pref->security_required);
                }
                printf("\n");
            }
        }
    } else {
        printf("  ✗ Not found\n");
    }
    printf("\n");
    
    printf("========================================\n");
    printf("  Test Complete\n");
    printf("========================================\n\n");
    
    // 清理
    magic_config_cleanup(&config);
    
    return 0;
}
