/**
 * @file magic_lmi.h
 * @brief MAGIC LMI (Link Management Interface) 链路管理接口
 * @description 实现 ARINC 839 LMI 规范, 用于 CM Core 与 DLM 之间的通信
 * 
 * 本文件定义:
 * - DLM 客户端状态管理结构体
 * - LMI 上下文和服务器功能
 * - IEEE 802.21 标准原语的 API 封装
 * - 事件回调机制
 * 
 * 架构说明:
 * +------------+     Unix Socket     +------------+
 * |  CM Core   | <=================> |    DLM     |
 * | (MAGIC核心)|   /tmp/magic_lmi.sock| (链路模块) |
 * +------------+                     +------------+
 * 
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MAGIC_LMI_H         /* 头文件保护宏开始 */
#define MAGIC_LMI_H         /* 防止重复包含 */

#include "magic_config.h"   /* MAGIC 配置定义 */
#include "mih_protocol.h"   /* MIH 协议定义 */
#include "mih_extensions.h" /* MIH 扩展定义 */
#include <pthread.h>        /* POSIX 线程支持 */
#include <stdbool.h>        /* 布尔类型支持 */

/*===========================================================================
 * DLM 客户端状态结构体 (扩展 MIH 支持)
 * 
 * 本节定义 DLM (Data Link Module) 客户端的状态管理结构
 * 每个 DLM 实例连接到 LMI 服务器后, 会创建一个 DlmClient 实例
 *===========================================================================*/

#define MAX_DLM_CLIENTS     10      /* 最大 DLM 客户端连接数 */
#define MAX_BEARERS         8       /* 每个 DLM 客户端最大 Bearer 数量 */

/*---------------------------------------------------------------------------
 * BearerState - 承载状态结构体
 * 
 * Bearer 是在链路上建立的逻辑通道
 * 每个 Bearer 有独立的 QoS 参数和流量统计
 *---------------------------------------------------------------------------*/
typedef struct {
    bool                is_active;      /* 是否处于活动状态 */
    BEARER_ID           bearer_id;      /* Bearer 唯一标识符 (0-255) */
    QOS_PARAM           qos_params;     /* 该 Bearer 的 QoS 参数 */
    time_t              created_time;   /* Bearer 创建时间戳 */
    uint64_t            tx_bytes;       /* 发送字节数统计 */
    uint64_t            rx_bytes;       /* 接收字节数统计 */
} BearerState;

/*---------------------------------------------------------------------------
 * DlmClient - DLM 客户端状态结构体
 * 
 * 管理一个 DLM 实例的完整状态
 * 包括连接信息、链路参数、Bearer 列表等
 *---------------------------------------------------------------------------*/
typedef struct {
    /*-----------------------------------------------------------------------
     * 基本连接信息
     *-----------------------------------------------------------------------*/
    int                     client_fd;          /* 客户端 Socket 文件描述符 */
    char                    link_id[MAX_ID_LEN];/* 链路标识符 (如 "SATCOM", "CELLULAR") */
    char                    dlm_id[MAX_ID_LEN]; /* DLM 实例标识符 */
    bool                    is_registered;      /* 是否已完成注册握手 */
    time_t                  last_heartbeat;     /* 最后一次心跳时间 */
    time_t                  last_seen;          /* 最后一次收到消息的时间 (用于超时检测) */
    
    /*-----------------------------------------------------------------------
     * MIH 协议扩展字段
     *-----------------------------------------------------------------------*/
    LINK_TUPLE_ID           link_identifier;    /* MIH 链路元组标识符 */
    MIH_Link_Capabilities   capabilities;       /* 链路能力信息 */
    mih_link_parameters_t   link_params;        /* 当前链路参数 */
    pid_t                   dlm_pid;            /* DLM 进程 ID (用于监控) */
    HARDWARE_HEALTH         health_status;      /* 硬件健康状态 */
    BearerState             bearers[MAX_BEARERS];/* Bearer 状态数组 */
    uint8_t                 num_active_bearers; /* 当前活动 Bearer 数量 */
    
    /*-----------------------------------------------------------------------
     * IEEE 802.21 标准原语支持
     *-----------------------------------------------------------------------*/
    LINK_CAPABILITY         link_capability;    /* 链路能力 (Capability_Discover 返回) */
    uint16_t                subscribed_events;  /* 已订阅的事件位图 (LINK_EVENT_TYPE) */
    LINK_PARAMETERS         current_parameters; /* 当前链路参数 (Get_Parameters 返回) */
} DlmClient;

/*===========================================================================
 * LMI Context (Link Management Interface) - LMI 上下文
 * 
 * LMI 上下文是 LMI 服务器的核心数据结构
 * 管理所有 DLM 客户端连接和事件回调
 *===========================================================================*/

#define MAX_EVENT_CALLBACKS     16  /* 最大事件回调注册数 */

/* 前向声明: LMI 上下文结构体 */
struct MagicLmiContext;

/**
 * @brief 链路事件回调函数类型
 * 
 * 当 DLM 发送事件通知时, LMI 服务器会调用注册的回调函数
 * 
 * @param ctx       LMI 上下文指针
 * @param link_id   触发事件的链路标识符
 * @param event_type 事件类型 (LINK_EVENT_TYPE 值)
 * @param event_data 事件数据指针 (根据事件类型解析)
 */
typedef void (*lmi_event_callback_t)(
    struct MagicLmiContext* ctx,    /* LMI 上下文 */
    const char* link_id,            /* 链路标识符 */
    uint16_t event_type,            /* 事件类型 */
    const void* event_data          /* 事件数据 (类型取决于 event_type) */
);

/*---------------------------------------------------------------------------
 * EventCallbackEntry - 事件回调记录
 * 用于存储注册的事件回调
 *---------------------------------------------------------------------------*/
typedef struct {
    uint16_t                event_type;     /* 订阅的事件类型 */
    lmi_event_callback_t    callback;       /* 回调函数指针 */
} EventCallbackEntry;

/*---------------------------------------------------------------------------
 * MagicLmiContext - LMI 上下文结构体
 * 
 * 核心数据结构, 管理:
 * - LMI 服务器 Socket
 * - 所有 DLM 客户端连接
 * - 事件回调注册表
 *---------------------------------------------------------------------------*/
typedef struct MagicLmiContext {
    /*-----------------------------------------------------------------------
     * 流式服务器状态 (SOCK_STREAM - 用于完整 MIH 传输层)
     *-----------------------------------------------------------------------*/
    int             server_fd;          /* 服务器监听 Socket 文件描述符 */
    pthread_t       server_thread;      /* 服务器处理线程 */
    bool            running;            /* 服务器运行状态标志 */
    
    /*-----------------------------------------------------------------------
     * 数据报服务器状态 (SOCK_DGRAM - 用于 DLM 原型简化协议)
     * DLM 原型使用 2 字节类型码 + 结构体的简化格式
     *-----------------------------------------------------------------------*/
    int             dgram_fd;           /* 数据报服务器 Socket 文件描述符 */
    pthread_t       dgram_thread;       /* 数据报服务器处理线程 */
    bool            dgram_running;      /* 数据报服务器运行状态标志 */
    
    /*-----------------------------------------------------------------------
     * 心跳监控线程 (用于检测 DLM 超时)
     *-----------------------------------------------------------------------*/
    pthread_t       heartbeat_monitor_thread;   /* 心跳监控线程 */
    bool            heartbeat_monitor_running;  /* 心跳监控运行状态 */
    
    /*-----------------------------------------------------------------------
     * 客户端管理
     *-----------------------------------------------------------------------*/
    DlmClient       clients[MAX_DLM_CLIENTS];   /* DLM 客户端数组 */
    pthread_mutex_t clients_mutex;              /* 客户端数组保护锁 */
    
    /*-----------------------------------------------------------------------
     * 配置引用
     *-----------------------------------------------------------------------*/
    MagicConfig*    config;             /* MAGIC 系统配置指针 */
    
    /*-----------------------------------------------------------------------
     * 事件回调机制
     *-----------------------------------------------------------------------*/
    EventCallbackEntry  event_callbacks[MAX_EVENT_CALLBACKS];   /* 回调注册表 */
    int                 num_callbacks;                          /* 已注册回调数 */
    pthread_mutex_t     callbacks_mutex;                        /* 回调表保护锁 */
} MagicLmiContext;

/*===========================================================================
 * LMI 基础 API 函数
 * 
 * 这些函数提供 LMI 服务器的基本生命周期管理
 * 包括初始化、启动、客户端查找、状态更新和清理
 *===========================================================================*/

/**
 * @brief 初始化 LMI 接口
 * 
 * 初始化 LMI 上下文的所有字段:
 * - 清空客户端数组
 * - 初始化互斥锁
 * - 设置初始状态
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @return 0=成功, -1=失败
 */
int magic_lmi_init(MagicLmiContext* ctx);

/**
 * @brief 启动 LMI 服务器
 * 
 * 创建 Unix Domain Socket 并开始监听 DLM 连接
 * 启动服务器处理线程
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param config 指向 MAGIC 配置的指针
 * @return 0=成功, -1=失败
 */
int magic_lmi_start_server(MagicLmiContext* ctx, MagicConfig* config);

/**
 * @brief 启动数据报模式 LMI 服务器
 * 
 * 创建 Unix Domain Datagram Socket 并开始接收 DLM 原型消息
 * DLM 原型使用简化的 2 字节类型码 + 结构体格式
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param socket_path 数据报套接字路径 (默认 /tmp/mihf.sock)
 * @return 0=成功, -1=失败
 */
int magic_lmi_start_dgram_server(MagicLmiContext* ctx, const char* socket_path);

/**
 * @brief 按链路 ID 查找 DLM 客户端
 * 
 * 在已连接的 DLM 客户端中查找指定链路
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 要查找的链路标识符
 * @return 找到的 DlmClient 指针, 未找到返回 NULL
 */
DlmClient* magic_lmi_find_by_link(MagicLmiContext* ctx, const char* link_id);

/**
 * @brief 更新链路状态
 * 
 * 更新指定链路的活动状态
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 链路标识符
 * @param is_active true=在线, false=离线
 */
void magic_lmi_update_link_status(MagicLmiContext* ctx, 
                                  const char* link_id, 
                                  bool is_active);

/**
 * @brief 清理 LMI 接口
 * 
 * 关闭所有连接, 停止服务器线程, 释放资源
 * 
 * @param ctx 指向 LMI 上下文的指针
 */
void magic_lmi_cleanup(MagicLmiContext* ctx);

/*===========================================================================
 * MIH 原语支持 API (ARINC 839 Attachment 2)
 * 
 * 这些函数实现 ARINC 839 定义的 MIH 原语
 * 用于 CM Core 与 DLM 之间的资源管理通信
 *===========================================================================*/

/**
 * @brief 处理 MIH_LINK_RESOURCE.Request - MIH 链路资源请求
 * 
 * 用于请求或释放链路资源 (Bearer)
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param request 资源请求结构体
 * @param confirm 输出: 资源确认结构体
 * @return 0=成功, -1=失败
 */
int magic_dlm_mih_link_resource_request(
    MagicLmiContext* ctx,
    const MIH_Link_Resource_Request* request,
    MIH_Link_Resource_Confirm* confirm
);

/**
 * @brief 处理 LINK_RESOURCE.Request - 链路层资源请求
 * 
 * DLM 内部使用, 向链路层请求或释放资源
 * 
 * @param client 指向 DLM 客户端的指针
 * @param request 资源请求结构体
 * @param confirm 输出: 资源确认结构体
 * @return 0=成功, -1=失败
 */
int magic_dlm_link_resource_request(
    DlmClient* client,
    const LINK_Resource_Request* request,
    LINK_Resource_Confirm* confirm
);

/**
 * @brief 分配新的 Bearer ID
 * 
 * 在 DLM 客户端上创建新的 Bearer
 * 
 * @param client 指向 DLM 客户端的指针
 * @return 分配的 Bearer ID, 失败返回 0xFF
 */
BEARER_ID magic_dlm_allocate_bearer(DlmClient* client);

/**
 * @brief 释放 Bearer
 * 
 * 删除指定的 Bearer, 释放相关资源
 * 
 * @param client 指向 DLM 客户端的指针
 * @param bearer_id 要释放的 Bearer ID
 * @return 0=成功, -1=失败
 */
int magic_dlm_release_bearer(DlmClient* client, BEARER_ID bearer_id);

/**
 * @brief 查找 Bearer
 * 
 * 按 ID 查找指定的 Bearer 状态
 * 
 * @param client 指向 DLM 客户端的指针
 * @param bearer_id 要查找的 Bearer ID
 * @return 找到的 BearerState 指针, 未找到返回 NULL
 */
BearerState* magic_dlm_find_bearer(DlmClient* client, BEARER_ID bearer_id);

/**
 * @brief 更新硬件健康状态
 * 
 * 更新 DLM 客户端的硬件健康信息
 * 
 * @param client 指向 DLM 客户端的指针
 * @param health 新的健康状态
 */
void magic_dlm_update_health(DlmClient* client, const HARDWARE_HEALTH* health);

/*===========================================================================
 * IEEE 802.21 标准原语 API (ARINC 839 Attachment 2 Section 2.1)
 * 
 * 这些函数封装了 IEEE 802.21 标准定义的链路管理原语
 * 提供给 CM Core 调用, 用于:
 * - 发现链路能力
 * - 订阅/取消订阅事件
 * - 查询链路参数
 *===========================================================================*/

/**
 * @brief Link_Capability_Discover - 发现链路能力
 * 
 * 查询指定链路的能力信息
 * 
 * 典型使用场景:
 * - 系统启动时发现可用链路的功能
 * - 切换前评估目标链路的能力
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 要查询的链路标识符
 * @param confirm 输出: 链路能力确认结构体
 * @return 0=成功, -1=失败
 */
int magic_lmi_capability_discover(
    MagicLmiContext* ctx,
    const char* link_id,
    LINK_Capability_Discover_Confirm* confirm
);

/**
 * @brief Link_Event_Subscribe - 订阅链路事件
 * 
 * 订阅指定链路的事件通知
 * 订阅后, DLM 会在事件发生时主动通知 CM Core
 * 
 * 可订阅事件 (LINK_EVENT_TYPE):
 * - LINK_EVENT_UP: 链路上线
 * - LINK_EVENT_DOWN: 链路下线
 * - LINK_EVENT_GOING_DOWN: 链路即将断开
 * - LINK_EVENT_DETECTED: 发现新链路
 * - LINK_EVENT_PARAM_REPORT: 参数变化报告
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 目标链路标识符
 * @param event_list 要订阅的事件位图 (LINK_EVENT_TYPE 组合)
 * @param confirm 输出: 订阅确认结构体
 * @return 0=成功, -1=失败
 */
int magic_lmi_event_subscribe(
    MagicLmiContext* ctx,
    const char* link_id,
    uint16_t event_list,
    LINK_Event_Subscribe_Confirm* confirm
);

/**
 * @brief Link_Event_Unsubscribe - 取消订阅链路事件
 * 
 * 取消已订阅的事件通知
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 目标链路标识符
 * @param event_list 要取消订阅的事件位图
 * @param confirm 输出: 取消订阅确认结构体
 * @return 0=成功, -1=失败
 */
int magic_lmi_event_unsubscribe(
    MagicLmiContext* ctx,
    const char* link_id,
    uint16_t event_list,
    LINK_Event_Unsubscribe_Confirm* confirm
);

/**
 * @brief Link_Get_Parameters - 获取链路参数
 * 
 * 主动查询链路当前参数
 * 这是 "拉" 模式, 与 Link_Parameters_Report 的 "推" 模式互补
 * 
 * 可查询参数 (LINK_PARAM_QUERY_TYPE):
 * - LINK_PARAM_QUERY_DATA_RATE: 数据速率
 * - LINK_PARAM_QUERY_SIGNAL_STRENGTH: 信号强度
 * - LINK_PARAM_QUERY_LATENCY: 延迟
 * - LINK_PARAM_QUERY_PACKET_LOSS: 丢包率
 * - LINK_PARAM_QUERY_IP_CONFIG: IP 配置
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 目标链路标识符
 * @param param_types 要查询的参数类型位图
 * @param confirm 输出: 参数确认结构体
 * @return 0=成功, -1=失败
 */
int magic_lmi_get_parameters(
    MagicLmiContext* ctx,
    const char* link_id,
    uint16_t param_types,
    LINK_Get_Parameters_Confirm* confirm
);

/*===========================================================================
 * 链路事件处理回调机制 (用于 CM Core 接收 DLM 事件)
 * 
 * CM Core 可以注册回调函数来处理特定类型的链路事件
 * 当 DLM 发送事件指示时, LMI 服务器会调用对应的回调
 * 
 * 事件处理流程:
 * 1. DLM 发送事件指示 (如 Link_Going_Down)
 * 2. LMI 服务器接收事件
 * 3. LMI 服务器查找已注册的回调
 * 4. 调用匹配的回调函数
 * 5. CM Core 在回调中处理事件 (如触发链路切换)
 *===========================================================================*/

/**
 * @brief 注册事件回调
 * 
 * 注册一个回调函数来处理特定类型的链路事件
 * 
 * 使用示例:
 * ```c
 * // 注册链路即将断开事件处理
 * magic_lmi_register_event_callback(ctx, LINK_EVENT_GOING_DOWN, my_handler);
 * 
 * // 注册多个事件 (需要多次调用)
 * magic_lmi_register_event_callback(ctx, LINK_EVENT_UP, on_link_up);
 * magic_lmi_register_event_callback(ctx, LINK_EVENT_DOWN, on_link_down);
 * ```
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param event_type 要订阅的事件类型 (LINK_EVENT_TYPE 值)
 * @param callback 回调函数指针
 * @return 0=成功, -1=失败 (回调表已满)
 */
int magic_lmi_register_event_callback(
    MagicLmiContext* ctx,
    uint16_t event_type,
    lmi_event_callback_t callback
);

/**
 * @brief 触发 Link_Going_Down 预警处理
 * 
 * 当链路即将断开时, 调用此函数触发切换准备
 * 
 * 处理流程:
 * 1. 查找注册的 LINK_EVENT_GOING_DOWN 回调
 * 2. 调用回调函数, 传递断开时间和原因
 * 3. CM Core 开始准备链路切换
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 即将断开的链路标识符
 * @param time_to_down_ms 预计断开时间 (毫秒)
 * @param reason 断开原因 (LINK_DOWN_REASON 值)
 */
void magic_lmi_handle_link_going_down(
    MagicLmiContext* ctx,
    const char* link_id,
    uint32_t time_to_down_ms,
    LINK_DOWN_REASON reason
);

/**
 * @brief 触发 Link_Detected 处理
 * 
 * 当检测到新的可用链路时, 调用此函数通知 CM Core
 * 
 * 使用场景:
 * - 飞机进入新基站覆盖区域
 * - 卫星链路重新建立连接
 * - 检测到机场 Wi-Fi 热点
 * 
 * @param ctx 指向 LMI 上下文的指针
 * @param indication 新链路信息结构体
 */
void magic_lmi_handle_link_detected(
    MagicLmiContext* ctx,
    const LINK_Detected_Indication* indication
);

#endif /* MAGIC_LMI_H */  /* 头文件保护宏结束 */
