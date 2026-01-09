/**
 * ============================================================================
 * @file    dlm_satcom_standard.c
 * @brief   SATCOM 数据链路管理器 (DLM) - Standard MIH 实现
 *
 * @details 遵循 ARINC 839-2014 MIH 协议规范的 SATCOM 链路标准实现
 *          - 使用 UNIX Domain Socket Stream (/tmp/magic_lmi.sock)
 *          - 实现完整的 12 字节 MIH 消息头
 *          - 支持 MIH_EXT_Link_Register 注册流程
 *          - 支持 Link_Up/Link_Down/Link_Resource 等原语
 *
 * @author  MAGIC 航空通信系统团队
 * @version 2.0.0
 * @date    2025
 * ============================================================================
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* 引入公共 DLM 定义 */
#include "../DLM_COMMON/dlm_common.h"

/* 引入 MIH 协议定义 */
#include "../extensions/app_magic/mih_extensions.h"
#include "../extensions/app_magic/mih_protocol.h"

/* ============================================================================
 * SATCOM 链路特定常量
 * ============================================================================
 */

#define DEFAULT_CONFIG_PATH "../DLM_CONFIG/dlm_satcom.ini"
#define MIH_STANDARD_SOCKET_PATH "/tmp/magic_core.sock"

/* ============================================================================
 * 传输层定义 (Standard MIH)
 * ============================================================================
 */

/* 12字节传输头 */
typedef struct {
  uint16_t primitive_type; // MIH 原语类型码
  uint16_t message_length; // 总长度 (Header + Payload)
  uint32_t transaction_id; // 事务 ID
  uint32_t timestamp;      // 发送时间戳
} __attribute__((packed)) mih_transport_header_t;

/* ============================================================================
 * 全局上下文
 * ============================================================================
 */

static dlm_context_t g_ctx;
static dlm_network_config_t g_net_config;
static volatile int g_running = 1;
static uint32_t g_transaction_id_counter = 1;

/* ============================================================================
 * 前向声明
 * ============================================================================
 */

static int dlm_init_config_manager(const char *config_path);
static int dlm_init_state_simulator(void);
static int dlm_init_socket(void);
static int dlm_connect_to_server(void);
static int dlm_send_mih_message(uint16_t type, const void *payload,
                                uint16_t payload_len);
static int dlm_send_register_request(void);
static void dlm_signal_handler(int sig);
static void *dlm_reporting_thread(void *arg);
static void *dlm_message_receiver_thread(void *arg); // 新增接收线程
static void *dlm_packet_monitor_thread(void *arg);

/* Standard MIH request handlers (missing in initial standard version) */
static int dlm_handle_capability_discover(const LINK_Capability_Discover_Request *req);
static int dlm_handle_get_parameters(const LINK_Get_Parameters_Request *req);
static int dlm_handle_event_subscribe(const LINK_Event_Subscribe_Request *req);

/* 辅助函数：网络状态检查 (复用原型逻辑) */
static bool check_interface_status(const char *iface) {
  char path[128];
  char status[16];
  snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
  FILE *fp = fopen(path, "r");
  if (!fp)
    return false;
  if (fgets(status, sizeof(status), fp) == NULL) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  status[strcspn(status, "\n")] = 0;
  return (strcmp(status, "up") == 0);
}

/* ==========================================================================
 * 抓包监控线程 (与 WIFI 标准版对齐)
 * ========================================================================== */

static void *dlm_packet_monitor_thread(void *arg) {
  (void)arg;

  printf("[SATCOM-PKT] Packet monitor thread started, iface=%s\n",
         g_ctx.config.interface_name);

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    printf("[SATCOM-PKT] ERROR: pipe() failed: %s\n", strerror(errno));
    return NULL;
  }

  pid_t pid = fork();
  if (pid < 0) {
    printf("[SATCOM-PKT] ERROR: fork() failed: %s\n", strerror(errno));
    close(pipefd[0]);
    close(pipefd[1]);
    return NULL;
  }

  if (pid == 0) {
    (void)dup2(pipefd[1], STDOUT_FILENO);
    (void)dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    const char *filter = "udp";
    char *const args[] = {
        "tcpdump",
        "-i",
        (char *)g_ctx.config.interface_name,
        "-n",
        "-s",
        "0",
        "-vv",
        "-X",
        "-U",
        "-l",
        (char *)filter,
        NULL,
    };

    execvp("tcpdump", args);
    _exit(127);
  }

  close(pipefd[1]);
  FILE *fp = fdopen(pipefd[0], "r");
  if (!fp) {
    printf("[SATCOM-PKT] ERROR: fdopen() failed: %s\n", strerror(errno));
    kill(pid, SIGINT);
    (void)waitpid(pid, NULL, 0);
    close(pipefd[0]);
    return NULL;
  }

  printf("[SATCOM-PKT] tcpdump started (pid=%d)\n", (int)pid);

  char line[512];
  while (g_running) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int rc = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (rc == 0)
      continue;

    if (!fgets(line, sizeof(line), fp))
      break;
    printf("[SATCOM-PKT] %s", line);
    fflush(stdout);
  }

  kill(pid, SIGINT);
  (void)waitpid(pid, NULL, 0);
  fclose(fp);

  printf("[SATCOM-PKT] Packet monitor thread exit\n");
  return NULL;
}

/* ============================================================================
 * 配置管理器初始化
 * ============================================================================
 */

static int dlm_init_config_manager(const char *config_path) {
  if (dlm_load_config(config_path, &g_ctx.config, &g_net_config) != 0) {
    fprintf(stderr, "[SATCOM] 加载配置文件失败: %s\n", config_path);
    return -1;
  }
  printf("[SATCOM-CM] 配置管理器初始化完成 (Standard MIH Mode)\n");
  return 0;
}

/* ============================================================================
 * 状态模拟器初始化
 * ============================================================================
 */

static int dlm_init_state_simulator(void) {
  dlm_state_init(&g_ctx.state);
  g_ctx.state.is_connected = 0;
  g_ctx.state.simulated_rssi = g_net_config.initial_rssi_dbm;
  g_ctx.state.signal_quality = 75;
  return 0;
}

/* ============================================================================
 * Transport Layer Implementation (Standard MIH)
 * ============================================================================
 */

static int dlm_init_socket(void) {
  /* 创建 STREAM Socket */
  g_ctx.socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (g_ctx.socket_fd < 0) {
    perror("[SATCOM] socket() failed");
    return -1;
  }
  return 0;
}

static int dlm_connect_to_server(void) {
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, MIH_STANDARD_SOCKET_PATH, sizeof(addr.sun_path) - 1);

  printf("[SATCOM] Connecting to MIH Server at %s ...\n",
         MIH_STANDARD_SOCKET_PATH);

  if (connect(g_ctx.socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("[SATCOM] connect() failed");
    return -1;
  }

  printf("[SATCOM] Connected! Sending Registration...\n");
  return dlm_send_register_request();
}

/**
 * @brief 构建并发送 Standard MIH 消息
 */
static int dlm_send_mih_message(uint16_t type, const void *payload,
                                uint16_t payload_len) {
  uint8_t buffer[4096];
  if (sizeof(mih_transport_header_t) + payload_len > sizeof(buffer)) {
    fprintf(stderr, "[SATCOM] Message too large\n");
    return -1;
  }

  /* 1. 构造头部 */
  mih_transport_header_t *hdr = (mih_transport_header_t *)buffer;
  hdr->primitive_type = type;
  hdr->message_length = sizeof(mih_transport_header_t) + payload_len;
  hdr->transaction_id = g_transaction_id_counter++;
  hdr->timestamp = (uint32_t)time(NULL);

  /* 2. 拷贝 Payload */
  if (payload && payload_len > 0) {
    memcpy(buffer + sizeof(mih_transport_header_t), payload, payload_len);
  }

  /* 3. 发送 */
  ssize_t sent =
      send(g_ctx.socket_fd, buffer, hdr->message_length, MSG_NOSIGNAL);
  if (sent < 0) {
    perror("[SATCOM] send() failed");
    return -1;
  }
  return 0;
}

/**
 * @brief 发送 MIH 注册请求
 */
static int dlm_send_register_request(void) {
  MIH_EXT_Link_Register_Request req;
  memset(&req, 0, sizeof(req));

  /* 填充 Link Tuple ID */
  req.link_identifier.link_type = g_ctx.config.link_id;
  strncpy(req.link_identifier.link_addr, g_ctx.config.link_name,
          sizeof(req.link_identifier.link_addr) - 1);
  strncpy(req.link_identifier.poa_addr, g_ctx.config.interface_name,
          sizeof(req.link_identifier.poa_addr) - 1);

  /* 填充能力 */
  req.capabilities.max_bandwidth_kbps = g_ctx.config.max_bandwidth_fl;
  req.capabilities.typical_latency_ms = g_ctx.config.reported_delay_ms;
  req.capabilities.cost_per_mb = g_ctx.config.cost_per_mb_cents;
  req.capabilities.coverage = 1; // Global
  req.capabilities.security_level = g_ctx.config.security_level;
  req.capabilities.mtu = g_ctx.config.mtu;

  req.dlm_pid = getpid();

  printf("[SATCOM] Sending MIH_EXT_Link_Register (0x8101)...\n");
  printf("[SATCOM-DEBUG] link_id=0x%02X, link_name=%s, pid=%d\n", 
         req.link_identifier.link_type, req.link_identifier.link_addr, req.dlm_pid);
  printf("[SATCOM-DEBUG] bw=%u kbps, latency=%u ms, cost=%u\n",
         req.capabilities.max_bandwidth_kbps, req.capabilities.typical_latency_ms, 
         req.capabilities.cost_per_mb);
  return dlm_send_mih_message(MIH_EXT_LINK_REGISTER_REQUEST, &req, sizeof(req));
}

/* ============================================================================
 * 心跳与上报 (Updated for Standard MIH)
 * ============================================================================
 */

static void dlm_send_heartbeat(void) {
  /* Standard MIH 心跳可能使用 MIH_EXT_Heartbeat (0x8F01) */
  /* payload 可能是空的或者自定义 */
  /* 假设定义在 mih_extensions.h 中的 MIH_EXT_Heartbeat 结构 */
  /* 如果没有特定 payload，可以发空 payload */

  // 检查 mih_extensions.h 中是否有 MIH_EXT_Heartbeat 结构体
  // 根据之前的 grep，似乎只有 primitive ID 定义。
  // 我们发送一个空的 payload 作为心跳，或者根据 MsgHeartbeat (Legacy) 适配。
  // Standard MIH 通常只需要 header 就能保活，但为了严谨：

  // 这里发送一个简单的心跳 Payload (复用 Legacy MsgHeartbeat 或者自定义)
  MIH_EXT_Heartbeat hb;
  memset(&hb, 0, sizeof(hb));

  hb.link_identifier.link_type = g_ctx.config.link_id;
  strncpy(hb.link_identifier.link_addr, g_ctx.config.link_name,
          sizeof(hb.link_identifier.link_addr) - 1);
  strncpy(hb.link_identifier.poa_addr, g_ctx.config.interface_name,
          sizeof(hb.link_identifier.poa_addr) - 1);

  hb.health_status = HEALTH_STATUS_OK;
  hb.tx_bytes = g_ctx.state.tx_bytes;
  hb.rx_bytes = g_ctx.state.rx_bytes;
  hb.active_bearers = g_ctx.state.num_active_bearers;

  dlm_send_mih_message(MIH_EXT_HEARTBEAT, &hb, sizeof(hb));
}

/* ============================================================================
 * 物理链路操作 (Migrated from Prototype)
 * ============================================================================
 */

static int dlm_physical_link_up(void) {
  char cmd[256];
  int ret;

  printf("[SATCOM-PHY] 激活链路: %s\n", g_ctx.config.interface_name);

  /* 启用接口 */
  ret = dlm_interface_up(g_ctx.config.interface_name);

  /* 配置IP地址 */
  snprintf(cmd, sizeof(cmd), "ip addr add %s/%s dev %s 2>/dev/null",
           g_net_config.ip_address, g_net_config.netmask,
           g_ctx.config.interface_name);
  system(cmd);
  printf("[SATCOM-PHY] 配置IP: %s/%s\n", g_net_config.ip_address,
         g_net_config.netmask);

  pthread_mutex_lock(&g_ctx.state.mutex);
  g_ctx.state.is_connected = 1;
  g_ctx.state.interface_up = 1;
  g_ctx.state.is_going_down = 0;
  g_ctx.state.last_up_time = time(NULL);
  pthread_mutex_unlock(&g_ctx.state.mutex);

  /* 发送 Link_Up 指示 */
  mih_link_up_ind_t ind;
  memset(&ind, 0, sizeof(ind));
  ind.link_id.link_type = g_ctx.config.link_id;
  strncpy(ind.link_id.link_addr, g_ctx.config.link_name,
          sizeof(ind.link_id.link_addr) - 1);

  ind.link_params.current_bandwidth_kbps = g_ctx.config.max_bandwidth_fl;
  ind.link_params.current_latency_ms = g_ctx.config.reported_delay_ms;
  ind.link_params.signal_strength_dbm = g_ctx.state.simulated_rssi;
  ind.link_params.signal_quality = g_ctx.state.signal_quality;
  ind.link_params.link_state = LINK_STATE_UP;

  inet_pton(AF_INET, g_net_config.ip_address, &ind.link_params.ip_address);
  inet_pton(AF_INET, g_net_config.netmask, &ind.link_params.netmask);

  printf("[SATCOM-IND] 准备发送 Link_Up.indication:\n");
  printf("  - link_type: 0x%02X\n", ind.link_id.link_type);
  printf("  - link_addr: %s\n", ind.link_id.link_addr);
  printf("  - bandwidth: %u kbps\n", ind.link_params.current_bandwidth_kbps);
  printf("  - latency: %u ms\n", ind.link_params.current_latency_ms);
  printf("  - RSSI: %d dBm\n", ind.link_params.signal_strength_dbm);
  printf("  - state: %u\n", ind.link_params.link_state);
  printf("  - sizeof(ind): %zu bytes\n", sizeof(ind));

  dlm_send_mih_message(MIH_LINK_UP_INDICATION, &ind, sizeof(ind));
  printf("[SATCOM-IND] Link_Up.indication 已发送\n");

  return ret;
}

static int dlm_physical_link_down(int reason_code) {
  char cmd[256];
  int ret = 0;

  printf("[SATCOM-PHY] 停用链路: %s (原因=%d)\n", g_ctx.config.interface_name,
         reason_code);

  /* 删除IP地址 */
  snprintf(cmd, sizeof(cmd), "ip addr del %s/%s dev %s 2>/dev/null",
           g_net_config.ip_address, g_net_config.netmask,
           g_ctx.config.interface_name);
  system(cmd);

  pthread_mutex_lock(&g_ctx.state.mutex);
  g_ctx.state.is_connected = 0;
  g_ctx.state.interface_up = 0;
  g_ctx.state.is_going_down = 0;
  g_ctx.state.last_down_time = time(NULL);
  pthread_mutex_unlock(&g_ctx.state.mutex);

  /* 发送 Link_Down 指示 */
  mih_link_down_ind_t ind;
  memset(&ind, 0, sizeof(ind));
  ind.link_id.link_type = g_ctx.config.link_id;
  strncpy(ind.link_id.link_addr, g_ctx.config.link_name,
          sizeof(ind.link_id.link_addr) - 1);

  ind.reason_code = reason_code;

  dlm_send_mih_message(MIH_LINK_DOWN_INDICATION, &ind, sizeof(ind));
  printf("[SATCOM-IND] 发送 Link_Down.indication\n");

  return ret;
}

/* ============================================================================
 * 原语处理函数
 * ============================================================================
 */

static int dlm_handle_link_resource(const LINK_Resource_Request *req) {
  printf("[SATCOM-PRIM] 处理 Link_Resource.request\n");

  LINK_Resource_Confirm confirm;
  memset(&confirm, 0, sizeof(confirm));

  if (req->resource_action == RESOURCE_ACTION_REQUEST) {
    uint8_t bearer_id = 0;
    uint32_t req_bw_fl =
        req->has_qos_params ? req->qos_parameters.forward_link_rate : 1000;
    uint32_t req_bw_rl =
        req->has_qos_params ? req->qos_parameters.return_link_rate : 200;
    uint8_t cos_id =
        req->has_qos_params ? req->qos_parameters.cos_id : COS_BEST_EFFORT;

    int ret = dlm_allocate_bearer(&g_ctx.state, &g_ctx.config, req_bw_fl,
                                  req_bw_rl, cos_id, &bearer_id);

    if (ret == 0) {
      confirm.status = STATUS_SUCCESS;
      confirm.has_bearer_id = true;
      confirm.bearer_identifier = bearer_id;
      printf("  - 分配 Bearer ID: %u (FL:%u/RL:%u kbps)\n", bearer_id,
             req_bw_fl, req_bw_rl);
    } else {
      confirm.status = STATUS_INSUFFICIENT_RESOURCES;
      printf("  - 资源不足\n");
    }
  } else {
    if (req->has_bearer_id) {
      int ret = dlm_release_bearer(&g_ctx.state, req->bearer_identifier);
      confirm.status = (ret == 0) ? STATUS_SUCCESS : STATUS_INVALID_BEARER;
      printf("  - 释放 Bearer ID: %u, 结果: %s\n", req->bearer_identifier,
             status_to_string(confirm.status));
    } else {
      confirm.status = STATUS_INVALID_BEARER;
    }
  }

  return dlm_send_mih_message(MIH_LINK_RESOURCE_CNF, &confirm, sizeof(confirm));
}

static int dlm_handle_capability_discover(const LINK_Capability_Discover_Request *req) {
  printf("[SATCOM-PRIM] 处理 Link_Capability_Discover.request\n");

  LINK_Capability_Discover_Confirm confirm;
  memset(&confirm, 0, sizeof(confirm));

  if (req) {
    confirm.link_identifier = req->link_identifier;
  } else {
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    strncpy(confirm.link_identifier.link_addr, g_ctx.config.link_name,
            sizeof(confirm.link_identifier.link_addr) - 1);
  }

  confirm.status = STATUS_SUCCESS;
  confirm.has_capability = true;
  confirm.capability.supported_events = LINK_EVENT_ALL;
  confirm.capability.supported_commands = 0;
  confirm.capability.max_bandwidth_kbps = g_ctx.config.max_bandwidth_fl;
  confirm.capability.typical_latency_ms = g_ctx.config.reported_delay_ms;
  confirm.capability.link_type = g_ctx.config.link_id;
  confirm.capability.security_level = g_ctx.config.security_level;
  confirm.capability.mtu = g_ctx.config.mtu;
  confirm.capability.is_asymmetric = g_ctx.config.is_asymmetric;

  return dlm_send_mih_message(MIH_LINK_CAPABILITY_DISCOVER_CNF, &confirm,
                              sizeof(confirm));
}

static int dlm_handle_get_parameters(const LINK_Get_Parameters_Request *req) {
  printf("[SATCOM-PRIM] 处理 Link_Get_Parameters.request\n");

  LINK_Get_Parameters_Confirm confirm;
  memset(&confirm, 0, sizeof(confirm));

  if (req) {
    confirm.link_identifier = req->link_identifier;
    confirm.returned_params = req->param_type_list;
  } else {
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    strncpy(confirm.link_identifier.link_addr, g_ctx.config.link_name,
            sizeof(confirm.link_identifier.link_addr) - 1);
    confirm.returned_params = LINK_PARAM_QUERY_ALL;
  }

  pthread_mutex_lock(&g_ctx.state.mutex);
  confirm.status = STATUS_SUCCESS;
  confirm.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;
  confirm.parameters.signal_quality = g_ctx.state.signal_quality;
  confirm.parameters.current_latency_ms = g_ctx.config.reported_delay_ms;
  confirm.parameters.current_jitter_ms = g_ctx.config.delay_jitter_ms;
  confirm.parameters.current_rx_rate_kbps =
      g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
  confirm.parameters.current_tx_rate_kbps =
      g_ctx.config.max_bandwidth_rl - g_ctx.state.current_usage_rl;
  confirm.parameters.available_bandwidth_kbps =
      g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;
  confirm.parameters.link_state = g_ctx.state.is_connected ? 1 : 0;
  confirm.parameters.active_bearers = g_ctx.state.num_active_bearers;
  pthread_mutex_unlock(&g_ctx.state.mutex);

  /* IP config (best-effort): reuse configured values if available */
  inet_pton(AF_INET, g_net_config.ip_address, &confirm.parameters.ip_address);
  inet_pton(AF_INET, g_net_config.netmask, &confirm.parameters.netmask);
  inet_pton(AF_INET, g_net_config.gateway, &confirm.parameters.gateway);

  return dlm_send_mih_message(MIH_LINK_GET_PARAMETERS_CNF, &confirm,
                              sizeof(confirm));
}

static int dlm_handle_event_subscribe(const LINK_Event_Subscribe_Request *req) {
  printf("[SATCOM-PRIM] 处理 Link_Event_Subscribe.request\n");

  LINK_Event_Subscribe_Confirm confirm;
  memset(&confirm, 0, sizeof(confirm));

  if (req) {
    confirm.link_identifier = req->link_identifier;
  } else {
    confirm.link_identifier.link_type = g_ctx.config.link_id;
    strncpy(confirm.link_identifier.link_addr, g_ctx.config.link_name,
            sizeof(confirm.link_identifier.link_addr) - 1);
  }

  pthread_mutex_lock(&g_ctx.state.mutex);
  if (req) {
    g_ctx.state.subscribed_events |= req->event_list;
    confirm.subscribed_events = req->event_list;
  } else {
    /* If request missing, conservatively subscribe none */
    confirm.subscribed_events = 0;
  }
  pthread_mutex_unlock(&g_ctx.state.mutex);

  confirm.status = STATUS_SUCCESS;
  return dlm_send_mih_message(MIH_LINK_EVENT_SUBSCRIBE_CNF, &confirm,
                              sizeof(confirm));
}

/**
 * @brief 发送链路参数报告 (标准 IEEE 802.21 格式)
 *
 * 定期向 LMI 服务器汇报卫星链路的状态，包括当前的 RSSI、可用带宽和延迟。
 */
static int dlm_send_parameters_report(void) {
  pthread_mutex_lock(&g_ctx.state.mutex);
  if (!g_ctx.state.is_connected) {
    pthread_mutex_unlock(&g_ctx.state.mutex);
    return 0;
  }

  /* 构造标准 MIH 链路参数报告指示结构体 */
  LINK_Parameters_Report_Indication ind;
  memset(&ind, 0, sizeof(ind));

  /* 1. 设置链路标识符 */
  ind.link_identifier.link_type = g_ctx.config.link_id; /* 链路类型 (SATCOM) */
  strncpy(ind.link_identifier.link_addr, g_ctx.config.link_name,
          sizeof(ind.link_identifier.link_addr) - 1); /* 链路名称 */

  /* 2. 设置参数位图 (标记所有参数均为最新) */
  ind.changed_params = 0xFFFF; // 报告所有支持的参数

  /* 3. 填充实时链路参数 */
  /* 计算可用带宽 (FL) */
  ind.parameters.available_bandwidth_kbps =
      g_ctx.config.max_bandwidth_fl - g_ctx.state.current_usage_fl;

  /* 卫星链路延迟通常较高且固定 */
  ind.parameters.current_latency_ms = g_ctx.config.reported_delay_ms;

  /* 卫星信号强度 (模拟值) */
  ind.parameters.signal_strength_dbm = g_ctx.state.simulated_rssi;

  /* 信号质量百分比 */
  ind.parameters.signal_quality = g_ctx.state.signal_quality;

  /* 链路始终处于在线状态 (Up) */
  ind.parameters.link_state = 1;

  /* 填充 IP 配置 */
  inet_pton(AF_INET, g_net_config.ip_address, &ind.parameters.ip_address);
  inet_pton(AF_INET, g_net_config.netmask, &ind.parameters.netmask);
  inet_pton(AF_INET, g_net_config.gateway, &ind.parameters.gateway);

  /* 获取当前时间戳 */
  ind.report_timestamp = (uint32_t)time(NULL);

  pthread_mutex_unlock(&g_ctx.state.mutex);

  /* 本地输出用于调试 */
  printf("[SATCOM-IND] Parameters Report: RSSI=%d dBm, BW=%u kbps\n",
         ind.parameters.signal_strength_dbm,
         ind.parameters.available_bandwidth_kbps);

  /* 发送标准 MIH 原语消息 (0x0205) */
  return dlm_send_mih_message(MIH_LINK_PARAMETERS_REPORT_IND, &ind,
                              sizeof(ind));
}

static void *dlm_message_receiver_thread(void *arg) {
  uint8_t recv_buf[4096];
  printf("[SATCOM-THR] Receiver Thread started\n");

  while (g_running) {
    ssize_t n = recv(g_ctx.socket_fd, recv_buf, sizeof(mih_transport_header_t),
                     MSG_WAITALL);
    if (n <= 0) {
      if (n == 0) {
        fprintf(stderr, "[SATCOM] Server closed connection cleanly\n");
      } else {
        fprintf(stderr, "[SATCOM] recv() error: %s (errno=%d)\n", strerror(errno), errno);
      }
      g_running = 0;
      break;
    }

    mih_transport_header_t *hdr = (mih_transport_header_t *)recv_buf;
    uint16_t payload_len = hdr->message_length - sizeof(mih_transport_header_t);

    if (payload_len > 0) {
      n = recv(g_ctx.socket_fd, recv_buf + sizeof(mih_transport_header_t),
               payload_len, MSG_WAITALL);
      if (n <= 0) {
        fprintf(stderr, "[SATCOM] recv() payload error: %s\n", strerror(errno));
        break;
      }
    }

    uint8_t *payload = recv_buf + sizeof(mih_transport_header_t);

    switch (hdr->primitive_type) {
    case MIH_EXT_LINK_REGISTER_CONFIRM:
      /* 收到注册确认 (Transaction ID 用于匹配) */
      printf("[SATCOM-RX] Received Register Confirm (ID=%u, payload_len=%u)\n",
             hdr->transaction_id, payload_len);
      if (payload_len > 0) {
        printf("[SATCOM-DEBUG] Register response has %u bytes payload\n", payload_len);
      }
      /* 注册成功后检查接口状态 */
      if (check_interface_status(g_ctx.config.interface_name)) {
        printf("[SATCOM] Interface %s is UP, sending Link_Up_Indication\n",
               g_ctx.config.interface_name);
        dlm_physical_link_up();
      }
      break;

    case MIH_LINK_RESOURCE_REQ:
      /* 收到资源请求原语 (如由 CM 请求分配新的 Bearer) */
      if (payload_len >= sizeof(LINK_Resource_Request)) {
        dlm_handle_link_resource((LINK_Resource_Request *)payload);
      }
      break;

    case MIH_EXT_HEARTBEAT_ACK:
      /* 收到心跳确认包，证明 LMI 服务器存活 */
      break;

    case MIH_LINK_CAPABILITY_DISCOVER_REQ:
      /* 链路能力发现请求 (ARINC 839 标准原语) */
      if (payload_len >= sizeof(LINK_Capability_Discover_Request)) {
        dlm_handle_capability_discover((const LINK_Capability_Discover_Request *)payload);
      } else {
        dlm_handle_capability_discover(NULL);
      }
      break;

    case MIH_LINK_GET_PARAMETERS_REQ:
      /* 链路参数查询请求 */
      if (payload_len >= sizeof(LINK_Get_Parameters_Request)) {
        dlm_handle_get_parameters((const LINK_Get_Parameters_Request *)payload);
      } else {
        dlm_handle_get_parameters(NULL);
      }
      break;

    case MIH_LINK_EVENT_SUBSCRIBE_REQ:
      /* 事件订阅请求 */
      if (payload_len >= sizeof(LINK_Event_Subscribe_Request)) {
        dlm_handle_event_subscribe((const LINK_Event_Subscribe_Request *)payload);
      } else {
        dlm_handle_event_subscribe(NULL);
      }
      break;

    default:
      /* 捕获未定义的其他 MIH 原语 */
      printf("[SATCOM-RX] Received Unknown Primitive: 0x%04X\n",
             hdr->primitive_type);
      break;
    }
  }
  return NULL;
}

static void *dlm_reporting_thread(void *arg) {
  (void)arg;
  printf("[SATCOM-THR] Reporting Thread started\n");
  bool prev_iface_up = false;

  while (g_running) {
    sleep(1);

    /* 检查网卡实际物理状态 */
    bool curr_iface_up = check_interface_status(g_ctx.config.interface_name);

    if (prev_iface_up && !curr_iface_up) {
      /* Link Down Detected */
      printf("[SATCOM-LINK] Interface DOWN detected: %s\n",
             g_ctx.config.interface_name);
      dlm_physical_link_down(LINK_DOWN_REASON_FAILURE);
    } else if (!prev_iface_up && curr_iface_up) {
      /* Link Up Detected */
      printf("[SATCOM-LINK] Interface UP detected: %s\n",
             g_ctx.config.interface_name);
      pthread_mutex_lock(&g_ctx.state.mutex);
      bool is_active = g_ctx.state.is_connected;
      pthread_mutex_unlock(&g_ctx.state.mutex);

      if (!is_active) {
        dlm_physical_link_up();
      }
    }

    prev_iface_up = curr_iface_up;

    /* 心跳 & 报告 (仅在链路已连接时发送) */
    pthread_mutex_lock(&g_ctx.state.mutex);
    bool is_connected = g_ctx.state.is_connected;
    pthread_mutex_unlock(&g_ctx.state.mutex);

    if (is_connected) {
      dlm_send_heartbeat();

      static int report_counter = 0;
      if (++report_counter >= g_ctx.config.reporting_interval_sec) {
        report_counter = 0;
        dlm_send_parameters_report();
      }
    }
  }
  return NULL;
}

/* ============================================================================
 * Main
 * ============================================================================
 */

static void dlm_signal_handler(int sig) { g_running = 0; }

int main(int argc, char **argv) {
  const char *config_path = DEFAULT_CONFIG_PATH;
  if (argc > 1 && argv[1] && argv[1][0] != '\0') {
    config_path = argv[1];
  }

  printf("========================================\n");
  printf("SATCOM DLM 标准版 v2.0\n");
  printf("ARINC 839-2014 MIH 协议实现 (Standard MIH)\n");
  printf("配置文件: %s\n", config_path);
  printf("========================================\n\n");

  signal(SIGINT, dlm_signal_handler);
  signal(SIGTERM, dlm_signal_handler);

  dlm_init_config_manager(config_path);
  dlm_init_state_simulator();

  if (dlm_init_socket() != 0)
    return -1;
  if (dlm_connect_to_server() != 0) {
    fprintf(stderr, "[SATCOM] Failed to connect to Standard MIH Server. Is "
                    "app_magic running?\n");
    return -1;
  }

  /* 启动线程 */
  pthread_t rpt_thread, rx_thread, pkt_thread;
  pthread_create(&rpt_thread, NULL, dlm_reporting_thread, NULL);
  pthread_create(&rx_thread, NULL, dlm_message_receiver_thread, NULL);
  pthread_create(&pkt_thread, NULL, dlm_packet_monitor_thread, NULL);

  /* 主线程等待 */
  pthread_join(rx_thread, NULL);
  pthread_join(rpt_thread, NULL);
  pthread_join(pkt_thread, NULL);

  close(g_ctx.socket_fd);
  printf("[SATCOM] Terminated.\n");
  return 0;
}
