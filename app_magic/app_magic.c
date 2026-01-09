/**
 * @file app_magic.c
 * @brief MAGIC (Multi-link Aggregation Gateway for Internet Connectivity)
 * Extension
 * @description
 *   统一的 MAGIC 扩展模块，集成：
 *   1. CIC (Client Interface Component) - 客户端认证和会话管理
 *   2. Policy Engine - 策略决策引擎
 *   3. LMI - Link Management Interface (ARINC 839)
 *   4. XML Config Parser - 配置文件解析
 *
 * @version 2.1
 * @date 2025-12-02
 */

#include "app_magic.h"
#include "magic_cic_push.h"
#include <freeDiameter/extension.h>
#include <signal.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

/**
 * @brief MAGIC 核心进程间通信用的 Unix Domain Socket 路径。
 */
#define MAGIC_CORE_SOCKET_PATH "/tmp/magic_core.sock"

/*===========================================================================
 * 全局变量
 *===========================================================================*/

/**
 * @brief 存储 MAGIC 扩展所有运行时状态的全局上下文对象。
 * @note 线程安全性：受各子模块内部的 Mutex 保护。
 */
MagicContext g_magic_ctx;

/*===========================================================================
 * LMI→MSCR 桥接回调 (v2.1: 链路事件自动触发 MSCR 广播)
 *===========================================================================*/

/**
 * @brief LMI 链路事件回调 - 桥接到 MSCR 广播。
 * @details 当 DLM (Data Link Manager) 报告链路状态变化（如 UP/DOWN）时，
 *          该回调函数会被触发，并自动通过 CIC 模块触发 MSCR
 * 广播通知所有订阅的客户端。
 *
 * @param[in,out] lmi_ctx LMI 上下文指针。
 * @param[in]     link_id 发生状态变化的链路 ID（UTF-8 字符串）。
 * @param[in]     event_type 事件类型 (LINK_EVENT_UP 或 LINK_EVENT_DOWN)。
 * @param[in]     event_data 事件附带的数据指针。
 *
 * @return void
 *
 * @note 该函数会被 LMI 接收线程调用，因此必须保证重入安全性。
 * @warning link_id 必须非空，否则会直接返回。
 */
static void on_lmi_link_event_for_mscr(struct MagicLmiContext *lmi_ctx,
                                       const char *link_id, uint16_t event_type,
                                       const void *event_data) {
  (void)lmi_ctx;
  (void)event_data;

  if (!link_id) {
    return;
  }

  bool is_up = (event_type == LINK_EVENT_UP);

  fd_log_notice("[MAGIC] LMI→MSCR bridge: Link %s event %s", link_id,
                is_up ? "UP" : "DOWN");

  /* 调用 CIC 推送模块触发 MSCR 广播 */
  magic_cic_on_link_status_change(&g_magic_ctx, link_id, is_up);
}

/*===========================================================================
 * 信号处理
 *===========================================================================*/

/**
 * @brief MAGIC 扩展的信号处理函数。
 * @details 处理 SIGINT 和 SIGTERM 信号，用于优雅地停止扩展运行。通过设置
 * g_magic_ctx.running 实现异步退出通知。
 *
 * @param[in] signum 接收到的信号编号。
 *
 * @return void
 *
 * @note 这是一个异步信号处理函数，仅安全地修改标志位。
 */
static void magic_signal_handler(int signum) {
  (void)signum;
  fd_log_notice("[MAGIC] Received shutdown signal");
  g_magic_ctx.running = false;
}

/*===========================================================================
 * 模块初始化
 *===========================================================================*/

/**
 * @brief MAGIC 扩展的入口初始化函数。
 * @details 该函数在 freeDiameter 加载扩展时被调用。
 *          逻辑流程：
 *          1. 清零全局上下文，初始化基本控制标志。
 *          2. 从 XML 文件并行加载 Datalink、Policy 和 Client 配置。
 *          3. 初始化策略引擎、LMI 管理器、会话管理器。
 *          4. 配置并初始化数据平面 (NAPT/Routing) 和流量统计后台。
 *          5. 连接外部 ADIF 飞机数据源。
 *          6. 注册 Diameter 应用及其对应的消息处理器。
 *          7. 设置信号捕捉，确保资源在程序退出时能被正确回收。
 *
 * @param[in] conffile 配置文件基准路径，若为 NULL 则默认为编译预设路径。
 *
 * @return int 成功返回 0；失败返回具体的错误码（如 EINVAL, ENOMEM）。
 *
 * @warning 若初始化过程中任一关键模块失败，将回滚已完成的部分并终止加载。
 */
static int magic_entry(char *conffile) {
  int ret;

  TRACE_ENTRY("%s", "MAGIC Extension Initializing");

  fd_log_notice("========================================");
  fd_log_notice("  MAGIC Extension v2.0");
  fd_log_notice("  Unified ARINC 839-2014 Implementation");
  fd_log_notice("========================================");

  memset(&g_magic_ctx, 0, sizeof(g_magic_ctx));
  g_magic_ctx.running = true;

  /* ========================================
   * 步骤 1: 加载 XML 配置文件
   * ======================================== */

  // 使用传入的配置路径，若未指定则使用默认路径
  const char *config_base =
      conffile ? conffile
               : "/home/zhuwuhui/freeDiameter/extensions/app_magic/config";

  fd_log_notice("[MAGIC] Loading configuration files from: %s", config_base);

  // 加载数据链路配置
  ret = magic_config_load_datalinks(&g_magic_ctx.config, config_base);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to load Datalink_Profile.xml");
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Loaded %u DLM configs (v2.0)",
                g_magic_ctx.config.num_dlm_configs);

  // 加载策略配置
  ret = magic_config_load_policy(&g_magic_ctx.config, config_base);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to load Central_Policy_Profile.xml");
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Loaded policy configuration");

  // 加载客户端配置
  ret = magic_config_load_clients(&g_magic_ctx.config, config_base);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to load Client_Profile.xml");
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Loaded %u client profiles",
                g_magic_ctx.config.num_clients);

  /* ========================================
   * 步骤 2: 初始化策略引擎
   * ======================================== */

  ret = magic_policy_init(&g_magic_ctx.policy_ctx, &g_magic_ctx.config);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to initialize policy engine");
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Policy engine initialized");

  /* ========================================
   * 步骤 3: 初始化 LMI 接口
   * ======================================== */

  ret = magic_lmi_init(&g_magic_ctx.lmi_ctx);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to initialize LMI interface");
    magic_policy_cleanup(&g_magic_ctx.policy_ctx);
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ LMI interface initialized");

  /* v2.1: 将 LMI 上下文传递给策略引擎用于负载均衡 */
  g_magic_ctx.policy_ctx.lmi_ctx = &g_magic_ctx.lmi_ctx;

  /* ========================================
   * 步骤 4: 初始化会话管理器
   * ======================================== */

  ret = magic_session_init(&g_magic_ctx.session_mgr);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to initialize session manager");
    magic_lmi_cleanup(&g_magic_ctx.lmi_ctx);
    magic_policy_cleanup(&g_magic_ctx.policy_ctx);
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Session manager initialized");

  /* ========================================
   * 步骤 5: 初始化数据平面路由模块
   * ======================================== */

  /* 配置入口接口和 IP (客户端连接的接口) */
  /* 已根据当前系统接口映射调整为 ens39 (192.168.126.1) */
  const char *ingress_interface = "ens39";
  const char *ingress_ip = "192.168.126.1";

  ret = magic_dataplane_init(&g_magic_ctx.dataplane_ctx, ingress_interface,
                             ingress_ip);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to initialize dataplane");
    magic_session_cleanup(&g_magic_ctx.session_mgr);
    magic_lmi_cleanup(&g_magic_ctx.lmi_ctx);
    magic_policy_cleanup(&g_magic_ctx.policy_ctx);
    return EINVAL;
  }
  fd_log_notice("[MAGIC] ✓ Dataplane initialized (ingress: %s %s)",
                ingress_interface, ingress_ip);

  /* ========================================
   * 步骤 5b: 初始化流量监控模块 (v2.1)
   * ======================================== */

  ret = traffic_monitor_init(&g_magic_ctx.traffic_ctx, TRAFFIC_BACKEND_AUTO);
  if (ret < 0) {
    fd_log_notice(
        "[MAGIC] ⚠ Traffic monitor init failed (CDR will use cached values)");
    // 不返回错误，流量监控是可选功能
  } else {
    fd_log_notice("[MAGIC] ✓ Traffic monitor initialized (backend: %s)",
                  g_magic_ctx.traffic_ctx.backend == TRAFFIC_BACKEND_NFTABLES
                      ? "nftables"
                      : "iptables");
  }

  /* ========================================
   * 步骤 5c: 初始化 CDR 管理器 (v2.2)
   * ======================================== */

  ret = cdr_manager_init(&g_magic_ctx.cdr_mgr, NULL,
                         0); /* 使用默认路径和保留时间 */
  if (ret < 0) {
    fd_log_notice(
        "[MAGIC] ⚠ CDR manager init failed (accounting control disabled)");
    // 不返回错误，CDR 管理是可选功能
  } else {
    fd_log_notice(
        "[MAGIC] ✓ CDR manager initialized (dir=%s, retention=%u hours)",
        g_magic_ctx.cdr_mgr.base_dir, g_magic_ctx.cdr_mgr.retention_sec / 3600);
  }

  /* 注册 DLM 到数据平面 - v2.0: 使用 Socket 路径 */
  for (uint32_t i = 0; i < g_magic_ctx.config.num_dlm_configs; i++) {
    DLMConfig *dlm = &g_magic_ctx.config.dlm_configs[i];
    /* v2.0: DLM 通过 Unix Socket 通信，不需要网络接口注册 */
    /* 数据平面路由由 DLM 动态通知，此处仅记录 DLM 信息 */
    fd_log_notice("[MAGIC] ✓ DLM %s: Socket=%s, BW=%.0f/%.0f kbps",
                  dlm->dlm_name, dlm->dlm_socket_path, dlm->max_forward_bw_kbps,
                  dlm->max_return_bw_kbps);
  }

  /* ========================================
   * 步骤 6: 启动 LMI 服务器
   * ======================================== */

  ret = magic_lmi_start_server(&g_magic_ctx.lmi_ctx, &g_magic_ctx.config);
  if (ret < 0) {
    fd_log_notice("[MAGIC] ⚠ LMI server start failed (may not be needed)");
    // 不返回错误，允许无 DLM 模式运行
  } else {
    fd_log_notice("[MAGIC] ✓ LMI server started on /tmp/magic_core.sock");
  }

  /* ========================================
   * 步骤 6b: 启动 LMI Datagram 服务器 (用于 DLM 原型)
   * ======================================== */

  ret = magic_lmi_start_dgram_server(&g_magic_ctx.lmi_ctx, NULL);
  if (ret < 0) {
    fd_log_notice("[MAGIC] ⚠ LMI DGRAM server start failed (DLM prototypes "
                  "won't connect)");
    // 不返回错误，允许无 DLM 原型模式运行
  } else {
    fd_log_notice("[MAGIC] ✓ LMI DGRAM server started on /tmp/mihf.sock (for "
                  "DLM prototypes)");
  }

  /* ========================================
   * 步骤 6b2: 注册 LMI→MSCR 桥接回调 (v2.1)
   * ======================================== */

  magic_lmi_register_event_callback(&g_magic_ctx.lmi_ctx, LINK_EVENT_UP,
                                    on_lmi_link_event_for_mscr);
  magic_lmi_register_event_callback(&g_magic_ctx.lmi_ctx, LINK_EVENT_DOWN,
                                    on_lmi_link_event_for_mscr);
  fd_log_notice("[MAGIC] ✓ LMI→MSCR bridge callbacks registered");

  /* ========================================
   * 步骤 6c: 初始化 ADIF 客户端 (飞机数据接口)
   * ======================================== */

  AdifClientConfig adif_config = {.server_port = ADIF_DEFAULT_SERVER_PORT,
                                  .async_port = ADIF_DEFAULT_ASYNC_PORT,
                                  .refresh_period_ms = ADIF_DEFAULT_REFRESH_MS,
                                  .auto_reconnect = true,
                                  .reconnect_interval_ms = 5000};
  strncpy(adif_config.server_host, "127.0.0.1",
          sizeof(adif_config.server_host) - 1);

  ret = adif_client_init(&g_magic_ctx.adif_ctx, &adif_config);
  if (ret == 0) {
    fd_log_notice("[MAGIC] ✓ ADIF client initialized (server=%s:%u)",
                  adif_config.server_host, adif_config.server_port);

    /* 注册 ADIF 状态变化回调函数 (动态策略调整) */
    extern void on_adif_state_changed(const AdifAircraftState *state,
                                      void *user_data);
    adif_client_set_callback(&g_magic_ctx.adif_ctx, on_adif_state_changed,
                             &g_magic_ctx);
    fd_log_notice("[MAGIC] ✓ ADIF state change callback registered");

    /* 尝试连接到 ADIF 服务器 */
    ret = adif_client_connect(&g_magic_ctx.adif_ctx);
    if (ret == 0) {
      fd_log_notice("[MAGIC] ✓ ADIF client connected and subscribed");
    } else {
      fd_log_notice("[MAGIC] ⚠ ADIF server not available (run adif_simulator "
                    "for testing)");
      /* 不返回错误，允许无 ADIF 服务模式运行 */
    }
  } else {
    fd_log_notice("[MAGIC] ⚠ ADIF client init failed");
  }

  /* ========================================
   * 步骤 7: 注册 Diameter 应用和命令处理器
   * ======================================== */

  ret = magic_cic_init(&g_magic_ctx);
  if (ret < 0) {
    fd_log_error("[MAGIC] Failed to initialize CIC handlers");
    magic_dataplane_cleanup(&g_magic_ctx.dataplane_ctx);
    magic_lmi_cleanup(&g_magic_ctx.lmi_ctx);
    magic_policy_cleanup(&g_magic_ctx.policy_ctx);
    return EINVAL;
  }

  /* ========================================
   * 步骤 8: 设置信号处理
   * ======================================== */

  signal(SIGINT, magic_signal_handler);
  signal(SIGTERM, magic_signal_handler);

  fd_log_notice("========================================");
  fd_log_notice("  MAGIC Extension Ready");
  fd_log_notice("========================================");
  fd_log_notice("  DLM Configs: %u configured (v2.0)",
                g_magic_ctx.config.num_dlm_configs);
  fd_log_notice("  Clients:    %u configured", g_magic_ctx.config.num_clients);
  fd_log_notice("  LMI Server: %s",
                g_magic_ctx.lmi_ctx.server_fd >= 0 ? "Running" : "Disabled");
  fd_log_notice("  Dataplane:  %s", g_magic_ctx.dataplane_ctx.is_initialized
                                        ? "Enabled"
                                        : "Disabled");
  fd_log_notice("  ADIF:       %s",
                adif_client_is_connected(&g_magic_ctx.adif_ctx) ? "Connected"
                                                                : "Standalone");
  fd_log_notice("========================================\n");

  return 0;
}

/**
 * @brief MAGIC 扩展的卸载清理函数。
 * @details 在 freeDiameter 卸载扩展或主进程正常退出时被触发。
 *          按照依赖关系的逆序依次清理所有持有的资源、线程和文件句柄。
 *          顺序如下：CIC -> TrafficMonitor -> CDR -> Dataplane -> Session ->
 * LMI -> Policy -> ADIF -> Config。
 *
 * @return void
 *
 * @warning 必须确保此函数执行完毕，否则可能导致内核 netfilter 表项、Unix Socket
 * 文件等残留。
 */
void fd_ext_fini(void) {
  fd_log_notice("[MAGIC] Extension unloading...");

  // 清理各个组件
  magic_cic_cleanup(&g_magic_ctx);
  traffic_monitor_cleanup(&g_magic_ctx.traffic_ctx); /* v2.1: 清理流量监控 */
  cdr_manager_cleanup(&g_magic_ctx.cdr_mgr);         /* v2.2: 清理 CDR 管理器 */
  magic_dataplane_cleanup(&g_magic_ctx.dataplane_ctx);
  magic_session_cleanup(&g_magic_ctx.session_mgr);
  magic_lmi_cleanup(&g_magic_ctx.lmi_ctx);
  magic_policy_cleanup(&g_magic_ctx.policy_ctx);
  adif_client_cleanup(&g_magic_ctx.adif_ctx);
  magic_config_cleanup(&g_magic_ctx.config);

  fd_log_notice("[MAGIC] Extension unloaded");
}

/* 注册扩展 */
EXTENSION_ENTRY("app_magic", magic_entry);
