/**
 * @file cm_core_simple.c
 * @brief CM Core Server - 简化版本，用于 DLM 联调
 * @description
 *   这是一个简化的 CM (Connection Management) 核心服务器实现
 *   主要功能：
 *   1. 接收 DLM (Data Link Manager) 模块的注册请求
 *   2. 管理链路状态（UP/DOWN）
 *   3. 处理心跳消息，监控链路健康状态
 *   4. 提供 Unix Domain Socket IPC 接口
 */

#include <stdio.h>         // 标准输入输出
#include <stdlib.h>        // 标准库函数（malloc, free等）
#include <string.h>        // 字符串操作函数
#include <unistd.h>        // UNIX 标准函数（close, sleep等）
#include <signal.h>        // 信号处理
#include <pthread.h>       // POSIX 线程库
#include <sys/socket.h>    // Socket 编程
#include <sys/un.h>        // Unix Domain Socket 结构
#include <sys/select.h>    // select() 多路复用
#include <arpa/inet.h>     // 网络地址转换（inet_ntoa等）
#include <time.h>          // 时间函数
#include "ipc_protocol.h"  // IPC 协议定义（消息类型、结构体等）

/*===========================================================================
 * 活动 DLM 客户端数据结构
 * 描述：保存每个已注册 DLM 的状态信息
 *===========================================================================*/

typedef struct {
    // 连接信息
    int         fd;                         // Socket 文件描述符，用于与 DLM 通信
    bool        active;                     // 客户端是否活跃（true=已注册）
    uint32_t    assigned_id;                // CM Core 分配的唯一 ID
    
    // 标识信息
    char        dlm_id[MAX_IPC_NAME_LEN];          // DLM 标识符（如 "LTE_DLM", "Satcom_DLM"）
    char        link_profile_id[MAX_IPC_NAME_LEN]; // 链路配置 ID
    char        iface_name[MAX_IFACE_LEN];     // 网络接口名称（如 "eth0", "wlan0"）
    
    // 链路能力参数（Link Capabilities）
    uint32_t    max_bw_kbps;                // 最大带宽（千比特每秒）
    uint32_t    latency_ms;                 // 典型延迟（毫秒）
    uint32_t    cost_index;                 // 成本指数（0-100，越高越昂贵）
    uint8_t     priority;                   // 优先级（1-10，10为最高）
    uint8_t     coverage;                   // 覆盖范围类型（地面/海洋/全球等）
    
    // 当前链路状态（Current State）
    bool        is_link_up;                 // 链路是否在线（true=UP, false=DOWN）
    uint32_t    current_bw_kbps;            // 当前可用带宽
    time_t      last_heartbeat;             // 最后一次心跳时间戳（用于检测超时）
    
    // 统计信息（Statistics）
    uint64_t    tx_bytes;                   // 发送字节数
    uint64_t    rx_bytes;                   // 接收字节数
} ActiveDlmClient;

/*===========================================================================
 * 全局状态变量
 * 描述：管理服务器运行状态和所有已连接的 DLM 客户端
 *===========================================================================*/

// Unix Domain Socket 服务器文件描述符
static int g_server_fd = -1;

// 服务器运行标志（false=准备关闭，true=正常运行）
static bool g_running = true;

// DLM 客户端数组（最多支持 MAX_DLM_CLIENTS 个并发连接）
static ActiveDlmClient g_clients[MAX_DLM_CLIENTS];

// 客户端数组访问互斥锁（保证线程安全）
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 下一个要分配的客户端 ID（递增分配，从 1000 开始）
static uint32_t g_next_id = 1000;

/*===========================================================================
 * 信号处理函数
 * 功能：响应 SIGINT (Ctrl+C) 和 SIGTERM 信号，优雅关闭服务器
 *===========================================================================*/

void signal_handler(int signum)
{
    (void)signum;  // 抑制未使用参数警告
    printf("\n[CM CORE] Received shutdown signal\n");
    g_running = false;  // 设置标志，通知主循环退出
}

/*===========================================================================
 * Find Free Client Slot
 *===========================================================================*/

ActiveDlmClient* find_free_slot(void)
{
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (!g_clients[i].active) {
            return &g_clients[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * Find Client by FD
 *===========================================================================*/

ActiveDlmClient* find_client_by_fd(int fd)
{
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (g_clients[i].active && g_clients[i].fd == fd) {
            return &g_clients[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * Handle DLM Registration
 *===========================================================================*/

void handle_registration(int client_fd, const MsgRegister* reg)
{
    pthread_mutex_lock(&g_clients_mutex);
    
    ActiveDlmClient* client = find_free_slot();
    if (!client) {
        pthread_mutex_unlock(&g_clients_mutex);
        
        MsgRegisterAck ack = {
            .result = 1,
            .assigned_id = 0
        };
        strncpy(ack.message, "No free slot", sizeof(ack.message) - 1);
        send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
        return;
    }
    
    // Fill client info
    memset(client, 0, sizeof(ActiveDlmClient));
    client->active = true;
    client->fd = client_fd;
    client->assigned_id = g_next_id++;
    strncpy(client->dlm_id, reg->dlm_id, sizeof(client->dlm_id) - 1);
    strncpy(client->link_profile_id, reg->link_profile_id, sizeof(client->link_profile_id) - 1);
    strncpy(client->iface_name, reg->iface_name, sizeof(client->iface_name) - 1);
    
    client->max_bw_kbps = reg->max_bw_kbps;
    client->latency_ms = reg->typical_latency_ms;
    client->cost_index = reg->cost_index;
    client->priority = reg->priority;
    client->coverage = reg->coverage;
    client->last_heartbeat = time(NULL);
    
    pthread_mutex_unlock(&g_clients_mutex);
    
    // Send ACK
    MsgRegisterAck ack = {
        .result = 0,
        .assigned_id = client->assigned_id
    };
    strncpy(ack.message, "Registration successful", sizeof(ack.message) - 1);
    
    printf("[CM CORE] Sending REGISTER_ACK to fd=%d (assigned_id=%u)...\n", 
           client_fd, client->assigned_id);
    int send_result = send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
    printf("[CM CORE] send_ipc_msg returned: %d\n", send_result);
    
    printf("\n[CM CORE] ✓ DLM Registered:\n");
    printf("    DLM ID:          %s\n", client->dlm_id);
    printf("    Link Profile:    %s\n", client->link_profile_id);
    printf("    Interface:       %s\n", client->iface_name);
    printf("    Assigned ID:     %u\n", client->assigned_id);
    printf("    Max Bandwidth:   %u kbps\n", client->max_bw_kbps);
    printf("    Latency:         %u ms\n", client->latency_ms);
    printf("    Cost Index:      %u\n", client->cost_index);
    printf("    Priority:        %u\n", client->priority);
    printf("\n");
}

/*===========================================================================
 * Handle Link Event
 *===========================================================================*/

void handle_link_event(int client_fd, const MsgLinkEvent* event)
{
    pthread_mutex_lock(&g_clients_mutex);
    
    ActiveDlmClient* client = find_client_by_fd(client_fd);
    if (client) {
        client->is_link_up = event->is_link_up;
        client->current_bw_kbps = event->current_bw_kbps;
        
        printf("[CM CORE] Link Event from %s: %s\n",
               client->dlm_id,
               event->is_link_up ? "UP ✓" : "DOWN ✗");
        
        if (event->is_link_up) {
            struct in_addr ip;
            ip.s_addr = event->ip_address;
            printf("    IP:        %s\n", inet_ntoa(ip));
            printf("    Bandwidth: %u kbps\n", event->current_bw_kbps);
            printf("    Latency:   %u ms\n", event->current_latency_ms);
        }
        printf("\n");
    }
    
    pthread_mutex_unlock(&g_clients_mutex);
}

/*===========================================================================
 * Handle Heartbeat
 *===========================================================================*/

void handle_heartbeat(int client_fd, const MsgHeartbeat* hb)
{
    pthread_mutex_lock(&g_clients_mutex);
    
    ActiveDlmClient* client = find_client_by_fd(client_fd);
    if (client) {
        client->last_heartbeat = time(NULL);
        client->tx_bytes = hb->tx_bytes;
        client->rx_bytes = hb->rx_bytes;
    }
    
    pthread_mutex_unlock(&g_clients_mutex);
}

/*===========================================================================
 * Handle Client Messages
 *===========================================================================*/

void handle_client(void* arg)
{
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[4096];
    IpcHeader* header = (IpcHeader*)buffer;
    
    while (g_running) {
        int n = recv_ipc_msg(client_fd, header, buffer + sizeof(IpcHeader), 
                             sizeof(buffer) - sizeof(IpcHeader));
        if (n < 0) {
            printf("[CM CORE] Client fd=%d disconnected\n", client_fd);
            break;
        }
        
        // Process message based on type
        switch (header->type) {
            case MSG_TYPE_REGISTER:
                handle_registration(client_fd, (MsgRegister*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_LINK_EVENT:
                handle_link_event(client_fd, (MsgLinkEvent*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_HEARTBEAT:
                handle_heartbeat(client_fd, (MsgHeartbeat*)(buffer + sizeof(IpcHeader)));
                break;
                
            case MSG_TYPE_SHUTDOWN:
                printf("[CM CORE] DLM requested shutdown\n");
                goto cleanup;
                
            default:
                printf("[CM CORE] Unknown message type: 0x%02x\n", header->type);
                break;
        }
    }
    
cleanup:
    // Remove client
    pthread_mutex_lock(&g_clients_mutex);
    ActiveDlmClient* client = find_client_by_fd(client_fd);
    if (client) {
        printf("[CM CORE] Removing client: %s\n", client->dlm_id);
        client->active = false;
    }
    pthread_mutex_unlock(&g_clients_mutex);
    
    close(client_fd);
}

/*===========================================================================
 * Print Active Links
 *===========================================================================*/

void print_active_links(void)
{
    pthread_mutex_lock(&g_clients_mutex);
    
    int count = 0;
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (g_clients[i].active) count++;
    }
    
    printf("\n========================================\n");
    printf(" Active Links: %d\n", count);
    printf("========================================\n");
    
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
        if (!g_clients[i].active) continue;
        
        ActiveDlmClient* c = &g_clients[i];
        printf(" [%u] %s (%s) - %s\n",
               c->assigned_id,
               c->dlm_id,
               c->iface_name,
               c->is_link_up ? "UP" : "DOWN");
    }
    printf("========================================\n\n");
    
    pthread_mutex_unlock(&g_clients_mutex);
}

/*===========================================================================
 * Status Thread
 *===========================================================================*/

void* status_thread_func(void* arg)
{
    (void)arg;
    
    while (g_running) {
        sleep(30);  // 每 30 秒打印一次状态
        print_active_links();
    }
    
    return NULL;
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void)
{
    // 禁用输出缓冲，确保日志实时写入
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    printf("========================================\n");
    printf("  MAGIC CM Core Server (Simple)\n");
    printf("========================================\n\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    memset(g_clients, 0, sizeof(g_clients));
    
    // Create server socket
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // Remove old socket file
    unlink(MAGIC_CORE_SOCKET_PATH);
    
    // Bind
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MAGIC_CORE_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        return 1;
    }
    
    // Listen
    if (listen(g_server_fd, 10) < 0) {
        perror("listen");
        close(g_server_fd);
        return 1;
    }
    
    printf("[CM CORE] Server listening on %s\n", MAGIC_CORE_SOCKET_PATH);
    printf("[CM CORE] Waiting for DLM connections...\n\n");
    
    // Start status thread
    pthread_t status_thread;
    pthread_create(&status_thread, NULL, status_thread_func, NULL);
    
    // Accept connections
    while (g_running) {
        fd_set readfds;
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        
        FD_ZERO(&readfds);
        FD_SET(g_server_fd, &readfds);
        
        int ret = select(g_server_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) break;
        if (ret == 0) continue;
        
        int client_fd = accept(g_server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        printf("[CM CORE] New connection accepted (fd=%d)\n", client_fd);
        
        // Create thread for client
        pthread_t client_thread;
        int* fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        
        if (pthread_create(&client_thread, NULL, (void*(*)(void*))handle_client, fd_ptr) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(fd_ptr);
        } else {
            pthread_detach(client_thread);
        }
    }
    
    // Cleanup
    printf("\n[CM CORE] Shutting down...\n");
    pthread_join(status_thread, NULL);
    close(g_server_fd);
    unlink(MAGIC_CORE_SOCKET_PATH);
    
    printf("[CM CORE] Stopped\n");
    return 0;
}
