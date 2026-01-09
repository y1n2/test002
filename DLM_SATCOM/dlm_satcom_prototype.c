/**
 * ============================================================================
 * @file    dlm_satcom_prototype.c
 * @brief   SATCOM 数据链路管理器 (DLM) - 原型阶段实现
 * 
 * @details 遵循 ARINC 839-2014 MIH 协议规范的 SATCOM 链路原型实现
 *          - 配置管理器 (CM): 管理静态配置参数
 *          - 状态模拟器 (SS): 管理动态运行状态
 *          - 原型阶段"仅报告不操作"原则
 *          - 仅 Link_Up/Link_Down 执行物理操作
 * 
 * @note    SATCOM 链路规格:
 *          - Link ID: 0x01
 *          - 接口名: eth_sat
 *          - 延迟: 600ms (GEO 卫星)
 *          - 最大带宽: FL 30 Mbps / RL 2 Mbps (非对称)
 *          - 成本因子: 0.5 (高成本)
 *          - RSSI 阈值: -85 dBm
 * 
 * @author  MAGIC 航空通信系统团队
 * @version 1.0.0
 * @date    2025
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
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

/* 引入公共 DLM 定义 */
#include "../DLM_COMMON/dlm_common.h"

/* 引入 MIH 协议定义 */
#include "../extensions/app_magic/mih_protocol.h"

/* ============================================================================
 * SATCOM 链路特定常量
 * ============================================================================ */

#define DEFAULT_CONFIG_PATH         "../DLM_CONFIG/dlm_satcom.ini"

/* ============================================================================
 * 全局上下文
 * ============================================================================ */

static dlm_context_t g_ctx;
static dlm_network_config_t g_net_config;
static volatile int g_running = 1;
static struct sockaddr_un g_mihf_addr;

/* ============================================================================
 * 前向声明
 * ============================================================================ */

static int dlm_init_config_manager(const char* config_path);
static int dlm_init_state_simulator(void);
static int dlm_init_socket(void);
static int dlm_send_to_mihf(uint16_t msg_type, const void* data, size_t len);
static void dlm_signal_handler(int sig);
static void* dlm_reporting_thread(void* arg);
static void* dlm_simulation_thread(void* arg);
static void* dlm_packet_monitor_thread(void* arg);

/* ============================================================================
 * 网卡状态监控
 * ============================================================================ */

/**
 * @brief 检查网卡实际物理状态
 * @return true=网卡UP且有链路, false=网卡DOWN或无链路
 */
static bool check_interface_status(const char *iface) {
  char path[128];
  char status[16];
  FILE *fp;
  
  snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
  fp = fopen(path, "r");
  if (!fp) {
    return false;
  }
  
  if (fgets(status, sizeof(status), fp) == NULL) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  
  status[strcspn(status, "\n")] = 0;
  return (strcmp(status, "up") == 0);
}

/* ============================================================================
 * 配置管理器初始化 - 从配置文件加载
 * ============================================================================ */

static int dlm_init_config_manager(const char* config_path) {
    /* 从配置文件加载 */
    if (dlm_load_config(config_path, &g_ctx.config, &g_net_config) != 0) {
        fprintf(stderr, "[SATCOM] 加载配置文件失败: %s\n", config_path);
        return -1;
    }
    
    printf("[SATCOM-CM] 配置管理器初始化完成:\n");
    printf("  - Link ID: 0x%02X, 接口: %s\n", 
           g_ctx.config.link_id, g_ctx.config.interface_name);
    printf("  - IP: %s, 网关: %s\n",
           g_net_config.ip_address, g_net_config.gateway);
    printf("  - 带宽 FL/RL: %u/%u kbps (高度非对称)\n",
           g_ctx.config.max_bandwidth_fl, g_ctx.config.max_bandwidth_rl);
    printf("  - 延迟: %u ms (GEO卫星), RSSI阈值: %d dBm\n",
           g_ctx.config.reported_delay_ms, g_ctx.config.rssi_threshold_dbm);
    printf("  - 成本因子: %.2f (高成本卫星通信)\n", g_ctx.config.cost_factor);
    
    return 0;
}

/* ============================================================================
 * 状态模拟器初始化
 * ============================================================================ */

static int dlm_init_state_simulator(void) {
    dlm_state_init(&g_ctx.state);
    g_ctx.state.is_connected = 0;
    g_ctx.state.simulated_rssi = g_net_config.initial_rssi_dbm;
    g_ctx.state.signal_quality = 75;
    
    printf("[SATCOM-SS] 状态模拟器初始化完成:\n");
    printf("  - RSSI: %d dBm, 信号质量: %u%%\n",
           g_ctx.state.simulated_rssi, g_ctx.state.signal_quality);
    
    return 0;
}

/* ============================================================================
 * 物理链路操作
 * ============================================================================ */

static int dlm_physical_link_up(void) {
    char cmd[256];
    int ret;
    
    printf("[SATCOM-PHY] 激活链路: %s\n", g_ctx.config.interface_name);
    
    /* 启用接口 */
    ret = dlm_interface_up(g_ctx.config.interface_name);
    
    /* 配置IP地址 */
    snprintf(cmd, sizeof(cmd), "ip addr add %s/%s dev %s 2>/dev/null",
             g_net_config.ip_address, g_net_config.netmask, g_ctx.config.interface_name);
    system(cmd);
    printf("[SATCOM-PHY] 配置IP: %s/%s\n", g_net_config.ip_address, g_net_config.netmask);
    
    pthread_mutex_lock(&g_ctx.state.mutex);
    g_ctx.state.is_connected = 1;
    g_ctx.state.interface_up = 1;
    g_ctx.state.is_going_down = 0;
    g_ctx.state.last_up_time = time(NULL);
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    /* 发送 Link_Up 指示 */
    LINK_Up_Indication ind;
    memset(&ind, 0, sizeof(ind));
    ind.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(ind.link_identifier.link_addr, sizeof(ind.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符，与服务器配置匹配 */
    snprintf(ind.link_identifier.poa_addr, sizeof(ind.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    ind.up_timestamp = (uint32_t)time(NULL);
    ind.parameters.current_tx_rate_kbps = g_ctx.config.max_bandwidth_rl;
    ind.parameters.current_rx_rate_kbps = g_ctx.config.max_bandwidth_fl;
    ind.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;
    ind.parameters.signal_quality = g_ctx.state.signal_quality;
    ind.parameters.current_latency_ms = g_ctx.config.reported_delay_ms;
    
    dlm_send_to_mihf(MIH_LINK_UP_IND, &ind, sizeof(ind));
    printf("[SATCOM-IND] 发送 Link_Up.indication\n");
    
    return ret;
}

static int dlm_physical_link_down(int reason_code) {
    char cmd[256];
    int ret = 0;
    
    printf("[SATCOM-PHY] 停用链路: %s (原因=%d)\n", 
           g_ctx.config.interface_name, reason_code);
    
    /* 删除IP地址 - 但保留接口 UP 状态 */
    snprintf(cmd, sizeof(cmd), "ip addr del %s/%s dev %s 2>/dev/null",
             g_net_config.ip_address, g_net_config.netmask, g_ctx.config.interface_name);
    system(cmd);
    printf("[SATCOM-PHY] 删除IP: %s\n", g_net_config.ip_address);
    
    /* 
     * 注意: 不再关闭物理接口
     * 原因: 接口可能被其他服务使用，DLM 退出不应影响物理层
     * 如果需要关闭接口，请取消下面注释:
     * ret = dlm_interface_down(g_ctx.config.interface_name);
     */
    printf("[SATCOM-PHY] 保持接口 %s 状态不变\n", g_ctx.config.interface_name);
    
    pthread_mutex_lock(&g_ctx.state.mutex);
    g_ctx.state.is_connected = 0;
    g_ctx.state.interface_up = 0;
    g_ctx.state.is_going_down = 0;
    g_ctx.state.last_down_time = time(NULL);
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    /* 发送 Link_Down 指示 */
    LINK_Down_Indication ind;
    memset(&ind, 0, sizeof(ind));
    ind.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(ind.link_identifier.link_addr, sizeof(ind.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符 */
    snprintf(ind.link_identifier.poa_addr, sizeof(ind.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    ind.reason_code = (uint8_t)reason_code;
    ind.down_timestamp = (uint32_t)time(NULL);
    snprintf(ind.reason_text, sizeof(ind.reason_text), 
             "Satellite link disconnected");
    
    dlm_send_to_mihf(MIH_LINK_DOWN_IND, &ind, sizeof(ind));
    printf("[SATCOM-IND] 发送 Link_Down.indication\n");
    
    return ret;
}

/* ============================================================================
 * MIH 原语处理
 * ============================================================================ */

static int dlm_handle_capability_discover(void) {
    printf("[SATCOM-PRIM] 处理 Link_Capability_Discover.request\n");
    
    LINK_Capability_Discover_Confirm confirm;
    memset(&confirm, 0, sizeof(confirm));
    
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(confirm.link_identifier.link_addr, sizeof(confirm.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符 */
    snprintf(confirm.link_identifier.poa_addr, sizeof(confirm.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    confirm.status = STATUS_SUCCESS;
    confirm.has_capability = true;
    confirm.capability.link_type = g_ctx.config.link_id;
    confirm.capability.max_bandwidth_kbps = g_ctx.config.max_bandwidth_fl;
    confirm.capability.typical_latency_ms = g_ctx.config.reported_delay_ms;
    confirm.capability.supported_events = LINK_EVENT_ALL;
    confirm.capability.security_level = g_ctx.config.security_level;
    confirm.capability.mtu = g_ctx.config.mtu;
    confirm.capability.is_asymmetric = g_ctx.config.is_asymmetric;
    
    dlm_send_to_mihf(MIH_LINK_CAPABILITY_DISCOVER_CNF, &confirm, sizeof(confirm));
    
    printf("  - 带宽: FL=%u kbps, RL=%u kbps (非对称)\n",
           g_ctx.config.max_bandwidth_fl, g_ctx.config.max_bandwidth_rl);
    printf("  - 延迟: %u ms (GEO卫星)\n", g_ctx.config.reported_delay_ms);
    
    return 0;
}

static int dlm_handle_get_parameters(void) {
    printf("[SATCOM-PRIM] 处理 Link_Get_Parameters.request\n");
    
    pthread_mutex_lock(&g_ctx.state.mutex);
    
    LINK_Get_Parameters_Confirm confirm;
    memset(&confirm, 0, sizeof(confirm));
    
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    confirm.status = STATUS_SUCCESS;
    confirm.returned_params = LINK_PARAM_QUERY_ALL;
    confirm.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;
    confirm.parameters.signal_quality = g_ctx.state.signal_quality;
    confirm.parameters.current_tx_rate_kbps = g_ctx.config.max_bandwidth_rl - g_ctx.state.current_usage_rl;
    confirm.parameters.current_rx_rate_kbps = g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
    confirm.parameters.current_latency_ms = g_ctx.config.reported_delay_ms;
    confirm.parameters.current_jitter_ms = g_ctx.config.delay_jitter_ms;
    confirm.parameters.available_bandwidth_kbps = g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
    confirm.parameters.link_state = g_ctx.state.is_connected ? 1 : 0;
    confirm.parameters.active_bearers = g_ctx.state.num_active_bearers;
    
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    dlm_send_to_mihf(MIH_LINK_GET_PARAMETERS_CNF, &confirm, sizeof(confirm));
    
    printf("  - RSSI: %d dBm, 质量: %u%%, 可用带宽: %u kbps\n",
           confirm.parameters.signal_strength_dbm,
           confirm.parameters.signal_quality,
           confirm.parameters.available_bandwidth_kbps);
    
    return 0;
}

static int dlm_handle_event_subscribe(const LINK_Event_Subscribe_Request* req) {
    printf("[SATCOM-PRIM] 处理 Link_Event_Subscribe.request\n");
    printf("  - 请求订阅事件: 0x%04X\n", req->event_list);
    
    pthread_mutex_lock(&g_ctx.state.mutex);
    g_ctx.state.subscribed_events |= req->event_list;
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    LINK_Event_Subscribe_Confirm confirm;
    memset(&confirm, 0, sizeof(confirm));
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    confirm.status = STATUS_SUCCESS;
    confirm.subscribed_events = req->event_list;
    
    dlm_send_to_mihf(MIH_LINK_EVENT_SUBSCRIBE_CNF, &confirm, sizeof(confirm));
    return 0;
}

static int dlm_handle_link_resource(const LINK_Resource_Request* req) {
    printf("[SATCOM-PRIM] 处理 Link_Resource.request\n");
    printf("  - 操作: %s\n", resource_action_to_string(req->resource_action));
    
    LINK_Resource_Confirm confirm;
    memset(&confirm, 0, sizeof(confirm));
    
    if (req->resource_action == RESOURCE_ACTION_REQUEST) {
        /* 原型阶段: 模拟资源分配 */
        uint8_t bearer_id = 0;
        uint32_t req_bw_fl = req->has_qos_params ? req->qos_parameters.forward_link_rate : 1000;
        uint32_t req_bw_rl = req->has_qos_params ? req->qos_parameters.return_link_rate : 200;
        uint8_t cos_id = req->has_qos_params ? req->qos_parameters.cos_id : COS_BEST_EFFORT;
        
        /* SATCOM RL带宽有限，需要严格管理 */
        if (req_bw_rl > 500) {
            printf("  - [警告] SATCOM RL带宽有限, 请求 %u kbps 可能影响其他服务\n", req_bw_rl);
        }
        
        int ret = dlm_allocate_bearer(&g_ctx.state, &g_ctx.config,
                                      req_bw_fl, req_bw_rl, cos_id, &bearer_id);
        
        if (ret == 0) {
            confirm.status = STATUS_SUCCESS;
            confirm.has_bearer_id = true;
            confirm.bearer_identifier = bearer_id;
            printf("  - [原型模式] 分配 Bearer ID: %u\n", bearer_id);
        } else {
            confirm.status = STATUS_INSUFFICIENT_RESOURCES;
            printf("  - [原型模式] 资源不足 (SATCOM RL带宽限制)\n");
        }
    } else {
        /* 释放资源 */
        if (req->has_bearer_id) {
            int ret = dlm_release_bearer(&g_ctx.state, req->bearer_identifier);
            confirm.status = (ret == 0) ? STATUS_SUCCESS : STATUS_INVALID_BEARER;
            printf("  - [原型模式] 释放 Bearer ID: %u, 结果: %s\n",
                   req->bearer_identifier, status_to_string(confirm.status));
        } else {
            confirm.status = STATUS_INVALID_BEARER;
        }
    }
    
    dlm_send_to_mihf(MIH_LINK_RESOURCE_CNF, &confirm, sizeof(confirm));
    return 0;
}

/* ============================================================================
 * 指示发送函数
 * ============================================================================ */

static int dlm_send_going_down_indication(uint32_t time_to_down_ms, int reason) {
    printf("[SATCOM-IND] 发送 Link_Going_Down.indication (剩余=%ums)\n", time_to_down_ms);
    
    LINK_Going_Down_Indication ind;
    memset(&ind, 0, sizeof(ind));
    ind.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(ind.link_identifier.link_addr, sizeof(ind.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符 */
    snprintf(ind.link_identifier.poa_addr, sizeof(ind.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    ind.time_to_down_ms = time_to_down_ms;
    ind.reason_code = (uint8_t)reason;
    ind.confidence = 70;  /* 卫星信号预测准确度一般 */
    snprintf(ind.reason_text, sizeof(ind.reason_text), 
             "Satellite signal degrading");
    
    return dlm_send_to_mihf(MIH_LINK_GOING_DOWN_IND, &ind, sizeof(ind));
}

static int dlm_send_parameters_report(void) {
    pthread_mutex_lock(&g_ctx.state.mutex);
    
    if (!g_ctx.state.is_connected) {
        pthread_mutex_unlock(&g_ctx.state.mutex);
        return 0;
    }
    
    LINK_Parameters_Report_Indication ind;
    memset(&ind, 0, sizeof(ind));
    ind.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(ind.link_identifier.link_addr, sizeof(ind.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符 */
    snprintf(ind.link_identifier.poa_addr, sizeof(ind.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    ind.changed_params = LINK_PARAM_QUERY_SIGNAL_STRENGTH | LINK_PARAM_QUERY_AVAILABLE_BW | LINK_PARAM_QUERY_LATENCY;
    ind.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;
    ind.parameters.signal_quality = g_ctx.state.signal_quality;
    ind.parameters.available_bandwidth_kbps = g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
    ind.parameters.current_latency_ms = g_ctx.config.reported_delay_ms;
    ind.parameters.link_state = 1;
    /* 设置 DLM 自己的 IP 地址作为网关（客户端流量需要路由到 DLM） */
    inet_pton(AF_INET, g_net_config.ip_address, &ind.parameters.gateway);
    /* 同时设置 DLM 的 IP 地址 */
    inet_pton(AF_INET, g_net_config.ip_address, &ind.parameters.ip_address);
    ind.report_timestamp = (uint32_t)time(NULL);
    
    g_ctx.state.last_report_time = time(NULL);
    
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    printf("[SATCOM-IND] Parameters Report: RSSI=%d dBm, BW=%u kbps, Delay=%ums\n",
           ind.parameters.signal_strength_dbm, 
           ind.parameters.available_bandwidth_kbps,
           ind.parameters.current_latency_ms);
    
    return dlm_send_to_mihf(MIH_LINK_PARAMETERS_REPORT_IND, &ind, sizeof(ind));
}

/* ============================================================================
 * 数据包监控线程 - 监控实际网络流量
 * ============================================================================ */

static int dlm_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* tcpdump -X 输出每行通常包含 4位hex word："4500 003c ..."。
 * 这里只解析 4位hex word，避免误把 ASCII 区域当成 hex。 */
static void dlm_append_hex_words_from_line(const char* line, uint8_t* out, size_t* out_len, size_t out_cap) {
    const char* colon = strchr(line, ':');
    if (!colon) return;
    const char* p = colon + 1;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) &&
            isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3])) {
            int n0 = dlm_hex_nibble(p[0]);
            int n1 = dlm_hex_nibble(p[1]);
            int n2 = dlm_hex_nibble(p[2]);
            int n3 = dlm_hex_nibble(p[3]);
            if (n0 >= 0 && n1 >= 0 && n2 >= 0 && n3 >= 0) {
                if (*out_len + 2 <= out_cap) {
                    out[(*out_len)++] = (uint8_t)((n0 << 4) | n1);
                    out[(*out_len)++] = (uint8_t)((n2 << 4) | n3);
                }
            }
            p += 4;
            continue;
        }

        /* 跳过非hex token（通常进入 ASCII 区域） */
        while (*p && !isspace((unsigned char)*p)) p++;
    }
}

static size_t dlm_guess_ipv4_offset(const uint8_t* pkt, size_t len) {
    if (len >= 18 && pkt[12] == 0x81 && pkt[13] == 0x00 && pkt[16] == 0x08 && pkt[17] == 0x00) {
        return 18; /* 802.1Q VLAN */
    }
    if (len >= 14 && pkt[12] == 0x08 && pkt[13] == 0x00) {
        return 14; /* Ethernet */
    }
    return 0; /* 可能是 L3 起始 */
}

static void dlm_print_payload_summary(const char* tag, const uint8_t* payload, size_t payload_len) {
    if (!payload || payload_len == 0) return;

    char ascii[129];
    size_t show_len = payload_len > 128 ? 128 : payload_len;
    for (size_t i = 0; i < show_len; i++) {
        unsigned char c = payload[i];
        ascii[i] = (c >= 32 && c <= 126) ? (char)c : '.';
    }
    ascii[show_len] = '\0';

    printf("[%s-PKT] UDP payload len=%zu ascii=\"%s\"\n", tag, payload_len, ascii);

    /* 打印前 64 字节 hex */
    size_t hex_len = payload_len > 64 ? 64 : payload_len;
    printf("[%s-PKT] UDP payload hex:", tag);
    for (size_t i = 0; i < hex_len; i++) {
        printf(" %02x", payload[i]);
    }
    if (payload_len > hex_len) printf(" ...");
    printf("\n");
}

static void dlm_try_parse_udp_payload_from_tcpdump(const char* tag, char** lines, int line_count) {
    if (!lines || line_count <= 0) return;

    for (int i = 0; i < line_count; i++) {
        const char* hdr = lines[i];
        if (!hdr) continue;
        if (strstr(hdr, " IP ") == NULL) continue;
        if (strstr(hdr, " UDP") == NULL) continue;
        if (strstr(hdr, "length") == NULL) continue;

        uint8_t pkt[4096];
        size_t pkt_len = 0;

        for (int j = i + 1; j < line_count; j++) {
            const char* l = lines[j];
            if (!l) break;
            const char* t = l;
            while (*t == ' ' || *t == '\t') t++;
            if (strncmp(t, "0x", 2) != 0) break;
            dlm_append_hex_words_from_line(t, pkt, &pkt_len, sizeof(pkt));
        }

        if (pkt_len < 20) continue;
        size_t ip_off = dlm_guess_ipv4_offset(pkt, pkt_len);
        if (ip_off + 20 > pkt_len) continue;
        if ((pkt[ip_off] >> 4) != 4) continue;

        size_t ip_hlen = (size_t)(pkt[ip_off] & 0x0F) * 4;
        if (ip_hlen < 20) continue;
        if (ip_off + ip_hlen + 8 > pkt_len) continue;
        if (pkt[ip_off + 9] != 17) continue; /* UDP */

        size_t udp_off = ip_off + ip_hlen;
        uint16_t udp_len = (uint16_t)((pkt[udp_off + 4] << 8) | pkt[udp_off + 5]);
        if (udp_len < 8) continue;

        size_t payload_off = udp_off + 8;
        size_t payload_len = (size_t)udp_len - 8;
        if (payload_off > pkt_len) continue;
        if (payload_off + payload_len > pkt_len) {
            /* tcpdump 可能未抓全（snaplen 太小），但我们已设置 -s 0；这里做保护 */
            if (pkt_len > payload_off) payload_len = pkt_len - payload_off;
            else payload_len = 0;
        }

        if (payload_len > 0) {
            dlm_print_payload_summary(tag, pkt + payload_off, payload_len);
        }
    }
}

static void* dlm_packet_monitor_thread(void* arg) {
    (void)arg;
    char cmd[512];
    FILE *fp;
    char line[512];
    
    printf("[SATCOM-PKT] 数据包监控线程启动，监控接口: %s\n", g_ctx.config.interface_name);
    
    /* 只关注 UDP 测试端口，避免 ARP 吃掉 -c 计数；-s 0 抓全 payload。
     * 说明：以前使用 timeout 3 + sleep(1) 会产生空档，若测试包跨越空档可能“发5包只看到2包”。 */
    snprintf(cmd, sizeof(cmd),
             "timeout 6 tcpdump -i %s -n -s 0 -c 200 -vv -X -U -l 'udp and port 5000' 2>&1",
             g_ctx.config.interface_name);

    /* 缓冲输出：捕获完整的包信息和数据内容 */
    while (g_running) {
        fp = popen(cmd, "r");
        if (fp != NULL) {
            #define MAX_PKT_LINES 256
            char* pkts[MAX_PKT_LINES];
            int pkt_count = 0;
            bool has_traffic = false;
            
            while (fgets(line, sizeof(line), fp) != NULL && g_running) {
                /* 检查是否有实际流量（包含IP、ARP或数据内容标记）*/
                if (strstr(line, " IP ") != NULL || strstr(line, "ARP") != NULL || 
                    strstr(line, "0x0000:") != NULL || strstr(line, "0x0010:") != NULL) {
                    has_traffic = true;
                }
                /* 保存所有行（包括包头、十六进制dump和总结）*/
                if (pkt_count < MAX_PKT_LINES) {
                    pkts[pkt_count] = strdup(line);
                    if (pkts[pkt_count]) pkt_count++;
                }
            }
            pclose(fp);

            if (has_traffic && pkt_count > 0) {
                printf("\n[SATCOM-PKT] ═══════════════════════════════════════\n");
                printf("[SATCOM-PKT] 执行命令: %s\n", cmd);
                printf("[SATCOM-PKT] ───────────────────────────────────────\n");
                dlm_try_parse_udp_payload_from_tcpdump("SATCOM", pkts, pkt_count);
                for (int i = 0; i < pkt_count; i++) {
                    printf("[SATCOM-PKT] %s", pkts[i]);
                    free(pkts[i]);
                }
                printf("[SATCOM-PKT] ═══════════════════════════════════════\n\n");
                fflush(stdout);
            } else {
                /* 没有流量，释放内存 */
                for (int i = 0; i < pkt_count; i++) {
                    free(pkts[i]);
                }
            }
        }

        /* 不再额外 sleep，避免在两次 tcpdump 之间产生抓包空档 */
    }
    
    printf("[SATCOM-PKT] 数据包监控线程退出\n");
    return NULL;
}

/* ============================================================================
 * 通信和线程函数
 * ============================================================================ */

static int dlm_init_socket(void) {
    struct sockaddr_un addr;
    
    g_ctx.socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_ctx.socket_fd < 0) {
        perror("[SATCOM] socket() 失败");
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_net_config.dlm_path, sizeof(addr.sun_path) - 1);
    unlink(g_net_config.dlm_path);
    
    if (bind(g_ctx.socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SATCOM] bind() 失败");
        close(g_ctx.socket_fd);
        return -1;
    }
    
    memset(&g_mihf_addr, 0, sizeof(g_mihf_addr));
    g_mihf_addr.sun_family = AF_UNIX;
    strncpy(g_mihf_addr.sun_path, g_net_config.mihf_path, sizeof(g_mihf_addr.sun_path) - 1);
    
    printf("[SATCOM] 套接字已初始化: %s\n", g_net_config.dlm_path);
    return 0;
}

static int dlm_send_to_mihf(uint16_t msg_type, const void* data, size_t len) {
    uint8_t buf[2048];
    if (len + 2 > sizeof(buf)) return -1;
    *(uint16_t*)buf = msg_type;
    memcpy(buf + 2, data, len);

    ssize_t sent = sendto(g_ctx.socket_fd, buf, len + 2, 0,
                          (struct sockaddr*)&g_mihf_addr, sizeof(g_mihf_addr));
    
    if (sent < 0 && errno != ENOENT) {
        perror("[SATCOM] sendto MIHF 失败");
        return -1;
    }
    return 0;
}

static void dlm_signal_handler(int sig) {
    printf("\n[SATCOM] 收到信号 %d, 正在关闭...\n", sig);
    g_running = 0;
}

/**
 * @brief 发送 Link_Up 心跳消息（不执行物理操作）
 * 作为看门狗消息定期发送，确保 MIHF 知道链路在线
 */
static void dlm_send_link_up_heartbeat(void) {
    pthread_mutex_lock(&g_ctx.state.mutex);
    if (!g_ctx.state.is_connected) {
        pthread_mutex_unlock(&g_ctx.state.mutex);
        return;
    }
    
    LINK_Up_Indication ind;
    memset(&ind, 0, sizeof(ind));
    ind.link_identifier.link_type = g_ctx.config.link_id;
    snprintf(ind.link_identifier.link_addr, sizeof(ind.link_identifier.link_addr),
             "%s", g_ctx.config.link_name);  /* 使用 link_name 作为标识符 */
    snprintf(ind.link_identifier.poa_addr, sizeof(ind.link_identifier.poa_addr),
             "%s", g_ctx.config.interface_name);  /* 在 poa_addr 中存储网络接口名 */
    ind.up_timestamp = (uint32_t)time(NULL);
    ind.parameters.current_tx_rate_kbps = g_ctx.config.max_bandwidth_rl - g_ctx.state.current_usage_rl;
    ind.parameters.current_rx_rate_kbps = g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
    ind.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;
    ind.parameters.signal_quality = g_ctx.state.signal_quality;
    
    pthread_mutex_unlock(&g_ctx.state.mutex);
    
    dlm_send_to_mihf(MIH_LINK_UP_IND, &ind, sizeof(ind));
    printf("[SATCOM-HB] Link_Up heartbeat sent\n");
}

static void* dlm_reporting_thread(void* arg) {
    (void)arg;
    printf("[SATCOM-THR] 参数上报线程已启动\n");
    
    bool prev_iface_up = false;
    
    while (g_running) {
        sleep(g_ctx.config.reporting_interval_sec);
        if (!g_running)
            break;
        
        /* 检查网卡实际物理状态 */
        bool curr_iface_up = check_interface_status(g_ctx.config.interface_name);
        
        /* 检测网卡状态变化 */
        if (prev_iface_up && !curr_iface_up) {
            /* 网卡从 UP 变为 DOWN */
            printf("[SATCOM-MON] 检测到网卡 %s 状态变为 DOWN\n", g_ctx.config.interface_name);
            
            pthread_mutex_lock(&g_ctx.state.mutex);
            if (g_ctx.state.is_connected) {
                g_ctx.state.is_connected = 0;
                g_ctx.state.interface_up = 0;
                pthread_mutex_unlock(&g_ctx.state.mutex);
                
                /* 发送 Link_Down.indication */
                dlm_physical_link_down(LINK_DOWN_REASON_FAILURE);
            } else {
                pthread_mutex_unlock(&g_ctx.state.mutex);
            }
        } else if (!prev_iface_up && curr_iface_up) {
            /* 网卡从 DOWN 变为 UP */
            printf("[SATCOM-MON] 检测到网卡 %s 状态变为 UP\n", g_ctx.config.interface_name);
            
            pthread_mutex_lock(&g_ctx.state.mutex);
            bool was_down = !g_ctx.state.is_connected;
            pthread_mutex_unlock(&g_ctx.state.mutex);
            
            if (was_down) {
                /* 自动触发链路建立 */
                usleep(500000);
                dlm_physical_link_up();
            }
        } else if (curr_iface_up) {
            /* 网卡状态正常，发送心跳 */
            pthread_mutex_lock(&g_ctx.state.mutex);
            bool is_connected = g_ctx.state.is_connected;
            pthread_mutex_unlock(&g_ctx.state.mutex);
            
            if (is_connected) {
                dlm_send_link_up_heartbeat();
                dlm_send_parameters_report();
            }
        }
        
        prev_iface_up = curr_iface_up;
    }
    
    printf("[SATCOM-THR] 参数上报线程已退出\n");
    return NULL;
}

static void* dlm_simulation_thread(void* arg) {
    (void)arg;
    printf("[SATCOM-THR] 状态模拟线程已启动\n");
    
    while (g_running) {
        sleep(3);  /* 卫星信号变化较慢 */
        if (g_running && g_ctx.state.is_connected) {
            dlm_simulate_rssi(&g_ctx.state, &g_ctx.config);
            
            /* 检查是否需要发送 Going_Down */
            pthread_mutex_lock(&g_ctx.state.mutex);
            bool trigger_going_down = (g_ctx.state.simulated_rssi < g_ctx.config.rssi_threshold_dbm) 
                                      && !g_ctx.state.is_going_down;
            if (trigger_going_down) {
                g_ctx.state.is_going_down = 1;
            }
            pthread_mutex_unlock(&g_ctx.state.mutex);
            
            if (trigger_going_down) {
                dlm_send_going_down_indication(g_net_config.going_down_lead_time_ms, 
                                               LINK_DOWN_REASON_SIGNAL_LOSS);
            }
        }
    }
    
    printf("[SATCOM-THR] 状态模拟线程已退出\n");
    return NULL;
}

/* ============================================================================
 * 消息处理主循环
 * ============================================================================ */

static void dlm_message_loop(void) {
    uint8_t buffer[2048];
    ssize_t recv_len;
    
    while (g_running) {
        recv_len = recv(g_ctx.socket_fd, buffer, sizeof(buffer), 0);
        if (recv_len <= 0) {
            if (errno != EINTR && recv_len < 0) {
                perror("[SATCOM] recv() 失败");
            }
            continue;
        }
        
        /* 简单消息分发 - 原型阶段使用消息类型码 */
        uint16_t msg_type = 0;
        if (recv_len >= 2) {
            msg_type = *(uint16_t*)buffer;
        }
        
        printf("[SATCOM-MSG] 收到消息类型: 0x%04X\n", msg_type);
        
        switch (msg_type) {
            case MIH_LINK_CAPABILITY_DISCOVER_REQ:
                dlm_handle_capability_discover();
                break;
            case MIH_LINK_GET_PARAMETERS_REQ:
                dlm_handle_get_parameters();
                break;
            case MIH_LINK_EVENT_SUBSCRIBE_REQ:
                if ((size_t)recv_len >= sizeof(LINK_Event_Subscribe_Request)) {
                    dlm_handle_event_subscribe((LINK_Event_Subscribe_Request*)buffer);
                }
                break;
            case MIH_LINK_RESOURCE_REQ:
                if ((size_t)recv_len >= sizeof(LINK_Resource_Request)) {
                    dlm_handle_link_resource((LINK_Resource_Request*)buffer);
                }
                break;
            default:
                printf("[SATCOM-MSG] 未知消息类型\n");
                break;
        }
    }
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(int argc, char* argv[]) {
    pthread_t report_thread, sim_thread, pkt_monitor_thread;
    const char* config_path = DEFAULT_CONFIG_PATH;
    
    /* 支持命令行指定配置文件 */
    if (argc > 1) {
        config_path = argv[1];
    }
    
    printf("========================================\n");
    printf("SATCOM DLM 原型 v1.0\n");
    printf("ARINC 839-2014 MIH 协议实现\n");
    printf("GEO 卫星通信 (高延迟/高成本)\n");
    printf("配置文件: %s\n", config_path);
    printf("========================================\n\n");
    
    srand((unsigned int)time(NULL));
    {
        /* 使用 sigaction 注册信号处理，确保阻塞系统调用被中断 (不使用 SA_RESTART) */
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = dlm_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0; /* 不设置 SA_RESTART，这样 recv() 会在信号到来时返回 EINTR */
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
    }
    
    if (dlm_init_config_manager(config_path) != 0) {
        fprintf(stderr, "[SATCOM] 配置管理器初始化失败\n");
        return EXIT_FAILURE;
    }
    
    if (dlm_init_state_simulator() != 0) {
        fprintf(stderr, "[SATCOM] 状态模拟器初始化失败\n");
        return EXIT_FAILURE;
    }
    
    if (dlm_init_socket() != 0) {
        fprintf(stderr, "[SATCOM] 套接字初始化失败\n");
        return EXIT_FAILURE;
    }
    
    pthread_create(&report_thread, NULL, dlm_reporting_thread, NULL);
    pthread_create(&sim_thread, NULL, dlm_simulation_thread, NULL);
    pthread_create(&pkt_monitor_thread, NULL, dlm_packet_monitor_thread, NULL);
    
    printf("\n[SATCOM] 原型模式: 自动激活链路...\n");
    dlm_physical_link_up();
    
    /* 链路激活后启动 UDP 监听 */
    dlm_udp_listener_t udp_listener;
    memset(&udp_listener, 0, sizeof(udp_listener));
    sleep(1);  /* 等待接口完全就绪 */
    if (dlm_udp_listener_start(&udp_listener, g_net_config.ip_address, 
                                DLM_UDP_LISTEN_PORT, "SATCOM") != 0) {
        fprintf(stderr, "[SATCOM] UDP 监听启动失败（非致命错误）\n");
    }
    
    /* 打印初始网络状态 */
    dlm_print_network_status(&g_ctx.config, &g_ctx.state);
    
    printf("\n[SATCOM] DLM 已启动, 等待消息...\n");
    printf("按 Ctrl+C 退出\n\n");
    
    dlm_message_loop();
    
    /* 清理 */
    printf("\n[SATCOM] 正在清理...\n");
    
    /* 停止 UDP 监听 */
    dlm_udp_listener_stop(&udp_listener);
    
    dlm_physical_link_down(LINK_DOWN_REASON_EXPLICIT);
    
    pthread_join(report_thread, NULL);
    pthread_join(sim_thread, NULL);
    pthread_join(pkt_monitor_thread, NULL);
    
    close(g_ctx.socket_fd);
    unlink(g_net_config.dlm_path);
    dlm_state_destroy(&g_ctx.state);
    
    printf("[SATCOM] DLM 已停止\n");
    return EXIT_SUCCESS;
}
