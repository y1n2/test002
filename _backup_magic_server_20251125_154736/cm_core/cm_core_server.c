/**
 * @file cm_core_server.c
 * @brief Central Management Core Server
 * @description CM Core 作为服务器端，监听 Unix Domain Socket，
 *              接收来自 DLM 进程的注册和状态更新消息
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include "../lmi/magic_ipc_protocol.h"

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_LINKS           10
#define MAX_CLIENTS         10
#define HEARTBEAT_TIMEOUT   30      // 30秒无心跳则认为DLM离线

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/* 已注册的链路信息 */
typedef struct {
    bool                    active;
    uint32_t                link_id;
    int                     client_fd;          // DLM 客户端 socket
    pid_t                   dlm_pid;
    
    /* Link Profile */
    char                    link_name[MAX_LINK_NAME_LEN];
    char                    interface_name[MAX_IFACE_NAME_LEN];
    ipc_link_type_t         link_type;
    ipc_coverage_t          coverage_type;
    
    /* Capabilities */
    uint32_t                max_bandwidth_kbps;
    uint32_t                latency_ms;
    uint32_t                cost_per_mb;
    uint8_t                 priority;
    uint16_t                mtu;
    
    /* Current State */
    ipc_link_state_t        current_state;
    uint32_t                current_bandwidth;
    int32_t                 signal_strength;
    
    /* Statistics */
    uint64_t                tx_bytes;
    uint64_t                rx_bytes;
    uint32_t                tx_packets;
    uint32_t                rx_packets;
    
    /* Health Monitoring */
    time_t                  last_heartbeat;
    time_t                  registered_time;
    uint32_t                heartbeat_miss_count;
} registered_link_t;

/* CM Core 全局上下文 */
typedef struct {
    int                     server_fd;
    bool                    running;
    
    registered_link_t       links[MAX_LINKS];
    int                     active_link_count;
    uint32_t                next_link_id;
    
    pthread_mutex_t         links_mutex;
    pthread_t               accept_thread;
    pthread_t               monitor_thread;
} cm_core_context_t;

static cm_core_context_t g_cm_ctx;

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

static int cm_init_server_socket(void);
static void* cm_accept_thread_func(void *arg);
static void* cm_monitor_thread_func(void *arg);
static void cm_handle_client_message(int client_fd);
static void cm_process_register_request(int client_fd, const ipc_register_req_t *req);
static void cm_process_heartbeat(int client_fd, const ipc_heartbeat_t *hb);
static void cm_process_link_status(int client_fd, const ipc_link_status_t *status);
static void cm_process_stats_response(int client_fd, const ipc_stats_resp_t *stats);
static void cm_send_register_response(int client_fd, uint32_t link_id, uint8_t result, const char *error_msg);
static registered_link_t* cm_find_link_by_fd(int client_fd);
static registered_link_t* cm_find_link_by_id(uint32_t link_id);
static void cm_print_active_links(void);
static void cm_signal_handler(int signum);

/*===========================================================================
 * Main Entry
 *===========================================================================*/

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("  MAGIC CM Core Server v1.0\n");
    printf("========================================\n");
    
    // 初始化上下文
    memset(&g_cm_ctx, 0, sizeof(cm_core_context_t));
    g_cm_ctx.running = true;
    g_cm_ctx.next_link_id = 1000;
    pthread_mutex_init(&g_cm_ctx.links_mutex, NULL);
    
    // 设置信号处理
    signal(SIGINT, cm_signal_handler);
    signal(SIGTERM, cm_signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略 SIGPIPE（客户端断开时）
    
    // 初始化服务器 Socket
    if (cm_init_server_socket() < 0) {
        fprintf(stderr, "[CM] Failed to initialize server socket\n");
        return 1;
    }
    
    printf("[CM] Server listening on %s\n", CM_SOCKET_PATH);
    printf("[CM] Waiting for DLM connections...\n\n");
    
    // 启动接受连接线程
    if (pthread_create(&g_cm_ctx.accept_thread, NULL, cm_accept_thread_func, NULL) != 0) {
        fprintf(stderr, "[CM] Failed to create accept thread\n");
        return 1;
    }
    
    // 启动监控线程
    if (pthread_create(&g_cm_ctx.monitor_thread, NULL, cm_monitor_thread_func, NULL) != 0) {
        fprintf(stderr, "[CM] Failed to create monitor thread\n");
        return 1;
    }
    
    // 主线程等待退出信号
    while (g_cm_ctx.running) {
        sleep(1);
    }
    
    // 清理资源
    printf("\n[CM] Shutting down server...\n");
    pthread_join(g_cm_ctx.accept_thread, NULL);
    pthread_join(g_cm_ctx.monitor_thread, NULL);
    
    close(g_cm_ctx.server_fd);
    unlink(CM_SOCKET_PATH);
    pthread_mutex_destroy(&g_cm_ctx.links_mutex);
    
    printf("[CM] Server stopped\n");
    return 0;
}

/*===========================================================================
 * Initialize Server Socket
 *===========================================================================*/

static int cm_init_server_socket(void)
{
    struct sockaddr_un addr;
    
    // 创建 Unix Domain Socket
    g_cm_ctx.server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_cm_ctx.server_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // 删除旧的 socket 文件
    unlink(CM_SOCKET_PATH);
    
    // 绑定地址
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CM_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(g_cm_ctx.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_cm_ctx.server_fd);
        return -1;
    }
    
    // 监听
    if (listen(g_cm_ctx.server_fd, DLM_SOCKET_BACKLOG) < 0) {
        perror("listen");
        close(g_cm_ctx.server_fd);
        return -1;
    }
    
    return 0;
}

/*===========================================================================
 * Accept Thread - 接受新的 DLM 连接
 *===========================================================================*/

static void* cm_accept_thread_func(void *arg)
{
    (void)arg;
    
    while (g_cm_ctx.running) {
        struct sockaddr_un client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(g_cm_ctx.server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        
        printf("[CM] New DLM connection accepted (fd=%d)\n", client_fd);
        
        // 为每个客户端创建处理线程
        pthread_t client_thread;
        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        
        if (pthread_create(&client_thread, NULL, (void*(*)(void*))cm_handle_client_message, fd_ptr) != 0) {
            fprintf(stderr, "[CM] Failed to create client thread\n");
            close(client_fd);
            free(fd_ptr);
        } else {
            pthread_detach(client_thread);  // 分离线程，自动回收资源
        }
    }
    
    return NULL;
}

/*===========================================================================
 * Handle Client Messages
 *===========================================================================*/

static void cm_handle_client_message(int client_fd)
{
    ipc_message_t msg;
    
    while (g_cm_ctx.running) {
        int received = ipc_recv_message(client_fd, &msg, sizeof(msg));
        if (received < 0) {
            printf("[CM] Client fd=%d disconnected\n", client_fd);
            break;
        }
        
        printf("[CM] Received message: type=%s, seq=%u, link_id=%u\n",
               ipc_msg_type_to_string(msg.header.msg_type),
               msg.header.sequence_num,
               msg.header.link_id);
        
        // 处理不同类型的消息
        switch (msg.header.msg_type) {
            case MSG_REGISTER_REQUEST:
                cm_process_register_request(client_fd, &msg.register_req);
                break;
                
            case MSG_HEARTBEAT:
                cm_process_heartbeat(client_fd, &msg.heartbeat);
                break;
                
            case MSG_LINK_UP:
            case MSG_LINK_DOWN:
            case MSG_LINK_DEGRADED:
            case MSG_LINK_RESTORED:
                cm_process_link_status(client_fd, &msg.link_status);
                break;
                
            case MSG_STATS_RESPONSE:
                cm_process_stats_response(client_fd, &msg.stats_resp);
                break;
                
            case MSG_UNREGISTER:
                printf("[CM] DLM unregistering (link_id=%u)\n", msg.header.link_id);
                goto cleanup;
                
            default:
                printf("[CM] Unknown message type: 0x%04x\n", msg.header.msg_type);
                break;
        }
    }
    
cleanup:
    // 清理该客户端的链路信息
    pthread_mutex_lock(&g_cm_ctx.links_mutex);
    registered_link_t *link = cm_find_link_by_fd(client_fd);
    if (link) {
        printf("[CM] Removing link: %s (link_id=%u)\n", link->link_name, link->link_id);
        link->active = false;
        g_cm_ctx.active_link_count--;
    }
    pthread_mutex_unlock(&g_cm_ctx.links_mutex);
    
    close(client_fd);
}

/*===========================================================================
 * Process REGISTER_REQUEST
 *===========================================================================*/

static void cm_process_register_request(int client_fd, const ipc_register_req_t *req)
{
    pthread_mutex_lock(&g_cm_ctx.links_mutex);
    
    // 查找空闲槽位
    registered_link_t *link = NULL;
    for (int i = 0; i < MAX_LINKS; i++) {
        if (!g_cm_ctx.links[i].active) {
            link = &g_cm_ctx.links[i];
            break;
        }
    }
    
    if (!link) {
        pthread_mutex_unlock(&g_cm_ctx.links_mutex);
        cm_send_register_response(client_fd, 0, 1, "No available slot");
        return;
    }
    
    // 分配链路 ID
    uint32_t new_link_id = g_cm_ctx.next_link_id++;
    
    // 填充链路信息
    memset(link, 0, sizeof(registered_link_t));
    link->active = true;
    link->link_id = new_link_id;
    link->client_fd = client_fd;
    link->dlm_pid = req->dlm_pid;
    
    strncpy(link->link_name, req->link_name, MAX_LINK_NAME_LEN - 1);
    strncpy(link->interface_name, req->interface_name, MAX_IFACE_NAME_LEN - 1);
    link->link_type = req->link_type;
    link->coverage_type = req->coverage_type;
    
    link->max_bandwidth_kbps = req->max_bandwidth_kbps;
    link->latency_ms = req->latency_ms;
    link->cost_per_mb = req->cost_per_mb;
    link->priority = req->priority;
    link->mtu = req->mtu;
    
    link->current_state = LINK_STATE_AVAILABLE;
    link->registered_time = time(NULL);
    link->last_heartbeat = time(NULL);
    
    g_cm_ctx.active_link_count++;
    
    pthread_mutex_unlock(&g_cm_ctx.links_mutex);
    
    // 发送成功响应
    cm_send_register_response(client_fd, new_link_id, 0, "Registration successful");
    
    printf("[CM] ✓ DLM registered successfully:\n");
    printf("     Link ID:     %u\n", new_link_id);
    printf("     Name:        %s\n", link->link_name);
    printf("     Type:        %s\n", ipc_link_type_to_string(link->link_type));
    printf("     Interface:   %s\n", link->interface_name);
    printf("     Max BW:      %u kbps\n", link->max_bandwidth_kbps);
    printf("     Latency:     %u ms\n", link->latency_ms);
    printf("     Cost:        %u cents/MB\n", link->cost_per_mb);
    printf("     Priority:    %u\n", link->priority);
    printf("\n");
    
    cm_print_active_links();
}

/*===========================================================================
 * Send REGISTER_RESPONSE
 *===========================================================================*/

static void cm_send_register_response(int client_fd, uint32_t link_id, uint8_t result, const char *error_msg)
{
    ipc_register_resp_t resp;
    
    ipc_init_header(&resp.header, MSG_REGISTER_RESPONSE, sizeof(resp), link_id);
    resp.assigned_link_id = link_id;
    resp.registration_result = result;
    strncpy(resp.error_msg, error_msg ? error_msg : "", sizeof(resp.error_msg) - 1);
    
    if (ipc_send_message(client_fd, &resp, sizeof(resp)) < 0) {
        fprintf(stderr, "[CM] Failed to send register response\n");
    }
}

/*===========================================================================
 * Process HEARTBEAT
 *===========================================================================*/

static void cm_process_heartbeat(int client_fd, const ipc_heartbeat_t *hb)
{
    pthread_mutex_lock(&g_cm_ctx.links_mutex);
    
    registered_link_t *link = cm_find_link_by_id(hb->header.link_id);
    if (link) {
        link->last_heartbeat = time(NULL);
        link->heartbeat_miss_count = 0;
        link->current_state = hb->link_state;
        
        // 更新统计信息
        link->tx_bytes = hb->bytes_sent;
        link->rx_bytes = hb->bytes_received;
        link->tx_packets = hb->packets_sent;
        link->rx_packets = hb->packets_received;
        
        // 发送心跳确认
        ipc_msg_header_t ack;
        ipc_init_header(&ack, MSG_HEARTBEAT_ACK, sizeof(ack), link->link_id);
        ipc_send_message(client_fd, &ack, sizeof(ack));
    }
    
    pthread_mutex_unlock(&g_cm_ctx.links_mutex);
}

/*===========================================================================
 * Process LINK_STATUS
 *===========================================================================*/

static void cm_process_link_status(int client_fd, const ipc_link_status_t *status)
{
    (void)client_fd;
    
    pthread_mutex_lock(&g_cm_ctx.links_mutex);
    
    registered_link_t *link = cm_find_link_by_id(status->header.link_id);
    if (link) {
        ipc_link_state_t old_state = link->current_state;
        link->current_state = status->new_state;
        link->current_bandwidth = status->current_bandwidth_kbps;
        link->signal_strength = status->signal_strength_dbm;
        
        printf("[CM] Link %s state changed: %s → %s\n",
               link->link_name,
               ipc_link_state_to_string(old_state),
               ipc_link_state_to_string(status->new_state));
        
        if (status->status_message[0]) {
            printf("     Message: %s\n", status->status_message);
        }
        
        printf("     Bandwidth: %u kbps, Signal: %d dBm\n\n",
               status->current_bandwidth_kbps,
               status->signal_strength_dbm);
    }
    
    pthread_mutex_unlock(&g_cm_ctx.links_mutex);
}

/*===========================================================================
 * Process STATS_RESPONSE
 *===========================================================================*/

static void cm_process_stats_response(int client_fd, const ipc_stats_resp_t *stats)
{
    (void)client_fd;
    
    pthread_mutex_lock(&g_cm_ctx.links_mutex);
    
    registered_link_t *link = cm_find_link_by_id(stats->header.link_id);
    if (link) {
        link->tx_bytes = stats->tx_bytes;
        link->rx_bytes = stats->rx_bytes;
        link->tx_packets = stats->tx_packets;
        link->rx_packets = stats->rx_packets;
        link->signal_strength = stats->signal_strength_dbm;
        
        printf("[CM] Statistics for %s:\n", link->link_name);
        printf("     TX: %lu bytes (%lu packets)\n", stats->tx_bytes, stats->tx_packets);
        printf("     RX: %lu bytes (%lu packets)\n", stats->rx_bytes, stats->rx_packets);
        printf("     Signal: %d dBm, Quality: %d%%\n", stats->signal_strength_dbm, stats->signal_quality);
        printf("\n");
    }
    
    pthread_mutex_unlock(&g_cm_ctx.links_mutex);
}

/*===========================================================================
 * Monitor Thread - 检测 DLM 健康状态
 *===========================================================================*/

static void* cm_monitor_thread_func(void *arg)
{
    (void)arg;
    
    while (g_cm_ctx.running) {
        sleep(10);  // 每10秒检查一次
        
        pthread_mutex_lock(&g_cm_ctx.links_mutex);
        
        time_t now = time(NULL);
        for (int i = 0; i < MAX_LINKS; i++) {
            if (!g_cm_ctx.links[i].active) continue;
            
            registered_link_t *link = &g_cm_ctx.links[i];
            time_t elapsed = now - link->last_heartbeat;
            
            if (elapsed > HEARTBEAT_TIMEOUT) {
                printf("[CM] ⚠ Link %s heartbeat timeout (%ld sec)\n", 
                       link->link_name, elapsed);
                
                link->heartbeat_miss_count++;
                
                // 如果连续3次超时，标记为不可用
                if (link->heartbeat_miss_count >= 3) {
                    printf("[CM] ✗ Link %s marked as UNAVAILABLE\n", link->link_name);
                    link->current_state = LINK_STATE_UNAVAILABLE;
                }
            }
        }
        
        pthread_mutex_unlock(&g_cm_ctx.links_mutex);
    }
    
    return NULL;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static registered_link_t* cm_find_link_by_fd(int client_fd)
{
    for (int i = 0; i < MAX_LINKS; i++) {
        if (g_cm_ctx.links[i].active && g_cm_ctx.links[i].client_fd == client_fd) {
            return &g_cm_ctx.links[i];
        }
    }
    return NULL;
}

static registered_link_t* cm_find_link_by_id(uint32_t link_id)
{
    for (int i = 0; i < MAX_LINKS; i++) {
        if (g_cm_ctx.links[i].active && g_cm_ctx.links[i].link_id == link_id) {
            return &g_cm_ctx.links[i];
        }
    }
    return NULL;
}

static void cm_print_active_links(void)
{
    printf("========================================\n");
    printf(" Active Links: %d\n", g_cm_ctx.active_link_count);
    printf("========================================\n");
    
    for (int i = 0; i < MAX_LINKS; i++) {
        if (!g_cm_ctx.links[i].active) continue;
        
        registered_link_t *link = &g_cm_ctx.links[i];
        printf(" [%u] %s (%s) - %s\n",
               link->link_id,
               link->link_name,
               link->interface_name,
               ipc_link_state_to_string(link->current_state));
    }
    printf("========================================\n\n");
}

static void cm_signal_handler(int signum)
{
    (void)signum;
    printf("\n[CM] Received shutdown signal\n");
    g_cm_ctx.running = false;
}
