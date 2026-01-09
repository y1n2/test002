/**
 * @file dlm_common.h
 * @brief DLM (Data Link Manager) 公共定义与工具函数
 * @details 包含链路配置、状态模拟、承载管理以及 INI 配置解析等核心数据结构和内联函数。
 *          该文件作为 MAGIC 系统中各 DLM 原型的基础头文件。
 * @date 2025-11-26
 */

#ifndef DLM_COMMON_H
#define DLM_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* 需要这些头以获得 timeval、strcasecmp 等声明 */
#include <sys/time.h>
#include <strings.h>

/* strdup 在某些编译环境下为 POSIX 声明，确保有明确原型以避免隐式声明导致的 64-bit 问题 */
char *strdup(const char *s);

/*===========================================================================
 * 链路类型定义
 *===========================================================================*/

/**
 * @brief 链路物理类型枚举
 */
typedef enum {
    DLM_LINK_TYPE_SATCOM   = 0x01, ///< 卫星通信链路
    DLM_LINK_TYPE_CELLULAR = 0x02, ///< 蜂窝移动网络 (4G/5G)
    DLM_LINK_TYPE_WIFI     = 0x03  ///< 无线局域网
} dlm_link_type_t;

/*===========================================================================
 * Configuration Manager (CM) - 静态配置
 *===========================================================================*/

/**
 * @brief 链路静态配置参数结构体
 */
typedef struct {
    uint8_t             link_id;            ///< 链路唯一标识符
    dlm_link_type_t     link_type;          ///< 链路物理类型
    char                interface_name[16]; ///< 网络接口名称 (如 ens33, wlan0)
    char                link_name[32];      ///< 链路友好显示名称
    uint32_t            max_bandwidth_fl;   ///< 前向链路 (Forward Link) 最大带宽 (kbps)
    uint32_t            max_bandwidth_rl;   ///< 反向链路 (Return Link) 最大带宽 (kbps)
    uint32_t            reported_delay_ms;  ///< 链路标称延迟 (ms)
    uint32_t            delay_jitter_ms;    ///< 延迟抖动范围 (ms)
    float               cost_factor;        ///< 成本权重因子
    uint32_t            cost_per_mb_cents;  ///< 每 MB 流量成本 (美分)
    int32_t             rssi_threshold_dbm; ///< 链路可用 RSSI 阈值
    int32_t             rssi_min_dbm;       ///< 模拟 RSSI 最小值
    int32_t             rssi_max_dbm;       ///< 模拟 RSSI 最大值
    uint8_t             security_level;     ///< 安全等级
    uint16_t            mtu;                ///< 最大传输单元
    uint32_t            reporting_interval_sec; ///< 状态报告周期 (秒)
    uint32_t            heartbeat_interval_sec; ///< 心跳周期 (秒)
    bool                is_asymmetric;      ///< 是否为非对称链路
    bool                ground_only;        ///< 是否仅在地面可用
} dlm_config_manager_t;

/*===========================================================================
 * Bearer信息结构
 *===========================================================================*/

#define MAX_BEARERS 16

/**
 * @brief 承载 (Bearer) 信息结构体
 */
typedef struct {
    uint8_t             bearer_id;       ///< 承载唯一 ID
    bool                active;          ///< 承载是否处于激活状态
    uint32_t            allocated_bw_fl; ///< 为该承载分配的前向带宽 (kbps)
    uint32_t            allocated_bw_rl; ///< 为该承载分配的反向带宽 (kbps)
    uint8_t             cos_id;          ///< 服务类别 (Class of Service) ID
    time_t              created_time;    ///< 承载创建时间戳
} dlm_bearer_info_t;

/*===========================================================================
 * State Simulator (SS) - 动态状态
 *===========================================================================*/

/**
 * @brief 链路动态状态模拟器结构体
 */
typedef struct {
    bool                is_connected;        ///< 链路是否已连接 (逻辑连接)
    bool                is_going_down;       ///< 链路是否即将断开 (Going Down 预警)
    bool                is_detected;         ///< 物理层是否检测到信号
    bool                interface_up;        ///< 操作系统网络接口是否已启动
    uint32_t            ip_address;          ///< 链路分配的 IP 地址 (网络字节序)
    uint32_t            netmask;             ///< 子网掩码 (网络字节序)
    int32_t             simulated_rssi;      ///< 当前模拟的 RSSI 值 (dBm)
    uint8_t             signal_quality;      ///< 信号质量百分比 (0-100)
    uint32_t            current_usage_fl;    ///< 当前前向带宽使用量 (kbps)
    uint32_t            current_usage_rl;    ///< 当前反向带宽使用量 (kbps)
    float               utilization_percent; ///< 链路带宽利用率百分比
    dlm_bearer_info_t   bearer_map[MAX_BEARERS]; ///< 承载信息映射表
    uint8_t             num_active_bearers;  ///< 当前活跃承载数量
    uint8_t             next_bearer_id;      ///< 下一个可用的承载 ID
    uint64_t            tx_bytes;            ///< 累计发送字节数
    uint64_t            rx_bytes;            ///< 累计接收字节数
    uint64_t            tx_packets;          ///< 累计发送数据包数
    uint64_t            rx_packets;          ///< 累计接收数据包数
    uint16_t            subscribed_events;   ///< 已订阅的事件位图
    time_t              last_up_time;        ///< 上次链路 UP 时间
    time_t              last_down_time;      ///< 上次链路 DOWN 时间
    time_t              last_report_time;    ///< 上次发送报告时间
    pthread_mutex_t     mutex;               ///< 状态保护互斥锁
} dlm_state_simulator_t;

/*===========================================================================
 * 事件类型位图定义
 *===========================================================================*/

#define DLM_EVENT_LINK_UP           (1 << 0)
#define DLM_EVENT_LINK_DOWN         (1 << 1)
#define DLM_EVENT_LINK_GOING_DOWN   (1 << 2)
#define DLM_EVENT_LINK_DETECTED     (1 << 3)
#define DLM_EVENT_PARAM_REPORT      (1 << 4)
#define DLM_EVENT_ALL               (0x001F)

/*===========================================================================
 * 状态码定义
 *===========================================================================*/

typedef enum {
    DLM_STATUS_SUCCESS              = 0,
    DLM_STATUS_FAILURE              = 1,
    DLM_STATUS_REJECTED             = 2,
    DLM_STATUS_CAPACITY_EXCEEDED    = 3,
    DLM_STATUS_LINK_NOT_AVAILABLE   = 4,
    DLM_STATUS_INVALID_BEARER       = 5
} dlm_status_t;

/*===========================================================================
 * DLM完整上下文结构
 *===========================================================================*/

/**
 * @brief DLM 完整运行上下文结构体
 */
typedef struct {
    bool                    running;          ///< DLM 运行标志位
    int                     socket_fd;        ///< 与 MIHF 通信的 Unix Socket 文件描述符
    uint32_t                assigned_id;      ///< MIHF 分配的注册 ID
    bool                    registered;       ///< 是否已成功在 MIHF 注册
    dlm_config_manager_t    config;           ///< 链路静态配置
    dlm_state_simulator_t   state;            ///< 链路动态状态
    pthread_t               reporting_thread; ///< 状态报告线程 ID
    pthread_t               monitor_thread;   ///< 链路监控线程 ID
    pthread_t               message_thread;   ///< 消息接收线程 ID
} dlm_context_t;

/*===========================================================================
 * 内联工具函数
 *===========================================================================*/

/**
 * @brief 获取状态码对应的字符串描述
 * @param status DLM 状态码
 * @return const char* 状态描述字符串
 */
static inline const char* dlm_status_str(dlm_status_t status)
{
    switch (status) {
        case DLM_STATUS_SUCCESS:            return "SUCCESS";
        case DLM_STATUS_FAILURE:            return "FAILURE";
        case DLM_STATUS_REJECTED:           return "REJECTED";
        case DLM_STATUS_CAPACITY_EXCEEDED:  return "CAPACITY_EXCEEDED";
        case DLM_STATUS_LINK_NOT_AVAILABLE: return "LINK_NOT_AVAILABLE";
        case DLM_STATUS_INVALID_BEARER:     return "INVALID_BEARER";
        default:                            return "UNKNOWN";
    }
}

/**
 * @brief 获取链路类型对应的字符串描述
 * @param type 链路类型枚举
 * @return const char* 链路类型字符串
 */
static inline const char* dlm_link_type_str(dlm_link_type_t type)
{
    switch (type) {
        case DLM_LINK_TYPE_SATCOM:   return "SATCOM";
        case DLM_LINK_TYPE_CELLULAR: return "CELLULAR";
        case DLM_LINK_TYPE_WIFI:     return "WIFI";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief 初始化链路状态模拟器
 * @details 清零状态结构体，初始化互斥锁，并设置默认的 RSSI 和信号质量。
 * @param state 指向待初始化的状态模拟器结构体的指针
 * @warning 必须在多线程访问前调用此函数，否则互斥锁行为未定义。
 */
static inline void dlm_state_init(dlm_state_simulator_t* state)
{
    memset(state, 0, sizeof(dlm_state_simulator_t));
    pthread_mutex_init(&state->mutex, NULL);
    state->next_bearer_id = 1;
    state->simulated_rssi = -60;
    state->signal_quality = 80;
}

/**
 * @brief 销毁链路状态模拟器
 * @details 释放互斥锁等资源。
 * @param state 指向待销毁的状态模拟器结构体的指针
 */
static inline void dlm_state_destroy(dlm_state_simulator_t* state)
{
    pthread_mutex_destroy(&state->mutex);
}

/**
 * @brief 启动指定的网络接口
 * @details 通过执行系统命令 `ip link set <iface> up` 来启动接口。
 * @param iface 接口名称 (如 "eth0")
 * @return int 成功返回 0，失败返回 -1
 * @note 该函数假设程序具有足够的权限 (如 root) 执行网络配置命令。
 */
static inline int dlm_interface_up(const char* iface)
{
    char cmd[256];
    int ret;
    
    /* 不使用 sudo，假设 DLM 以 root 运行 */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", iface);
    ret = system(cmd);
    
    if (ret != 0) {
        fprintf(stderr, "[DLM] 启动接口 %s 失败 (ret=%d)\n", iface, ret);
        return -1;
    }
    
    printf("[DLM] ✓ 接口 %s 已启动\n", iface);
    return 0;
}

/**
 * @brief 停止指定的网络接口
 * @details 通过执行系统命令 `ip link set <iface> down` 来停止接口。
 * @param iface 接口名称
 * @return int 成功返回 0，失败返回 -1
 */
static inline int dlm_interface_down(const char* iface)
{
    char cmd[256];
    int ret;
    
    /* 不使用 sudo，假设 DLM 以 root 运行 */
    snprintf(cmd, sizeof(cmd), "ip link set %s down", iface);
    ret = system(cmd);
    
    if (ret != 0) {
        fprintf(stderr, "[DLM] 停止接口 %s 失败 (ret=%d)\n", iface, ret);
        return -1;
    }
    
    printf("[DLM] ✓ 接口 %s 已停止\n", iface);
    return 0;
}

/**
 * @brief 模拟 RSSI 信号波动
 * @details 在配置的范围内随机调整 RSSI 值，并同步更新信号质量百分比。
 * @param state 状态模拟器指针
 * @param config 静态配置指针 (用于获取 RSSI 范围限制)
 */
static inline void dlm_simulate_rssi(dlm_state_simulator_t* state, 
                                     const dlm_config_manager_t* config)
{
    pthread_mutex_lock(&state->mutex);
    int32_t variation = (rand() % 7) - 3;
    state->simulated_rssi += variation;
    if (state->simulated_rssi < config->rssi_min_dbm)
        state->simulated_rssi = config->rssi_min_dbm;
    if (state->simulated_rssi > config->rssi_max_dbm)
        state->simulated_rssi = config->rssi_max_dbm;
    int32_t range = config->rssi_max_dbm - config->rssi_min_dbm;
    if (range > 0) {
        state->signal_quality = (uint8_t)(((state->simulated_rssi - config->rssi_min_dbm) * 100) / range);
    }
    pthread_mutex_unlock(&state->mutex);
}

/**
 * @brief 分配一个新的承载 (Bearer)
 * @details 检查链路剩余带宽是否满足要求，如果满足则在承载映射表中创建一个新条目。
 * @param state 状态模拟器指针
 * @param config 静态配置指针
 * @param req_bw_fl 请求的前向带宽 (kbps)
 * @param req_bw_rl 请求的反向带宽 (kbps)
 * @param cos_id 服务类别 ID
 * @param out_bearer_id [out] 输出分配到的承载 ID
 * @return int 成功返回 0，带宽不足返回 -1，承载槽位已满返回 -2
 * @warning 该操作会修改链路的当前带宽使用量。
 */
static inline int dlm_allocate_bearer(dlm_state_simulator_t* state,
                                      const dlm_config_manager_t* config,
                                      uint32_t req_bw_fl, uint32_t req_bw_rl,
                                      uint8_t cos_id, uint8_t* out_bearer_id)
{
    pthread_mutex_lock(&state->mutex);
    if ((state->current_usage_fl + req_bw_fl) > config->max_bandwidth_fl ||
        (state->current_usage_rl + req_bw_rl) > config->max_bandwidth_rl) {
        pthread_mutex_unlock(&state->mutex);
        return -1;
    }
    int slot = -1;
    for (int i = 0; i < MAX_BEARERS; i++) {
        if (!state->bearer_map[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&state->mutex);
        return -2;
    }
    uint8_t bearer_id = state->next_bearer_id++;
    state->bearer_map[slot].bearer_id = bearer_id;
    state->bearer_map[slot].active = true;
    state->bearer_map[slot].allocated_bw_fl = req_bw_fl;
    state->bearer_map[slot].allocated_bw_rl = req_bw_rl;
    state->bearer_map[slot].cos_id = cos_id;
    state->bearer_map[slot].created_time = time(NULL);
    state->current_usage_fl += req_bw_fl;
    state->current_usage_rl += req_bw_rl;
    state->num_active_bearers++;
    state->utilization_percent = (float)state->current_usage_fl * 100.0f / config->max_bandwidth_fl;
    *out_bearer_id = bearer_id;
    pthread_mutex_unlock(&state->mutex);
    return 0;
}

/**
 * @brief 释放指定的承载 (Bearer)
 * @details 根据承载 ID 查找并释放对应的资源，更新链路带宽使用量。
 * @param state 状态模拟器指针
 * @param bearer_id 要释放的承载 ID
 * @return int 成功返回 0，未找到承载返回 -1
 */
static inline int dlm_release_bearer(dlm_state_simulator_t* state, uint8_t bearer_id)
{
    pthread_mutex_lock(&state->mutex);
    int slot = -1;
    for (int i = 0; i < MAX_BEARERS; i++) {
        if (state->bearer_map[i].active && state->bearer_map[i].bearer_id == bearer_id) {
            slot = i; break;
        }
    }
    if (slot < 0) {
        pthread_mutex_unlock(&state->mutex);
        return -1;
    }
    state->current_usage_fl -= state->bearer_map[slot].allocated_bw_fl;
    state->current_usage_rl -= state->bearer_map[slot].allocated_bw_rl;
    state->num_active_bearers--;
    memset(&state->bearer_map[slot], 0, sizeof(dlm_bearer_info_t));
    pthread_mutex_unlock(&state->mutex);
    return 0;
}

/**
 * @brief 读取网络接口的统计信息
 * @details 从 `/sys/class/net/<iface>/statistics/` 读取累计收发字节数。
 * @param iface 接口名称
 * @param rx_bytes [out] 输出接收字节数
 * @param tx_bytes [out] 输出发送字节数
 * @return int 成功返回 0
 */
static inline int dlm_read_interface_stats(const char* iface,
                                           uint64_t* rx_bytes, uint64_t* tx_bytes)
{
    char path[128];
    FILE* fp;
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", iface);
    fp = fopen(path, "r");
    if (fp) { if (fscanf(fp, "%lu", rx_bytes) != 1) *rx_bytes = 0; fclose(fp); }
    else { *rx_bytes = 0; }
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", iface);
    fp = fopen(path, "r");
    if (fp) { if (fscanf(fp, "%lu", tx_bytes) != 1) *tx_bytes = 0; fclose(fp); }
    else { *tx_bytes = 0; }
    return 0;
}

/**
 * @brief 打印链路网络状态摘要
 * @details 在控制台输出格式化的链路信息表格，包括连接状态、信号、带宽、延迟、承载和流量统计。
 * @param config 静态配置指针
 * @param state 动态状态指针
 */
static inline void dlm_print_network_status(const dlm_config_manager_t* config, 
                                            const dlm_state_simulator_t* state)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║            DLM Network Status - %-10s                       ║\n", config->link_name);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ Link ID: 0x%02X    Interface: %-8s  Type: %-10s          ║\n", 
           config->link_id, config->interface_name, dlm_link_type_str(config->link_type));
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Connection]  Connected: %-3s  Interface UP: %-3s  Going Down: %-3s║\n",
           state->is_connected ? "YES" : "NO",
           state->interface_up ? "YES" : "NO",
           state->is_going_down ? "YES" : "NO");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Signal]      RSSI: %4d dBm (threshold: %4d)  Quality: %3u%%    ║\n",
           state->simulated_rssi, config->rssi_threshold_dbm, state->signal_quality);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Bandwidth]   Max FL: %6u kbps  Max RL: %6u kbps             ║\n",
           config->max_bandwidth_fl, config->max_bandwidth_rl);
    printf("║               Used FL: %5u kbps  Used RL: %5u kbps             ║\n",
           state->current_usage_fl, state->current_usage_rl);
    printf("║               Utilization: %5.1f%%                                ║\n",
           state->utilization_percent);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Latency]     Reported: %4u ms  Jitter: ±%2u ms                  ║\n",
           config->reported_delay_ms, config->delay_jitter_ms);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Bearers]     Active: %2u / %2d                                    ║\n",
           state->num_active_bearers, MAX_BEARERS);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Traffic]     TX: %12lu bytes  RX: %12lu bytes       ║\n", 
           state->tx_bytes, state->rx_bytes);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║ [Cost]        Factor: %.2f  Per MB: $%.2f                        ║\n",
           config->cost_factor, config->cost_per_mb_cents / 100.0f);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
}

/*===========================================================================
 * 扩展配置结构 - 包含从配置文件读取的网络参数
 *===========================================================================*/

/**
 * @brief 链路网络层配置结构体
 */
typedef struct {
    char interface_name[32];        ///< 网络接口名称
    char ip_address[32];            ///< 链路 IP 地址字符串
    char gateway[32];               ///< 默认网关 IP 字符串
    char netmask[32];               ///< 子网掩码字符串
    char mihf_path[128];            ///< MIHF Unix Socket 路径
    char dlm_path[128];             ///< DLM 自身的 Unix Socket 路径
    int  initial_rssi_dbm;          ///< 初始 RSSI 值
    int  going_down_lead_time_ms;   ///< Going Down 预警提前时间 (ms)
} dlm_network_config_t;

/*===========================================================================
 * INI 配置文件解析函数
 *===========================================================================*/

/**
 * @brief 去除字符串两端的空白字符
 * @param str 待处理的字符串
 * @return char* 指向处理后字符串起始位置的指针
 */
static inline char* dlm_trim(char* str) {
    while (*str == ' ' || *str == '\t') str++;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end-- = '\0';
    }
    return str;
}

/**
 * @brief 从 INI 文件加载链路配置
 * @details 解析指定的配置文件，填充链路静态配置和网络层配置结构体。
 * @param config_path 配置文件路径
 * @param config [out] 链路静态配置结构体指针
 * @param net_config [out] 网络层配置结构体指针
 * @return int 成功返回 0，失败返回 -1
 * @warning 如果文件格式不正确或无法打开，将返回错误。
 */
static inline int dlm_load_config(const char* config_path,
                                   dlm_config_manager_t* config,
                                   dlm_network_config_t* net_config)
{
    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "[DLM] 无法打开配置文件: %s\n", config_path);
        return -1;
    }
    
    char line[256];
    char section[64] = "";
    
    /* 设置默认值 */
    memset(net_config, 0, sizeof(*net_config));
    strcpy(net_config->mihf_path, "/tmp/mihf.sock");
    strcpy(net_config->gateway, "");
    net_config->initial_rssi_dbm = -60;
    net_config->going_down_lead_time_ms = 3000;
    
    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = dlm_trim(line);
        
        /* 跳过空行和注释 */
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        /* 解析 section */
        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }
        
        /* 解析 key = value */
        char* eq = strchr(trimmed, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char* key = dlm_trim(trimmed);
        char* value = dlm_trim(eq + 1);
        
        /* 根据 section 和 key 设置值 */
        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "link_id") == 0) {
                config->link_id = (uint8_t)strtol(value, NULL, 0);
            } else if (strcmp(key, "link_type") == 0) {
                if (strcasecmp(value, "satcom") == 0) config->link_type = DLM_LINK_TYPE_SATCOM;
                else if (strcasecmp(value, "cellular") == 0) config->link_type = DLM_LINK_TYPE_CELLULAR;
                else if (strcasecmp(value, "wifi") == 0) config->link_type = DLM_LINK_TYPE_WIFI;
            } else if (strcmp(key, "link_name") == 0) {
                strncpy(config->link_name, value, sizeof(config->link_name) - 1);
            }
        } else if (strcmp(section, "interface") == 0) {
            if (strcmp(key, "name") == 0) {
                strncpy(config->interface_name, value, sizeof(config->interface_name) - 1);
                strncpy(net_config->interface_name, value, sizeof(net_config->interface_name) - 1);
            } else if (strcmp(key, "ip_address") == 0) {
                strncpy(net_config->ip_address, value, sizeof(net_config->ip_address) - 1);
                /* 默认网关 = 本机 IP */
                strncpy(net_config->gateway, value, sizeof(net_config->gateway) - 1);
            } else if (strcmp(key, "netmask") == 0) {
                strncpy(net_config->netmask, value, sizeof(net_config->netmask) - 1);
            } else if (strcmp(key, "gateway") == 0) {
                strncpy(net_config->gateway, value, sizeof(net_config->gateway) - 1);
            }
        } else if (strcmp(section, "bandwidth") == 0) {
            if (strcmp(key, "max_forward_link_kbps") == 0) {
                config->max_bandwidth_fl = (uint32_t)atoi(value);
            } else if (strcmp(key, "max_return_link_kbps") == 0) {
                config->max_bandwidth_rl = (uint32_t)atoi(value);
            }
        } else if (strcmp(section, "latency") == 0) {
            if (strcmp(key, "delay_ms") == 0) {
                config->reported_delay_ms = (uint32_t)atoi(value);
            } else if (strcmp(key, "jitter_ms") == 0) {
                config->delay_jitter_ms = (uint32_t)atoi(value);
            }
        } else if (strcmp(section, "signal") == 0) {
            if (strcmp(key, "rssi_threshold_dbm") == 0) {
                config->rssi_threshold_dbm = atoi(value);
            } else if (strcmp(key, "rssi_min_dbm") == 0) {
                config->rssi_min_dbm = atoi(value);
            } else if (strcmp(key, "rssi_max_dbm") == 0) {
                config->rssi_max_dbm = atoi(value);
            } else if (strcmp(key, "initial_rssi_dbm") == 0) {
                net_config->initial_rssi_dbm = atoi(value);
            }
        } else if (strcmp(section, "cost") == 0) {
            if (strcmp(key, "factor") == 0) {
                config->cost_factor = (float)atof(value);
            } else if (strcmp(key, "per_mb_cents") == 0) {
                config->cost_per_mb_cents = (uint32_t)atoi(value);
            }
        } else if (strcmp(section, "network") == 0) {
            if (strcmp(key, "security_level") == 0) {
                config->security_level = (uint8_t)atoi(value);
            } else if (strcmp(key, "mtu") == 0) {
                config->mtu = (uint16_t)atoi(value);
            } else if (strcmp(key, "is_asymmetric") == 0) {
                config->is_asymmetric = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key, "ground_only") == 0) {
                config->ground_only = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);
            }
        } else if (strcmp(section, "timing") == 0) {
            if (strcmp(key, "reporting_interval_sec") == 0) {
                config->reporting_interval_sec = (uint32_t)atoi(value);
            } else if (strcmp(key, "heartbeat_interval_sec") == 0) {
                config->heartbeat_interval_sec = (uint32_t)atoi(value);
            } else if (strcmp(key, "going_down_lead_time_ms") == 0) {
                net_config->going_down_lead_time_ms = atoi(value);
            }
        } else if (strcmp(section, "socket") == 0) {
            if (strcmp(key, "mihf_path") == 0) {
                strncpy(net_config->mihf_path, value, sizeof(net_config->mihf_path) - 1);
            } else if (strcmp(key, "dlm_path") == 0) {
                strncpy(net_config->dlm_path, value, sizeof(net_config->dlm_path) - 1);
            }
        }
    }
    
    fclose(fp);
    
    printf("[DLM] 配置文件加载成功: %s\n", config_path);
    printf("  - 接口: %s, IP: %s, 网关: %s\n", 
           net_config->interface_name, net_config->ip_address, net_config->gateway);
    
    return 0;
}

/*===========================================================================
 * UDP 监听功能 - 用于接收和显示通过本链路发送的 UDP 数据
 *===========================================================================*/

#define DLM_UDP_LISTEN_PORT     5000    /* 默认监听端口 */
#define DLM_UDP_BUFFER_SIZE     2048    /* 接收缓冲区大小 */

/**
 * @brief UDP 监听器上下文结构体
 */
typedef struct {
    int         socket_fd;        ///< UDP Socket 文件描述符
    uint16_t    port;             ///< 监听端口
    char        bind_ip[32];      ///< 绑定的 IP 地址
    bool        running;          ///< 运行标志位
    pthread_t   thread;           ///< 监听线程 ID
    const char* link_name;        ///< 所属链路名称
    uint64_t    packets_received; ///< 已接收包数
    uint64_t    bytes_received;   ///< 已接收字节数
} dlm_udp_listener_t;

/**
 * @brief UDP 监听线程入口函数
 * @details 循环接收 UDP 数据包，并以格式化的方式打印到控制台。
 * @param arg 指向 dlm_udp_listener_t 结构体的指针
 * @return void* 线程退出状态
 */
static inline void* dlm_udp_listener_thread(void* arg)
{
    dlm_udp_listener_t* listener = (dlm_udp_listener_t*)arg;
    uint8_t buffer[DLM_UDP_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    printf("\n[%s-UDP] ════════════════════════════════════════════════════\n", listener->link_name);
    printf("[%s-UDP] UDP 监听已启动: %s:%u\n", listener->link_name, listener->bind_ip, listener->port);
    printf("[%s-UDP] 等待接收数据...\n", listener->link_name);
    printf("[%s-UDP] ════════════════════════════════════════════════════\n\n", listener->link_name);
    
    while (listener->running) {
        ssize_t recv_len = recvfrom(listener->socket_fd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&client_addr, &addr_len);
        
        if (recv_len < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (listener->running) {
                perror("[UDP] recvfrom 失败");
            }
            break;
        }
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            listener->packets_received++;
            listener->bytes_received += recv_len;
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            uint16_t client_port = ntohs(client_addr.sin_port);
            
            /* 获取当前时间 */
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
            
            printf("\n");
            printf("┌─────────────────────────────────────────────────────────────┐\n");
            printf("│ [%s] %s 收到 UDP 数据包 #%lu                      \n", 
                   time_str, listener->link_name, listener->packets_received);
            printf("├─────────────────────────────────────────────────────────────┤\n");
            printf("│ 来源: %s:%u                                       \n", client_ip, client_port);
            printf("│ 大小: %zd 字节                                              \n", recv_len);
            printf("├─────────────────────────────────────────────────────────────┤\n");
            printf("│ 数据内容:                                                   \n");
            
            /* 打印数据内容（可打印字符） */
            bool is_printable = true;
            for (ssize_t i = 0; i < recv_len && i < 64; i++) {
                if (buffer[i] < 32 && buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != '\t') {
                    is_printable = false;
                    break;
                }
            }
            
            if (is_printable) {
                printf("│ \"%s\"\n", buffer);
            } else {
                /* HEX 格式显示 */
                printf("│ HEX: ");
                for (ssize_t i = 0; i < recv_len && i < 32; i++) {
                    printf("%02X ", buffer[i]);
                }
                if (recv_len > 32) printf("...");
                printf("\n");
            }
            
            printf("├─────────────────────────────────────────────────────────────┤\n");
            printf("│ 统计: 已收 %lu 包, %lu 字节                         \n", 
                   listener->packets_received, listener->bytes_received);
            printf("└─────────────────────────────────────────────────────────────┘\n");
            printf("\n");
        }
    }
    
    printf("[%s-UDP] UDP 监听线程已退出\n", listener->link_name);
    return NULL;
}

/**
 * @brief 启动 UDP 监听器
 * @details 创建 UDP Socket，绑定到指定地址，并启动监听线程。
 * @param listener 监听器上下文指针
 * @param bind_ip 绑定的 IP 地址
 * @param port 监听端口
 * @param link_name 链路名称 (用于日志显示)
 * @return int 成功返回 0，失败返回 -1
 */
static inline int dlm_udp_listener_start(dlm_udp_listener_t* listener, 
                                         const char* bind_ip, 
                                         uint16_t port,
                                         const char* link_name)
{
    memset(listener, 0, sizeof(dlm_udp_listener_t));
    strncpy(listener->bind_ip, bind_ip, sizeof(listener->bind_ip) - 1);
    listener->port = port;
    listener->link_name = link_name;
    
    /* 创建 UDP socket */
    listener->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listener->socket_fd < 0) {
        perror("[UDP] 创建 socket 失败");
        return -1;
    }
    
    /* 设置 socket 选项 */
    int reuse = 1;
    setsockopt(listener->socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    /* 设置接收超时 */
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(listener->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* 绑定到指定 IP 和端口 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "[UDP] 无效的 IP 地址: %s\n", bind_ip);
        close(listener->socket_fd);
        return -1;
    }
    
    if (bind(listener->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[UDP] 绑定失败 (%s:%u): %s\n", bind_ip, port, strerror(errno));
        close(listener->socket_fd);
        return -1;
    }
    
    /* 启动监听线程 */
    listener->running = true;
    if (pthread_create(&listener->thread, NULL, dlm_udp_listener_thread, listener) != 0) {
        perror("[UDP] 创建监听线程失败");
        close(listener->socket_fd);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 停止 UDP 监听器
 * @details 停止监听线程并关闭 Socket。
 * @param listener 监听器上下文指针
 */
static inline void dlm_udp_listener_stop(dlm_udp_listener_t* listener)
{
    if (!listener->running) return;
    
    listener->running = false;
    
    /* 关闭 socket 以中断 recvfrom */
    if (listener->socket_fd >= 0) {
        shutdown(listener->socket_fd, SHUT_RDWR);
        close(listener->socket_fd);
        listener->socket_fd = -1;
    }
    
    /* 等待线程结束 */
    pthread_join(listener->thread, NULL);
    
    printf("[%s-UDP] UDP 监听已停止 (共收 %lu 包, %lu 字节)\n",
           listener->link_name, listener->packets_received, listener->bytes_received);
}

#endif /* DLM_COMMON_H */
