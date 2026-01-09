/**
 * @file lmi_test_example.c
 * @brief LMI 和 DLM 使用示例
 * 
 * 演示如何:
 * 1. 初始化 DLM 驱动
 * 2. 注册链路到中央管理模块
 * 3. 请求资源（建立连接）
 * 4. 处理异步事件
 * 5. 执行链路切换
 */

#include "lmi/magic_lmi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* 外部 DLM 操作表声明 */
extern lmi_operations_t dlm_satcom_ops;
extern lmi_operations_t dlm_cellular_ops;
extern lmi_operations_t dlm_wifi_ops;

/* 全局链路注册表 */
typedef struct {
    lmi_link_info_t info;
    lmi_operations_t *ops;
} registered_link_t;

static registered_link_t g_links[10];
static int g_link_count = 0;

/* ========================================================================
 * 回调函数实现
 * ======================================================================== */

/**
 * 链路事件回调 - 由 DLM 调用以通知 CM 链路状态变化
 */
void on_link_event(const lmi_link_event_t *event, void *user_data) {
    printf("\n[EVENT] Link: %s, Type: %s, State: %s -> %s\n",
           event->link_id,
           lmi_event_type_to_string(event->event_type),
           lmi_link_state_to_string(event->old_state),
           lmi_link_state_to_string(event->new_state));
    printf("[EVENT] Message: %s\n", event->message);
    
    /* 处理特定事件 */
    switch (event->event_type) {
    case LMI_EVENT_LINK_UP:
        printf("[CM] Link %s is now ACTIVE, updating routing table...\n", 
               event->link_id);
        // 实际应调用 Network Management 模块配置路由
        break;
        
    case LMI_EVENT_LINK_DOWN:
        printf("[CM] Link %s is DOWN, initiating handover...\n", 
               event->link_id);
        // 实际应触发链路切换逻辑
        break;
        
    case LMI_EVENT_HANDOVER_RECOMMEND:
        printf("[CM] Handover recommended from %s to %s\n",
               event->link_id, 
               event->ext.handover.recommended_link);
        // 实际应评估策略并执行切换
        break;
        
    case LMI_EVENT_CAPABILITY_CHANGE:
        printf("[CM] Link capability changed: signal=%d dBm, quality=%d%%\n",
               event->ext.quality.signal_strength,
               event->ext.quality.signal_quality);
        break;
        
    default:
        break;
    }
}

/**
 * 日志回调
 */
void on_log_message(int level, const char *message, void *user_data) {
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    printf("[%s] %s\n", level_str[level], message);
}

/* ========================================================================
 * 中央管理模块模拟函数
 * ======================================================================== */

/**
 * 初始化所有 DLM 并注册链路
 */
int cm_initialize_dlms(void) {
    printf("\n=== Step 1: Initialize DLMs ===\n");
    
    lmi_operations_t *dlm_list[] = {
        &dlm_satcom_ops,
        &dlm_cellular_ops,
        &dlm_wifi_ops,
    };
    
    for (int i = 0; i < 3; i++) {
        lmi_operations_t *ops = dlm_list[i];
        
        /* 初始化 DLM */
        int ret = ops->init(NULL, on_link_event, on_log_message, NULL);
        if (ret != LMI_SUCCESS) {
            printf("[CM] Failed to initialize DLM: %s\n", 
                   lmi_error_to_string(ret));
            continue;
        }
        
        /* 注册链路 */
        lmi_link_info_t link_info;
        ret = ops->register_link(&link_info);
        if (ret == LMI_SUCCESS) {
            g_links[g_link_count].info = link_info;
            g_links[g_link_count].ops = ops;
            g_link_count++;
            
            printf("[CM] Registered link: %s (%s) - Type: %s, State: %s\n",
                   link_info.link_id,
                   link_info.link_name,
                   lmi_link_type_to_string(link_info.link_type),
                   lmi_link_state_to_string(link_info.state));
            printf("      Capability: Tx=%llu kbps, Latency=%u ms\n",
                   (unsigned long long)(link_info.capability.max_tx_rate / 1000),
                   link_info.capability.typical_latency);
            printf("      Policy: Cost=%u, Security=%d, Priority=%u\n",
                   link_info.policy.cost_index,
                   link_info.policy.security,
                   link_info.policy.priority);
        }
    }
    
    printf("[CM] Total %d links registered\n", g_link_count);
    
    return LMI_SUCCESS;
}

/**
 * 根据策略选择最佳链路
 * 简化策略: 优先级最高且可用的链路
 */
registered_link_t* cm_select_best_link(void) {
    registered_link_t *best = NULL;
    uint32_t highest_priority = 0;
    
    for (int i = 0; i < g_link_count; i++) {
        lmi_link_state_e state;
        g_links[i].ops->get_state(g_links[i].info.link_id, &state);
        
        if (state == LMI_STATE_AVAILABLE) {
            uint32_t priority = g_links[i].info.policy.priority;
            
            /* WiFi 优先（最高优先级且成本最低） */
            if (priority > highest_priority) {
                highest_priority = priority;
                best = &g_links[i];
            }
        }
    }
    
    return best;
}

/**
 * 请求资源（建立连接）
 */
int cm_allocate_resource(const char *link_id, const char *client_id) {
    printf("\n=== Step 2: Allocate Resource on %s ===\n", link_id);
    
    /* 查找链路 */
    registered_link_t *link = NULL;
    for (int i = 0; i < g_link_count; i++) {
        if (strcmp(g_links[i].info.link_id, link_id) == 0) {
            link = &g_links[i];
            break;
        }
    }
    
    if (!link) {
        printf("[CM] Link %s not found\n", link_id);
        return LMI_ERR_LINK_NOT_FOUND;
    }
    
    /* 构造资源请求 */
    lmi_resource_request_t request = {0};
    request.session_id = lmi_generate_session_id();
    request.action = LMI_RESOURCE_ALLOCATE;
    request.min_tx_rate = 512000;       // 512 kbps 最低
    request.requested_tx_rate = 2048000; // 期望 2 Mbps
    request.min_rx_rate = 512000;
    request.requested_rx_rate = 2048000;
    request.qos_class = 2;
    request.max_delay_ms = 500;
    request.timeout_sec = 300;
    request.persistent = true;
    snprintf(request.client_id, sizeof(request.client_id), "%s", client_id);
    
    /* 发送请求 */
    lmi_resource_response_t response = {0};
    int ret = link->ops->request_resource(link_id, &request, &response);
    
    if (ret == LMI_SUCCESS && response.result_code == LMI_SUCCESS) {
        printf("[CM] Resource allocated successfully!\n");
        printf("     Session ID: %u\n", response.session_id);
        printf("     Granted Tx: %llu kbps, Rx: %llu kbps\n",
               (unsigned long long)(response.granted_tx_rate / 1000),
               (unsigned long long)(response.granted_rx_rate / 1000));
        printf("     Local IP: %s\n", response.local_ip);
        printf("     Gateway: %s\n", response.gateway_ip);
        printf("     DNS: %s, %s\n", response.dns_primary, response.dns_secondary);
        
        return response.session_id;
    } else {
        printf("[CM] Resource allocation failed: %s\n", 
               response.error_message);
        return -1;
    }
}

/**
 * 释放资源
 */
int cm_release_resource(const char *link_id, lmi_session_id_t session_id) {
    printf("\n=== Step 3: Release Resource ===\n");
    
    registered_link_t *link = NULL;
    for (int i = 0; i < g_link_count; i++) {
        if (strcmp(g_links[i].info.link_id, link_id) == 0) {
            link = &g_links[i];
            break;
        }
    }
    
    if (!link) {
        return LMI_ERR_LINK_NOT_FOUND;
    }
    
    lmi_resource_request_t request = {0};
    request.session_id = session_id;
    request.action = LMI_RESOURCE_RELEASE;
    
    lmi_resource_response_t response = {0};
    int ret = link->ops->request_resource(link_id, &request, &response);
    
    if (ret == LMI_SUCCESS) {
        printf("[CM] Resource released successfully\n");
    }
    
    return ret;
}

/**
 * 关闭所有 DLM
 */
void cm_shutdown_dlms(void) {
    printf("\n=== Step 4: Shutdown DLMs ===\n");
    
    for (int i = 0; i < g_link_count; i++) {
        g_links[i].ops->shutdown();
    }
}

/* ========================================================================
 * 主程序
 * ======================================================================== */

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("  MAGIC LMI/DLM Test Example\n");
    printf("========================================\n");
    
    /* 1. 初始化所有 DLM */
    if (cm_initialize_dlms() != LMI_SUCCESS) {
        fprintf(stderr, "Failed to initialize DLMs\n");
        return 1;
    }
    
    sleep(2); // 等待 DLM 启动完成
    
    /* 2. 选择最佳链路并建立连接 */
    registered_link_t *best_link = cm_select_best_link();
    if (best_link) {
        printf("\n[CM] Policy decision: Select %s (Priority=%u, Cost=%u)\n",
               best_link->info.link_id,
               best_link->info.policy.priority,
               best_link->info.policy.cost_index);
        
        int session_id = cm_allocate_resource(best_link->info.link_id, "EFB-001");
        
        if (session_id > 0) {
            /* 3. 模拟数据传输 */
            printf("\n[CM] Simulating data transfer...\n");
            sleep(10);
            
            /* 4. 查询统计信息 */
            lmi_link_stats_t stats;
            if (best_link->ops->get_statistics(best_link->info.link_id, &stats) == LMI_SUCCESS) {
                printf("\n[CM] Link statistics:\n");
                printf("     TX: %llu bytes, RX: %llu bytes\n",
                       (unsigned long long)stats.bytes_transmitted,
                       (unsigned long long)stats.bytes_received);
                printf("     Uptime: %llu seconds\n",
                       (unsigned long long)stats.uptime_seconds);
            }
            
            /* 5. 释放资源 */
            cm_release_resource(best_link->info.link_id, session_id);
        }
    } else {
        printf("[CM] No available link found\n");
    }
    
    /* 6. 等待一些异步事件 */
    printf("\n[CM] Monitoring for 15 seconds...\n");
    sleep(15);
    
    /* 7. 清理 */
    cm_shutdown_dlms();
    
    printf("\n========================================\n");
    printf("  Test completed successfully!\n");
    printf("========================================\n");
    
    return 0;
}
