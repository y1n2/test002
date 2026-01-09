/**
 * ============================================================================
 * @file    mihf_simulator.c
 * @brief   MIHF 模拟器 - 用于本地集成测试
 * 
 * @details 本模拟器实现一个简单的 MIHF (MIH Function)，用于：
 *          - 接收 DLM 发送的 MIH 原语
 *          - 打印接收到的消息内容
 *          - 发送测试请求给 DLM
 *          - 验证 DLM 与 MIHF 之间的通信
 * 
 * @author  MAGIC 航空通信系统团队
 * @version 1.0.0
 * @date    2025-11-27
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <ctype.h>

/* 引入 MIH 协议定义 */
#include "../extensions/app_magic/mih_protocol.h"

/* ============================================================================
 * 常量定义
 * ============================================================================ */

#define MIHF_SOCKET_PATH        "/tmp/mihf.sock"
#define MAX_DLM_CONNECTIONS     8
#define BUFFER_SIZE             2048

/* DLM 套接字路径 */
static const char* DLM_SOCKET_PATHS[] = {
    "/tmp/dlm_cellular.sock",
    "/tmp/dlm_satcom.sock",
    "/tmp/dlm_wifi.sock",
    NULL
};

/* ============================================================================
 * 全局变量
 * ============================================================================ */

static volatile int g_running = 1;
static int g_socket_fd = -1;
/* 当命令线程正在读取用户输入时置为 1，process_message 会改为入队而不是直接打印 */
static volatile int g_command_active = 0;

/* 简单的收到消息队列，避免在命令输入时刷屏 */
#define MSG_QUEUE_CAP 64
typedef struct queued_msg_t {
    char text[512];
} queued_msg_t;

static queued_msg_t g_msg_queue[MSG_QUEUE_CAP];
static int g_msg_head = 0;
static int g_msg_tail = 0;
static pthread_mutex_t g_msg_mutex = PTHREAD_MUTEX_INITIALIZER;

static void enqueue_msg(const char* s) {
    pthread_mutex_lock(&g_msg_mutex);
    int next = (g_msg_tail + 1) % MSG_QUEUE_CAP;
    if (next == g_msg_head) {
        /* 队列已满，丢弃最旧的一条 */
        g_msg_head = (g_msg_head + 1) % MSG_QUEUE_CAP;
    }
    strncpy(g_msg_queue[g_msg_tail].text, s, sizeof(g_msg_queue[g_msg_tail].text)-1);
    g_msg_queue[g_msg_tail].text[sizeof(g_msg_queue[g_msg_tail].text)-1] = '\0';
    g_msg_tail = next;
    pthread_mutex_unlock(&g_msg_mutex);
}

static void flush_queued_msgs(void) {
    pthread_mutex_lock(&g_msg_mutex);
    while (g_msg_head != g_msg_tail) {
        printf("%s\n", g_msg_queue[g_msg_head].text);
        g_msg_head = (g_msg_head + 1) % MSG_QUEUE_CAP;
    }
    pthread_mutex_unlock(&g_msg_mutex);
}

/* ============================================================================
 * 信号处理
 * ============================================================================ */

static void signal_handler(int sig) {
    printf("\n[MIHF-SIM] 收到信号 %d，正在关闭...\n", sig);
    g_running = 0;
}

/* ============================================================================
 * 消息打印函数
 * ============================================================================ */

static void print_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", buffer);
}

static void print_link_identifier(const LINK_TUPLE_ID* link_id) {
    printf("  链路标识: type=0x%02X, addr=%s\n", 
           link_id->link_type, link_id->link_addr);
}

static void print_link_parameters(const LINK_PARAMETERS* params) {
    printf("  链路参数:\n");
    printf("    - TX/RX 速率: %u/%u kbps\n", 
           params->current_tx_rate_kbps, params->current_rx_rate_kbps);
    printf("    - 信号强度: %d dBm, 质量: %u%%\n",
           params->signal_strength_dbm, params->signal_quality);
    printf("    - 延迟: %u ms, 抖动: %u ms\n",
           params->current_latency_ms, params->current_jitter_ms);
    printf("    - 可用带宽: %u kbps\n", params->available_bandwidth_kbps);
    printf("    - 链路状态: %s, 活动 Bearer: %u\n",
           params->link_state == 1 ? "UP" : (params->link_state == 2 ? "GOING_DOWN" : "DOWN"),
           params->active_bearers);
}

/* ============================================================================
 * 消息处理函数
 * ============================================================================ */

static void handle_link_up_indication(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Up_Indication)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Up_Indication* ind = (const LINK_Up_Indication*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              📡 Link_Up.indication 接收                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&ind->link_identifier);
    printf("  上线时间戳: %u\n", ind->up_timestamp);
    print_link_parameters(&ind->parameters);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_link_down_indication(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Down_Indication)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Down_Indication* ind = (const LINK_Down_Indication*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              ❌ Link_Down.indication 接收                   ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&ind->link_identifier);
    printf("  断开原因: %s (%u)\n", 
           link_down_reason_to_string((LINK_DOWN_REASON)ind->reason_code), 
           ind->reason_code);
    printf("  原因描述: %s\n", ind->reason_text);
    printf("  断开时间戳: %u\n", ind->down_timestamp);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_link_going_down_indication(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Going_Down_Indication)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Going_Down_Indication* ind = (const LINK_Going_Down_Indication*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            ⚠️  Link_Going_Down.indication 接收              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&ind->link_identifier);
    printf("  预计断开时间: %u ms\n", ind->time_to_down_ms);
    printf("  断开原因: %s (%u)\n", 
           link_down_reason_to_string((LINK_DOWN_REASON)ind->reason_code),
           ind->reason_code);
    printf("  置信度: %u%%\n", ind->confidence);
    printf("  原因描述: %s\n", ind->reason_text);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_link_detected_indication(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Detected_Indication)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Detected_Indication* ind = (const LINK_Detected_Indication*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              🔍 Link_Detected.indication 接收               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&ind->link_identifier);
    printf("  链路类型: %s\n", link_param_type_to_string(ind->link_type));
    printf("  最大带宽: %u kbps\n", ind->max_bandwidth_kbps);
    printf("  信号强度: %d dBm, 质量: %u%%\n", 
           ind->signal_strength_dbm, ind->signal_quality);
    printf("  安全等级: %u\n", ind->security_supported);
    printf("  检测时间戳: %u\n", ind->detection_timestamp);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_link_parameters_report(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Parameters_Report_Indication)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Parameters_Report_Indication* ind = (const LINK_Parameters_Report_Indication*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            📊 Link_Parameters_Report.indication 接收        ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&ind->link_identifier);
    printf("  变化的参数: 0x%04X\n", ind->changed_params);
    print_link_parameters(&ind->parameters);
    printf("  报告时间戳: %u\n", ind->report_timestamp);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_capability_discover_confirm(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Capability_Discover_Confirm)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Capability_Discover_Confirm* cnf = (const LINK_Capability_Discover_Confirm*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║        ✅ Link_Capability_Discover.confirm 接收             ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&cnf->link_identifier);
    printf("  状态: %s\n", status_to_string(cnf->status));
    if (cnf->has_capability) {
        printf("  链路能力:\n");
        printf("    - 类型: 0x%02X\n", cnf->capability.link_type);
        printf("    - 最大带宽: %u kbps\n", cnf->capability.max_bandwidth_kbps);
        printf("    - 典型延迟: %u ms\n", cnf->capability.typical_latency_ms);
        printf("    - 支持事件: 0x%08X\n", cnf->capability.supported_events);
        printf("    - 安全等级: %u\n", cnf->capability.security_level);
        printf("    - MTU: %u\n", cnf->capability.mtu);
        printf("    - 非对称: %s\n", cnf->capability.is_asymmetric ? "是" : "否");
    }
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_get_parameters_confirm(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Get_Parameters_Confirm)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Get_Parameters_Confirm* cnf = (const LINK_Get_Parameters_Confirm*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           ✅ Link_Get_Parameters.confirm 接收               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&cnf->link_identifier);
    printf("  状态: %s\n", status_to_string(cnf->status));
    printf("  返回的参数: 0x%04X\n", cnf->returned_params);
    print_link_parameters(&cnf->parameters);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_event_subscribe_confirm(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Event_Subscribe_Confirm)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Event_Subscribe_Confirm* cnf = (const LINK_Event_Subscribe_Confirm*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          ✅ Link_Event_Subscribe.confirm 接收               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    print_link_identifier(&cnf->link_identifier);
    printf("  状态: %s\n", status_to_string(cnf->status));
    printf("  已订阅事件: 0x%04X\n", cnf->subscribed_events);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void handle_resource_confirm(const uint8_t* data, size_t len) {
    if (len < sizeof(LINK_Resource_Confirm)) {
        printf("  [错误] 消息长度不足\n");
        return;
    }
    
    const LINK_Resource_Confirm* cnf = (const LINK_Resource_Confirm*)data;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            ✅ Link_Resource.confirm 接收                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("  状态: %s\n", status_to_string(cnf->status));
    if (cnf->has_bearer_id) {
        printf("  Bearer ID: %u\n", cnf->bearer_identifier);
    }
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

/* ============================================================================
 * 消息分发
 * ============================================================================ */

static void process_message(const uint8_t* data, size_t len, struct sockaddr_un* from) {
    if (len < 2) {
        printf("[MIHF-SIM] 收到无效消息 (长度=%zu)\n", len);
        return;
    }
    
    uint16_t msg_type = *(uint16_t*)data;
    
    char shortbuf[512];
    snprintf(shortbuf, sizeof(shortbuf), "[MIHF-SIM] 收到消息: type=0x%04X, len=%zu, from=%s", 
             msg_type, len, from->sun_path);
    /* 如果用户正在输入命令，则把可读摘要入队，等待命令线程空闲时再打印 */
    if (g_command_active) {
        enqueue_msg(shortbuf);
        return;
    }
    print_timestamp();
    printf("%s\n", shortbuf);
    
    switch (msg_type) {
        case MIH_LINK_UP_IND:
            handle_link_up_indication(data, len);
            break;
        case MIH_LINK_DOWN_IND:
            handle_link_down_indication(data, len);
            break;
        case MIH_LINK_GOING_DOWN_IND:
            handle_link_going_down_indication(data, len);
            break;
        case MIH_LINK_DETECTED_IND:
            handle_link_detected_indication(data, len);
            break;
        case MIH_LINK_PARAMETERS_REPORT_IND:
            handle_link_parameters_report(data, len);
            break;
        case MIH_LINK_CAPABILITY_DISCOVER_CNF:
            handle_capability_discover_confirm(data, len);
            break;
        case MIH_LINK_GET_PARAMETERS_CNF:
            handle_get_parameters_confirm(data, len);
            break;
        case MIH_LINK_EVENT_SUBSCRIBE_CNF:
            handle_event_subscribe_confirm(data, len);
            break;
        case MIH_LINK_RESOURCE_CNF:
            handle_resource_confirm(data, len);
            break;
            default:
            /* 有时候 DLM 直接发送结构体而没有前置的 2 字节类型码（老的原型实现）
             * 检查首字节是否看起来像 link_identifier.link_type（1/2/3），
             * 且第二个字节是可打印字符（链路地址通常以 "eth" 开头），
             * 优先尝试按常见的确认/指示结构解析，减少误报和用户看不到回复的问题。
             */
            if ((data[0] == 0x01 || data[0] == 0x02 || data[0] == 0x03)
                && isprint(data[1])) {
                /* 优先检测较短的 Confirm 结构 —— Capability_Discover 和 Get_Parameters */
                if (len >= sizeof(LINK_Capability_Discover_Confirm)
                    && len < sizeof(LINK_Up_Indication)) {
                    print_timestamp();
                    printf("[MIHF-SIM] 检测到来自 DLM 的原始结构（无类型头），按 Capability_Discover.confirm 解析（len=%zu）\n", len);
                    handle_capability_discover_confirm(data, len);
                    break;
                }

                if (len >= sizeof(LINK_Get_Parameters_Confirm)
                    && len < sizeof(LINK_Parameters_Report_Indication)) {
                    print_timestamp();
                    printf("[MIHF-SIM] 检测到来自 DLM 的原始结构（无类型头），按 Get_Parameters.confirm 解析（len=%zu）\n", len);
                    handle_get_parameters_confirm(data, len);
                    break;
                }

                /* 回退到原有策略：优先按 Up 指示或参数报告解析 */
                print_timestamp();
                printf("[MIHF-SIM] 检测到来自 DLM 的原始结构（无类型头），按 Link_Up/Parameters 解析（len=%zu）\n", len);
                if (len >= sizeof(LINK_Up_Indication)) {
                    handle_link_up_indication(data, len);
                } else if (len >= sizeof(LINK_Parameters_Report_Indication)) {
                    handle_link_parameters_report(data, len);
                } else {
                    /* 尝试按参数报告打印（健壮降级） */
                    handle_link_parameters_report(data, len);
                }
                break;
            }

            /* 仍无法识别，打印简短说明（不打印原始十六进制） */
            if (g_command_active) {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "[MIHF-SIM] 未知消息类型: 0x%04X (len=%zu) 来自 %s，且无法按已知结构解析。 建议检查发送方是否在消息前加上 2 字节类型码。",
                         msg_type, len, from->sun_path);
                enqueue_msg(tmp);
            } else {
                print_timestamp();
                printf("[MIHF-SIM] 未知消息类型: 0x%04X (len=%zu) 来自 %s，且无法按已知结构解析。\n",
                       msg_type, len, from->sun_path);
                printf("  建议：检查发送方是否在消息前加上 2 字节类型码，或更新模拟器以支持新结构。\n");
            }
            break;
    }
}

/* ============================================================================
 * 请求发送函数
 * ============================================================================ */

static int send_to_dlm(const char* dlm_path, const void* data, size_t len) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, dlm_path, sizeof(addr.sun_path) - 1);
    
    ssize_t sent = sendto(g_socket_fd, data, len, 0,
                          (struct sockaddr*)&addr, sizeof(addr));
    
    if (sent < 0) {
        if (errno == ENOENT) {
            /* DLM 套接字不存在 */
            return -1;
        }
        perror("[MIHF-SIM] sendto() 失败");
        return -1;
    }
    
    return 0;
}

static void send_capability_discover_request(const char* dlm_path) {
    printf("[MIHF-SIM] 发送 Link_Capability_Discover.request 到 %s\n", dlm_path);
    
    /* 构造请求消息 - 消息头使用 uint16_t 类型码 */
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    /* 消息类型码 */
    *(uint16_t*)buffer = MIH_LINK_CAPABILITY_DISCOVER_REQ;
    
    /* 发送 */
    if (send_to_dlm(dlm_path, buffer, sizeof(LINK_Capability_Discover_Request) + 2) == 0) {
        printf("  ✓ 已发送\n");
    }
}

static void send_get_parameters_request(const char* dlm_path) {
    printf("[MIHF-SIM] 发送 Link_Get_Parameters.request 到 %s\n", dlm_path);
    
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    *(uint16_t*)buffer = MIH_LINK_GET_PARAMETERS_REQ;
    
    /* 设置查询所有参数 */
    LINK_Get_Parameters_Request* req = (LINK_Get_Parameters_Request*)(buffer + 2);
    req->param_type_list = LINK_PARAM_QUERY_ALL;
    
    if (send_to_dlm(dlm_path, buffer, sizeof(LINK_Get_Parameters_Request) + 2) == 0) {
        printf("  ✓ 已发送\n");
    }
}

static void send_event_subscribe_request(const char* dlm_path) {
    printf("[MIHF-SIM] 发送 Link_Event_Subscribe.request 到 %s\n", dlm_path);
    
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    *(uint16_t*)buffer = MIH_LINK_EVENT_SUBSCRIBE_REQ;
    
    LINK_Event_Subscribe_Request* req = (LINK_Event_Subscribe_Request*)(buffer + 2);
    req->event_list = LINK_EVENT_ALL;
    
    if (send_to_dlm(dlm_path, buffer, sizeof(LINK_Event_Subscribe_Request) + 2) == 0) {
        printf("  ✓ 已发送\n");
    }
}

static void send_resource_request(const char* dlm_path, uint32_t fl_rate, uint32_t rl_rate) {
    printf("[MIHF-SIM] 发送 Link_Resource.request 到 %s (FL=%u, RL=%u kbps)\n", 
           dlm_path, fl_rate, rl_rate);
    
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    *(uint16_t*)buffer = MIH_LINK_RESOURCE_REQ;
    
    LINK_Resource_Request* req = (LINK_Resource_Request*)(buffer + 2);
    req->resource_action = RESOURCE_ACTION_REQUEST;
    req->has_bearer_id = false;
    req->has_qos_params = true;
    req->qos_parameters.cos_id = COS_INTERACTIVE;
    req->qos_parameters.forward_link_rate = fl_rate;
    req->qos_parameters.return_link_rate = rl_rate;
    
    if (send_to_dlm(dlm_path, buffer, sizeof(LINK_Resource_Request) + 2) == 0) {
        printf("  ✓ 已发送\n");
    }
}

/* ============================================================================
 * 交互式命令处理
 * ============================================================================ */

static void print_help(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    MIHF 模拟器命令                           ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  c <n>  - 发送 Link_Capability_Discover.request              ║\n");
    printf("║  p <n>  - 发送 Link_Get_Parameters.request                   ║\n");
    printf("║  s <n>  - 发送 Link_Event_Subscribe.request                  ║\n");
    printf("║  r <n>  - 发送 Link_Resource.request (分配资源)              ║\n");
    printf("║  a      - 向所有 DLM 发送请求                                ║\n");
    printf("║  l      - 列出 DLM 套接字状态                                ║\n");
    printf("║  h      - 显示帮助                                           ║\n");
    printf("║  q      - 退出                                               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  <n>: 1=CELLULAR, 2=SATCOM, 3=WIFI                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

static void list_dlm_status(void) {
    printf("\n[MIHF-SIM] DLM 套接字状态:\n");
    for (int i = 0; DLM_SOCKET_PATHS[i] != NULL; i++) {
        int exists = access(DLM_SOCKET_PATHS[i], F_OK) == 0;
        printf("  [%d] %s: %s\n", i + 1, DLM_SOCKET_PATHS[i], 
               exists ? "✓ 存在" : "✗ 不存在");
    }
    printf("\n");
}

static void* command_thread(void* arg) {
    (void)arg;
    char line[128];
    
    printf("[MIHF-SIM] 交互式命令线程已启动\n");
    print_help();
    
    while (g_running) {
        /* 标记正在读取用户输入，接收线程应入队消息而不打印 */
        g_command_active = 1;
        printf("MIHF> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            g_command_active = 0;
            break;
        }

        /* 用户完成输入后，先清理并打印之前积累的消息 */
        g_command_active = 0;
        flush_queued_msgs();
        
        char cmd = line[0];
        int dlm_idx = -1;

        /* 更健壮地提取第一个整数（支持 "c 1", "c<1>", "c <1>" 等） */
        char numbuf[16];
        int ni = 0;
        for (size_t i = 1; i < strlen(line) && ni < (int)(sizeof(numbuf)-1); i++) {
            if (line[i] >= '0' && line[i] <= '9') {
                numbuf[ni++] = line[i];
            } else if (ni > 0) {
                /* 已经收集到一段数字，遇到非数字则停止 */
                break;
            }
        }
        numbuf[ni] = '\0';
        if (ni > 0) {
            dlm_idx = atoi(numbuf) - 1;
        }
        
        switch (cmd) {
            case 'c':
            case 'C':
                if (dlm_idx >= 0 && dlm_idx < 3) {
                    send_capability_discover_request(DLM_SOCKET_PATHS[dlm_idx]);
                } else {
                    printf("无效的 DLM 索引\n");
                }
                break;
                
            case 'p':
            case 'P':
                if (dlm_idx >= 0 && dlm_idx < 3) {
                    send_get_parameters_request(DLM_SOCKET_PATHS[dlm_idx]);
                } else {
                    printf("无效的 DLM 索引\n");
                }
                break;
                
            case 's':
            case 'S':
                if (dlm_idx >= 0 && dlm_idx < 3) {
                    send_event_subscribe_request(DLM_SOCKET_PATHS[dlm_idx]);
                } else {
                    printf("无效的 DLM 索引\n");
                }
                break;
                
            case 'r':
            case 'R':
                if (dlm_idx >= 0 && dlm_idx < 3) {
                    send_resource_request(DLM_SOCKET_PATHS[dlm_idx], 1000, 500);
                } else {
                    printf("无效的 DLM 索引\n");
                }
                break;
                
            case 'a':
            case 'A':
                printf("[MIHF-SIM] 向所有 DLM 发送请求...\n");
                for (int i = 0; DLM_SOCKET_PATHS[i] != NULL; i++) {
                    if (access(DLM_SOCKET_PATHS[i], F_OK) == 0) {
                        send_capability_discover_request(DLM_SOCKET_PATHS[i]);
                        usleep(100000);
                        send_get_parameters_request(DLM_SOCKET_PATHS[i]);
                        usleep(100000);
                    }
                }
                break;
                
            case 'l':
            case 'L':
                list_dlm_status();
                break;
                
            case 'h':
            case 'H':
            case '?':
                print_help();
                break;
                
            case 'q':
            case 'Q':
                g_running = 0;
                break;
                
            case '\n':
                break;
                
            default:
                printf("未知命令: %c (输入 h 查看帮助)\n", cmd);
                break;
        }
    }
    
    printf("[MIHF-SIM] 命令线程已退出\n");
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(int argc, char* argv[]) {
    struct sockaddr_un addr;
    pthread_t cmd_thread;
    
    (void)argc;
    (void)argv;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 MIHF 模拟器 v1.0                             ║\n");
    printf("║            用于 DLM 集成测试                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* 创建 Unix 域套接字 */
    g_socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_socket_fd < 0) {
        perror("[MIHF-SIM] socket() 失败");
        return EXIT_FAILURE;
    }
    
    /* 绑定到 MIHF 套接字路径 */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MIHF_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    unlink(MIHF_SOCKET_PATH);
    
    if (bind(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[MIHF-SIM] bind() 失败");
        close(g_socket_fd);
        return EXIT_FAILURE;
    }
    
    printf("[MIHF-SIM] 监听套接字: %s\n", MIHF_SOCKET_PATH);
    
    /* 启动命令线程 */
    if (pthread_create(&cmd_thread, NULL, command_thread, NULL) != 0) {
        perror("[MIHF-SIM] pthread_create() 失败");
        close(g_socket_fd);
        unlink(MIHF_SOCKET_PATH);
        return EXIT_FAILURE;
    }
    
    /* 主消息接收循环 */
    printf("[MIHF-SIM] 等待 DLM 消息...\n\n");
    
    while (g_running) {
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(g_socket_fd, &readfds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(g_socket_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[MIHF-SIM] select() 失败");
            break;
        }
        
        if (ret > 0 && FD_ISSET(g_socket_fd, &readfds)) {
            uint8_t buffer[BUFFER_SIZE];
            struct sockaddr_un from_addr;
            socklen_t from_len = sizeof(from_addr);
            
            ssize_t recv_len = recvfrom(g_socket_fd, buffer, sizeof(buffer), 0,
                                        (struct sockaddr*)&from_addr, &from_len);
            
            if (recv_len > 0) {
                process_message(buffer, (size_t)recv_len, &from_addr);
            }
        }
    }
    
    /* 清理 */
    printf("\n[MIHF-SIM] 正在清理...\n");
    pthread_join(cmd_thread, NULL);
    close(g_socket_fd);
    unlink(MIHF_SOCKET_PATH);
    
    printf("[MIHF-SIM] 已退出\n");
    return EXIT_SUCCESS;
}
