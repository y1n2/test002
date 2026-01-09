/**
 * @file dlm_common.c
 * @brief DLM 通用函数实现
 */

#include "dlm_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

/*===========================================================================
 * Context Initialization
 *===========================================================================*/

void dlm_init_context(DlmContext* ctx, const MsgRegister* initial_config)
{
    memset(ctx, 0, sizeof(DlmContext));
    memcpy(&ctx->config, initial_config, sizeof(MsgRegister));
    ctx->ipc_fd = -1;
    ctx->running = true;
    pthread_mutex_init(&ctx->mutex, NULL);
}

/*===========================================================================
 * Registration
 *===========================================================================*/

int dlm_register(DlmContext* ctx)
{
    struct sockaddr_un addr;
    
    printf("[%s] Connecting to CM Core...\n", ctx->config.dlm_id);
    
    // 1. 创建 Socket
    ctx->ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ctx->ipc_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // 2. 连接到 CM Core
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MAGIC_CORE_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // 重试连接（CM 可能还没启动）
    for (int retry = 0; retry < 5; retry++) {
        if (connect(ctx->ipc_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            break;
        }
        
        if (retry < 4) {
            printf("[%s] CM Core not ready, retrying in 2s...\n", ctx->config.dlm_id);
            sleep(2);
        } else {
            perror("connect");
            close(ctx->ipc_fd);
            ctx->ipc_fd = -1;
            return -1;
        }
    }
    
    printf("[%s] Connected to CM Core\n", ctx->config.dlm_id);
    
    // 3. 发送注册消息
    if (send_ipc_msg(ctx->ipc_fd, MSG_TYPE_REGISTER, &ctx->config, sizeof(MsgRegister)) < 0) {
        fprintf(stderr, "[%s] Failed to send REGISTER message\n", ctx->config.dlm_id);
        close(ctx->ipc_fd);
        ctx->ipc_fd = -1;
        return -1;
    }
    
    printf("[%s] Sent REGISTER message to CM Core\n", ctx->config.dlm_id);
    printf("    Link Profile ID: %s\n", ctx->config.link_profile_id);
    printf("    Interface:       %s\n", ctx->config.iface_name);
    printf("    Max Bandwidth:   %u kbps\n", ctx->config.max_bw_kbps);
    printf("    Latency:         %u ms\n", ctx->config.typical_latency_ms);
    printf("    Cost Index:      %u\n", ctx->config.cost_index);
    printf("    Priority:        %u\n", ctx->config.priority);
    
    // 4. 等待 ACK
    printf("[%s] Waiting for REGISTER_ACK...\n", ctx->config.dlm_id);
    IpcHeader ack_header;
    MsgRegisterAck ack;
    
    int recv_result = recv_ipc_msg(ctx->ipc_fd, &ack_header, &ack, sizeof(ack));
    printf("[%s] recv_ipc_msg returned: %d (errno=%d, %s)\n", 
           ctx->config.dlm_id, recv_result, errno, strerror(errno));
    
    if (recv_result < 0) {
        fprintf(stderr, "[%s] Failed to receive REGISTER_ACK\n", ctx->config.dlm_id);
        close(ctx->ipc_fd);
        ctx->ipc_fd = -1;
        return -1;
    }
    
    printf("[%s] Received ACK: type=0x%02x, result=%u, assigned_id=%u\n",
           ctx->config.dlm_id, ack_header.type, ack.result, ack.assigned_id);
    
    if (ack.result != 0) {
        fprintf(stderr, "[%s] Registration failed: %s\n", ctx->config.dlm_id, ack.message);
        close(ctx->ipc_fd);
        ctx->ipc_fd = -1;
        return -1;
    }
    
    ctx->assigned_id = ack.assigned_id;
    ctx->registered = true;
    
    printf("[%s] ✓ Registration successful! Assigned ID: %u\n", 
           ctx->config.dlm_id, ctx->assigned_id);
    
    return 0;
}

/*===========================================================================
 * Network Interface Status Check
 *===========================================================================*/

bool check_eth_link_status(const char* iface_name)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ - 1);
    
    // 获取接口标志
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    close(sock);
    
    // 检查 UP 和 RUNNING 标志
    return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
}

/*===========================================================================
 * Get Network Interface Statistics
 *===========================================================================*/

int get_eth_stats(const char* iface_name, uint64_t* tx_bytes, uint64_t* rx_bytes,
                  uint32_t* tx_packets, uint32_t* rx_packets)
{
    char path[256];
    FILE* fp;
    
    // 读取 TX bytes
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", iface_name);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%lu", tx_bytes) != 1) *tx_bytes = 0;
        fclose(fp);
    }
    
    // 读取 RX bytes
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", iface_name);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%lu", rx_bytes) != 1) *rx_bytes = 0;
        fclose(fp);
    }
    
    // 读取 TX packets
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_packets", iface_name);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%u", tx_packets) != 1) *tx_packets = 0;
        fclose(fp);
    }
    
    // 读取 RX packets
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_packets", iface_name);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%u", rx_packets) != 1) *rx_packets = 0;
        fclose(fp);
    }
    
    return 0;
}

/*===========================================================================
 * Get Network Interface IP Info
 *===========================================================================*/

int get_eth_ip_info(const char* iface_name, uint32_t* ip_addr, uint32_t* netmask)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ - 1);
    
    // 获取 IP 地址
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
        *ip_addr = addr->sin_addr.s_addr;
    } else {
        *ip_addr = 0;
    }
    
    // 获取子网掩码
    if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_netmask;
        *netmask = addr->sin_addr.s_addr;
    } else {
        *netmask = 0;
    }
    
    close(sock);
    return 0;
}

/*===========================================================================
 * Send Link Event
 *===========================================================================*/

int send_link_event(DlmContext* ctx, bool is_up)
{
    MsgLinkEvent event;
    memset(&event, 0, sizeof(event));
    
    strncpy(event.dlm_id, ctx->config.dlm_id, sizeof(event.dlm_id) - 1);
    event.is_link_up = is_up;
    
    if (is_up) {
        event.current_bw_kbps = ctx->config.max_bw_kbps;
        event.current_latency_ms = ctx->config.typical_latency_ms;
        event.ip_address = ctx->ip_address;
        event.netmask = ctx->netmask;
    } else {
        event.current_bw_kbps = 0;
        event.current_latency_ms = 0;
    }
    
    return send_ipc_msg(ctx->ipc_fd, MSG_TYPE_LINK_EVENT, &event, sizeof(event));
}

/*===========================================================================
 * Link Monitor Thread
 *===========================================================================*/

void* link_monitor_loop(void* arg)
{
    DlmContext* ctx = (DlmContext*)arg;
    bool last_state = false;
    
    printf("[%s] Monitor thread started\n", ctx->config.dlm_id);
    
    while (ctx->running) {
        sleep(3);  // 每 3 秒检查一次
        
        // 检查网卡状态
        bool current_state = check_eth_link_status(ctx->config.iface_name);
        
        pthread_mutex_lock(&ctx->mutex);
        
        // 更新统计信息
        get_eth_stats(ctx->config.iface_name, 
                      &ctx->tx_bytes, &ctx->rx_bytes,
                      &ctx->tx_packets, &ctx->rx_packets);
        
        // 更新 IP 信息
        if (current_state) {
            get_eth_ip_info(ctx->config.iface_name, &ctx->ip_address, &ctx->netmask);
        }
        
        ctx->is_up = current_state;
        
        pthread_mutex_unlock(&ctx->mutex);
        
        // 状态变化时发送事件
        if (current_state != last_state) {
            printf("[%s] Link state changed: %s -> %s\n",
                   ctx->config.dlm_id,
                   last_state ? "UP" : "DOWN",
                   current_state ? "UP" : "DOWN");
            
            send_link_event(ctx, current_state);
            last_state = current_state;
            
            if (current_state) {
                struct in_addr ip;
                ip.s_addr = ctx->ip_address;
                printf("[%s] IP Address: %s\n", ctx->config.dlm_id, inet_ntoa(ip));
            }
        }
    }
    
    printf("[%s] Monitor thread stopped\n", ctx->config.dlm_id);
    return NULL;
}

/*===========================================================================
 * Heartbeat Thread
 *===========================================================================*/

void* heartbeat_loop(void* arg)
{
    DlmContext* ctx = (DlmContext*)arg;
    
    printf("[%s] Heartbeat thread started\n", ctx->config.dlm_id);
    
    while (ctx->running) {
        sleep(10);  // 每 10 秒发送心跳
        
        MsgHeartbeat hb;
        memset(&hb, 0, sizeof(hb));
        
        pthread_mutex_lock(&ctx->mutex);
        strncpy(hb.dlm_id, ctx->config.dlm_id, sizeof(hb.dlm_id) - 1);
        hb.is_healthy = ctx->is_up;
        hb.tx_bytes = ctx->tx_bytes;
        hb.rx_bytes = ctx->rx_bytes;
        pthread_mutex_unlock(&ctx->mutex);
        
        if (send_ipc_msg(ctx->ipc_fd, MSG_TYPE_HEARTBEAT, &hb, sizeof(hb)) < 0) {
            fprintf(stderr, "[%s] Failed to send heartbeat\n", ctx->config.dlm_id);
        }
    }
    
    printf("[%s] Heartbeat thread stopped\n", ctx->config.dlm_id);
    return NULL;
}
