/**
 * @file magic_ipc_protocol.h
 * @brief MAGIC IPC Protocol for CM-DLM Communication via Unix Domain Socket
 * @author MAGIC Team
 * @date 2025-11-24
 * 
 * 定义 CM Core 与 DLM 进程之间的通信协议
 * 通信方式：Unix Domain Socket
 * 架构：CM Core 为服务器端，所有 DLM 为客户端
 */

#ifndef MAGIC_IPC_PROTOCOL_H
#define MAGIC_IPC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/*===========================================================================
 * Socket Configuration
 *===========================================================================*/

#define CM_SOCKET_PATH          "/tmp/magic_cm.sock"
#define DLM_SOCKET_BACKLOG      10
#define MAX_MESSAGE_SIZE        4096
#define MAX_LINK_NAME_LEN       64
#define MAX_IFACE_NAME_LEN      16

/*===========================================================================
 * Message Types (DLM -> CM)
 *===========================================================================*/

typedef enum {
    /* Registration & Lifecycle */
    MSG_REGISTER_REQUEST        = 0x0001,  // DLM 注册到 CM
    MSG_REGISTER_RESPONSE       = 0x0002,  // CM 确认注册
    MSG_HEARTBEAT               = 0x0003,  // DLM 心跳消息
    MSG_HEARTBEAT_ACK           = 0x0004,  // CM 心跳响应
    MSG_UNREGISTER              = 0x0005,  // DLM 注销
    
    /* Link State Notifications (DLM -> CM) */
    MSG_LINK_UP                 = 0x0010,  // 链路激活
    MSG_LINK_DOWN               = 0x0011,  // 链路断开
    MSG_LINK_DEGRADED           = 0x0012,  // 链路质量下降
    MSG_LINK_RESTORED           = 0x0013,  // 链路质量恢复
    MSG_CAPABILITY_CHANGE       = 0x0014,  // 链路能力变化
    
    /* Resource Management (CM -> DLM) */
    MSG_ALLOCATE_REQUEST        = 0x0020,  // CM 请求分配资源
    MSG_ALLOCATE_RESPONSE       = 0x0021,  // DLM 响应分配结果
    MSG_RELEASE_REQUEST         = 0x0022,  // CM 请求释放资源
    MSG_RELEASE_RESPONSE        = 0x0023,  // DLM 确认释放
    MSG_SUSPEND_REQUEST         = 0x0024,  // CM 暂停链路
    MSG_RESUME_REQUEST          = 0x0025,  // CM 恢复链路
    
    /* Statistics Query */
    MSG_STATS_REQUEST           = 0x0030,  // CM 查询统计信息
    MSG_STATS_RESPONSE          = 0x0031,  // DLM 返回统计信息
    
    /* Error Handling */
    MSG_ERROR                   = 0x00FF   // 错误消息
} ipc_msg_type_t;

/*===========================================================================
 * Link Types (same as LMI)
 *===========================================================================*/

typedef enum {
    LINK_TYPE_SATCOM    = 1,   // 卫星链路 (eth1)
    LINK_TYPE_CELLULAR  = 2,   // 蜂窝链路 (eth2)
    LINK_TYPE_WIFI      = 3    // WiFi 链路 (eth3)
} ipc_link_type_t;

/*===========================================================================
 * Link State (same as LMI)
 *===========================================================================*/

typedef enum {
    LINK_STATE_UNAVAILABLE  = 0,   // 不可用
    LINK_STATE_AVAILABLE    = 1,   // 可用但未激活
    LINK_STATE_ACTIVATING   = 2,   // 激活中
    LINK_STATE_ACTIVE       = 3,   // 已激活
    LINK_STATE_SUSPENDED    = 4,   // 挂起
    LINK_STATE_ERROR        = 5    // 错误状态
} ipc_link_state_t;

/*===========================================================================
 * Coverage Type
 *===========================================================================*/

typedef enum {
    COVERAGE_GLOBAL         = 1,   // 全球覆盖（卫星）
    COVERAGE_TERRESTRIAL    = 2,   // 陆地覆盖（蜂窝）
    COVERAGE_GATE_ONLY      = 3    // 仅停机位（WiFi）
} ipc_coverage_t;

/*===========================================================================
 * Message Header (所有消息的公共头部)
 *===========================================================================*/

typedef struct {
    uint16_t    msg_type;           // 消息类型 (ipc_msg_type_t)
    uint16_t    msg_length;         // 消息总长度（包含头部）
    uint32_t    sequence_num;       // 序列号
    uint32_t    timestamp;          // 时间戳（秒）
    uint32_t    link_id;            // 链路 ID（0表示未分配）
} __attribute__((packed)) ipc_msg_header_t;

/*===========================================================================
 * REGISTER_REQUEST Message (DLM -> CM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    /* Link Identity */
    char                link_name[MAX_LINK_NAME_LEN];      // 例如："SATCOM_Link"
    char                interface_name[MAX_IFACE_NAME_LEN]; // 例如："eth1"
    uint8_t             link_type;                          // ipc_link_type_t
    uint8_t             coverage_type;                      // ipc_coverage_t
    uint16_t            reserved;
    
    /* Link Capabilities */
    uint32_t            max_bandwidth_kbps;    // 最大带宽（kbps）
    uint32_t            min_bandwidth_kbps;    // 最小带宽
    uint32_t            latency_ms;            // 延迟（毫秒）
    uint32_t            cost_per_mb;           // 成本（每MB，单位：分）
    uint8_t             priority;              // 优先级（1-10）
    uint8_t             security_level;        // 安全等级
    uint16_t            mtu;                   // MTU 大小
    
    /* DLM Process Info */
    pid_t               dlm_pid;               // DLM 进程 PID
} __attribute__((packed)) ipc_register_req_t;

/*===========================================================================
 * REGISTER_RESPONSE Message (CM -> DLM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint32_t            assigned_link_id;      // CM 分配的链路 ID
    uint8_t             registration_result;   // 0=成功, 1=失败
    char                error_msg[128];        // 错误信息（如果失败）
} __attribute__((packed)) ipc_register_resp_t;

/*===========================================================================
 * LINK_UP/LINK_DOWN Message (DLM -> CM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint8_t             new_state;             // ipc_link_state_t
    uint8_t             previous_state;        // 之前的状态
    uint16_t            reserved;
    
    /* Network Interface Status */
    uint32_t            ip_address;            // IPv4 地址（网络字节序）
    uint32_t            netmask;               // 子网掩码
    uint32_t            gateway;               // 默认网关
    
    /* Current Capabilities (may change) */
    uint32_t            current_bandwidth_kbps; // 当前可用带宽
    uint32_t            current_latency_ms;     // 当前延迟
    int32_t             signal_strength_dbm;    // 信号强度（dBm）
    
    char                status_message[128];    // 状态描述
} __attribute__((packed)) ipc_link_status_t;

/*===========================================================================
 * HEARTBEAT Message (DLM -> CM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint8_t             link_state;            // 当前状态
    uint8_t             health_status;         // 0=正常, 1=警告, 2=错误
    uint16_t            reserved;
    
    /* Quick Stats */
    uint64_t            bytes_sent;
    uint64_t            bytes_received;
    uint32_t            packets_sent;
    uint32_t            packets_received;
    uint32_t            errors;
    uint32_t            drops;
} __attribute__((packed)) ipc_heartbeat_t;

/*===========================================================================
 * ALLOCATE_REQUEST Message (CM -> DLM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint32_t            session_id;            // 会话 ID
    uint32_t            requested_bandwidth;   // 请求的带宽（kbps）
    uint32_t            max_latency;           // 最大可接受延迟（ms）
    uint8_t             qos_class;             // QoS 等级（0-7）
    uint8_t             security_required;     // 是否需要加密
    uint16_t            reserved;
    
    /* Traffic Flow Info (optional) */
    uint32_t            src_ip;
    uint32_t            dst_ip;
    uint16_t            src_port;
    uint16_t            dst_port;
} __attribute__((packed)) ipc_allocate_req_t;

/*===========================================================================
 * ALLOCATE_RESPONSE Message (DLM -> CM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint32_t            session_id;            // 对应的会话 ID
    uint8_t             allocation_result;     // 0=成功, 其他=错误码
    uint8_t             reserved[3];
    
    uint32_t            granted_bandwidth;     // 实际分配的带宽
    uint32_t            estimated_latency;     // 预计延迟
    
    char                error_msg[128];
} __attribute__((packed)) ipc_allocate_resp_t;

/*===========================================================================
 * STATS_RESPONSE Message (DLM -> CM)
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    /* Traffic Statistics */
    uint64_t            tx_bytes;
    uint64_t            rx_bytes;
    uint64_t            tx_packets;
    uint64_t            rx_packets;
    uint64_t            tx_errors;
    uint64_t            rx_errors;
    uint64_t            tx_dropped;
    uint64_t            rx_dropped;
    
    /* Link Quality */
    int32_t             signal_strength_dbm;
    int32_t             signal_quality;        // 0-100
    uint32_t            current_bandwidth;
    uint32_t            current_latency;
    
    /* Connection Info */
    uint32_t            connection_duration;   // 连接持续时间（秒）
    uint32_t            uptime;                // 链路正常运行时间
    uint32_t            reconnect_count;       // 重连次数
} __attribute__((packed)) ipc_stats_resp_t;

/*===========================================================================
 * ERROR Message
 *===========================================================================*/

typedef struct {
    ipc_msg_header_t    header;
    
    uint16_t            error_code;
    uint16_t            reserved;
    char                error_description[256];
} __attribute__((packed)) ipc_error_msg_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

#define IPC_SUCCESS                 0
#define IPC_ERR_INVALID_MSG         1
#define IPC_ERR_INVALID_LINK_ID     2
#define IPC_ERR_LINK_NOT_READY      3
#define IPC_ERR_RESOURCE_UNAVAIL    4
#define IPC_ERR_INSUFFICIENT_BW     5
#define IPC_ERR_TIMEOUT             6
#define IPC_ERR_HARDWARE_FAILURE    7
#define IPC_ERR_INTERNAL            99

/*===========================================================================
 * Union for Easy Message Handling
 *===========================================================================*/

typedef union {
    ipc_msg_header_t        header;
    ipc_register_req_t      register_req;
    ipc_register_resp_t     register_resp;
    ipc_link_status_t       link_status;
    ipc_heartbeat_t         heartbeat;
    ipc_allocate_req_t      allocate_req;
    ipc_allocate_resp_t     allocate_resp;
    ipc_stats_resp_t        stats_resp;
    ipc_error_msg_t         error_msg;
} ipc_message_t;

/*===========================================================================
 * Helper Functions (implemented in magic_ipc_utils.c)
 *===========================================================================*/

/**
 * @brief 初始化消息头部
 */
void ipc_init_header(ipc_msg_header_t *header, uint16_t msg_type, 
                     uint16_t msg_length, uint32_t link_id);

/**
 * @brief 发送消息到 socket
 * @return 0 成功, -1 失败
 */
int ipc_send_message(int sockfd, const void *msg, size_t msg_len);

/**
 * @brief 从 socket 接收消息
 * @return 接收的字节数, -1 失败
 */
int ipc_recv_message(int sockfd, void *msg_buf, size_t buf_size);

/**
 * @brief 消息类型转字符串（用于日志）
 */
const char* ipc_msg_type_to_string(ipc_msg_type_t msg_type);

/**
 * @brief 链路类型转字符串
 */
const char* ipc_link_type_to_string(ipc_link_type_t link_type);

/**
 * @brief 链路状态转字符串
 */
const char* ipc_link_state_to_string(ipc_link_state_t state);

#endif /* MAGIC_IPC_PROTOCOL_H */
