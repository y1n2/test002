/**
 * @file dlm_common.h
 * @brief DLM 通用数据结构和函数
 */

#ifndef DLM_COMMON_H
#define DLM_COMMON_H

#include <pthread.h>
#include <stdbool.h>
#include "ipc_protocol.h"

/*===========================================================================
 * DLM Context - 每个 DLM 进程的运行时上下文
 *===========================================================================*/

typedef struct {
    // 静态配置（从 Datalink_Profile.xml 读取或硬编码）
    MsgRegister config;
    
    // IPC 运行时状态
    int         ipc_fd;             // 连接到 CM Core 的 Socket FD
    uint32_t    assigned_id;        // CM 分配的链路 ID
    bool        registered;         // 是否已注册
    
    // 链路运行时状态
    bool        is_up;              // 链路当前是否 UP
    uint32_t    current_bw;         // 当前可用带宽
    uint32_t    ip_address;         // 当前 IP 地址
    uint32_t    netmask;
    
    // 统计信息
    uint64_t    tx_bytes;
    uint64_t    rx_bytes;
    uint32_t    tx_packets;
    uint32_t    rx_packets;
    
    // 线程控制
    pthread_t   monitor_thread;     // 网卡状态监控线程
    pthread_t   heartbeat_thread;   // 心跳线程
    pthread_mutex_t mutex;          // 保护共享数据
    bool        running;            // 运行标志
    
} DlmContext;

/*===========================================================================
 * Function Declarations
 *===========================================================================*/

/**
 * @brief 初始化 DLM 上下文
 */
void dlm_init_context(DlmContext* ctx, const MsgRegister* initial_config);

/**
 * @brief 连接并注册到 CM Core
 * @return 0=成功, -1=失败
 */
int dlm_register(DlmContext* ctx);

/**
 * @brief 检查网卡链路状态
 * @param iface_name 网卡名称 (eth1/eth2/eth3)
 * @return true=UP, false=DOWN
 */
bool check_eth_link_status(const char* iface_name);

/**
 * @brief 获取网卡统计信息
 */
int get_eth_stats(const char* iface_name, uint64_t* tx_bytes, uint64_t* rx_bytes,
                  uint32_t* tx_packets, uint32_t* rx_packets);

/**
 * @brief 获取网卡 IP 地址
 */
int get_eth_ip_info(const char* iface_name, uint32_t* ip_addr, uint32_t* netmask);

/**
 * @brief 链路监控线程函数
 */
void* link_monitor_loop(void* arg);

/**
 * @brief 心跳线程函数
 */
void* heartbeat_loop(void* arg);

/**
 * @brief 发送链路状态事件
 */
int send_link_event(DlmContext* ctx, bool is_up);

#endif /* DLM_COMMON_H */
