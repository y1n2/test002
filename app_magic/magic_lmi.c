/**
 * @file magic_lmi.c
 * @brief MAGIC LMI (Link Management Interface) 实现文件。
 * @details 负责实现 CM Core 侧的 LMI 逻辑，包括：
 *          - LMI 服务器的生命周期管理 (Init/Start/Stop)。
 *          - DLM 客户端连接与心跳维护。
 *          - IPC 消息的编解码与分发。
 *          - MIH 原语的处理与转发。
 *
 * 架构：
 * - 主线程：管理服务器 Socket 和客户端列表。
 * - 客户端线程：每个连接的 DLM 独占一个线程处理消息交互。
 * - 监控线程：处理心跳超时和死连接清理。
 *
 * @author MAGIC System Development Team
 * @date 2025-11-30
 */

#include "magic_lmi.h"              /* LMI 接口定义 */
#include "magic_cic_push.h"         /* CIC 推送功能 (MNTR/MSCR) */
#include "mih_extensions.h"         /* MIH 扩展定义 */
#include "mih_transport.h"          /* MIH 传输层 */
#include <arpa/inet.h>              /* IP 地址转换 */
#include <errno.h>                  /* 错误码定义 */
#include <freeDiameter/extension.h> /* freeDiameter 扩展框架 */
#include <string.h>                 /* 字符串操作函数 */
#include <sys/socket.h>             /* Socket API */
#include <sys/un.h>                 /* Unix Domain Socket */
#include <unistd.h>                 /* Unix 标准函数 */

/* 访问数据平面与全局上下文 */
#include "app_magic.h"
#include "magic_dataplane.h"

#define DLM_SOCK_PATH MIH_SOCKET_PATH /* 使用 MIH 标准路径 */

/* 心跳监控配置 */
#define HEARTBEAT_TIMEOUT_SEC                                                  \
  30 /* 心跳超时时间 (秒) - DLM 无响应超过此时间判定为掉线 */
#define MONITOR_CHECK_INTERVAL_SEC                                             \
  10 /* 监控检查间隔 (秒) - 监控线程扫描周期                    \
      */

/*===========================================================================
 * IPC 协议定义 (Inter-Process Communication)
 *
 * 本节定义 CM Core 与 DLM 之间的 IPC 通信协议
 * 支持两种协议栈:
 * 1. 旧版 IPC 协议 - 用于兼容性
 * 2. MIH 传输层协议 - 标准 ARINC 839 协议
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * IPC 协议头结构体 (旧版协议)
 * 用于兼容旧版 DLM 实现
 *---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * IPC 协议头结构体 (旧版协议)
 *---------------------------------------------------------------------------*/

/**
 * @brief IPC 协议头 (旧版兼容)。
 * @details 用于在 Unix Domain Socket 上传输非标准 MIH 消息。
 */
typedef struct {
  uint8_t type;      ///< 消息类型 (MessageType 枚举值)。
  uint32_t length;   ///< 负载长度 (字节)，不含头部。
  uint32_t sequence; ///< 序列号，用于请求/响应匹配。
} __attribute__((packed)) IpcHeader;

/*---------------------------------------------------------------------------
 * IPC 消息类型枚举 (旧版协议)
 * 定义 CM Core 与 DLM 之间交换的消息类型
 *---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * IPC 消息类型枚举 (旧版协议)
 *---------------------------------------------------------------------------*/

/**
 * @brief IPC 消息类型。
 */
typedef enum {
  MSG_TYPE_REGISTER = 0x01,      ///< 注册消息 - DLM 向 CM Core 注册。
  MSG_TYPE_REGISTER_ACK = 0x02,  ///< 注册确认 - CM Core 确认注册。
  MSG_TYPE_LINK_EVENT = 0x03,    ///< 链路事件 - DLM 报告链路状态变化。
  MSG_TYPE_RESOURCE_REQ = 0x04,  ///< 资源请求 - MIH Link_Resource 请求。
  MSG_TYPE_RESOURCE_RESP = 0x05, ///< 资源响应 - MIH Link_Resource 响应。
  MSG_TYPE_HEARTBEAT = 0x06,     ///< 心跳消息 - 保持连接活跃。
  MSG_TYPE_SHUTDOWN = 0x07,      ///< 关闭消息 - 优雅关闭连接。
  MSG_TYPE_POLICY_REQ = 0x08,    ///< 策略请求 - 请求链路选择策略。
  MSG_TYPE_POLICY_RESP = 0x09    ///< 策略响应 - 返回链路选择结果。
} MessageType;

/*---------------------------------------------------------------------------
 * IPC 消息结构体定义 (旧版协议)
 * 这些结构体定义了 CM Core 与 DLM 之间交换的具体消息格式
 *---------------------------------------------------------------------------*/

/**
 * @brief 注册消息结构体 - DLM 向 CM Core 注册时发送
 * 包含 DLM 的基本信息和能力参数
 */
/**
 * @brief 注册消息结构体。
 * @details DLM 连接后发送的第一个报文，声明自身身份和能力。
 */
typedef struct {
  char dlm_id[32];             ///< DLM 实例标识符 (如 "DLM_SATCOM")。
  char link_profile_id[64];    ///< 链路配置模板 ID。
  char iface_name[16];         ///< 网络接口名称 (如 "eth1", "wlan0")。
  uint32_t cost_index;         ///< 链路成本指数 (越低越优先)。
  uint32_t max_bw_kbps;        ///< 最大带宽 (kbps)。
  uint32_t typical_latency_ms; ///< 典型延迟 (毫秒)。
  uint8_t priority;            ///< 链路优先级 (0-255, 越高越优先)。
  uint8_t coverage;            ///< 覆盖范围 (0-100, 百分比)。
} __attribute__((packed)) MsgRegister;

/**
 * @brief 注册确认消息结构体。
 * @details CM Core 对注册请求的响应。
 */
typedef struct {
  uint8_t result;       ///< 结果码: 0=成功, 1=失败。
  uint32_t assigned_id; ///< 分配的客户端 ID (成功时有效)。
  char message[64];     ///< 结果描述消息。
} __attribute__((packed)) MsgRegisterAck;

/**
 * @brief 链路事件消息结构体。
 * @details DLM 报告链路状态变更通知 (Legacy 模式使用)。
 */
typedef struct {
  char dlm_id[32];             ///< DLM 标识符。
  bool is_link_up;             ///< 链路状态: true=上线, false=下线。
  uint32_t current_bw_kbps;    ///< 当前带宽 (kbps)。
  uint32_t current_latency_ms; ///< 当前延迟 (毫秒)。
  int32_t signal_strength_dbm; ///< 信号强度 (dBm)。
  uint32_t ip_address;         ///< IP 地址 (网络字节序)。
  uint32_t netmask;            ///< 子网掩码 (网络字节序)。
} __attribute__((packed)) MsgLinkEvent;

/**
 * @brief 心跳消息结构体。
 * @details 保持连接活跃并传递基本统计信息。
 */
typedef struct {
  char dlm_id[32];   ///< DLM 标识符。
  bool is_healthy;   ///< 健康状态: true=正常, false=异常。
  uint64_t tx_bytes; ///< 发送字节数统计。
  uint64_t rx_bytes; ///< 接收字节数统计。
} __attribute__((packed)) MsgHeartbeat;

/**
 * @brief 策略请求消息结构体 - 请求链路选择策略
 * CM Core 向 DLM 请求链路选择建议
 */
/**
 * @brief 策略请求消息结构体。
 * @details CM Core 可能主动向 DLM 询问链路选择策略 (可选功能)。
 */
typedef struct {
  char client_id[64];             ///< 客户端标识符。
  char profile_name[64];          ///< 配置模板名称。
  uint32_t requested_bw_kbps;     ///< 请求的带宽 (kbps)。
  uint32_t requested_ret_bw_kbps; ///< 请求的反向带宽 (kbps)。
  uint8_t priority_class;         ///< 优先级等级。
  uint8_t qos_level;              ///< QoS 等级。
  uint8_t traffic_class;          ///< 流量类型。
  uint8_t flight_phase;           ///< 飞行阶段。
} __attribute__((packed)) MsgPolicyReq;

/**
 * @brief 策略响应消息结构体。
 */
typedef struct {
  uint8_t result_code;          ///< 结果码。
  char selected_link_id[64];    ///< 选中的链路 ID。
  uint32_t granted_bw_kbps;     ///< 授予的带宽 (kbps)。
  uint32_t granted_ret_bw_kbps; ///< 授予的反向带宽 (kbps)。
  uint8_t qos_level;            ///< QoS 等级。
  char reason[128];             ///< 选择原因描述。
} __attribute__((packed)) MsgPolicyResp;

/**
 * @brief MIH 资源请求消息结构体。
 * @details 封装 MIH Link_Resource.Request 原语 payload。
 */
typedef struct {
  char link_id[64];            ///< 目标链路 ID。
  RESOURCE_ACTION_TYPE action; ///< 操作类型: REQUEST 或 RELEASE。
  bool has_bearer_id;          ///< 是否包含 Bearer ID。
  BEARER_ID bearer_id;         ///< Bearer 标识符。
  bool has_qos_params;         ///< 是否包含 QoS 参数。
  QOS_PARAM qos_params;        ///< QoS 参数。
} __attribute__((packed)) MsgMihResourceReq;

/**
 * @brief MIH 资源响应消息结构体。
 * @details 封装 MIH Link_Resource.Confirm 原语 payload。
 */
typedef struct {
  STATUS status;       ///< 操作状态。
  bool has_bearer_id;  ///< 是否包含 Bearer ID。
  BEARER_ID bearer_id; ///< Bearer 标识符。
  char reason[128];    ///< 结果描述。
} __attribute__((packed)) MsgMihResourceResp;

/* 前向声明 - 声明将在后面定义的函数 */
static void *dlm_server_thread(void *arg);             /* DLM 服务器主线程 */
static void *handle_dlm_client_thread(void *arg);      /* DLM 客户端处理线程 */
static void *heartbeat_monitor_thread_func(void *arg); /* 心跳监控线程 */
static void *udp_listener_thread(void *arg);           /* UDP 监听线程 */
static __attribute__((unused)) int
send_ipc_msg(int fd, uint8_t type, const void *payload,
             uint32_t payload_len); /* 发送 IPC 消息 */
static __attribute__((unused)) int
recv_ipc_msg(int fd, IpcHeader *header, void *payload,
             uint32_t max_payload_len); /* 接收 IPC 消息 */
static __attribute__((unused)) void handle_mih_resource_request(
    MagicLmiContext *ctx, int client_fd,
    const MsgMihResourceReq *req); /* 处理 MIH 资源请求 */
static void
trigger_lmi_event_callbacks(MagicLmiContext *ctx, const char *link_id,
                            uint16_t event_type,
                            const void *event_data); /* 触发事件回调 */
static void handle_mih_parameters_report(
    MagicLmiContext *ctx, int client_fd,
    const LINK_Parameters_Report_Indication *ind); /* 处理 MIH 参数报告 */
static void handle_udp_heartbeat(MagicLmiContext *ctx,
                                 const struct sockaddr_in *from_addr,
                                 const void *data,
                                 size_t data_len); /* 处理 UDP 心跳 */

/*===========================================================================
 * LMI 基础 API 函数实现
 *
 * 这些函数提供 LMI 接口的基本生命周期管理
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
int magic_lmi_init(MagicLmiContext *ctx) {
  /* 参数验证 */
  if (!ctx) {
    return -1; /* 上下文指针无效 */
  }

  /* 初始化上下文为零值 */
  memset(ctx, 0, sizeof(MagicLmiContext));
  ctx->server_fd = -1;                    /* 流式服务器 Socket 未初始化 */
  ctx->dgram_fd = -1;                     /* 数据报服务器 Socket 未初始化 */
  ctx->udp_fd = -1;                       /* UDP 监听服务器 Socket 未初始化 */
  ctx->running = false;                   /* 流式服务器未运行 */
  ctx->dgram_running = false;             /* 数据报服务器未运行 */
  ctx->udp_running = false;               /* UDP 监听服务器未运行 */
  ctx->heartbeat_monitor_running = false; /* 心跳监控线程未运行 */
  ctx->udp_port = 1947;                   /* 默认 UDP 监听端口 */

  /* 初始化客户端管理互斥锁 */
  pthread_mutex_init(&ctx->clients_mutex, NULL);

  /* 初始化事件回调机制 */
  pthread_mutex_init(&ctx->callbacks_mutex, NULL);
  ctx->num_callbacks = 0;
  memset(ctx->event_callbacks, 0, sizeof(ctx->event_callbacks));

  /* 记录初始化成功 */
  fd_log_notice("[app_magic] LMI interface initialized");
  return 0;
}

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
int magic_lmi_start_server(MagicLmiContext *ctx, MagicConfig *config) {
  /* 参数验证 */
  if (!ctx || !config) {
    return -1; /* 参数无效 */
  }

  /* 保存配置引用 */
  ctx->config = config;

  /* 创建 Unix Domain Socket */
  ctx->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (ctx->server_fd < 0) {
    fd_log_error("[app_magic] Failed to create DLM socket");
    return -1; /* Socket 创建失败 */
  }

  /* 删除可能存在的旧 socket 文件 */
  unlink(DLM_SOCK_PATH);

  /* 准备 socket 地址结构 */
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, DLM_SOCK_PATH, sizeof(addr.sun_path) - 1);

  /* 绑定 socket 到文件路径 */
  if (bind(ctx->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fd_log_error("[app_magic] Failed to bind DLM socket");
    close(ctx->server_fd);
    return -1; /* 绑定失败 */
  }

  /* 开始监听连接 */
  if (listen(ctx->server_fd, 5) < 0) {
    fd_log_error("[app_magic] Failed to listen on DLM socket");
    close(ctx->server_fd);
    return -1; /* 监听失败 */
  }

  /* 启动服务器线程 */
  ctx->running = true;
  if (pthread_create(&ctx->server_thread, NULL, dlm_server_thread, ctx) != 0) {
    fd_log_error("[app_magic] Failed to create DLM server thread");
    close(ctx->server_fd);
    return -1; /* 线程创建失败 */
  }

  /* 启动心跳监控线程 */
  ctx->heartbeat_monitor_running = true;
  if (pthread_create(&ctx->heartbeat_monitor_thread, NULL,
                     heartbeat_monitor_thread_func, ctx) != 0) {
    fd_log_error("[app_magic] Failed to create heartbeat monitor thread");
    ctx->running = false;
    close(ctx->server_fd);
    return -1; /* 线程创建失败 */
  }

  /* 记录服务器启动成功 */
  fd_log_notice("[app_magic] DLM server started on %s", DLM_SOCK_PATH);
  return 0;
}

/**
 * @brief 按链路 ID 查找 DLM 客户端
 *
 * 在已连接的 DLM 客户端中查找指定链路
 *
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 要查找的链路标识符
 * @return 找到的 DlmClient 指针, 未找到返回 NULL
 */
DlmClient *magic_lmi_find_by_link(MagicLmiContext *ctx, const char *link_id) {
  /* 参数验证 */
  if (!ctx || !link_id) {
    return NULL; /* 参数无效 */
  }

  /* 在客户端数组中查找匹配的链路 */
  pthread_mutex_lock(&ctx->clients_mutex);

  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        strcmp(ctx->clients[i].link_id, link_id) == 0) {
      /* 找到匹配的客户端 */
      pthread_mutex_unlock(&ctx->clients_mutex);
      return &ctx->clients[i];
    }
  }

  /* 未找到匹配的客户端 */
  pthread_mutex_unlock(&ctx->clients_mutex);
  return NULL;
}

/**
 * @brief 更新链路状态
 *
 * 更新指定链路的活动状态
 *
 * @param ctx 指向 LMI 上下文的指针
 * @param link_id 链路标识符
 * @param is_active true=在线, false=离线
 */
void magic_lmi_update_link_status(MagicLmiContext *ctx, const char *link_id,
                                  bool is_active) {
  /* 参数验证 */
  if (!ctx || !ctx->config) {
    return; /* 参数无效 */
  }

  /* 在配置中查找链路并更新状态 */
  DatalinkProfile *link = magic_config_find_datalink(ctx->config, link_id);
  if (link) {
    link->is_active = is_active;
    fd_log_notice("[app_magic] Link %s status: %s", link_id,
                  is_active ? "ACTIVE" : "INACTIVE");
  }
}

/**
 * @brief 清理 LMI 接口
 *
 * 关闭所有连接, 停止服务器线程, 释放资源
 *
 * @param ctx 指向 LMI 上下文的指针
 */
void magic_lmi_cleanup(MagicLmiContext *ctx) {
  /* 参数验证 */
  if (!ctx) {
    return; /* 上下文无效 */
  }

  /* 停止流式服务器运行 */
  ctx->running = false;

  /* 停止数据报服务器运行 */
  ctx->dgram_running = false;

  /* 停止 UDP 监听服务器运行 */
  ctx->udp_running = false;

  /* 停止心跳监控线程 */
  if (ctx->heartbeat_monitor_running) {
    ctx->heartbeat_monitor_running = false;
    pthread_join(ctx->heartbeat_monitor_thread, NULL);
    fd_log_debug("[app_magic] Heartbeat monitor thread stopped");
  }

  /* 关闭流式服务器 Socket */
  if (ctx->server_fd >= 0) {
    close(ctx->server_fd);
    unlink(DLM_SOCK_PATH); /* 删除 socket 文件 */
  }

  /* 关闭数据报服务器 Socket */
  if (ctx->dgram_fd >= 0) {
    close(ctx->dgram_fd);
    unlink(MIH_DGRAM_SOCKET_PATH); /* 删除 socket 文件 */
  }

  /* 关闭 UDP 监听服务器 Socket */
  if (ctx->udp_fd >= 0) {
    close(ctx->udp_fd);
    fd_log_debug("[app_magic] UDP listener socket closed");
  }

  /* 销毁互斥锁 */
  pthread_mutex_destroy(&ctx->clients_mutex);

  /* 记录清理完成 */
  fd_log_notice("[app_magic] LMI interface cleaned up");
}

/*===========================================================================
 * MIH 原语处理函数
 *
 * 这些函数处理从 DLM 接收到的 MIH 原语
 * 实现 ARINC 839 Attachment 2 定义的标准原语
 *===========================================================================*/

/**
 * @brief 处理 MIH_EXT_Link_Register.request
 *
 * 当 DLM 首次连接时发送注册请求
 * CM Core 验证 DLM 身份并分配客户端槽位
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param req 注册请求结构体
 */
static void
handle_mih_register_request(MagicLmiContext *ctx, int client_fd,
                            const MIH_EXT_Link_Register_Request *req) {
  /* 准备注册确认响应 */
  MIH_EXT_Link_Register_Confirm confirm;
  memset(&confirm, 0, sizeof(confirm));

  /* 查找空闲的客户端槽位 */
  pthread_mutex_lock(&ctx->clients_mutex);

  int client_index = -1;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (!ctx->clients[i].is_registered) {
      client_index = i; /* 找到空闲槽位 */
      break;
    }
  }

  if (client_index == -1) {
    /* 没有可用的客户端槽位 */
    confirm.status = STATUS_INSUFFICIENT_RESOURCES;
    fd_log_error("[app_magic] No available client slots for DLM registration");
  } else {
    /* 分配客户端槽位并填充信息 */
    DlmClient *client = &ctx->clients[client_index];

    /* 设置客户端基本信息 */
    client->is_registered = true;
    client->client_fd = client_fd;
    memcpy(&client->link_identifier, &req->link_identifier,
           sizeof(LINK_TUPLE_ID));
    memcpy(client->link_id, req->link_identifier.link_addr,
           sizeof(client->link_id));
    memcpy(&client->capabilities, &req->capabilities,
           sizeof(MIH_Link_Capabilities));
    client->dlm_pid = req->dlm_pid;
    client->last_heartbeat = time(NULL);
    client->last_seen = time(NULL); /* 初始化最后可见时间,防止心跳超时误判 */

    /* 更新链路状态为活动 */
    DatalinkProfile *link =
        magic_config_find_datalink(ctx->config, client->link_id);
    if (link) {
      link->is_active = true;
    }

    /* 设置响应状态 */
    confirm.status = STATUS_SUCCESS;
    confirm.assigned_id = client_index + 1;

    /* 记录注册成功信息 */
    fd_log_notice("[app_magic] ✓ DLM registered: %s (assigned_id=%u, max_bw=%u "
                  "kbps, latency=%u ms, interface=%s)",
                  client->link_id, confirm.assigned_id,
                  client->capabilities.max_bandwidth_kbps,
                  client->capabilities.typical_latency_ms,
                  client->link_identifier.poa_addr[0]
                      ? client->link_identifier.poa_addr
                      : "(none)");
  }

  /* 解锁互斥锁 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 发送注册确认响应 */
  mih_transport_send(client_fd, MIH_EXT_LINK_REGISTER_CONFIRM, &confirm,
                     sizeof(confirm));
}

/**
 * @brief 处理 MIH_Link_Up.indication (标准对齐版)
 *
 * 当链路变为可用状态时, DLM 发送此指示。
 * 注意：此处使用的 mih_link_up_ind_t 结构已在 mih_protocol.h 中对齐，
 * 确保了与 Standard DLM 发出的 64 字节结构体完全匹配。
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param ind 链路上线指示结构体
 */
static void handle_mih_link_up_indication(MagicLmiContext *ctx, int client_fd,
                                          const mih_link_up_ind_t *ind) {
  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (client) {
    /* 更新链路参数 */
    memcpy(&client->link_params, &ind->link_params,
           sizeof(mih_link_parameters_t));

    /* 标记链路为UP状态 */
    client->is_link_up = true;

    /* 更新配置中的链路状态为 Active */
    DatalinkProfile *link =
        magic_config_find_datalink(ctx->config, client->link_id);
    if (link) {
      link->is_active = true;
    }

    /* 记录链路上线事件 */
    struct in_addr addr;
    addr.s_addr = ind->link_params.ip_address;

    fd_log_notice("[app_magic] ✓ Link UP: %s (IP: %s, BW: %u kbps, Latency: %u "
                  "ms) → Online",
                  client->link_id, inet_ntoa(addr),
                  ind->link_params.current_bandwidth_kbps,
                  ind->link_params.current_latency_ms);
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);
}

/**
 * @brief 处理 MIH_Link_Down.indication (标准对齐版)
 *
 * 当链路变为不可用状态时, DLM 发送此指示。
 * 结构体已对齐，确保 24 字节大小与 Standard DLM 发送的完全一致。
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param ind 链路下线指示结构体
 */
static void handle_mih_link_down_indication(MagicLmiContext *ctx, int client_fd,
                                            const mih_link_down_ind_t *ind) {
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (client) {
    /* 标记链路为DOWN状态 */
    client->is_link_up = false;
  }

  pthread_mutex_unlock(&ctx->clients_mutex);

  char link_id[MAX_ID_LEN] = {0};

  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      /* 保存链路 ID 用于后续通知 */
      strncpy(link_id, ctx->clients[i].link_id, sizeof(link_id) - 1);

      /* 更新链路状态为离线 */
      DatalinkProfile *link =
          magic_config_find_datalink(ctx->config, ctx->clients[i].link_id);
      if (link) {
        link->is_active = false;
      }

      /* 记录链路下线事件 */
      fd_log_notice("[app_magic] ✗ Link DOWN: %s (reason=%u)",
                    ctx->clients[i].link_id, ind->reason_code);
      break;
    }
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 如果找到了对应的链路，通知所有使用该链路的客户端会话 */
  if (link_id[0] != '\0') {
    /* 获取全局 MAGIC 上下文 */
    extern MagicContext g_magic_ctx;
    MagicContext *magic_ctx = &g_magic_ctx;

    /* 遍历所有会话，查找使用该链路的会话 */
    pthread_mutex_lock(&magic_ctx->session_mgr.mutex);

    int notified_count = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
      ClientSession *session = &magic_ctx->session_mgr.sessions[i];

      /* 跳过未使用或已终止的会话 */
      if (!session->in_use || session->state == SESSION_STATE_CLOSED) {
        continue;
      }

      /* 检查会话是否使用了下线的链路 */
      if (strcmp(session->assigned_link_id, link_id) == 0) {
        /* 准备 MNTR 通知参数 */
        MNTRParams mntr_params;
        memset(&mntr_params, 0, sizeof(mntr_params));

        /* 链路丢失 - MAGIC-Status-Code = 2007 (LINK_ERROR) - ARINC 839 规范 */
        mntr_params.magic_status_code = MAGIC_STATUS_LINK_ERROR;
        mntr_params.error_message = "Link Down - datalink connection lost";
        mntr_params.new_granted_bw = 0; /* 链路断开，带宽为 0 */
        mntr_params.new_granted_ret_bw = 0;
        mntr_params.force_send = true; /* 强制发送，绕过风暴抑制 */

        /* 发送 MNTR 通知 */
        fd_log_notice(
            "[app_magic] Sending MNTR (LINK_DOWN) to session %s (link=%s)",
            session->session_id, link_id);

        if (magic_cic_send_mntr(magic_ctx, session, &mntr_params) == 0) {
          notified_count++;

          /* 更新会话状态为 SUSPENDED (挂起) */
          session->state = SESSION_STATE_SUSPENDED;

          /* 记录旧带宽值用于链路恢复时参考 */
          fd_log_debug("[app_magic] Session %s suspended (was: %u kbps)",
                       session->session_id, session->granted_bw_kbps);
        } else {
          fd_log_error("[app_magic] Failed to send MNTR to session %s",
                       session->session_id);
        }
      }
    }

    pthread_mutex_unlock(&magic_ctx->session_mgr.mutex);

    fd_log_notice("[app_magic] Link down notification sent to %d session(s) "
                  "using link %s",
                  notified_count, link_id);
  }
}

/**
 * @brief 处理 MIH_EXT_Heartbeat
 *
 * DLM 发送的心跳消息, 用于证明 DLM 仍然正常工作
 * CM Core 更新最后心跳时间并发送确认
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param hb 心跳消息结构体
 */
static void handle_mih_heartbeat(MagicLmiContext *ctx, int client_fd,
                                 const MIH_EXT_Heartbeat *hb) {
  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端并更新心跳时间 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      ctx->clients[i].last_heartbeat = time(NULL);

      /* 记录心跳信息 */
      fd_log_debug("[app_magic] Heartbeat from %s (health=%u, tx=%lu, rx=%lu)",
                   ctx->clients[i].link_id, hb->health_status, hb->tx_bytes,
                   hb->rx_bytes);

      /* 发送心跳确认 */
      MIH_EXT_Heartbeat_Ack ack;
      memset(&ack, 0, sizeof(ack));
      ack.ack_status = 0; /* OK */
      ack.server_timestamp = (uint32_t)time(NULL);

      mih_transport_send(client_fd, MIH_EXT_HEARTBEAT_ACK, &ack, sizeof(ack));
      break;
    }
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);
}

/**
 * @brief 处理 MIH 标准参数报告指示
 *
 * 当链路参数发生显著变化时, DLM 主动发送此报告
 * CM Core 更新客户端缓存的链路参数
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param ind 标准参数报告指示结构体
 */
static void
handle_mih_parameters_report(MagicLmiContext *ctx, int client_fd,
                             const LINK_Parameters_Report_Indication *ind) {
  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (client) {
    /* 更新转换后的标准参数 */
    memcpy(&client->current_parameters, &ind->parameters,
           sizeof(LINK_PARAMETERS));

    /* 同时更新内部转换结构以保持向后兼容 */
    client->link_params.current_bandwidth_kbps =
        ind->parameters.available_bandwidth_kbps;
    client->link_params.current_latency_ms = ind->parameters.current_latency_ms;
    client->link_params.signal_strength_dbm =
        ind->parameters.signal_strength_dbm;
    client->link_params.signal_quality = ind->parameters.signal_quality;
    client->link_params.ip_address = ind->parameters.ip_address;

    /* 更新最后活动时间 */
    client->last_seen = time(NULL);

    fd_log_debug(
        "[app_magic] ✓ Parameters Report from %s: RSSI=%d dBm, BW=%u kbps",
        client->link_id, ind->parameters.signal_strength_dbm,
        ind->parameters.available_bandwidth_kbps);
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);
}

/*===========================================================================
 * IPC 辅助函数 (Legacy - 保留用于兼容性)
 *
 * 这些函数实现旧版 IPC 协议, 用于与不支持 MIH 传输层的 DLM 通信
 * 新代码应优先使用 MIH 传输层
 *===========================================================================*/

/**
 * @brief 发送 IPC 消息
 *
 * 发送一个完整的 IPC 消息, 包括头部和负载
 *
 * @param fd Socket 文件描述符
 * @param type 消息类型 (MessageType 枚举值)
 * @param payload 消息负载指针
 * @param payload_len 负载长度 (字节)
 * @return 0=成功, -1=失败
 */
static __attribute__((unused)) int
send_ipc_msg(int fd, uint8_t type, const void *payload, uint32_t payload_len) {
  /* 使用静态变量维护序列号 */
  static uint32_t seq = 0;

  /* 构建消息头部 */
  IpcHeader header = {
      .type = type,          /* 消息类型 */
      .length = payload_len, /* 负载长度 */
      .sequence = ++seq      /* 递增序列号 */
  };

  /* 发送头部 */
  if (send(fd, &header, sizeof(header), 0) != sizeof(header)) {
    return -1; /* 头部发送失败 */
  }

  /* 发送负载 (如果有) */
  if (payload_len > 0 && payload != NULL) {
    if (send(fd, payload, payload_len, 0) != (ssize_t)payload_len) {
      return -1; /* 负载发送失败 */
    }
  }

  return 0; /* 发送成功 */
}

/**
 * @brief 接收 IPC 消息
 *
 * 接收一个完整的 IPC 消息, 包括头部和负载
 *
 * @param fd Socket 文件描述符
 * @param header 输出: 消息头部
 * @param payload 输出: 消息负载缓冲区
 * @param max_payload_len 最大负载长度
 * @return 实际接收的负载长度, -1=失败
 */
static __attribute__((unused)) int recv_ipc_msg(int fd, IpcHeader *header,
                                                void *payload,
                                                uint32_t max_payload_len) {
  /* 接收消息头部 */
  ssize_t n = recv(fd, header, sizeof(IpcHeader), MSG_WAITALL);
  if (n != sizeof(IpcHeader)) {
    return -1; /* 头部接收失败 */
  }

  /* 检查是否有负载 */
  if (header->length > 0) {
    /* 验证负载长度 */
    if (header->length > max_payload_len) {
      return -1; /* 负载过大 */
    }

    /* 接收负载 */
    n = recv(fd, payload, header->length, MSG_WAITALL);
    if (n != (ssize_t)header->length) {
      return -1; /* 负载接收失败 */
    }
  }

  return header->length; /* 返回负载长度 */
}

/*===========================================================================
 * DLM 消息处理函数
 *
 * 这些函数处理旧版 IPC 协议的 DLM 消息
 * 用于与不支持 MIH 传输层的 DLM 实现兼容
 *===========================================================================*/

/**
 * @brief 处理 DLM 注册消息
 *
 * 当 DLM 使用旧版协议连接时, 发送注册消息
 * CM Core 验证 DLM 身份并分配客户端槽位
 *
 * @param ctx LMI 上下文
 * @param client_fd 客户端 Socket 文件描述符
 * @param reg 注册消息结构体
 */
static __attribute__((unused)) void
handle_dlm_registration(MagicLmiContext *ctx, int client_fd,
                        const MsgRegister *reg) {
  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* v2.0: 在配置中查找对应的 DLM */
  DLMConfig *dlm = NULL;
  for (uint32_t i = 0; i < ctx->config->num_dlm_configs; i++) {
    if (strcmp(ctx->config->dlm_configs[i].dlm_name, reg->dlm_id) == 0) {
      dlm = &ctx->config->dlm_configs[i];
      break;
    }
  }

  if (!dlm) {
    /* 未找到对应的 DLM 配置 */
    fd_log_error("[app_magic] Unknown DLM: %s", reg->dlm_id);
    pthread_mutex_unlock(&ctx->clients_mutex);

    /* 发送注册失败确认 */
    MsgRegisterAck ack = {.result = 1, .assigned_id = 0};
    strncpy(ack.message, "Unknown DLM ID", sizeof(ack.message) - 1);
    send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
    return;
  }

  /* 查找空闲的客户端槽位 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (!ctx->clients[i].is_registered) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (!client) {
    /* 没有空闲槽位 */
    fd_log_error("[app_magic] No free DLM slot");
    pthread_mutex_unlock(&ctx->clients_mutex);

    /* 发送注册失败确认 */
    MsgRegisterAck ack = {.result = 1, .assigned_id = 0};
    strncpy(ack.message, "No free slot", sizeof(ack.message) - 1);
    send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));
    return;
  }

  /* 注册 DLM 客户端 */
  memset(client, 0, sizeof(DlmClient));
  client->client_fd = client_fd;
  client->is_registered = true;
  strncpy(client->dlm_id, reg->dlm_id, sizeof(client->dlm_id) - 1);
  strncpy(client->link_id, dlm->dlm_name, sizeof(client->link_id) - 1);
  client->last_heartbeat = time(NULL);

  /* v2.0: 标记 DLM 为活动状态 */
  dlm->is_active = true;

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 发送注册成功确认 */
  MsgRegisterAck ack = {
      .result = 0, .assigned_id = 1000 + (uint32_t)(client - ctx->clients)};
  strncpy(ack.message, "Registration successful", sizeof(ack.message) - 1);
  send_ipc_msg(client_fd, MSG_TYPE_REGISTER_ACK, &ack, sizeof(ack));

  /* 记录注册成功信息 */
  fd_log_notice("[app_magic] ✓ DLM Registered:");
  fd_log_notice("[app_magic]     DLM: %s", reg->dlm_id);
  fd_log_notice("[app_magic]     Link: %s (Socket: %s)", dlm->dlm_name,
                dlm->dlm_socket_path);
  fd_log_notice("[app_magic]     Interface: %s", reg->iface_name);
  fd_log_notice("[app_magic]     BW: %u kbps", reg->max_bw_kbps);
}

/**
 * @brief 处理链路事件消息 (Legacy IPC)。
 * @details 处理从 DLM 接收到的 `MSG_TYPE_LINK_EVENT` 消息。
 *          更新链路状态 (UP/DOWN) 并记录日志。
 *
 * @param ctx LMI 上下文指针。
 * @param client_fd 客户端 Socket 文件描述符。
 * @param event 链路事件消息结构体指针。
 */
static __attribute__((unused)) void
handle_link_event(MagicLmiContext *ctx, int client_fd,
                  const MsgLinkEvent *event) {
  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的 DLM 客户端 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (!client) {
    /* 未找到对应的客户端 */
    pthread_mutex_unlock(&ctx->clients_mutex);
    return;
  }

  /* 更新链路状态 */
  DatalinkProfile *link =
      magic_config_find_datalink(ctx->config, client->link_id);
  if (link) {
    link->is_active = event->is_link_up;
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 记录链路事件 */
  if (event->is_link_up) {
    fd_log_notice("[app_magic] Link UP: %s", client->link_id);
    struct in_addr ip_addr;
    memcpy(&ip_addr, &event->ip_address, sizeof(ip_addr));
    fd_log_notice("[app_magic]     IP: %s", inet_ntoa(ip_addr));
    fd_log_notice("[app_magic]     BW: %u kbps", event->current_bw_kbps);
  } else {
    fd_log_notice("[app_magic] Link DOWN: %s", client->link_id);
  }
}

/**
 * @brief 处理心跳消息 (Legacy IPC)。
 * @details 处理从 DLM 接收到的 `MSG_TYPE_HEARTBEAT` 消息。
 *          更新客户端的最后心跳时间，防止连接超时。
 *
 * @param ctx LMI 上下文指针。
 * @param client_fd 客户端 Socket 文件描述符。
 * @param hb 心跳消息结构体指针。
 */
static __attribute__((unused)) void
handle_heartbeat(MagicLmiContext *ctx, int client_fd, const MsgHeartbeat *hb) {
  /* 参数验证 */
  (void)hb; /* 避免未使用参数警告 */

  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的客户端并更新心跳时间 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      ctx->clients[i].last_heartbeat = time(NULL);
      break;
    }
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);
}

/*===========================================================================
 * DLM 客户端处理线程
 *
 * 每个连接的 DLM 客户端都有一个专用线程处理其消息
 * 线程负责接收和分发不同类型的 MIH 消息
 *===========================================================================*/

/**
 * @brief DLM 客户端线程参数结构体
 *
 * 用于传递给客户端处理线程的参数
 */
typedef struct {
  MagicLmiContext *ctx; /* LMI 上下文指针 */
  int client_fd;        /* 客户端 Socket 文件描述符 */
} ClientThreadArg;

/**
 * @brief DLM 客户端消息处理线程。
 * @details 为每个连接的 DLM 客户端创建的专用线程。
 *          使用 MIH 传输层接收和处理来自 DLM 的 MIH 原语。
 *          线程会一直运行，直到连接断开或服务器停止。
 *
 * @param arg 线程参数 (`ClientThreadArg*`)，包含客户端文件描述符和上下文。
 * @return NULL (线程退出时)。
 *
 * @warning 此函数在独立线程中运行，访问共享资源 (如 `ctx->clients`)
 * 时必须加锁。
 */
static void *handle_dlm_client_thread(void *arg) {
  /* 解析线程参数 */
  ClientThreadArg *thread_arg = (ClientThreadArg *)arg;
  MagicLmiContext *ctx = thread_arg->ctx;
  int client_fd = thread_arg->client_fd;

  /* 释放参数结构体内存 */
  free(thread_arg);

  /* 消息缓冲区 */
  char buffer[4096];
  mih_transport_header_t mih_header;

  /* 记录线程启动 */
  fd_log_debug("[app_magic] DLM client thread started (fd=%d)", client_fd);

  /* 消息处理循环 */
  while (ctx->running) {
    /* 使用 MIH 传输层接收消息 */
    int n = mih_transport_recv(client_fd, &mih_header, buffer, sizeof(buffer));
    if (n < 0) {
      /* 接收失败, 客户端断开连接 */
      fd_log_debug("[app_magic] DLM client disconnected (fd=%d)", client_fd);
      break;
    }

    /* 根据 MIH 原语类型分发处理 */
    switch (mih_header.primitive_type) {
    case MIH_EXT_LINK_REGISTER_REQUEST:
      /* MIH 扩展链路注册请求 */
      handle_mih_register_request(ctx, client_fd,
                                  (MIH_EXT_Link_Register_Request *)buffer);
      break;

    case MIH_LINK_UP_INDICATION:
      /* MIH 链路上线指示 */
      handle_mih_link_up_indication(ctx, client_fd,
                                    (mih_link_up_ind_t *)buffer);
      break;

    case MIH_LINK_DOWN_INDICATION:
      /* MIH 链路下线指示 */
      handle_mih_link_down_indication(ctx, client_fd,
                                      (mih_link_down_ind_t *)buffer);
      break;

    case MIH_EXT_HEARTBEAT:
      /* MIH 扩展心跳消息 */
      handle_mih_heartbeat(ctx, client_fd, (MIH_EXT_Heartbeat *)buffer);
      break;

    case MIH_LINK_PARAMETERS_REPORT_IND:
      /* MIH 标准参数报告指示 */
      handle_mih_parameters_report(ctx, client_fd,
                                   (LINK_Parameters_Report_Indication *)buffer);
      break;

    default:
      /* 未知的 MIH 原语类型 */
      fd_log_debug("[app_magic] Unknown MIH primitive: 0x%04X",
                   mih_header.primitive_type);
      break;
    }
  }

  /* 清理客户端状态 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找并清理对应的客户端 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      /* 更新链路状态为离线 */
      DatalinkProfile *link =
          magic_config_find_datalink(ctx->config, ctx->clients[i].link_id);
      if (link) {
        link->is_active = false;
      }

      /* 记录断开连接信息 */
      fd_log_notice("[app_magic] DLM disconnected: %s", ctx->clients[i].dlm_id);

      /* 清空客户端信息 */
      memset(&ctx->clients[i], 0, sizeof(DlmClient));
      break;
    }
  }

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 关闭客户端 Socket */
  close(client_fd);

  return NULL;
}

/*===========================================================================
 * DLM 服务器主线程
 *
 * 监听 DLM 连接请求并创建客户端处理线程
 *===========================================================================*/

/**
 * @brief DLM 服务器主线程。
 * @details 监听 Unix Domain Socket 上的 DLM 连接请求。
 *          accept 新连接后，为每个客户端创建独立的 `handle_dlm_client_thread`
 * 线程。
 *
 * @param arg 线程参数 (`MagicLmiContext*`)。
 * @return NULL (服务器停止时)。
 */
static void *dlm_server_thread(void *arg) {
  /* 获取 LMI 上下文 */
  MagicLmiContext *ctx = (MagicLmiContext *)arg;

  /* 记录服务器线程启动 */
  fd_log_notice("[app_magic] DLM server thread started");

  /* 连接接受循环 */
  while (ctx->running) {
    /* 客户端地址结构 */
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);

    /* 接受新连接 */
    int client_fd =
        accept(ctx->server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
      if (ctx->running) {
        /* 记录接受连接失败的错误 */
        fd_log_error("[app_magic] DLM accept failed: %s", strerror(errno));
      }
      break;
    }

    /* 记录新客户端连接 */
    fd_log_debug("[app_magic] New DLM client connected (fd=%d)", client_fd);

    /* 为每个客户端创建处理线程 */
    ClientThreadArg *thread_arg = malloc(sizeof(ClientThreadArg));
    if (thread_arg) {
      /* 设置线程参数 */
      thread_arg->ctx = ctx;
      thread_arg->client_fd = client_fd;

      /* 创建客户端处理线程 */
      pthread_t client_thread;
      if (pthread_create(&client_thread, NULL, handle_dlm_client_thread,
                         thread_arg) == 0) {
        /* 分离线程, 自动清理资源 */
        pthread_detach(client_thread);
      } else {
        /* 线程创建失败 */
        fd_log_error("[app_magic] Failed to create client thread");
        free(thread_arg);
        close(client_fd);
      }
    } else {
      /* 内存分配失败 */
      close(client_fd);
    }
  }

  /* 记录服务器线程退出 */
  fd_log_notice("[app_magic] DLM server thread exiting");

  return NULL;
}

/*===========================================================================
 * MIH 原语实现 (ARINC 839 Attachment 2)
 *
 * 实现 ARINC 839-2014 Attachment 2 中定义的 MIH 原语
 * 包括链路注册、状态指示、资源管理和参数报告等功能
 *===========================================================================*/

/**
 * @brief 分配新的 Bearer ID。
 * @details 在指定 DLM 客户端上查找并激活一个空闲的 Bearer 槽位。
 *
 * @param client DLM 客户端指针。
 * @return 分配的 Bearer ID (1-255)。如果无可用槽位，返回 0。
 */
BEARER_ID magic_dlm_allocate_bearer(DlmClient *client) {
  /* 参数验证 */
  if (!client)
    return 0;

  /* 查找空闲的 Bearer 槽位 */
  for (uint8_t i = 0; i < MAX_BEARERS; i++) {
    if (!client->bearers[i].is_active) {
      /* 激活 Bearer */
      client->bearers[i].is_active = true;
      client->bearers[i].bearer_id = i + 1;
      client->bearers[i].created_time = time(NULL);
      client->bearers[i].tx_bytes = 0;
      client->bearers[i].rx_bytes = 0;
      client->num_active_bearers++;

      /* 返回分配的 Bearer ID */
      return client->bearers[i].bearer_id;
    }
  }

  /* 没有空闲槽位 */
  return 0;
}

/**
 * @brief 释放 Bearer。
 * @details 释放指定的 Bearer 资源，将其标记为非活动状态。
 *
 * @param client DLM 客户端指针。
 * @param bearer_id 要释放的 Bearer ID。
 * @return 0 成功，-1 失败 (ID 无效或未找到)。
 */
int magic_dlm_release_bearer(DlmClient *client, BEARER_ID bearer_id) {
  /* 参数验证 */
  if (!client || bearer_id == 0 || bearer_id > MAX_BEARERS) {
    return -1;
  }

  /* 计算数组索引 */
  uint8_t index = bearer_id - 1;

  /* 检查 Bearer 是否激活 */
  if (client->bearers[index].is_active) {
    /* 清空 Bearer 状态 */
    memset(&client->bearers[index], 0, sizeof(BearerState));
    client->num_active_bearers--;
    return 0;
  }

  /* Bearer 未找到 */
  return -1;
}

/**
 * @brief 查找 Bearer。
 * @details 根据 Bearer ID 查找对应的 Bearer 状态结构体。
 *
 * @param client DLM 客户端指针。
 * @param bearer_id 要查找的 Bearer ID。
 * @return 找到的 BearerState 指针。未找到或 ID 无效返回 NULL。
 */
BearerState *magic_dlm_find_bearer(DlmClient *client, BEARER_ID bearer_id) {
  /* 参数验证 */
  if (!client || bearer_id == 0 || bearer_id > MAX_BEARERS) {
    return NULL;
  }

  /* 计算数组索引 */
  uint8_t index = bearer_id - 1;

  /* 检查 Bearer 是否激活 */
  if (client->bearers[index].is_active) {
    return &client->bearers[index];
  }

  /* Bearer 未找到 */
  return NULL;
}

/**
 * @brief 处理 MIH 资源请求消息（内部辅助函数）。
 * @details 执行 MIH_LINK_RESOURCE.Request 的核心逻辑，包括：
 *          - 验证 QoS 参数 (如果需要)。
 *          - 分配、更新或释放 Bearer。
 *          - 构造响应状态和原因。
 *          此函数不直接发送消息，只填充响应结构体。
 *
 * @param ctx LMI 上下文指针。
 * @param client DLM 客户端指针。
 * @param req 资源请求消息。
 * @param resp [out] 资源响应消息。
 */
static void handle_mih_resource_request_internal(MagicLmiContext *ctx,
                                                 DlmClient *client,
                                                 const MsgMihResourceReq *req,
                                                 MsgMihResourceResp *resp) {
  /* 初始化响应消息 */
  memset(resp, 0, sizeof(*resp));

  /* 处理资源请求 */
  if (req->action == RESOURCE_ACTION_REQUEST) {
    /* 资源分配请求 */
    if (!req->has_qos_params) {
      /* QoS 参数是必需的 */
      resp->status = STATUS_FAILURE;
      strncpy(resp->reason, "QoS parameters required",
              sizeof(resp->reason) - 1);
    } else {
      QOS_PARAM qos_local;
      memcpy(&qos_local, &req->qos_params, sizeof(qos_local));
      if (!validate_qos_params(&qos_local)) {
        /* QoS 参数无效 */
        resp->status = STATUS_QOS_NOT_SUPPORTED;
        strncpy(resp->reason, "Invalid QoS parameters",
                sizeof(resp->reason) - 1);
      } else {
        if (req->has_bearer_id) {
          /* 更新现有 Bearer */
          BearerState *bearer = magic_dlm_find_bearer(client, req->bearer_id);
          if (!bearer) {
            /* Bearer 不存在 */
            resp->status = STATUS_INVALID_BEARER;
            strncpy(resp->reason, "Bearer not found", sizeof(resp->reason) - 1);
          } else {
            /* 更新 Bearer QoS 参数 */
            bearer->qos_params = req->qos_params;
            resp->status = STATUS_SUCCESS;
            resp->has_bearer_id = true;
            resp->bearer_id = req->bearer_id;
            strncpy(resp->reason, "Resources updated",
                    sizeof(resp->reason) - 1);
          }
        } else {
          /* 分配新 Bearer */
          BEARER_ID new_bearer_id = magic_dlm_allocate_bearer(client);
          if (new_bearer_id == 0) {
            /* 没有空闲的 Bearer 槽位 */
            resp->status = STATUS_INSUFFICIENT_RESOURCES;
            strncpy(resp->reason, "No free bearer slots",
                    sizeof(resp->reason) - 1);
          } else {
            /* 设置新 Bearer 的 QoS 参数 */
            BearerState *bearer = magic_dlm_find_bearer(client, new_bearer_id);
            bearer->qos_params = req->qos_params;
            resp->status = STATUS_SUCCESS;
            resp->has_bearer_id = true;
            resp->bearer_id = new_bearer_id;
            snprintf(resp->reason, sizeof(resp->reason),
                     "Allocated bearer ID %u", new_bearer_id);
          }
        }
      }
    }
  }

  else if (req->action == RESOURCE_ACTION_RELEASE) {
    /* 资源释放请求 */
    if (!req->has_bearer_id) {
      /* Bearer ID 是必需的 */
      resp->status = STATUS_FAILURE;
      strncpy(resp->reason, "Bearer ID required", sizeof(resp->reason) - 1);
    } else {
      /* 释放 Bearer */
      if (magic_dlm_release_bearer(client, req->bearer_id) == 0) {
        resp->status = STATUS_SUCCESS;
        strncpy(resp->reason, "Bearer released", sizeof(resp->reason) - 1);
      } else {
        /* Bearer 不存在 */
        resp->status = STATUS_INVALID_BEARER;
        strncpy(resp->reason, "Bearer not found", sizeof(resp->reason) - 1);
      }
    }
  } else {
    /* 未知的操作类型 */
    resp->status = STATUS_FAILURE;
    strncpy(resp->reason, "Unknown action", sizeof(resp->reason) - 1);
  }
}

/**
 * @brief 处理 MIH 资源请求消息（IPC 入口）。
 * @details 处理通过 Legacy IPC 接收到的 `MSG_TYPE_RESOURCE_REQ` 消息。
 *          查找对应的客户端，调用内部处理函数，并发送响应。
 *
 * @param ctx LMI 上下文指针。
 * @param client_fd 客户端 Socket 文件描述符。
 * @param req 资源请求消息指针。
 */
static __attribute__((unused)) void
handle_mih_resource_request(MagicLmiContext *ctx, int client_fd,
                            const MsgMihResourceReq *req) {
  /* 响应消息 */
  MsgMihResourceResp resp;

  /* 记录请求信息 */
  fd_log_notice("[app_magic] MIH_LINK_RESOURCE.Request:");
  fd_log_notice("[app_magic]   Link: %s", req->link_id);
  fd_log_notice("[app_magic]   Action: %s",
                resource_action_to_string(req->action));

  /* 锁定客户端数组 */
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找对应的 DLM 客户端 */
  DlmClient *client = NULL;
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        ctx->clients[i].client_fd == client_fd) {
      client = &ctx->clients[i];
      break;
    }
  }

  if (!client) {
    /* 客户端未找到 */
    fd_log_error("[app_magic] DLM client not found");
    memset(&resp, 0, sizeof(resp));
    resp.status = STATUS_FAILURE;
    strncpy(resp.reason, "Client not registered", sizeof(resp.reason) - 1);
    pthread_mutex_unlock(&ctx->clients_mutex);
    send_ipc_msg(client_fd, MSG_TYPE_RESOURCE_RESP, &resp, sizeof(resp));
    return;
  }

  /* 调用内部处理函数 */
  handle_mih_resource_request_internal(ctx, client, req, &resp);

  /* 解锁客户端数组 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 发送响应消息 */
  send_ipc_msg(client_fd, MSG_TYPE_RESOURCE_RESP, &resp, sizeof(resp));

  /* 记录响应信息 */
  fd_log_notice("[app_magic] MIH_LINK_RESOURCE.Confirm:");
  fd_log_notice("[app_magic]   Status: %s", status_to_string(resp.status));
  if (resp.has_bearer_id) {
    fd_log_notice("[app_magic]   Bearer ID: %u", resp.bearer_id);
  }
  fd_log_notice("[app_magic]   Reason: %s", resp.reason);
}

/**
 * @brief 实现 MIH_LINK_RESOURCE.Request (High-Level API)。
 * @details 供高层应用程序 (如 SESS 模块) 调用的 API，用于发起资源操作。
 *          提供符合 IEEE 802.21 标准的接口。
 *
 * @param ctx LMI 上下文指针。
 * @param request 资源请求参数结构体。
 * @param confirm [out] 资源确认结果结构体。
 * @return 0 成功 (Status=Success)，-1 失败。
 */
int magic_dlm_mih_link_resource_request(
    MagicLmiContext *ctx, const MIH_Link_Resource_Request *request,
    MIH_Link_Resource_Confirm *confirm) {
  /* 参数验证 */
  if (!ctx || !request || !confirm) {
    return -1;
  }

  /* 初始化确认消息 */
  memset(confirm, 0, sizeof(MIH_Link_Resource_Confirm));

  /* 查找对应的链路 */
  DatalinkProfile *link = magic_config_find_datalink(
      ctx->config, request->link_identifier.link_addr);
  if (!link || !link->is_active) {
    /* 链路不可用 */
    confirm->status = STATUS_LINK_NOT_AVAILABLE;
    return -1;
  }

  /* 构造内部消息 */
  MsgMihResourceReq req;
  memset(&req, 0, sizeof(req));

  /* v2.0: 设置 DLM 名称 */
  strncpy(req.link_id, link->dlm_name, sizeof(req.link_id) - 1);

  /* 设置操作类型 */
  req.action = request->resource_action;

  /* 设置 Bearer ID（如果有） */
  req.has_bearer_id = request->has_bearer_id;
  if (request->has_bearer_id) {
    req.bearer_id = request->bearer_identifier;
  }

  /* 设置 QoS 参数（如果有） */
  req.has_qos_params = request->has_qos_params;
  if (request->has_qos_params) {
    req.qos_params = request->qos_parameters;
  }

  /* v2.0: 查找对应的 DLM 客户端 */
  DlmClient *dlm_client = magic_lmi_find_by_link(ctx, link->dlm_name);
  if (!dlm_client) {
    /* DLM 客户端不可用 */
    confirm->status = STATUS_LINK_NOT_AVAILABLE;
    return -1;
  }

  /* 处理请求（直接调用内部函数，不通过IPC） */
  MsgMihResourceResp resp;
  pthread_mutex_lock(&ctx->clients_mutex);
  handle_mih_resource_request_internal(ctx, dlm_client, &req, &resp);
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* 填充确认消息 */
  confirm->source_identifier = request->destination_id;
  confirm->link_identifier = request->link_identifier;
  confirm->status = resp.status;
  confirm->has_bearer_id = resp.has_bearer_id;
  if (resp.has_bearer_id) {
    confirm->bearer_identifier = resp.bearer_id;
  }

  /* 记录 API 调用结果 */
  fd_log_notice(
      "[app_magic] MIH_LINK_RESOURCE high-level API: status=%s, bearer=%d",
      status_to_string(resp.status), resp.has_bearer_id ? resp.bearer_id : 0);

  /* 返回操作结果 */
  return (confirm->status == STATUS_SUCCESS) ? 0 : -1;
}

/**
 * @brief 实现 LINK_RESOURCE.Request (Link-Layer API)。
 * @details 供内部或底层模块使用的 API，直接操作 DLM 客户端的 Bearer 资源。
 *          通常在接收到 MIH 原语后调用以同步状态。
 *
 * @param client DLM 客户端指针。
 * @param request 链路资源请求参数。
 * @param confirm [out] 链路资源确认结果。
 * @return 0 成功，-1 失败。
 */
int magic_dlm_link_resource_request(DlmClient *client,
                                    const LINK_Resource_Request *request,
                                    LINK_Resource_Confirm *confirm) {
  /* 参数验证 */
  if (!client || !request || !confirm) {
    return -1;
  }

  /* 初始化确认消息 */
  memset(confirm, 0, sizeof(LINK_Resource_Confirm));

  /* 处理资源请求 */
  if (request->resource_action == RESOURCE_ACTION_REQUEST) {
    /* 资源分配请求 */
    if (!request->has_qos_params) {
      /* QoS 参数是必需的 */
      confirm->status = STATUS_FAILURE;
      return -1;
    }

    /* 分配或更新 Bearer */
    BEARER_ID bearer_id;
    if (request->has_bearer_id) {
      /* 更新现有 Bearer */
      bearer_id = request->bearer_identifier;
      BearerState *bearer = magic_dlm_find_bearer(client, bearer_id);
      if (!bearer) {
        /* Bearer 不存在 */
        confirm->status = STATUS_INVALID_BEARER;
        return -1;
      }
      /* 更新 QoS 参数 */
      bearer->qos_params = request->qos_parameters;
    } else {
      /* 分配新 Bearer */
      bearer_id = magic_dlm_allocate_bearer(client);
      if (bearer_id == 0) {
        /* 没有空闲资源 */
        confirm->status = STATUS_INSUFFICIENT_RESOURCES;
        return -1;
      }
      /* 设置新 Bearer 的 QoS 参数 */
      BearerState *bearer = magic_dlm_find_bearer(client, bearer_id);
      bearer->qos_params = request->qos_parameters;
    }

    /* 设置成功响应 */
    confirm->status = STATUS_SUCCESS;
    confirm->has_bearer_id = true;
    confirm->bearer_identifier = bearer_id;

  } else if (request->resource_action == RESOURCE_ACTION_RELEASE) {
    /* 资源释放请求 */
    if (!request->has_bearer_id) {
      /* Bearer ID 是必需的 */
      confirm->status = STATUS_FAILURE;
      return -1;
    }

    /* 释放 Bearer */
    if (magic_dlm_release_bearer(client, request->bearer_identifier) != 0) {
      /* Bearer 不存在 */
      confirm->status = STATUS_INVALID_BEARER;
      return -1;
    }

    /* 设置成功响应 */
    confirm->status = STATUS_SUCCESS;
  }

  /* 操作成功 */
  return 0;
}

/**
 * @brief 更新硬件健康状态。
 * @details 更新 DLM 客户端的硬件健康状态信息 (Hardware Health)。
 *          用于监控链路层设备的运行状态 (如温度、电池、故障码)。
 *
 * @param client DLM 客户端指针。
 * @param health 硬件健康状态信息指针。
 */
void magic_dlm_update_health(DlmClient *client, const HARDWARE_HEALTH *health) {
  /* 参数验证 */
  if (!client || !health) {
    return;
  }

  /* 更新健康状态 */
  client->health_status = *health;

  /* 记录健康状态更新 */
  fd_log_debug("[app_magic] Hardware health updated for %s: %.*s",
               client->dlm_id, health->length, health->health_info);
}

/*===========================================================================
 * 数据报模式服务器实现 (SOCK_DGRAM)
 *
 * 用于接收 DLM 原型发送的简化格式消息
 * 消息格式: [2字节类型码] + [原始结构体数据]
 *
 * 支持的 MIH 原语:
 * - Link_Up.indication (0x0202)
 * - Link_Down.indication (0x0203)
 * - Link_Going_Down.indication (0x0204)
 * - Link_Detected.indication (0x0201)
 * - Link_Parameters_Report.indication (0x0205)
 * - Link_Capability_Discover.confirm (0x0102)
 * - Link_Get_Parameters.confirm (0x0108)
 * - Link_Event_Subscribe.confirm (0x0104)
 * - Link_Resource.confirm (0x0302)
 *===========================================================================*/

/**
 * @brief 根据套接字路径查找或创建 DLM 客户端 (数据报模式)。
 * @details 数据报模式下，使用源套接字路径作为客户端标识。
 *          如果客户端不存在，创建一个新的客户端记录并进行部分初始化。
 *
 * @param ctx LMI 上下文指针。
 * @param sock_path 源套接字路径。
 * @return DlmClient 指针。如果客户端表已满，返回 NULL。
 *
 * @warning 此函数会锁定 `ctx->clients_mutex`，调用前请确保未持有该锁。
 */
static DlmClient *find_or_create_dgram_client(MagicLmiContext *ctx,
                                              const char *sock_path) {
  pthread_mutex_lock(&ctx->clients_mutex);

  /* 查找已存在的客户端 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered) {
      /* 使用 link_id 匹配套接字路径中的 DLM 类型 */
      if (strstr(sock_path, "cellular") &&
          strstr(ctx->clients[i].dlm_id, "CELLULAR")) {
        pthread_mutex_unlock(&ctx->clients_mutex);
        return &ctx->clients[i];
      }
      if (strstr(sock_path, "satcom") &&
          strstr(ctx->clients[i].dlm_id, "SATCOM")) {
        pthread_mutex_unlock(&ctx->clients_mutex);
        return &ctx->clients[i];
      }
      if (strstr(sock_path, "wifi") && strstr(ctx->clients[i].dlm_id, "WIFI")) {
        pthread_mutex_unlock(&ctx->clients_mutex);
        return &ctx->clients[i];
      }
    }
  }

  /* 查找空闲槽位创建新客户端 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (!ctx->clients[i].is_registered) {
      DlmClient *client = &ctx->clients[i];
      memset(client, 0, sizeof(DlmClient));
      client->is_registered = true;
      client->client_fd = -1; /* 数据报模式无持久连接 */
      client->last_heartbeat = time(NULL);

      /* 从套接字路径提取 DLM 类型 - 使用与配置文件一致的 LINK_xxx 格式 */
      if (strstr(sock_path, "cellular")) {
        strncpy(client->dlm_id, "DLM_CELLULAR_DGRAM",
                sizeof(client->dlm_id) - 1);
        strncpy(client->link_id, "LINK_CELLULAR", sizeof(client->link_id) - 1);
        client->link_identifier.link_type = LINK_PARAM_TYPE_FDD_LTE;
      } else if (strstr(sock_path, "satcom")) {
        strncpy(client->dlm_id, "DLM_SATCOM_DGRAM", sizeof(client->dlm_id) - 1);
        strncpy(client->link_id, "LINK_SATCOM", sizeof(client->link_id) - 1);
        client->link_identifier.link_type = LINK_PARAM_TYPE_SATCOM_KU;
      } else if (strstr(sock_path, "wifi")) {
        strncpy(client->dlm_id, "DLM_WIFI_DGRAM", sizeof(client->dlm_id) - 1);
        strncpy(client->link_id, "LINK_WIFI", sizeof(client->link_id) - 1);
        client->link_identifier.link_type = LINK_PARAM_TYPE_802_11;
      } else {
        strncpy(client->dlm_id, "DLM_UNKNOWN", sizeof(client->dlm_id) - 1);
        strncpy(client->link_id, "LINK_UNKNOWN", sizeof(client->link_id) - 1);
      }

      fd_log_notice("[app_magic] Created DGRAM client for %s: %s (link_id=%s)",
                    sock_path, client->dlm_id, client->link_id);
      pthread_mutex_unlock(&ctx->clients_mutex);
      return client;
    }
  }

  pthread_mutex_unlock(&ctx->clients_mutex);
  return NULL;
}

/**
 * @brief 同步客户端参数 (Internal Helper)。
 * @details 将接收到的标准 `LINK_PARAMETERS` 结构体数据同步到 MAGIC
 * 内部使用的扩展参数结构 (`mih_link_parameters_t`)。
 *          确保带宽、信号强度等关键指标在不同结构体间的一致性。
 *
 * @param client DLM 客户端指针。
 */
static void sync_client_params(DlmClient *client) {
  client->link_params.current_bandwidth_kbps =
      client->current_parameters.available_bandwidth_kbps;
  if (client->link_params.current_bandwidth_kbps == 0) {
    /* 如果可用带宽为0，尝试使用接收速率 */
    client->link_params.current_bandwidth_kbps =
        client->current_parameters.current_rx_rate_kbps;
  }
  client->link_params.current_latency_ms =
      client->current_parameters.current_latency_ms;
  client->link_params.signal_strength_dbm =
      client->current_parameters.signal_strength_dbm;
  client->link_params.ip_address = client->current_parameters.ip_address;
  client->link_params.netmask = client->current_parameters.netmask;
  client->link_params.link_state = client->current_parameters.link_state;
  client->link_params.signal_quality =
      client->current_parameters.signal_quality;
}

/**
 * @brief 处理数据报模式的 Link_Up.indication。
 * @details 处理从 UDP/DGRAM 套接字接收到的链路上线指示。
 *          - 如果客户端不存在，自动创建。
 *          - 更新配置中的链路状态为 Active。
 *          - 首次上线时触发 `LINK_EVENT_UP` 回调。
 *
 * @param ctx LMI 上下文指针。
 * @param from_path 消息来源路径 (Unix Socket Path)。
 * @param data 消息数据指针。
 * @param len 消息长度。
 */
static void handle_dgram_link_up_indication(MagicLmiContext *ctx,
                                            const char *from_path,
                                            const uint8_t *data, size_t len) {
  fd_log_notice(
      "[app_magic] Processing Link_Up.indication: len=%zu, expected>=%zu", len,
      sizeof(LINK_Up_Indication));

  if (len < sizeof(LINK_Up_Indication)) {
    fd_log_error("[app_magic] DGRAM Link_Up.indication too short: %zu < %zu",
                 len, sizeof(LINK_Up_Indication));
    return;
  }

  const LINK_Up_Indication *ind = (const LINK_Up_Indication *)data;

  /* 查找或创建客户端 */
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);
  if (!client) {
    fd_log_error("[app_magic] Cannot find/create client for %s", from_path);
    return;
  }

  /* 更新链路标识符和参数 */
  pthread_mutex_lock(&ctx->clients_mutex);
  memcpy(&client->link_identifier, &ind->link_identifier,
         sizeof(LINK_TUPLE_ID));
  memcpy(&client->current_parameters, &ind->parameters,
         sizeof(LINK_PARAMETERS));

  /* 同步到扩展参数结构，供 MSXA 等逻辑使用 */
  sync_client_params(client);

  client->last_heartbeat = time(NULL);
  client->last_seen = time(NULL); /* 更新最后接收消息时间 */
  pthread_mutex_unlock(&ctx->clients_mutex);

  /* v2.0: 更新配置中的 DLM 状态 */
  bool first_up = false;
  fd_log_notice(
      "[app_magic] Looking up DLM: client->link_id='%s', ctx->config=%p",
      client->link_id, (void *)ctx->config);
  if (ctx->config) {
    DLMConfig *dlm = magic_config_find_dlm(ctx->config, client->link_id);
    fd_log_notice("[app_magic] magic_config_find_dlm returned: %p",
                  (void *)dlm);
    if (dlm) {
      fd_log_notice("[app_magic] Found DLM: name='%s', is_active=%d",
                    dlm->dlm_name, dlm->is_active);
      if (!dlm->is_active) {
        first_up = true;
        dlm->is_active = true;
        fd_log_notice("[app_magic] Set dlm->is_active = true for %s",
                      dlm->dlm_name);
      }
    } else {
      fd_log_error("[app_magic] DLM '%s' NOT FOUND in config!",
                   client->link_id);
    }
  } else {
    fd_log_error("[app_magic] ctx->config is NULL!");
  }

  /* 首次上线打印详细日志，后续心跳只打印调试日志 */
  if (first_up) {
    fd_log_notice(
        "[app_magic] ✓ Link %s is now ONLINE (via Link_Up.indication)",
        client->link_id);
    fd_log_notice("[app_magic]     Link: %s (type=0x%02X)",
                  ind->link_identifier.link_addr,
                  ind->link_identifier.link_type);
    fd_log_notice("[app_magic]     BW: TX=%u RX=%u kbps",
                  ind->parameters.current_tx_rate_kbps,
                  ind->parameters.current_rx_rate_kbps);

    /* 注意：不在这里注册到数据平面，等客户端 MCCR 成功分配链路时再注册 */

    /* v2.1: 触发事件回调 (用于 MSCR 广播) */
    trigger_lmi_event_callbacks(ctx, client->link_id, LINK_EVENT_UP, ind);
  } else {
    fd_log_debug("[app_magic] Link_Up heartbeat from %s (link=%s)", from_path,
                 client->link_id);
  }
}

/**
 * @brief 处理数据报模式的 Link_Down.indication。
 * @details 处理从 UDP/DGRAM 套接字接收到的链路下线指示。
 *          - 更新配置中的链路状态为 Inactive。
 *          - 从数据平面 (Data Plane) 注销链路路由。
 *          - 触发 `LINK_EVENT_DOWN` 回调。
 *
 * @param ctx LMI 上下文指针。
 * @param from_path 消息来源路径。
 * @param data 消息数据指针。
 * @param len 消息长度。
 */
static void handle_dgram_link_down_indication(MagicLmiContext *ctx,
                                              const char *from_path,
                                              const uint8_t *data, size_t len) {
  if (len < sizeof(LINK_Down_Indication)) {
    fd_log_error("[app_magic] DGRAM Link_Down.indication too short: %zu", len);
    return;
  }

  const LINK_Down_Indication *ind = (const LINK_Down_Indication *)data;
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);

  if (client && ctx->config) {
    DatalinkProfile *link =
        magic_config_find_datalink(ctx->config, client->link_id);
    if (link) {
      link->is_active = false;
    }

    /* v2.2: 从数据平面注销链路 */
    int ret = magic_dataplane_unregister_link(&g_magic_ctx.dataplane_ctx,
                                              client->link_id);
    if (ret == 0) {
      fd_log_notice("[app_magic] ✓ 链路已从数据平面注销: %s", client->link_id);
    } else {
      fd_log_debug("[app_magic] 链路未在数据平面注册，跳过注销: %s",
                   client->link_id);
    }
  }

  fd_log_notice("[app_magic] ✗ DGRAM Link_Down.indication from %s:", from_path);
  fd_log_notice("[app_magic]     Link: %s, Reason: %s (%u)",
                ind->link_identifier.link_addr,
                link_down_reason_to_string((LINK_DOWN_REASON)ind->reason_code),
                ind->reason_code);

  /* v2.1: 触发事件回调 (用于 MSCR 广播) */
  if (client) {
    trigger_lmi_event_callbacks(ctx, client->link_id, LINK_EVENT_DOWN, ind);
  }
}

/**
 * @brief 处理数据报模式的 Link_Parameters_Report.indication。
 * @details 处理链路参数报告（如信号强度变化）。
 *          - 更新客户端缓存的参数。
 *          - 隐式保活：收到报告视为链路在线。
 *
 * @param ctx LMI 上下文指针。
 * @param from_path 消息来源路径。
 * @param data 消息数据指针。
 * @param len 消息长度。
 */
static void handle_dgram_parameters_report(MagicLmiContext *ctx,
                                           const char *from_path,
                                           const uint8_t *data, size_t len) {
  if (len < sizeof(LINK_Parameters_Report_Indication)) {
    fd_log_error("[app_magic] DGRAM Parameters_Report too short: %zu", len);
    return;
  }

  const LINK_Parameters_Report_Indication *ind =
      (const LINK_Parameters_Report_Indication *)data;
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);

  if (client) {
    pthread_mutex_lock(&ctx->clients_mutex);
    memcpy(&client->current_parameters, &ind->parameters,
           sizeof(LINK_PARAMETERS));

    /* 同步到扩展参数结构，供 MSXA 等逻辑使用 */
    sync_client_params(client);

    client->last_heartbeat = time(NULL);
    client->last_seen = time(NULL); /* 更新最后接收消息时间 */
    pthread_mutex_unlock(&ctx->clients_mutex);

    /* 隐式 Link_Up: 如果链路能发送参数报告，说明它已经在线 */
    if (ctx->config) {
      DatalinkProfile *link =
          magic_config_find_datalink(ctx->config, client->link_id);
      if (link && !link->is_active) {
        link->is_active = true;
        fd_log_notice("[app_magic] ✓ Link %s marked ONLINE (implicit via "
                      "Parameters_Report)",
                      client->link_id);
      }
    }

    /* 注意：不在这里更新 dataplane 的网关，等客户端 MCCR
     * 成功分配链路时再创建路由表 */
  }

  /* 参数报告使用调试级别日志，避免刷屏 */
  fd_log_debug(
      "[app_magic] Parameters_Report from %s: Signal=%d dBm, BW=%u kbps",
      from_path, ind->parameters.signal_strength_dbm,
      ind->parameters.available_bandwidth_kbps);
}

/**
 * @brief 处理数据报模式的 Link_Capability_Discover.confirm
 */
static void handle_dgram_capability_confirm(MagicLmiContext *ctx,
                                            const char *from_path,
                                            const uint8_t *data, size_t len) {
  if (len < sizeof(LINK_Capability_Discover_Confirm)) {
    fd_log_error("[app_magic] DGRAM Capability_Confirm too short: %zu", len);
    return;
  }

  const LINK_Capability_Discover_Confirm *cnf =
      (const LINK_Capability_Discover_Confirm *)data;
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);

  if (client && cnf->has_capability) {
    pthread_mutex_lock(&ctx->clients_mutex);
    memcpy(&client->link_capability, &cnf->capability, sizeof(LINK_CAPABILITY));
    client->last_heartbeat = time(NULL);
    pthread_mutex_unlock(&ctx->clients_mutex);
  }

  fd_log_notice("[app_magic] ✓ DGRAM Capability_Discover.confirm from %s:",
                from_path);
  fd_log_notice("[app_magic]     Status: %s", status_to_string(cnf->status));
  if (cnf->has_capability) {
    fd_log_notice("[app_magic]     Max BW: %u kbps, Latency: %u ms, MTU: %u",
                  cnf->capability.max_bandwidth_kbps,
                  cnf->capability.typical_latency_ms, cnf->capability.mtu);
  }
}

/**
 * @brief 处理数据报模式的 Link_Get_Parameters.confirm
 */
static void handle_dgram_get_parameters_confirm(MagicLmiContext *ctx,
                                                const char *from_path,
                                                const uint8_t *data,
                                                size_t len) {
  if (len < sizeof(LINK_Get_Parameters_Confirm)) {
    fd_log_error("[app_magic] DGRAM Get_Parameters.confirm too short: %zu",
                 len);
    return;
  }

  const LINK_Get_Parameters_Confirm *cnf =
      (const LINK_Get_Parameters_Confirm *)data;
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);

  if (client) {
    pthread_mutex_lock(&ctx->clients_mutex);
    memcpy(&client->current_parameters, &cnf->parameters,
           sizeof(LINK_PARAMETERS));
    client->last_heartbeat = time(NULL);
    pthread_mutex_unlock(&ctx->clients_mutex);
  }

  fd_log_notice("[app_magic] ✓ DGRAM Get_Parameters.confirm from %s:",
                from_path);
  fd_log_notice("[app_magic]     Status: %s, Params: 0x%04X",
                status_to_string(cnf->status), cnf->returned_params);
}

/**
 * @brief 处理数据报模式的 Link_Event_Subscribe.confirm
 */
static void handle_dgram_event_subscribe_confirm(MagicLmiContext *ctx,
                                                 const char *from_path,
                                                 const uint8_t *data,
                                                 size_t len) {
  if (len < sizeof(LINK_Event_Subscribe_Confirm)) {
    fd_log_error("[app_magic] DGRAM Event_Subscribe.confirm too short: %zu",
                 len);
    return;
  }

  const LINK_Event_Subscribe_Confirm *cnf =
      (const LINK_Event_Subscribe_Confirm *)data;
  DlmClient *client = find_or_create_dgram_client(ctx, from_path);

  if (client) {
    pthread_mutex_lock(&ctx->clients_mutex);
    client->subscribed_events = cnf->subscribed_events;
    client->last_heartbeat = time(NULL);
    pthread_mutex_unlock(&ctx->clients_mutex);
  }

  fd_log_notice("[app_magic] ✓ DGRAM Event_Subscribe.confirm from %s:",
                from_path);
  fd_log_notice("[app_magic]     Status: %s, Subscribed: 0x%04X",
                status_to_string(cnf->status), cnf->subscribed_events);
}

/**
 * @brief 处理数据报模式的 Link_Resource.confirm
 */
static void handle_dgram_resource_confirm(MagicLmiContext *ctx,
                                          const char *from_path,
                                          const uint8_t *data, size_t len) {
  if (len < sizeof(LINK_Resource_Confirm)) {
    fd_log_error("[app_magic] DGRAM Resource.confirm too short: %zu", len);
    return;
  }

  const LINK_Resource_Confirm *cnf = (const LINK_Resource_Confirm *)data;

  fd_log_notice("[app_magic] ✓ DGRAM Resource.confirm from %s:", from_path);
  fd_log_notice("[app_magic]     Status: %s", status_to_string(cnf->status));
  if (cnf->has_bearer_id) {
    fd_log_notice("[app_magic]     Bearer ID: %u", cnf->bearer_identifier);
  }
}

/**
 * @brief 处理数据报模式接收到的消息。
 * @details 根据 2 字节的消息类型码 (`msg_type`) 分发到对应的具体处理函数。
 *          支持 IEEE 802.21 标准原语和 ARINC 839 扩展原语。
 *
 * @param ctx LMI 上下文指针。
 * @param from_path 消息来源路径。
 * @param msg_type 消息类型码 (Host Byte Order)。
 * @param data 消息负载指针 (不含类型码)。
 * @param len 负载长度。
 */
static void process_dgram_message(MagicLmiContext *ctx, const char *from_path,
                                  uint16_t msg_type, const uint8_t *data,
                                  size_t len) {
  fd_log_notice("[app_magic] DGRAM RX: type=0x%04X, len=%zu, from=%s", msg_type,
                len, from_path);

  switch (msg_type) {
  /* IEEE 802.21 指示原语 (0x02xx) */
  case MIH_LINK_DETECTED_IND:
    fd_log_notice("[app_magic] DGRAM Link_Detected.indication from %s",
                  from_path);
    /* TODO: 实现 Link_Detected 处理 */
    break;

  case MIH_LINK_UP_IND:
    handle_dgram_link_up_indication(ctx, from_path, data, len);
    break;

  case MIH_LINK_DOWN_IND:
    handle_dgram_link_down_indication(ctx, from_path, data, len);
    break;

  case MIH_LINK_GOING_DOWN_IND:
    fd_log_notice("[app_magic] DGRAM Link_Going_Down.indication from %s",
                  from_path);
    /* TODO: 实现 Link_Going_Down 处理 */
    break;

  case MIH_LINK_PARAMETERS_REPORT_IND:
    handle_dgram_parameters_report(ctx, from_path, data, len);
    break;

  /* IEEE 802.21 确认原语 (0x01xx) */
  case MIH_LINK_CAPABILITY_DISCOVER_CNF:
    handle_dgram_capability_confirm(ctx, from_path, data, len);
    break;

  case MIH_LINK_GET_PARAMETERS_CNF:
    handle_dgram_get_parameters_confirm(ctx, from_path, data, len);
    break;

  case MIH_LINK_EVENT_SUBSCRIBE_CNF:
    handle_dgram_event_subscribe_confirm(ctx, from_path, data, len);
    break;

  case MIH_LINK_EVENT_UNSUBSCRIBE_CNF:
    fd_log_notice("[app_magic] DGRAM Event_Unsubscribe.confirm from %s",
                  from_path);
    break;

  case MIH_LINK_CONFIGURE_THRESHOLDS_CNF:
    fd_log_notice("[app_magic] DGRAM Configure_Thresholds.confirm from %s",
                  from_path);
    break;

  /* ARINC 839 扩展原语 (0x03xx) */
  case MIH_LINK_RESOURCE_CNF:
    handle_dgram_resource_confirm(ctx, from_path, data, len);
    break;

  default:
    fd_log_notice("[app_magic] DGRAM unknown message type: 0x%04X from %s",
                  msg_type, from_path);
    break;
  }
}

/**
 * @brief 数据报服务器主线程 (DGRAM)。
 * @details 创建并运行数据报模式服务器循环。
 *          - 使用 `select()` 监听 Socket，支持超时和非阻塞退出。
 *          - 接收消息格式：[Type(2B)][Payload...]。
 *
 * @param arg 线程参数 (`MagicLmiContext*`)。
 * @return NULL。
 */
static void *dgram_server_thread(void *arg) {
  MagicLmiContext *ctx = (MagicLmiContext *)arg;

  fd_log_notice("[app_magic] DGRAM server thread started (fd=%d)",
                ctx->dgram_fd);

  uint8_t buffer[MIH_MAX_MESSAGE_SIZE];
  struct sockaddr_un from_addr;
  socklen_t from_len;

  while (ctx->dgram_running) {
    /* 使用 select() 实现超时接收 */
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(ctx->dgram_fd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(ctx->dgram_fd + 1, &readfds, NULL, NULL, &tv);

    if (ret < 0) {
      if (errno == EINTR)
        continue;
      fd_log_error("[app_magic] DGRAM select() failed: %s", strerror(errno));
      break;
    }

    if (ret == 0) {
      /* 超时，继续循环 */
      continue;
    }

    /* 接收消息 */
    from_len = sizeof(from_addr);
    memset(&from_addr, 0, sizeof(from_addr));

    uint16_t msg_type;
    int payload_len =
        mih_transport_recvfrom(ctx->dgram_fd, (struct sockaddr *)&from_addr,
                               &from_len, &msg_type, buffer, sizeof(buffer));

    if (payload_len < 0) {
      fd_log_error("[app_magic] DGRAM recvfrom() failed");
      continue;
    }

    /* 处理消息 */
    process_dgram_message(ctx, from_addr.sun_path, msg_type, buffer,
                          (size_t)payload_len);
  }

  fd_log_notice("[app_magic] DGRAM server thread exiting");
  return NULL;
}

/**
 * @brief 启动数据报模式 LMI 服务器。
 * @details 创建 Unix Domain Datagram Socket 并启动接收线程。
 *          用于兼容旧版或轻量级 DLM 客户端，无需建立持久连接。
 *
 * @param ctx LMI 上下文指针。
 * @param socket_path Socket 文件路径 (默认 `/tmp/mihf.sock`)。
 * @return 0 成功，-1 失败。
 */
int magic_lmi_start_dgram_server(MagicLmiContext *ctx,
                                 const char *socket_path) {
  /* 参数验证 */
  if (!ctx) {
    return -1;
  }

  const char *path = socket_path ? socket_path : MIH_DGRAM_SOCKET_PATH;

  /* 创建数据报套接字 */
  ctx->dgram_fd = mih_transport_create_dgram_server(path);
  if (ctx->dgram_fd < 0) {
    fd_log_error("[app_magic] Failed to create DGRAM server on %s", path);
    return -1;
  }

  /* 启动服务器线程 */
  ctx->dgram_running = true;
  if (pthread_create(&ctx->dgram_thread, NULL, dgram_server_thread, ctx) != 0) {
    fd_log_error("[app_magic] Failed to create DGRAM server thread");
    close(ctx->dgram_fd);
    ctx->dgram_fd = -1;
    return -1;
  }

  fd_log_notice("[app_magic] ✓ DGRAM server started on %s", path);
  fd_log_notice("[app_magic]   Accepts messages with 2-byte type code prefix");
  return 0;
}

/*===========================================================================
 * UDP 监听服务器实现 (接收 DLM 原型 UDP 广播心跳)
 *===========================================================================*/

/**
 * @brief 处理接收到的 UDP 心跳消息。
 * @details 解析 DLM 通过 UDP 广播发送的心跳包。
 *          - 如果是新 DLM，自动注册并创建客户端记录。
 *          - 验证 DLM 是否在 `Datalink_Profile.xml` 中定义且启用。
 *          - 触发 `LINK_EVENT_UP` 事件（如果是首次收到）。
 *
 * @param ctx LMI 上下文指针。
 * @param from_addr 发送方网络地址。
 * @param data 消息数据指针。
 * @param data_len 消息长度。
 */
static void handle_udp_heartbeat(MagicLmiContext *ctx,
                                 const struct sockaddr_in *from_addr,
                                 const void *data, size_t data_len) {
  if (!ctx || !from_addr || !data || data_len < sizeof(MsgHeartbeat)) {
    return;
  }

  /* 解析心跳消息 */
  const MsgHeartbeat *hb = (const MsgHeartbeat *)data;

  fd_log_debug("[app_magic] Received UDP heartbeat from %s:%d - DLM: %s",
               inet_ntoa(from_addr->sin_addr), ntohs(from_addr->sin_port),
               hb->dlm_id);

  /* 验证链路是否在配置文件中定义 */
  if (ctx->config) {
    DatalinkProfile *link = magic_config_find_datalink(ctx->config, hb->dlm_id);
    if (!link) {
      fd_log_error("[app_magic] ✗ Rejected UDP heartbeat from %s - Link not "
                   "defined in configuration: %s",
                   inet_ntoa(from_addr->sin_addr), hb->dlm_id);
      return;
    }

    if (!link->enabled) {
      fd_log_error("[app_magic] ✗ Rejected UDP heartbeat from %s - Link is "
                   "disabled in configuration: %s",
                   inet_ntoa(from_addr->sin_addr), hb->dlm_id);
      return;
    }
  }

  /* 查找或创建 DLM 客户端 */
  pthread_mutex_lock(&ctx->clients_mutex);

  DlmClient *client = NULL;
  int client_index = -1;

  /* 先查找是否已经存在 */
  for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
    if (ctx->clients[i].is_registered &&
        strcmp(ctx->clients[i].dlm_id, hb->dlm_id) == 0) {
      client = &ctx->clients[i];
      client_index = i;
      break;
    }
  }

  /* 如果不存在，创建新记录 */
  if (!client) {
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
      if (!ctx->clients[i].is_registered) {
        client = &ctx->clients[i];
        client_index = i;

        /* 初始化新客户端 */
        memset(client, 0, sizeof(DlmClient));
        client->is_registered = true;
        client->client_fd = -1; /* UDP 不使用连接 */
        strncpy(client->dlm_id, hb->dlm_id, sizeof(client->dlm_id) - 1);
        strncpy(client->link_id, hb->dlm_id, sizeof(client->link_id) - 1);
        client->last_heartbeat = time(NULL);
        client->last_seen = time(NULL);

        /* 更新链路状态为活动 */
        if (ctx->config) {
          DatalinkProfile *link =
              magic_config_find_datalink(ctx->config, client->link_id);
          if (link) {
            link->is_active = true;
          }
        }

        fd_log_notice(
            "[app_magic] ✓ DLM registered via UDP: %s (index=%d, from %s:%d)",
            client->dlm_id, client_index, inet_ntoa(from_addr->sin_addr),
            ntohs(from_addr->sin_port));

        /* 触发链路上线事件 */
        trigger_lmi_event_callbacks(ctx, client->link_id, LINK_EVENT_UP, NULL);

        break;
      }
    }

    if (!client) {
      fd_log_error("[app_magic] No available client slots for UDP DLM: %s",
                   hb->dlm_id);
      pthread_mutex_unlock(&ctx->clients_mutex);
      return;
    }
  } else {
    /* 更新现有客户端的心跳时间 */
    client->last_heartbeat = time(NULL);
    client->last_seen = time(NULL);

    fd_log_debug(
        "[app_magic] Updated heartbeat for DLM: %s (last_seen updated)",
        client->dlm_id);
  }

  pthread_mutex_unlock(&ctx->clients_mutex);
}

/**
 * @brief UDP 监听线程主函数。
 * @details 持续监听 UDP 端口 (默认 1947) 接收心跳广播。
 *          使用 `select()` 实现非阻塞 I/O。
 *
 * @param arg 线程参数 (`MagicLmiContext*`)。
 * @return NULL。
 */
static void *udp_listener_thread(void *arg) {
  MagicLmiContext *ctx = (MagicLmiContext *)arg;

  fd_log_notice("[app_magic] UDP listener thread started (port=%u)",
                ctx->udp_port);

  uint8_t buffer[4096]; /* UDP 接收缓冲区 */
  struct sockaddr_in from_addr;
  socklen_t from_len;

  while (ctx->udp_running) {
    /* 使用 select() 实现超时接收 */
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(ctx->udp_fd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(ctx->udp_fd + 1, &readfds, NULL, NULL, &tv);

    if (ret < 0) {
      if (errno == EINTR)
        continue;
      fd_log_error("[app_magic] UDP select() failed: %s", strerror(errno));
      break;
    }

    if (ret == 0) {
      /* 超时，继续循环 */
      continue;
    }

    /* 接收 UDP 数据包 */
    from_len = sizeof(from_addr);
    memset(&from_addr, 0, sizeof(from_addr));

    ssize_t recv_len = recvfrom(ctx->udp_fd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&from_addr, &from_len);

    if (recv_len < 0) {
      fd_log_error("[app_magic] UDP recvfrom() failed: %s", strerror(errno));
      continue;
    }

    if (recv_len == 0) {
      /* 空数据包，忽略 */
      continue;
    }

    /* 处理接收到的消息 */
    handle_udp_heartbeat(ctx, &from_addr, buffer, (size_t)recv_len);
  }

  fd_log_notice("[app_magic] UDP listener thread exiting");
  return NULL;
}

/**
 * @brief 启动 UDP 监听服务器。
 * @details 创建 UDP Socket 并绑定到 0.0.0.0 (所有接口)。
 *          启用 `SO_BROADCAST` 和 `SO_REUSEADDR` 选项。
 *
 * @param ctx LMI 上下文指针。
 * @param port UDP 监听端口 (0 表示使用默认值 1947)。
 * @return 0 成功，-1 失败。
 */
int magic_lmi_start_udp_listener(MagicLmiContext *ctx, uint16_t port) {
  /* 参数验证 */
  if (!ctx) {
    return -1;
  }

  /* 设置端口 (0 表示使用默认值 1947) */
  if (port == 0) {
    port = 1947;
  }
  ctx->udp_port = port;

  /* 创建 UDP socket */
  ctx->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (ctx->udp_fd < 0) {
    fd_log_error("[app_magic] Failed to create UDP socket: %s",
                 strerror(errno));
    return -1;
  }

  /* 设置 socket 选项: 允许地址重用 */
  int reuse = 1;
  if (setsockopt(ctx->udp_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    fd_log_error("[app_magic] Failed to set SO_REUSEADDR: %s", strerror(errno));
    close(ctx->udp_fd);
    ctx->udp_fd = -1;
    return -1;
  }

  /* 设置 socket 选项: 允许广播 */
  int broadcast = 1;
  if (setsockopt(ctx->udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast,
                 sizeof(broadcast)) < 0) {
    fd_log_error("[app_magic] Failed to set SO_BROADCAST: %s", strerror(errno));
    close(ctx->udp_fd);
    ctx->udp_fd = -1;
    return -1;
  }

  /* 绑定到所有接口的指定端口 */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY; /* 绑定到 0.0.0.0 */
  addr.sin_port = htons(port);

  if (bind(ctx->udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fd_log_error("[app_magic] Failed to bind UDP socket to port %u: %s", port,
                 strerror(errno));
    close(ctx->udp_fd);
    ctx->udp_fd = -1;
    return -1;
  }

  /* 启动 UDP 监听线程 */
  ctx->udp_running = true;
  if (pthread_create(&ctx->udp_thread, NULL, udp_listener_thread, ctx) != 0) {
    fd_log_error("[app_magic] Failed to create UDP listener thread");
    close(ctx->udp_fd);
    ctx->udp_fd = -1;
    ctx->udp_running = false;
    return -1;
  }

  fd_log_notice("[app_magic] ✓ UDP listener started on 0.0.0.0:%u", port);
  fd_log_notice(
      "[app_magic]   Listening for DLM prototype UDP broadcast heartbeats");
  return 0;
}

/*===========================================================================
 * 事件回调机制实现 (MSCR/MSCA 合规性修复)
 *===========================================================================*/

/**
 * @brief 触发已注册的事件回调 (Internal Helper)。
 * @details 遍历回调表，调用所有匹配 `event_type` 的回调函数。
 *          注意：调用回调时会临时解锁互斥量，以避免死锁。
 *
 * @param ctx LMI 上下文指针。
 * @param link_id 触发事件的链路 ID。
 * @param event_type 事件类型 (`LINK_EVENT_TYPE`)。
 * @param event_data 事件数据指针 (类型取决于 event_type)。
 */
static void trigger_lmi_event_callbacks(MagicLmiContext *ctx,
                                        const char *link_id,
                                        uint16_t event_type,
                                        const void *event_data) {
  if (!ctx || !link_id) {
    return;
  }

  pthread_mutex_lock(&ctx->callbacks_mutex);

  int triggered = 0;
  for (int i = 0; i < ctx->num_callbacks; i++) {
    if (ctx->event_callbacks[i].event_type == event_type &&
        ctx->event_callbacks[i].callback != NULL) {

      /* 调用回调 (解锁以避免死锁) */
      lmi_event_callback_t cb = ctx->event_callbacks[i].callback;
      pthread_mutex_unlock(&ctx->callbacks_mutex);

      cb(ctx, link_id, event_type, event_data);
      triggered++;

      pthread_mutex_lock(&ctx->callbacks_mutex);
    }
  }

  pthread_mutex_unlock(&ctx->callbacks_mutex);

  if (triggered > 0) {
    fd_log_debug(
        "[app_magic] Triggered %d callback(s) for event 0x%04X on link %s",
        triggered, event_type, link_id);
  }
}

/**
 * @brief 注册事件回调。
 * @details 注册一个函数来处理特定类型的链路事件。
 *          支持多个回调处理同一事件类型 (One-to-Many)。
 *
 * @param ctx LMI 上下文指针。
 * @param event_type 要订阅的事件类型 (`LINK_EVENT_TYPE`)。
 * @param callback 回调函数指针。
 * @return 0 成功，-1 失败 (回调表满)。
 */
int magic_lmi_register_event_callback(MagicLmiContext *ctx, uint16_t event_type,
                                      lmi_event_callback_t callback) {
  if (!ctx || !callback) {
    return -1;
  }

  pthread_mutex_lock(&ctx->callbacks_mutex);

  /* 检查回调表是否已满 */
  if (ctx->num_callbacks >= MAX_EVENT_CALLBACKS) {
    pthread_mutex_unlock(&ctx->callbacks_mutex);
    fd_log_error("[app_magic] Event callback table full (%d)",
                 MAX_EVENT_CALLBACKS);
    return -1;
  }

  /* 添加新回调 */
  ctx->event_callbacks[ctx->num_callbacks].event_type = event_type;
  ctx->event_callbacks[ctx->num_callbacks].callback = callback;
  ctx->num_callbacks++;

  pthread_mutex_unlock(&ctx->callbacks_mutex);

  fd_log_notice(
      "[app_magic] ✓ Registered event callback for type 0x%04X (total: %d)",
      event_type, ctx->num_callbacks);

  return 0;
}
/*===========================================================================
 * 心跳监控线程
 *
 * 功能：定期检查所有已注册 DLM 的心跳超时情况
 * - 每隔 MONITOR_CHECK_INTERVAL_SEC 扫描一次所有客户端
 * - 对于超过 HEARTBEAT_TIMEOUT_SEC 未发送心跳的 DLM，触发 LINK_EVENT_DOWN
 * - 避免被动等待 Link_Down.indication，实现主动故障检测
 *
 * 典型场景：DLM 进程被 SIGKILL 杀死，无法发送 Link_Down 消息
 *===========================================================================*/

/**
 * @brief 心跳监控线程函数。
 * @details 后台线程，定期 (每 5s) 检查所有 DLM 客户端的 `last_seen` 时间戳。
 *          - 如果 `now - last_seen > HEARTBEAT_TIMEOUT`，判定为超时。
 *          - 触发 `LINK_EVENT_DOWN` 事件并清理客户端资源。
 *          用于处理 DLM 异常退出 (Crash/Kill) 的情况。
 *
 * @param arg 线程参数 (`MagicLmiContext*`)。
 * @return NULL。
 */
static void *heartbeat_monitor_thread_func(void *arg) {
  MagicLmiContext *ctx = (MagicLmiContext *)arg;

  fd_log_notice("[app_magic] ✓ Heartbeat monitor thread started (timeout=%ds, "
                "interval=%ds)",
                HEARTBEAT_TIMEOUT_SEC, MONITOR_CHECK_INTERVAL_SEC);

  while (ctx->heartbeat_monitor_running) {
    time_t now = time(NULL);

    pthread_mutex_lock(&ctx->clients_mutex);

    /* 遍历所有客户端检查超时 */
    for (int i = 0; i < MAX_DLM_CLIENTS; i++) {
      DlmClient *client = &ctx->clients[i];

      /* 跳过未注册的客户端 */
      if (!client->is_registered) {
        continue;
      }

      /* 检查心跳超时 */
      time_t elapsed = now - client->last_seen;
      if (elapsed > HEARTBEAT_TIMEOUT_SEC) {
        fd_log_notice("[app_magic] ⚠ DLM heartbeat timeout detected: %s (last "
                      "seen %ld sec ago)",
                      client->link_id, (long)elapsed);

        /* 触发 LINK_EVENT_DOWN 回调 */
        trigger_lmi_event_callbacks(ctx, client->link_id, LINK_EVENT_DOWN,
                                    NULL);

        /* 标记为未注册（避免重复触发） */
        client->is_registered = false;

        /* 关闭客户端连接 */
        if (client->client_fd > 0) {
          close(client->client_fd);
          client->client_fd = -1;
        }

        fd_log_notice("[app_magic] ✓ Cleaned up timed-out DLM: %s",
                      client->link_id);
      }
    }

    pthread_mutex_unlock(&ctx->clients_mutex);

    /* 休眠一段时间再进行下一轮检查 */
    sleep(MONITOR_CHECK_INTERVAL_SEC);
  }

  fd_log_notice("[app_magic] Heartbeat monitor thread exiting");

  return NULL;
}