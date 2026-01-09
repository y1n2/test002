/**
 * @file magic_core_main.c
 * @brief MAGIC Core 主程序
 * @description 
 *   - 加载 XML 配置文件
 *   - 启动 CM Core 服务器
 *   - 管理 DLM 连接
 *   - 提供配置查询接口
 */

#include "xml_config_parser.h"
#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>

/*===========================================================================
 * 全局变量
 *===========================================================================*/

static MagicConfig g_config;
static volatile bool g_running = true;
static int g_server_fd = -1;

/*===========================================================================
 * DLM 客户端管理
 *===========================================================================*/

#define MAX_DLM_CLIENTS 10

typedef struct {
    int fd;
    char dlm_id[32];
    char link_id[64];
    bool is_registered;
    uint32_t assigned_id;
    time_t last_heartbeat;
    pthread_t thread;
} ActiveDlmClient;

static ActiveDlmClient g_dlm_clients[MAX_DLM_CLIENTS];
static pthread_mutex_t g_dlm_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_next_id = 1000;

/*===========================================================================
 * 信号处理
 *===========================================================================*/

static void signal_handler(int signum)
{
    (void)signum;
    printf("\n[MAGIC CORE] Received shutdown signal\n");
    g_running = false;
}

/*===========================================================================
 * DLM 客户端管理函数
 *===========================================================================*/

static ActiveDlmClient* find_free_dlm_slot(void)
{
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (!g_dlm_clients[i].is_registered) {
            return &g_dlm_clients[i];
        }
    }
    return NULL;
}

static ActiveDlmClient* find_dlm_by_fd(int fd)
{
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (g_dlm_clients[i].is_registered && g_dlm_clients[i].fd == fd) {
            return &g_dlm_clients[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * DLM 消息处理
 *===========================================================================*/

static void handle_dlm_registration(int client_fd, const MsgRegister* reg)
{
    pthread_mutex_lock(&g_dlm_mutex);
    
    // 查找对应的链路配置
    DatalinkProfile* link = NULL;
    for (uint32_t i = 0; i < g_config.num_datalinks; i++) {
        if (strcmp(g_config.datalinks[i].dlm_driver_id, reg->dlm_id) == 0) {
            link = &g_config.datalinks[i];
            break;
        }
    }
    
    if (!link) {
        fprintf(stderr, "[MAGIC CORE] Warning: Unknown DLM: %s\n", reg->dlm_id);
        pthread_mutex_unlock(&g_dlm_mutex);
        
        MsgRegisterAck ack = {
            .result = 1,
            .assigned_id = 0
        };
        strncpy(ack.message, "Unknown DLM ID", sizeof(ack.message) - 1);
        send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
        return;
    }
    
    // 查找空闲槽位
    ActiveDlmClient* client = find_free_dlm_slot();
    if (!client) {
        pthread_mutex_unlock(&g_dlm_mutex);
        
        MsgRegisterAck ack = {
            .result = 1,
            .assigned_id = 0
        };
        strncpy(ack.message, "No free slot", sizeof(ack.message) - 1);
        send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
        return;
    }
    
    // 注册 DLM
    memset(client, 0, sizeof(ActiveDlmClient));
    client->fd = client_fd;
    client->assigned_id = g_next_id++;
    client->is_registered = true;
    strncpy(client->dlm_id, reg->dlm_id, sizeof(client->dlm_id) - 1);
    strncpy(client->link_id, link->link_id, sizeof(client->link_id) - 1);
    client->last_heartbeat = time(NULL);
    
    // 标记链路为活动状态
    link->is_active = true;
    
    pthread_mutex_unlock(&g_dlm_mutex);
    
    // 发送 ACK
    MsgRegisterAck ack = {
        .result = 0,
        .assigned_id = client->assigned_id
    };
    strncpy(ack.message, "Registration successful", sizeof(ack.message) - 1);
    send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
    
    printf("\n[MAGIC CORE] ✓ DLM Registered:\n");
    printf("    DLM ID:        %s\n", reg->dlm_id);
    printf("    Link ID:       %s\n", link->link_id);
    printf("    Link Name:     %s\n", link->link_name);
    printf("    Interface:     %s\n", reg->iface_name);
    printf("    Assigned ID:   %u\n", client->assigned_id);
    printf("    Max Bandwidth: %u kbps (XML: %u kbps)\n", 
           reg->max_bw_kbps, link->capabilities.max_tx_rate_kbps);
    printf("    Latency:       %u ms (XML: %u ms)\n", 
           reg->typical_latency_ms, link->capabilities.typical_latency_ms);
    printf("    Cost Index:    %u (XML: %u)\n", 
           reg->cost_index, link->policy_attrs.cost_index);
    printf("\n");
}

static void handle_link_event(int client_fd, const MsgLinkEvent* event)
{
    ActiveDlmClient* client = find_dlm_by_fd(client_fd);
    if (!client) return;
    
    if (event->is_link_up) {
        printf("[MAGIC CORE] Link Event from %s: UP ✓\n", client->dlm_id);
        printf("    Link ID:    %s\n", client->link_id);
        printf("    IP:         %s\n", inet_ntoa(*(struct in_addr*)&event->ip_address));
        printf("    Bandwidth:  %u kbps\n", event->current_bw_kbps);
        printf("\n");
    } else {
        printf("[MAGIC CORE] Link Event from %s: DOWN ✗\n", client->dlm_id);
        printf("    Link ID:    %s\n", client->link_id);
        printf("\n");
    }
}

static void handle_heartbeat(int client_fd, const MsgHeartbeat* hb)
{
    pthread_mutex_lock(&g_dlm_mutex);
    
    ActiveDlmClient* client = find_dlm_by_fd(client_fd);
    if (client) {
        client->last_heartbeat = time(NULL);
    }
    
    pthread_mutex_unlock(&g_dlm_mutex);
}

static void handle_policy_request(int client_fd, const MsgPolicyReq* req)
{
    MsgPolicyResp resp;
    memset(&resp, 0, sizeof(resp));
    
    printf("[MAGIC CORE] Policy Request from CIC:\n");
    printf("    Client:     %s\n", req->client_id);
    printf("    Profile:    %s\n", req->profile_name);
    printf("    Bandwidth:  %u/%u kbps\n", req->requested_bw_kbps, req->requested_ret_bw_kbps);
    printf("    Priority:   %u\n", req->priority_class);
    printf("    QoS Level:  %u\n", req->qos_level);
    
    // 使用策略引擎选择最优链路
    // (这里需要集成 policy_engine.c，目前使用简化逻辑)
    
    // 简化策略：按优先级选择可用链路
    // 1. WiFi (最快最便宜，但覆盖范围小)
    // 2. Cellular/LTE (中速中价，陆地覆盖)
    // 3. Satcom (慢但全球覆盖)
    
    pthread_mutex_lock(&g_dlm_mutex);
    
    ActiveDlmClient* selected_dlm = NULL;
    const char* selected_link = NULL;
    
    // 遍历所有已注册的 DLM，选择最优链路
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (!g_dlm_clients[i].is_registered) continue;
        
        DatalinkProfile* link = magic_config_find_datalink(&g_config, g_dlm_clients[i].link_id);
        if (!link || !link->is_active) continue;
        
        // 简单策略：选择第一个满足带宽需求的链路
        if (link->capabilities.max_tx_rate_kbps >= req->requested_bw_kbps) {
            selected_dlm = &g_dlm_clients[i];
            selected_link = link->link_id;
            break;
        }
    }
    
    if (selected_dlm && selected_link) {
        // 成功找到合适的链路
        resp.result_code = 0;
        strncpy(resp.selected_link_id, selected_link, sizeof(resp.selected_link_id) - 1);
        resp.granted_bw_kbps = req->requested_bw_kbps;
        resp.granted_ret_bw_kbps = req->requested_ret_bw_kbps;
        resp.qos_level = req->qos_level;
        snprintf(resp.reason, sizeof(resp.reason), 
                "Selected %s (available)", selected_link);
        
        printf("[MAGIC CORE] ✓ Policy Decision:\n");
        printf("    Selected Link: %s\n", resp.selected_link_id);
        printf("    Granted BW:    %u/%u kbps\n", resp.granted_bw_kbps, resp.granted_ret_bw_kbps);
    } else {
        // 没有合适的链路
        resp.result_code = 1;
        strncpy(resp.selected_link_id, "NONE", sizeof(resp.selected_link_id) - 1);
        strncpy(resp.reason, "No suitable link available", sizeof(resp.reason) - 1);
        
        printf("[MAGIC CORE] ✗ No suitable link found\n");
    }
    
    pthread_mutex_unlock(&g_dlm_mutex);
    
    // 发送响应
    send_ipc_msg(client_fd, MSG_TYPE_POLICY_RESP, &resp, sizeof(resp));
    printf("\n");
}

/*===========================================================================
 * DLM 客户端线程
 *===========================================================================*/

static void* handle_dlm_client(void* arg)
{
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[4096];
    IpcHeader* header = (IpcHeader*)buffer;
    
    while (g_running) {
        int n = recv_ipc_msg(client_fd, header, buffer + sizeof(IpcHeader),
                             sizeof(buffer) - sizeof(IpcHeader));
        if (n < 0) {
            break;
        }
        
        switch (header->type) {
            case MSG_TYPE_REGISTER:
                handle_dlm_registration(client_fd, (MsgRegister*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_LINK_EVENT:
                handle_link_event(client_fd, (MsgLinkEvent*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_HEARTBEAT:
                handle_heartbeat(client_fd, (MsgHeartbeat*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_POLICY_REQ:
                handle_policy_request(client_fd, (MsgPolicyReq*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_SHUTDOWN:
                printf("[MAGIC CORE] DLM requested shutdown\n");
                goto cleanup;
                
            default:
                printf("[MAGIC CORE] Unknown message type: 0x%02x\n", header->type);
                break;
        }
    }
    
cleanup:
    // 移除客户端
    pthread_mutex_lock(&g_dlm_mutex);
    ActiveDlmClient* client = find_dlm_by_fd(client_fd);
    if (client) {
        printf("[MAGIC CORE] Removing DLM: %s (Link: %s)\n", 
               client->dlm_id, client->link_id);
        
        // 标记链路为非活动
        DatalinkProfile* link = magic_config_find_datalink(&g_config, client->link_id);
        if (link) {
            link->is_active = false;
        }
        
        memset(client, 0, sizeof(ActiveDlmClient));
    }
    pthread_mutex_unlock(&g_dlm_mutex);
    
    close(client_fd);
    return NULL;
}

/*===========================================================================
 * CM Core 服务器
 *===========================================================================*/

static int start_cm_core_server(void)
{
    // 创建 Unix Socket
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // 删除旧的 socket 文件
    unlink(MAGIC_CORE_SOCKET_PATH);
    
    // 绑定地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MAGIC_CORE_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        return -1;
    }
    
    // 监听
    if (listen(g_server_fd, 10) < 0) {
        perror("listen");
        close(g_server_fd);
        return -1;
    }
    
    printf("[MAGIC CORE] CM Core server listening on %s\n", MAGIC_CORE_SOCKET_PATH);
    printf("[MAGIC CORE] Waiting for DLM connections...\n\n");
    
    return 0;
}

static void accept_dlm_connections(void)
{
    while (g_running) {
        int client_fd = accept(g_server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断
            }
            perror("accept");
            break;
        }
        
        printf("[MAGIC CORE] New DLM connection accepted (fd=%d)\n", client_fd);
        
        // 创建线程处理客户端
        int* fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_dlm_client, fd_ptr) != 0) {
            perror("pthread_create");
            free(fd_ptr);
            close(client_fd);
        } else {
            pthread_detach(thread);
        }
    }
}

/*===========================================================================
 * 主函数
 *===========================================================================*/

int main(void)
{
    // 禁用输出缓冲
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║      MAGIC Core System v1.0            ║\n");
    printf("║  Multi-link Aggregation Gateway       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    
    // 安装信号处理器
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    // 初始化 DLM 客户端数组
    memset(g_dlm_clients, 0, sizeof(g_dlm_clients));
    
    // 加载 XML 配置
    magic_config_init(&g_config);
    if (magic_config_load_all(&g_config) != 0) {
        fprintf(stderr, "[MAGIC CORE] Failed to load configuration\n");
        return 1;
    }
    
    // 打印配置摘要
    magic_config_print_summary(&g_config);
    
    // 启动 CM Core 服务器
    if (start_cm_core_server() != 0) {
        fprintf(stderr, "[MAGIC CORE] Failed to start CM Core server\n");
        magic_config_cleanup(&g_config);
        return 1;
    }
    
    // 接受连接
    accept_dlm_connections();
    
    // 清理
    printf("\n[MAGIC CORE] Shutting down...\n");
    
    if (g_server_fd >= 0) {
        close(g_server_fd);
    }
    
    unlink(MAGIC_CORE_SOCKET_PATH);
    magic_config_cleanup(&g_config);
    
    printf("[MAGIC CORE] Stopped\n\n");
    
    return 0;
}
