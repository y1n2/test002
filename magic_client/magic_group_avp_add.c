/**
 * @file magic_group_avp_add.c
 * @brief ARINC 839-2014 MAGIC 协议 Grouped AVP 构造具体实现。
 * @details 本文件采用「超级宏 + 结构化封装」的设计风格，实现了 ARINC 839
 * 协议规定的 全部 19 种 Grouped AVP 的构造逻辑。通过引用 `add_avp.h` 中定义的
 *          `ADD_GROUPED`、`S_STR`、`S_U32` 等宏，极大地简化了代码编写，
 *          并确保了在 AVP 创建失败时的内存安全（自动清理资源）。
 *
 *          主要功能模块包括：
 *          - 客户端认证凭据 (Client-Credentials)
 *          - 通信请求/应答/上报参数
 * (Communication-Request/Answer/Report-Parameters)
 *          - 流量过滤模板 TFT (TFTtoGround-List, TFTtoAircraft-List)
 *          - 网络地址端口转换 NAPT (NAPT-List)
 *          - 数据链路模块状态 (DLM-Info, DLM-QoS-Level-List, Link-Status-Group)
 *          - 计费记录管理 (CDRs-Active, CDRs-Finished, CDRs-Forwarded,
 * CDRs-Unknown, CDRs-Updated)
 *
 * @note 本文件所有函数均依赖于全局配置 `g_cfg` 和全局字典句柄
 * `g_magic_dict`、`g_std_dict`。
 *       这些全局变量必须在调用本文件中任何函数之前完成初始化。
 *
 * @warning 本文件中的函数不是线程安全的。全局配置 `g_cfg`
 * 应在单线程环境下（通常是主线程） 解析完成后，再供其他模块只读访问。
 */

#include <ctype.h>
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stdio.h>
#include <string.h>

#include "add_avp.h"             ///< 包含 ADD_GROUPED 宏和 S_STR / S_U32 等子宏
#include "config.h"              ///< 全局配置结构体 g_cfg
#include "log.h"                 ///< 日志宏 (LOG_D, LOG_I, LOG_E 等)
#include "magic_dict_handles.h"  ///< 包含所有 MAGIC 协议字典对象句柄
#include "magic_group_avp_add.h" ///< 类型定义与函数声明

/* ================================================================== */
/*                        外部全局变量声明                              */
/* ================================================================== */

/**
 * @brief 程序全局配置实例。
 * @details 存储从配置文件（如 `EFB_magic.conf`）解析得到的所有客户端参数。
 *          在 `config.c` 的 `magic_conf_parse()` 函数中完成初始化。
 * @note
 * 线程安全性：本变量在程序启动时由主线程单次写入，之后作为只读数据供所有模块访问。
 *       在多线程环境下无需加锁保护。
 */
extern app_config_t g_cfg;

/**
 * @brief MAGIC 协议自定义 AVP 和命令的字典句柄集合。
 * @details 包含所有 Vendor-ID=13712 的 AVP 和 MAGIC 应用命令的 `struct
 * dict_object*` 指针。 在 `magic_dict_handles.c` 的 `magic_dict_init()`
 * 函数中完成初始化。
 * @note 线程安全性：本变量在 freeDiameter
 * 核心启动后由主线程单次写入，之后作为只读数据访问。
 */
extern struct magic_dict_handles g_magic_dict;

/**
 * @brief 标准 Diameter Base Protocol AVP 的字典句柄集合。
 * @details 包含 RFC 6733 定义的核心 AVP（如 Session-Id, Origin-Host）的 `struct
 * dict_object*` 指针。 与 `g_magic_dict` 同时在 `magic_dict_init()`
 * 中完成初始化。
 * @note 线程安全性：同 `g_magic_dict`，只读访问，无需加锁。
 */
extern struct std_diam_dict_handles g_std_dict;

/**
 * @def MAGIC_VENDOR_ID
 * @brief ARINC/AEEC 官方分配的 Diameter Vendor-ID。
 * @details 值为 13712。所有 MAGIC 协议自定义 AVP 均携带此 Vendor-ID，
 *          并自动在 AVP 头部设置 'V' (Vendor-Specific) 标志位。
 */
#define MAGIC_VENDOR_ID 13712

/* ================================================================== */
/*                        内部辅助函数前向声明                          */
/* ================================================================== */

/**
 * @brief 将 TFT-to-Ground 规则列表添加到指定的父 AVP 中。
 * @param parent_avp 目标父 AVP（通常是 Communication-Request-Parameters）。
 * @return int 成功返回 0，失败返回 -1。
 */
static int add_tft_to_ground_list_to_avp(struct avp *parent_avp);

/**
 * @brief 将 TFT-to-Aircraft 规则列表添加到指定的父 AVP 中。
 * @param parent_avp 目标父 AVP。
 * @return int 成功返回 0，失败返回 -1。
 */
static int add_tft_to_aircraft_list_to_avp(struct avp *parent_avp);

/**
 * @brief 将 NAPT 规则列表添加到消息根级别。
 * @param parent 目标 Diameter 消息对象。
 * @return int 始终返回 0。
 */
static int add_napt_list(struct msg *parent);

/* ================================================================== */
/*                  1. Client-Credentials (Code 20019)                  */
/* ================================================================== */

/**
 * @brief 添加 Client-Credentials Grouped AVP (Code 20019) 到消息。
 * @details 该 AVP 用于 MCAR (MAGIC-Client-Authentication-Request) 消息，
 *          携带客户端的身份认证凭据。
 *
 *          内部结构遵循 ARINC 839 规范：
 *          - User-Name (1): 标准 Diameter AVP，必填，从 `g_cfg.username` 读取。
 *          - Client-Password (10001): MAGIC 自定义 AVP，必填，从
 * `g_cfg.client_password` 读取。
 *          - Server-Password (10045): MAGIC 自定义 AVP，可选，从
 * `g_cfg.server_password` 读取。
 *
 * @param msg 目标 Diameter 消息对象（必须是 MCAR 请求消息）。
 *
 * @return int 成功返回 0。
 * @retval -1 如果 `g_cfg.username` 或 `g_cfg.client_password` 为空（未配置）。
 *
 * @warning 调用前必须确保 `g_cfg` 已通过 `magic_conf_parse()` 正确初始化。
 *          否则会因缺少必填字段而返回错误。
 *
 * @note 此函数使用 `ADD_GROUPED` 宏，宏内部会自动处理 AVP
 * 创建失败时的资源清理。
 */
int add_client_credentials(struct msg *msg) {
  LOG_D("[DEBUG] === add_client_credentials 开始 ===");
  LOG_D("[DEBUG] msg指针: %p", msg);

  /* ---------- 1. 检查 User-Name 是否配置 ---------- */
  if (g_cfg.username[0] == '\0') {
    LOG_E("[MAGIC] add_client_credentials 失败：User-Name 未配置！"
          "该字段在实际部署中为强制项，请在 magic.conf 中添加 USERNAME = xxx");
    return -1;
  }

  /* ---------- 2. 检查 Client-Password 是否配置 ---------- */
  if (g_cfg.client_password[0] == '\0') {
    LOG_E("[MAGIC] add_client_credentials 失败：Client-Password 未配置！"
          "该字段为协议强制项，请在 magic.conf 中添加 CLIENT_PASSWORD = xxx");
    return -1;
  }

  /* ---------- 3. 构造 Client-Credentials Grouped AVP ---------- */
  LOG_D("[DEBUG] 开始构造 Client-Credentials, model=%p",
        g_magic_dict.avp_client_credentials);
  LOG_D("[DEBUG] User-Name model=%p, username=%s", g_std_dict.avp_user_name,
        g_cfg.username);
  LOG_D("[DEBUG] Client-Password model=%p", g_magic_dict.avp_client_password);

  ADD_GROUPED(msg, g_magic_dict.avp_client_credentials, {
    /* 必填字段1：用户名（标准 AVP，Vendor=0） */
    LOG_D("[DEBUG] 准备添加 User-Name...");
    S_STD_STR(g_std_dict.avp_user_name, g_cfg.username);
    LOG_D("[DEBUG] User-Name 添加成功");

    /* 必填字段2：客户端密码（MAGIC 厂商 AVP，Vendor=13712） */
    LOG_D("[DEBUG] 准备添加 Client-Password...");
    S_STR(g_magic_dict.avp_client_password, g_cfg.client_password);
    LOG_D("[DEBUG] Client-Password 添加成功");

    /* 可选字段：服务器密码（MAGIC 厂商 AVP，Vendor=13712） */
    if (g_cfg.server_password[0] != '\0') {
      LOG_D("[DEBUG] 准备添加 Server-Password...");
      S_STR(g_magic_dict.avp_server_password, g_cfg.server_password);
      LOG_D("[DEBUG] Server-Password 添加成功");
    }
  });

  LOG_D("[MAGIC] Client-Credentials (20019) 添加成功 → User-Name: %s",
        g_cfg.username);
  return 0;
}

/* ================================================================== */
/*        2. Communication-Request-Parameters (Code 20001)              */
/* ================================================================== */

/**
 * @brief 添加 Communication-Request-Parameters Grouped AVP (Code 20001)
 * 到消息。
 * @details 该 AVP 是 MCCR (MAGIC-Communication-Change-Request) 消息的核心载荷，
 *          用于向服务端申请或修改通信资源。
 *
 *          主要子 AVP 包括（均从 `g_cfg` 全局配置读取）：
 *          - Profile-Name (10040): 业务配置文件名称，**必填**。
 *          - Requested-BW / Requested-Return-BW: 请求的上下行带宽。
 *          - Required-BW / Required-Return-BW: 最低保障带宽。
 *          - Priority-Type / Priority-Class: 流量优先级。
 *          - QoS-Level: 服务质量等级。
 *          - Flight-Phase / Altitude: 当前飞行状态。
 *          - TFTtoGround-List / TFTtoAircraft-List: 流量过滤规则（嵌套添加）。
 *          - NAPT-List: 端口地址映射规则（添加到消息根级别）。
 *
 * @param msg 目标 Diameter 消息对象。
 *
 * @return int 成功返回 0。
 * @retval -1 如果 `g_cfg.profile_name` 为空，或 AVP 创建/添加失败。
 *
 * @warning Profile-Name 是协议强制要求的字段。如果未配置，函数将返回错误，
 *          并在日志中输出详细的错误信息。
 */
int add_comm_req_params(struct msg *msg) {
  // Profile-Name 是会话的唯一标识，协议强制要求必须存在
  if (g_cfg.profile_name[0] == '\0') {
    LOG_E("[MAGIC] add_comm_req_params 失败：Profile-Name 为空，必须配置（如 "
          "VOICE、IP_DATA）");
    return -1;
  }

  /* 创建 Communication-Request-Parameters Grouped AVP */
  struct avp *comm_req_avp = NULL;
  if (fd_msg_avp_new(g_magic_dict.avp_comm_req_params, 0, &comm_req_avp) != 0) {
    LOG_E("创建 Communication-Request-Parameters AVP 失败");
    return -1;
  }

  /* === 必填字段：Profile-Name === */
  {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_profile_name, 0, &avp) == 0) {
      union avp_value val;
      val.os.data = (uint8_t *)g_cfg.profile_name;
      val.os.len = strlen(g_cfg.profile_name);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  /* === 可选字段：带宽相关 === */
  if (g_cfg.requested_bw > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_requested_bw, 0, &avp) == 0) {
      union avp_value val;
      val.f32 = (float)(g_cfg.requested_bw / 1000.0);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.requested_return_bw > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_requested_return_bw, 0, &avp) == 0) {
      union avp_value val;
      val.f32 = (float)(g_cfg.requested_return_bw / 1000.0);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.required_bw > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_required_bw, 0, &avp) == 0) {
      union avp_value val;
      val.f32 = (float)(g_cfg.required_bw / 1000.0);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.required_return_bw > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_required_return_bw, 0, &avp) == 0) {
      union avp_value val;
      val.f32 = (float)(g_cfg.required_return_bw / 1000.0);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  /* === 优先级和QoS === */
  if (g_cfg.priority_type > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_priority_type, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = g_cfg.priority_type;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.priority_class > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_priority_class, 0, &avp) == 0) {
      char priority_str[16];
      snprintf(priority_str, sizeof(priority_str), "%u", g_cfg.priority_class);
      union avp_value val;
      val.os.data = (uint8_t *)priority_str;
      val.os.len = strlen(priority_str);
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
      LOG_D("[CLIENT] Adding Priority-Class AVP, value=%s", priority_str);
    }
  }

  if (g_cfg.qos_level >= 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_qos_level, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = g_cfg.qos_level;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
      LOG_D("[CLIENT] Adding QoS-Level AVP, value=%u", g_cfg.qos_level);
    }
  }

  if (g_cfg.accounting_enabled) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_accounting_enabled, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = 1;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  /* === 飞行状态相关 === */
  if (g_cfg.flight_phase > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_flight_phase, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = g_cfg.flight_phase;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.altitude > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_altitude, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = g_cfg.altitude;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.timeout > 0) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_timeout, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = g_cfg.timeout;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.keep_request) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_keep_request, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = 1;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  if (g_cfg.auto_detect) {
    struct avp *avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_auto_detect, 0, &avp) == 0) {
      union avp_value val;
      val.u32 = 1;
      fd_msg_avp_setvalue(avp, &val);
      fd_msg_avp_add(comm_req_avp, MSG_BRW_LAST_CHILD, avp);
    }
  }

  /* === TFT 规则（按ARINC 839规范添加到 Communication-Request-Parameters）===
   */
  /* 结构: Communication-Request-Parameters (20001)
   *         └── TFTtoGround-List (20004)
   *               └── TFTtoGround-Rule (10030)
   */
  add_tft_to_ground_list_to_avp(comm_req_avp);
  add_tft_to_aircraft_list_to_avp(comm_req_avp);

  /* 将 Communication-Request-Parameters 添加到消息 */
  if (fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, comm_req_avp) != 0) {
    LOG_E("添加 Communication-Request-Parameters 到消息失败");
    return -1;
  }

  /* NAPT 规则单独添加到消息根级别 */
  add_napt_list(msg);

  LOG_D("[MAGIC] Communication-Request-Parameters (20001) 添加完成");
  return 0;
}

/* ================================================================== */
/*        3. Communication-Answer-Parameters (Code 20002)               */
/* ================================================================== */

/**
 * @brief 添加 Communication-Answer-Parameters Grouped AVP (Code 20002) 到消息。
 * @details 该 AVP 用于 MCCA (MAGIC-Communication-Change-Answer) 应答消息，
 *          服务端通过此 AVP 告知客户端实际分配的通信资源。
 *
 *          主要子 AVP 包括：
 *          - Profile-Name: 回填客户端请求中的会话标识。
 *          - Granted-BW / Granted-Return-BW: 服务端实际授予的带宽。
 *          - Priority-Type / Priority-Class / QoS-Level: 确认的优先级和 QoS。
 *          - Accounting-Enabled: 是否开启计费功能。
 *
 * @param msg 目标 Diameter 消息对象（应为 MCCA 应答消息）。
 *
 * @return int 始终返回 0（当前实现使用占位值，依赖宏内部错误处理）。
 *
 * @note 当前实现中，`Granted-BW` 等字段暂时使用 `g_cfg.requested_bw`
 * 作为占位值。 在实际服务端实现中，这些值应来自资源分配逻辑的计算结果。
 */
int add_comm_ans_params(struct msg *msg) {
  ADD_GROUPED(msg, g_magic_dict.avp_comm_ans_params, {
    // 以下所有字段在 Answer 消息中均为 REQUIRED，必须填写
    S_STR(g_magic_dict.avp_profile_name,
          g_cfg.profile_name); // 必须回填相同的会话名
    S_U64(g_magic_dict.avp_granted_bw,
          g_cfg.requested_bw); // 服务端实际分配的下行带宽（占位: 使用
                               // requested_bw）
    S_U64(g_magic_dict.avp_granted_return_bw,
          g_cfg.requested_return_bw); // 服务端实际分配的上行带宽（占位: 使用
                                      // requested_return_bw）
    S_U32(g_magic_dict.avp_priority_type,
          g_cfg.priority_type); // 确认优先级类型
    S_U32(g_magic_dict.avp_priority_class,
          g_cfg.priority_class);                        // 确认优先级等级
    S_U32(g_magic_dict.avp_qos_level, g_cfg.qos_level); // 确认 QoS 等级
    S_U32(g_magic_dict.avp_accounting_enabled,
          g_cfg.accounting_enabled ? 1 : 0); // 是否开启计费

    /* 注意：TFT 规则需要在 ADD_GROUPED 宏外部单独处理 */

    // 可选但推荐携带的字段
    if (g_cfg.timeout > 0)
      S_U32(g_magic_dict.avp_timeout, g_cfg.timeout);
    if (g_cfg.keep_request)
      S_U32(g_magic_dict.avp_keep_request, 1);
    if (g_cfg.auto_detect)
      S_U32(g_magic_dict.avp_auto_detect, 1);
    if (g_cfg.flight_phase > 0)
      S_U32(g_magic_dict.avp_flight_phase, g_cfg.flight_phase);
    if (g_cfg.altitude > 0)
      S_U32(g_magic_dict.avp_altitude, g_cfg.altitude);

    /* gateway_ip 字段在当前 config.h 中不存在，已忽略 */
  });

  LOG_D("[MAGIC] Communication-Answer-Parameters (20002) 添加完成");
  return 0;
}

/* ================================================================== */
/*        4. Communication-Report-Parameters (Code 20003)               */
/* ================================================================== */

/**
 * @brief 添加 Communication-Report-Parameters Grouped AVP (Code 20003) 到消息。
 * @details 该 AVP 用于客户端向服务端上报当前通信会话的实时状态，
 *          通常在 MSCR (MAGIC-Status-Change-Report) 消息中使用。
 *
 *          主要子 AVP 包括：
 *          - Profile-Name: 当前会话标识。
 *          - Granted-BW / Granted-Return-BW: 当前实际使用的带宽。
 *          - Priority-Type / Priority-Class / QoS-Level: 当前优先级和 QoS
 * 状态。
 *
 * @param msg 目标 Diameter 消息对象。
 *
 * @return int 始终返回 0。
 */
int add_comm_report_params(struct msg *msg) {
  ADD_GROUPED(msg, g_magic_dict.avp_comm_report_params, {
    // 必填：会话标识
    S_STR(g_magic_dict.avp_profile_name, g_cfg.profile_name);

    // 当前实际使用的带宽（可能与 granted 不同）
    if (g_cfg.requested_bw > 0)
      S_U64(g_magic_dict.avp_granted_bw, g_cfg.requested_bw);
    if (g_cfg.requested_return_bw > 0)
      S_U64(g_magic_dict.avp_granted_return_bw, g_cfg.requested_return_bw);

    // 当前优先级信息
    if (g_cfg.priority_type > 0)
      S_U32(g_magic_dict.avp_priority_type, g_cfg.priority_type);
    if (g_cfg.priority_class > 0)
      S_U32(g_magic_dict.avp_priority_class, g_cfg.priority_class);
    if (g_cfg.qos_level > 0)
      S_U32(g_magic_dict.avp_qos_level, g_cfg.qos_level);

    /* 注意：TFT/NAPT 规则需要在 ADD_GROUPED 宏外部单独处理 */

    /* 当前使用的网关地址：config 中无此字段，已忽略 */
  });

  LOG_D("[MAGIC] Communication-Report-Parameters (20003) 添加完成");
  return 0;
}

/* ================================================================== */
/*               5. TFTtoGround-List (Code 20004)                       */
/* ================================================================== */

/**
 * @brief 添加 TFTtoGround-List Grouped AVP (Code 20004) 到父 AVP。
 * @details 该 AVP 包含一系列“地面到飞机”方向的流量过滤模板 (TFT) 规则。
 *          每条规则使用 TFTtoGround-Rule (10030) 子 AVP 表示。
 *
 *          规则字符串的格式通常为：`"PROTO:SRC_IP:SRC_PORT-DST_IP:DST_PORT"`。
 *
 * @param parent_avp 父 AVP 对象（通常为 Communication-Request-Parameters）。
 *
 * @return int 成功返回 0，失败返回 -1。
 *
 * @warning 如果 `g_cfg.tft_ground_count` 为 0，函数将跳过添加并直接返回成功。
 *          如果单条规则添加失败，函数会跳过该规则继续处理其他规则（非原子操作）。
 *
 * @note 如果最终没有任何有效规则被添加，函数会释放已创建的空 List AVP
 * 以避免内存泄漏。
 */
static int add_tft_to_ground_list_to_avp(struct avp *parent_avp) {
  if (g_cfg.tft_ground_count == 0) {
    LOG_D("无 TFT_GROUND 规则，跳过 TFTtoGround-List");
    return 0;
  }

  /* 创建 TFTtoGround-List Grouped AVP (20004) */
  struct avp *list_avp = NULL;
  if (fd_msg_avp_new(g_magic_dict.avp_tft_to_ground_list, 0, &list_avp) != 0) {
    LOG_E("创建 TFTtoGround-List AVP 失败");
    return -1;
  }

  /* 遍历配置中的所有地到机规则，添加到 List 中 */
  int added_count = 0;
  for (int i = 0; i < g_cfg.tft_ground_count; i++) {
    if (g_cfg.tft_ground_rules[i][0] == '\0')
      continue;

    LOG_I("添加 TFT_GROUND.%d: %s", i + 1, g_cfg.tft_ground_rules[i]);

    /* 创建 TFTtoGround-Rule AVP (10030) */
    struct avp *rule_avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_tft_to_ground_rule, 0, &rule_avp) !=
        0) {
      LOG_E("创建 TFT-to-Ground-Rule AVP 失败");
      continue;
    }

    union avp_value val;
    val.os.data = (uint8_t *)g_cfg.tft_ground_rules[i];
    val.os.len = strlen(g_cfg.tft_ground_rules[i]);
    if (fd_msg_avp_setvalue(rule_avp, &val) != 0) {
      LOG_E("设置 TFT-to-Ground-Rule 值失败");
      continue;
    }

    /* 将 Rule 添加到 List */
    if (fd_msg_avp_add(list_avp, MSG_BRW_LAST_CHILD, rule_avp) != 0) {
      LOG_E("添加 TFT-to-Ground-Rule 到 List 失败");
      continue;
    }
    added_count++;
  }

  /* 将 TFTtoGround-List 添加到父 AVP (Communication-Request-Parameters) */
  if (added_count > 0) {
    if (fd_msg_avp_add(parent_avp, MSG_BRW_LAST_CHILD, list_avp) != 0) {
      LOG_E("添加 TFTtoGround-List 到父 AVP 失败");
      return -1;
    }
    LOG_I("共添加 %d 条 TFT_GROUND 规则到 TFTtoGround-List", added_count);
  } else {
    /* 没有有效规则，释放空 List AVP */
    fd_msg_free((struct msg *)list_avp);
  }

  return 0;
}

/* ================================================================== */
/*              6. TFTtoAircraft-List (Code 20005)                      */
/* ================================================================== */

/**
 * @brief 添加 TFTtoAircraft-List Grouped AVP (Code 20005) 到父 AVP。
 * @details 该 AVP 包含一系列“飞机到地面”方向的流量过滤模板 (TFT) 规则。
 *          每条规则使用 TFTtoAircraft-Rule (10031) 子 AVP 表示。
 *
 * @param parent_avp 父 AVP 对象。
 *
 * @return int 成功返回 0，失败返回 -1。
 *
 * @note 逻辑与 `add_tft_to_ground_list_to_avp()` 完全对称，仅方向不同。
 */
static int add_tft_to_aircraft_list_to_avp(struct avp *parent_avp) {
  if (g_cfg.tft_aircraft_count == 0) {
    LOG_D("无 TFT_AIR 规则，跳过 TFTtoAircraft-List");
    return 0;
  }

  /* 创建 TFTtoAircraft-List Grouped AVP (20005) */
  struct avp *list_avp = NULL;
  if (fd_msg_avp_new(g_magic_dict.avp_tft_to_aircraft_list, 0, &list_avp) !=
      0) {
    LOG_E("创建 TFTtoAircraft-List AVP 失败");
    return -1;
  }

  /* 遍历配置中的所有机到地规则，添加到 List 中 */
  int added_count = 0;
  for (int i = 0; i < g_cfg.tft_aircraft_count; i++) {
    if (g_cfg.tft_aircraft_rules[i][0] == '\0')
      continue;

    LOG_I("添加 TFT_AIR.%d: %s", i + 1, g_cfg.tft_aircraft_rules[i]);

    /* 创建 TFTtoAircraft-Rule AVP (10031) */
    struct avp *rule_avp = NULL;
    if (fd_msg_avp_new(g_magic_dict.avp_tft_to_aircraft_rule, 0, &rule_avp) !=
        0) {
      LOG_E("创建 TFT-to-Aircraft-Rule AVP 失败");
      continue;
    }

    union avp_value val;
    val.os.data = (uint8_t *)g_cfg.tft_aircraft_rules[i];
    val.os.len = strlen(g_cfg.tft_aircraft_rules[i]);
    if (fd_msg_avp_setvalue(rule_avp, &val) != 0) {
      LOG_E("设置 TFT-to-Aircraft-Rule 值失败");
      continue;
    }

    /* 将 Rule 添加到 List */
    if (fd_msg_avp_add(list_avp, MSG_BRW_LAST_CHILD, rule_avp) != 0) {
      LOG_E("添加 TFT-to-Aircraft-Rule 到 List 失败");
      continue;
    }
    added_count++;
  }

  /* 将 TFTtoAircraft-List 添加到父 AVP (Communication-Request-Parameters) */
  if (added_count > 0) {
    if (fd_msg_avp_add(parent_avp, MSG_BRW_LAST_CHILD, list_avp) != 0) {
      LOG_E("添加 TFTtoAircraft-List 到父 AVP 失败");
      return -1;
    }
    LOG_I("共添加 %d 条 TFT_AIR 规则到 TFTtoAircraft-List", added_count);
  } else {
    fd_msg_free((struct msg *)list_avp);
  }

  return 0;
}

/* ================================================================== */
/*                    7. NAPT-List (Code 20006)                         */
/* ================================================================== */

/**
 * @brief 添加 NAPT-List Grouped AVP (Code 20006) 到消息根级别。
 * @details 该 AVP 包含网络地址端口转换 (NAPT)
 * 规则列表，用于配置客户端的端口映射策略。 每条规则使用 NAPT-Rule (10032) 子
 * AVP 表示。
 *
 * @param parent 目标 Diameter 消息对象。
 *
 * @return int 始终返回 0。
 *
 * @note 与 TFT 规则不同，NAPT 规则直接添加到消息根级别，而非嵌套在其他 Grouped
 * AVP 中。
 */
static int add_napt_list(struct msg *parent) {
  for (int i = 0; i < g_cfg.napt_count; i++) {
    if (g_cfg.napt_rules[i][0] == '\0')
      continue;

    ADD_GROUPED(parent, g_magic_dict.avp_napt_list,
                { S_STR(g_magic_dict.avp_napt_rule, g_cfg.napt_rules[i]); });
  }
  return 0;
}

/* ================================================================== */
/*                      8. DLM-Info (Code 20008)                        */
/* ================================================================== */

/**
 * @brief 添加 DLM-Info Grouped AVP (Code 20008) 到父 AVP。
 * @details 这是 MAGIC
 * 协议中最复杂的嵌套结构，用于服务端向客户端通告单个数据链路模块 (DLM)
 * 的完整状态。
 *
 *          内部结构包含三层嵌套：
 *          1. DLM 基本信息：名称、可用性、最大链路数、最大带宽等。
 *          2. DLM-QoS-Level-List (20009)：该 DLM 支持的 QoS 等级列表（最多 3
 * 个）。
 *          3. Link-Status-Group (20011)：该 DLM 下每条物理链路的详细状态。
 *
 * @param parent 父 AVP 对象（通常为 Communication-Answer-Parameters）。
 * @param dlm 指向 `dlm_info_t` 结构体的指针，包含完整的 DLM 状态数据。
 *
 * @return int 始终返回 0（依赖宏内部错误处理）。
 *
 * @warning 如果 `dlm` 参数为 NULL，函数将直接返回而不添加任何 AVP。
 *          这是协议允许的行为（DLM-Info 为可选 AVP）。
 *
 * @note 此函数使用了多层嵌套的 `ADD_GROUPED` 宏，代码可读性较强但执行路径复杂。
 *       所有内存管理由宏自动处理。
 */
int add_dlm_info(struct avp *parent, const dlm_info_t *dlm) {
  /* 如果传入的 dlm 指针为 NULL，直接返回，不添加该 Grouped AVP（协议允许缺席）
   */
  if (dlm == NULL) {
    return 0;
  }

  /* 开始构造 DLM-Info Grouped AVP（Code 20008），所有子 AVP 都放在这个大组里 */
  ADD_GROUPED(parent, g_magic_dict.avp_dlm_info, {
    /* --------------------- DLM 基本信息 --------------------- */
    /* 规则：全部 REQUIRED */
    S_STR(g_magic_dict.avp_dlm_name,
          dlm->name); // DLM 名称，如 "SATCOM1"、"IRIDIUM"、"VDLM2"
    S_U32(g_magic_dict.avp_dlm_available,
          dlm->available); // 该 DLM 是否可用：1 = 可用，0 = 不可用
    S_U32(g_magic_dict.avp_dlm_max_links,
          dlm->max_links); // 该 DLM 理论上最多支持多少条并行链路
    S_U64(g_magic_dict.avp_dlm_max_bw,
          dlm->max_bw); // 该 DLM 最大下行带宽（单位：bit/s）

    /* 最大上行带宽是 OPTIONAL，只有当实际有上行能力时才填写 */
    if (dlm->max_return_bw > 0) {
      S_U64(g_magic_dict.avp_dlm_max_return_bw, dlm->max_return_bw);
    }

    /* --------------------- 当前已分配资源信息 --------------------- 规则：全部
     * REQUIRED */
    S_U32(g_magic_dict.avp_dlm_alloc_links,
          dlm->allocated_links); // 当前已经分配出去的链路数量
    S_U64(g_magic_dict.avp_dlm_alloc_bw,
          dlm->allocated_bw); // 当前已经分配的下行带宽总量

    /* 已分配上行带宽也是 OPTIONAL */
    if (dlm->allocated_return_bw > 0) {
      S_U64(g_magic_dict.avp_dlm_alloc_return_bw, dlm->allocated_return_bw);
    }

    /* --------------------- DLM-QoS-Level-List (20009) --------------------- */
    /* 协议规定一个 DLM 最多支持 3 种 QoS
     * 等级，用来告诉飞机该链路能提供哪几种服务质量 */
    for (int q = 0; q < dlm->qos_count && q < 3; q++) {
      /* 每一个 QoS 等级都包装成一个 DLM-QoS-Level-List Grouped AVP */
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_dlm_qos_level_list, {
        /* 子 AVP 只有 QoS-Level 一个 Unsigned32 */
        S_U32(g_magic_dict.avp_qos_level, dlm->qos_levels[q]);
      });
      /* 循环结束后会自动继续添加下一个 QoS 等级，最多 3 个 */
    }

    /* --------------------- Link-Status-Group (20011) 列表
     * --------------------- */
    /* 报告该 DLM 下每一条物理链路的详细状态（可以有 0 条或多条） */
    for (int l = 0; l < dlm->link_count; l++) {
      /* 每一条链路都用一个独立的 Link-Status-Group Grouped AVP 表示 */
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_link_status_group, {
        /* 以下字段全部为 REQUIRED */
        S_U32(g_magic_dict.avp_link_number,
              dlm->links[l].number); // 链路编号（从 1 开始）
        S_U32(g_magic_dict.avp_link_available,
              dlm->links[l].available); // 本链路当前是否可用
        S_U32(g_magic_dict.avp_qos_level,
              dlm->links[l].qos_level); // 本链路当前实际 QoS 等级
        S_U32(g_magic_dict.avp_link_conn_status,
              dlm->links[l].conn_status); // 物理连接状态（如 Connected = 1）
        S_U32(g_magic_dict.avp_link_login_status,
              dlm->links[l].login_status); // 登录/鉴权状态（如 LoggedIn = 1）
        S_U64(g_magic_dict.avp_link_max_bw,
              dlm->links[l].max_bw); // 本链路理论最大下行带宽

        /* 以下字段为 OPTIONAL，根据实际情况填写 */
        if (dlm->links[l].max_return_bw > 0) {
          S_U64(g_magic_dict.avp_link_max_return_bw,
                dlm->links[l].max_return_bw);
        }

        if (dlm->links[l].alloc_bw > 0) {
          S_U64(g_magic_dict.avp_link_alloc_bw, dlm->links[l].alloc_bw);
        }

        if (dlm->links[l].alloc_return_bw > 0) {
          S_U64(g_magic_dict.avp_link_alloc_return_bw,
                dlm->links[l].alloc_return_bw);
        }

        /* 如果链路有故障，携带错误描述字符串供飞机侧显示或记录 */
        if (dlm->links[l].error_str != NULL &&
            dlm->links[l].error_str[0] != '\0') {
          S_STR(g_magic_dict.avp_link_error_string, dlm->links[l].error_str);
        }
      });
      /* 一条链路添加完毕，循环继续添加下一条 */
    }
    /* 所有链路添加完毕，Link-Status-Group 列表结束 */
  });
  /* DLM-Info Grouped AVP 构造完成 */

  /* 函数始终返回 0，表示成功（所有错误均由宏内部自动 cleanup 处理） */
  return 0;
}

/* ================================================================== */
/*                   9. CDRs-Active (Code 20012)                        */
/* ================================================================== */

/**
 * @brief 添加 CDRs-Active Grouped AVP (Code 20012) 到消息。
 * @details 该 AVP 包含当前正在活跃记录中的计费数据记录 (CDR) 列表。
 *
 * @param msg 目标 Diameter 消息对象。
 * @param list 指向 `cdr_item_t` 数组的指针。
 * @param n 数组长度。
 *
 * @return int 始终返回 0。
 */
int add_cdrs_active(struct msg *msg, const cdr_item_t list[], size_t n) {
  if (n == 0)
    return 0; // 没有活跃 CDR 就直接返回，不添加该 AVP

  ADD_GROUPED(msg, g_magic_dict.avp_cdrs_active, {
    for (size_t i = 0; i < n; i++) {
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_cdr_info, {
        S_STR(g_magic_dict.avp_cdr_id, list[i].id); // CDR 唯一 ID，必填
        if (list[i].content && list[i].content[0] != '\0')
          S_STR(g_magic_dict.avp_cdr_content,
                list[i].content); // 可选的完整 CDR 内容
      });
    }
  });
  return 0;
}

/* ================================================================== */
/*                  10. CDRs-Finished (Code 20013)                      */
/* ================================================================== */

/**
 * @brief 添加 CDRs-Finished Grouped AVP (Code 20013) 到消息。
 * @details 该 AVP 包含已完成记录的计费数据记录 (CDR) 列表。
 *
 * @param msg 目标 Diameter 消息对象。
 * @param list 指向 `cdr_item_t` 数组的指针。
 * @param n 数组长度。
 *
 * @return int 始终返回 0。
 */
int add_cdrs_finished(struct msg *msg, const cdr_item_t list[], size_t n) {
  if (n == 0)
    return 0;

  ADD_GROUPED(msg, g_magic_dict.avp_cdrs_finished, {
    for (size_t i = 0; i < n; i++) {
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_cdr_info, {
        S_STR(g_magic_dict.avp_cdr_id, list[i].id);
        if (list[i].content)
          S_STR(g_magic_dict.avp_cdr_content, list[i].content);
      });
    }
  });
  return 0;
}

/* ================================================================== */
/*                 11. CDRs-Forwarded (Code 20014)                      */
/* ================================================================== */

/**
 * @brief 添加 CDRs-Forwarded Grouped AVP (Code 20014) 到消息。
 * @details 该 AVP 包含已成功转发给计费中心的 CDR 列表。
 *
 * @param msg 目标 Diameter 消息对象。
 * @param list 指向 `cdr_item_t` 数组的指针。
 * @param n 数组长度。
 *
 * @return int 成功返回 0。
 *
 * @warning 函数内部会校验每条 CDR 的 ID 是否为空，空 ID
 * 的记录会被跳过并记录错误日志。
 */
int add_cdrs_forwarded(struct msg *msg, const cdr_item_t list[], size_t n) {
  if (n == 0) {
    LOG_D("[MAGIC] CDRs-Forwarded 列表为空，跳过添加");
    return 0;
  }

  ADD_GROUPED(msg, g_magic_dict.avp_cdrs_forwarded, {
    for (size_t i = 0; i < n; i++) {
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_cdr_info, {
        // CDR-ID 必须存在
        if (!list[i].id || list[i].id[0] == '\0') {
          LOG_E("[MAGIC] CDRs-Forwarded 第%zu条 CDR-ID 为空，已跳过", i);
          continue;
        }
        S_STR(g_magic_dict.avp_cdr_id, list[i].id);

        // CDR-Content 为可选，但如果有就加上
        if (list[i].content && list[i].content[0] != '\0') {
          S_STR(g_magic_dict.avp_cdr_content, list[i].content);
        }
      });
    }
  });

  LOG_D("[MAGIC] CDRs-Forwarded 添加完成，共 %zu 条", n);
  return 0;
}

/* ================================================================== */
/*                  12. CDRs-Unknown (Code 20015)                       */
/* ================================================================== */

/**
 * @brief 添加 CDRs-Unknown Grouped AVP (Code 20015) 到消息。
 * @details 该 AVP 包含服务端无法识别的 CDR ID 列表，用于通知客户端哪些 CDR
 * 记录未被服务端接受。
 *
 * @param msg 目标 Diameter 消息对象。
 * @param ids 指向 CDR ID 字符串数组的指针。
 * @param n 数组长度。
 *
 * @return int 始终返回 0。
 */
int add_cdrs_unknown(struct msg *msg, const char *ids[], size_t n) {
  if (n == 0)
    return 0;

  ADD_GROUPED(msg, g_magic_dict.avp_cdrs_unknown, {
    for (size_t i = 0; i < n; i++) {
      if (ids[i] && ids[i][0] != '\0') {
        S_STR(g_magic_dict.avp_cdr_id, ids[i]);
      }
    }
  });

  LOG_D("[MAGIC] CDRs-Unknown 添加完成，共 %zu 条", n);
  return 0;
}

/* ================================================================== */
/*                  13. CDRs-Updated (Code 20016)                       */
/* ================================================================== */

/**
 * @brief 添加 CDRs-Updated Grouped AVP (Code 20016) 到消息。
 * @details 该 AVP 用于通知 CDR 记录的更新事件，每个更新包含一对 CDR ID：
 *          - CDR-Stopped (10049): 已停止的旧 CDR ID。
 *          - CDR-Started (10050): 新开始的 CDR ID。
 *
 * @param msg 目标 Diameter 消息对象。
 * @param pairs 指向 `cdr_start_stop_t` 配对数组的指针。
 * @param n 数组长度。
 *
 * @return int 始终返回 0。
 */
int add_cdrs_updated(struct msg *msg, const cdr_start_stop_t pairs[],
                     size_t n) {
  if (n == 0)
    return 0;

  ADD_GROUPED(msg, g_magic_dict.avp_cdrs_updated, {
    for (size_t i = 0; i < n; i++) {
      ADD_GROUPED(parent_for_sub, g_magic_dict.avp_cdr_start_stop_pair, {
        // 旧的已停止的 CDR（Stopped）
        if (pairs[i].stopped && pairs[i].stopped[0] != '\0')
          S_STR(g_magic_dict.avp_cdr_stopped, pairs[i].stopped);

        // 新的已开始的 CDR（Started）
        if (pairs[i].started && pairs[i].started[0] != '\0')
          S_STR(g_magic_dict.avp_cdr_started, pairs[i].started);
      });
    }
  });

  LOG_D("[MAGIC] CDRs-Updated 添加完成，共 %zu 对更新对", n);
  return 0;
}

/* ============== 至此，ARINC 839 MAGIC 协议全部 19 个 Grouped AVP 实现完毕
 * ============== */
/* 后续新增或修改 AVP，只需参考以上模板，复制粘贴即可 */