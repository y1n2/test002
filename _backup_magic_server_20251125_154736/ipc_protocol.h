/**
 * @file ipc_protocol.h
 * @brief MAGIC IPC Protocol - DLM 与 CM Core 通信协议
 * @description 基于 ARINC 839 LMI 原语的简化 IPC 协议
 */

#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

/*===========================================================================
 * Socket Configuration
 *===========================================================================*/

#define MAGIC_CORE_SOCKET_PATH  "/tmp/magic_core.sock"
#define MAX_DLM_CLIENTS         10
#define MAX_IPC_NAME_LEN        32
#define MAX_IFACE_LEN           16

/*===========================================================================
 * LMI Message Types (基于 ARINC 839)
 *===========================================================================*/

typedef enum {
    MSG_TYPE_REGISTER       = 0x01,  // DLM -> CM: 注册链路
    MSG_TYPE_REGISTER_ACK   = 0x02,  // CM -> DLM: 注册确认
    MSG_TYPE_LINK_EVENT     = 0x03,  // DLM -> CM: 链路状态变化
    MSG_TYPE_RESOURCE_REQ   = 0x04,  // CM -> DLM: 资源分配请求
    MSG_TYPE_RESOURCE_RESP  = 0x05,  // DLM -> CM: 资源分配响应
    MSG_TYPE_HEARTBEAT      = 0x06,  // DLM -> CM: 心跳
    MSG_TYPE_SHUTDOWN       = 0x07,  // DLM -> CM: 关闭通知
    MSG_TYPE_POLICY_REQ     = 0x08,  // CIC -> CM: 策略决策请求 (新增)
    MSG_TYPE_POLICY_RESP    = 0x09   // CM -> CIC: 策略决策响应 (新增)
} MessageType;

/*===========================================================================
 * IPC Message Header
 *===========================================================================*/

typedef struct {
    MessageType type;        // 消息类型
    uint32_t    length;      // 消息体长度（字节）
    uint32_t    sequence;    // 序列号
} __attribute__((packed)) IpcHeader;

/*===========================================================================
 * MSG_TYPE_REGISTER - 注册消息
 * DLM 启动时发送，包含链路静态配置
 *===========================================================================*/

typedef struct {
    char        dlm_id[MAX_IPC_NAME_LEN];           // "DLM_SATCOM"
    char        link_profile_id[MAX_IPC_NAME_LEN];  // "LINK_SATCOM"
    char        iface_name[MAX_IFACE_LEN];      // "eth1"
    
    // 链路静态能力（来自 Datalink_Profile.xml）
    uint32_t    cost_index;                     // 成本指数 (90/20/1)
    uint32_t    max_bw_kbps;                    // 最大带宽 (2048/20480/102400)
    uint32_t    typical_latency_ms;             // 典型延迟 (600/50/5)
    uint8_t     priority;                       // 优先级 (5/8/10)
    uint8_t     coverage;                       // 覆盖范围 (1=全球/2=陆地/3=停机位)
} __attribute__((packed)) MsgRegister;

/*===========================================================================
 * MSG_TYPE_REGISTER_ACK - 注册确认
 *===========================================================================*/

typedef struct {
    uint8_t     result;         // 0=成功, 非0=失败
    uint32_t    assigned_id;    // CM 分配的链路 ID
    char        message[64];    // 可选的描述信息
} __attribute__((packed)) MsgRegisterAck;

/*===========================================================================
 * MSG_TYPE_LINK_EVENT - 链路状态事件
 *===========================================================================*/

typedef struct {
    char        dlm_id[MAX_IPC_NAME_LEN];
    bool        is_link_up;                 // true=UP, false=DOWN
    
    // 动态链路能力（当前实际状态）
    uint32_t    current_bw_kbps;            // 当前可用带宽
    uint32_t    current_latency_ms;         // 当前延迟
    int32_t     signal_strength_dbm;        // 信号强度（可选）
    
    // IP 信息
    uint32_t    ip_address;                 // IPv4 地址（网络字节序）
    uint32_t    netmask;
} __attribute__((packed)) MsgLinkEvent;

/*===========================================================================
 * MSG_TYPE_RESOURCE_REQ - 资源分配请求
 *===========================================================================*/

typedef struct {
    uint32_t    session_id;                 // 会话 ID
    uint32_t    requested_bw_kbps;          // 请求的带宽
    uint32_t    max_latency_ms;             // 最大延迟要求
} __attribute__((packed)) MsgResourceReq;

/*===========================================================================
 * MSG_TYPE_RESOURCE_RESP - 资源分配响应
 *===========================================================================*/

typedef struct {
    uint32_t    session_id;
    uint8_t     result;                     // 0=成功, 非0=失败
    uint32_t    granted_bw_kbps;            // 实际分配的带宽
} __attribute__((packed)) MsgResourceResp;

/*===========================================================================
 * MSG_TYPE_HEARTBEAT - 心跳消息
 *===========================================================================*/

typedef struct {
    char        dlm_id[MAX_IPC_NAME_LEN];
    bool        is_healthy;                 // 健康状态
    uint64_t    tx_bytes;                   // 发送字节数
    uint64_t    rx_bytes;                   // 接收字节数
} __attribute__((packed)) MsgHeartbeat;

/*===========================================================================
 * MSG_TYPE_POLICY_REQ - 策略决策请求 (CIC -> MAGIC Core)
 *===========================================================================*/

typedef struct {
    char        client_id[64];              // 客户端标识
    char        profile_name[64];           // 业务配置名称 (IP_DATA/VOICE/VIDEO)
    uint32_t    requested_bw_kbps;          // 请求的前向带宽
    uint32_t    requested_ret_bw_kbps;      // 请求的返向带宽
    uint8_t     priority_class;             // 优先级等级 (1-8)
    uint8_t     qos_level;                  // QoS 等级 (0-3)
    uint8_t     traffic_class;              // 业务类别
    uint8_t     flight_phase;               // 飞行阶段
} __attribute__((packed)) MsgPolicyReq;

/*===========================================================================
 * MSG_TYPE_POLICY_RESP - 策略决策响应 (MAGIC Core -> CIC)
 *===========================================================================*/

typedef struct {
    uint8_t     result_code;                // 0=成功, 非0=失败
    char        selected_link_id[64];       // 选择的链路 ID
    uint32_t    granted_bw_kbps;            // 分配的前向带宽
    uint32_t    granted_ret_bw_kbps;        // 分配的返向带宽
    uint8_t     qos_level;                  // 实际 QoS 等级
    char        reason[128];                // 决策原因说明
} __attribute__((packed)) MsgPolicyResp;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief 发送 IPC 消息
 * @param fd Socket 文件描述符
 * @param type 消息类型
 * @param payload 消息体指针
 * @param payload_len 消息体长度
 * @return 0=成功, -1=失败
 */
static inline int send_ipc_msg(int fd, MessageType type, const void* payload, uint32_t payload_len)
{
    static uint32_t seq = 0;
    
    IpcHeader header = {
        .type = type,
        .length = payload_len,
        .sequence = ++seq
    };
    
    // 发送头部
    if (send(fd, &header, sizeof(header), 0) != sizeof(header)) {
        return -1;
    }
    
    // 发送消息体（如果有）
    if (payload_len > 0 && payload != NULL) {
        if (send(fd, payload, payload_len, 0) != (ssize_t)payload_len) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 接收 IPC 消息
 * @param fd Socket 文件描述符
 * @param header 输出：消息头
 * @param payload 输出：消息体缓冲区
 * @param max_payload_len 消息体缓冲区最大长度
 * @return 接收的消息体长度, -1=失败
 */
static inline int recv_ipc_msg(int fd, IpcHeader* header, void* payload, uint32_t max_payload_len)
{
    // 接收头部
    ssize_t n = recv(fd, header, sizeof(IpcHeader), MSG_WAITALL);
    if (n != sizeof(IpcHeader)) {
        return -1;
    }
    
    // 接收消息体
    if (header->length > 0) {
        if (header->length > max_payload_len) {
            return -1;  // 缓冲区太小
        }
        
        n = recv(fd, payload, header->length, MSG_WAITALL);
        if (n != (ssize_t)header->length) {
            return -1;
        }
    }
    
    return header->length;
}

/**
 * @brief 消息类型转字符串
 */
static inline const char* msg_type_to_string(MessageType type)
{
    switch (type) {
        case MSG_TYPE_REGISTER:      return "REGISTER";
        case MSG_TYPE_REGISTER_ACK:  return "REGISTER_ACK";
        case MSG_TYPE_LINK_EVENT:    return "LINK_EVENT";
        case MSG_TYPE_RESOURCE_REQ:  return "RESOURCE_REQ";
        case MSG_TYPE_RESOURCE_RESP: return "RESOURCE_RESP";
        case MSG_TYPE_HEARTBEAT:     return "HEARTBEAT";
        case MSG_TYPE_SHUTDOWN:      return "SHUTDOWN";
        default:                     return "UNKNOWN";
    }
}

#endif /* IPC_PROTOCOL_H */
