/**
 * @file magic_group_avp_add.c
 * @brief ARINC 839 MAGIC 协议 Grouped AVP 构造助手实现。
 * @details 采用混合风格构造 ARINC 839 协议要求的全部 19 个复杂 Grouped AVP。
 */

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <string.h>

#include "add_avp.h" // 必须包含你已添加的 ADD_GROUPED 宏和 S_STR / S_U32 等子宏
#include "config.h"  // 全局配置结构体 g_cfg（运行时参数）
#include "log.h"     // LOG_E / LOG_D 等日志宏（假设项目已有）
#include "magic_dict_handles.h" // 包含所有 MAGIC 协议字典对象（如 g_magic_dict.avp_xxx）
#include "magic_group_avp_add.h" // 类型定义与函数声明（cdr_item_t / dlm_info_t 等）

/* 外部全局变量声明（请确保在其他 .c 文件中已定义并初始化） */
extern app_config_t g_cfg; // 程序全局配置（从配置文件加载）
extern struct magic_dict_handles
    g_magic_dict; // freeDiameter 加载的 MAGIC 字典句柄

#define MAGIC_VENDOR_ID                                                        \
  13712 // ARINC 官方分配的 Vendor ID，所有 AVP 自动带 V 位

/**
 * @brief 添加 Client-Credentials Grouped AVP (Code 20019)。
 * @details 构造包含用户名和双向密码校验信息的 AVP。该 AVP
 * 是认证阶段（MAR）的核心。 逻辑：{ User-Name (REQUIRED), Client-Password
 * (REQUIRED), Server-Password (OPTIONAL) }。
 *
 * @param[in,out] msg 指向待填充 Diameter 消息的指针。
 *
 * @return int 成功返回 0；如果 `g_cfg` 中用户名或密码为空则返回
 * -1（协议强制项）。
 *
 * @warning 该函数依赖全局配置 `g_cfg`。如果配置加载失败，认证将直接报错。
 * @note User-Name 使用标准词典 AVP，其余使用 MAGIC 私有 AVP。
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
/* 2. Communication-Request-Parameters (AVP Code 20001)               */
/*    客户端向服务端发起通信资源请求时使用的核心 Grouped AVP           */
/* ================================================================== */
int add_comm_req_params(struct msg *msg) {
  // Profile-Name 是会话的唯一标识，协议强制要求必须存在
  if (g_cfg.profile_name[0] == '\0') {
    LOG_E("[MAGIC] add_comm_req_params 失败：Profile-Name 为空，必须配置（如 "
          "VOICE、IP_DATA）");
    return -1;
  }

  // 开始构造 20001 Grouped AVP
  ADD_GROUPED(msg, g_magic_dict.avp_comm_req_params, {
    // === 必填字段 ===
    S_STR(g_magic_dict.avp_profile_name, g_cfg.profile_name); // 会话类型名称

    // === 常用可选字段（根据实际需求填写） ===
    if (g_cfg.requested_bw > 0) // 客户端请求的下行带宽（bit/s）
      S_U64(g_magic_dict.avp_requested_bw, g_cfg.requested_bw);

    if (g_cfg.requested_return_bw > 0) // 客户端请求的上行带宽
      S_U64(g_magic_dict.avp_requested_return_bw, g_cfg.requested_return_bw);

    if (g_cfg.required_bw > 0) // 最低必须保证的下行带宽
      S_U64(g_magic_dict.avp_required_bw, g_cfg.required_bw);

    if (g_cfg.required_return_bw > 0) // 最低必须保证的上行带宽
      S_U64(g_magic_dict.avp_required_return_bw, g_cfg.required_return_bw);

    if (g_cfg.priority_type > 0) // 优先级类型（如紧急通信）
      S_U32(g_magic_dict.avp_priority_type, g_cfg.priority_type);

    if (g_cfg.priority_class > 0) // 优先级等级 1~8
      S_U32(g_magic_dict.avp_priority_class, g_cfg.priority_class);

    if (g_cfg.qos_level > 0) // QoS 等级 0~3
      S_U32(g_magic_dict.avp_qos_level, g_cfg.qos_level);

    if (g_cfg.accounting_enabled) // 是否需要计费
      S_U32(g_magic_dict.avp_accounting_enabled, 1);

    if (g_cfg.dlm_name[0] != '\0') // 指定希望使用的链路模块名称
      S_STR(g_magic_dict.avp_dlm_name, g_cfg.dlm_name);

    if (g_cfg.flight_phase > 0) // 当前飞行阶段（Ground/Takeoff/Cruise等）
      S_U32(g_magic_dict.avp_flight_phase, g_cfg.flight_phase);

    if (g_cfg.altitude > 0) // 当前高度（单位：米）
      S_U32(g_magic_dict.avp_altitude, g_cfg.altitude);

    if (g_cfg.timeout > 0) // 会话超时时间（秒）
      S_U32(g_magic_dict.avp_timeout, g_cfg.timeout);

    if (g_cfg.keep_request) // 是否要求长连接保持
      S_U32(g_magic_dict.avp_keep_request, 1);

    if (g_cfg.auto_detect) // 是否允许自动链路探测
      S_U32(g_magic_dict.avp_auto_detect, 1);
  });

  // 以下三个列表型 Grouped AVP 单独处理，避免宏嵌套过深影响可读性与编译性能
  add_tft_to_ground_list(msg);   // 地→机流量过滤规则列表
  add_tft_to_aircraft_list(msg); // 机→地流量过滤规则列表
  add_napt_list(msg);            // NAPT 端口映射规则列表

  LOG_D("[MAGIC] Communication-Request-Parameters (20001) 添加完成");
  return 0;
}

/* ================================================================== */
/* 3. Communication-Answer-Parameters (AVP Code 20002)                */
/*    服务端对客户端请求的应答，最重要的响应消息体                     */
/* ================================================================== */
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

    // 必须返回的流量过滤规则表（客户端据此配置防火墙）
    add_tft_to_ground_list(msg);
    add_tft_to_aircraft_list(msg);

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
/* 4. Communication-Report-Parameters (AVP Code 20003)                */
/*    客户端定期或事件触发向服务端上报当前通信状态                     */
/* ================================================================== */
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

    // 上报时通常也同步最新的过滤规则
    add_tft_to_ground_list(msg);
    add_tft_to_aircraft_list(msg);
    add_napt_list(msg);

    /* 当前使用的网关地址：config 中无此字段，已忽略 */
  });

  LOG_D("[MAGIC] Communication-Report-Parameters (20003) 添加完成");
  return 0;
}

/* ================================================================== */
/* 5. TFTtoGround-List (AVP Code 20004) —— 地→机流量过滤规则列表       */
/* ================================================================== */
static int add_tft_to_ground_list(struct msg *parent) {
  // 遍历配置中的所有地到机规则
  for (int i = 0; i < g_cfg.tft_ground_count; i++) {
    // 跳过空字符串规则
    if (g_cfg.tft_ground_rules[i][0] == '\0')
      continue;

    // 每一条规则都是一个独立的 TFTtoGround-Rule AVP
    ADD_GROUPED(parent, g_magic_dict.avp_tft_to_ground_list, {
      S_STR(g_magic_dict.avp_tft_to_ground_rule, g_cfg.tft_ground_rules[i]);
    });
  }
  return 0;
}

/* ================================================================== */
/* 6. TFTtoAircraft-List (AVP Code 20005) —— 机→地流量过滤规则列表     */
/* ================================================================== */
static int add_tft_to_aircraft_list(struct msg *parent) {
  for (int i = 0; i < g_cfg.tft_aircraft_count; i++) {
    if (g_cfg.tft_aircraft_rules[i][0] == '\0')
      continue;

    ADD_GROUPED(parent, g_magic_dict.avp_tft_to_aircraft_list, {
      S_STR(g_magic_dict.avp_tft_to_aircraft_rule, g_cfg.tft_aircraft_rules[i]);
    });
  }
  return 0;
}

/* ================================================================== */
/* 7. NAPT-List (AVP Code 20006) —— 端口映射规则列表                  */
/* ================================================================== */
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
/* 8. DLM-Info (AVP Code 20008) —— 最复杂的嵌套结构                  */
/*    服务端通过 Communication-Answer-Parameters 把当前所有可用       */
/*    数据链路模块（DLM）的完整状态通告给机载客户端                     */
/* ================================================================== */
/**
 * @brief 添加 DLM-Info Grouped AVP (Code 20008) —— 协议最复杂嵌套结构。
 * @details 该函数递归构造 DLM 状态信息，包括其能力、已分配资源、QoS
 * 级别列表以及下属物理链路状态组（Link-Status-Group）。
 *          这是服务端向飞机通告链路拓扑的核心手段。
 *
 * @param[in,out] parent 指向父 AVP (通常是 Communication-Answer-Parameters)
 * 的指针。
 * @param[in]     dlm    指向 `dlm_info_t` 业务结构体的指针。如果为 NULL
 * 则不执行添加。
 *
 * @return int 始终返回 0。内部异常通过 AVP 清理机制静默处理。
 *
 * @note 内部包含对 `DLM-QoS-Level-List` (20009) 和 `Link-Status-Group` (20011)
 * 的循环嵌套调用。
 * @warning `dlm->links` 指针必须有效且长度不小于 `dlm->link_count`。
 */

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

  /* --------------------- 当前已分配资源信息 ---------------------   规则：全部
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

  /* --------------------- Link-Status-Group (20011) 列表 ---------------------
   */
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
        S_U64(g_magic_dict.avp_link_max_return_bw, dlm->links[l].max_return_bw);
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
/* 9. CDRs-Active (AVP Code 20012) —— 当前活跃的计费记录列表          */
/* ================================================================== */
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
/* 10. CDRs-Finished (AVP Code 20013) —— 已结束的计费记录             */
/* ================================================================== */
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
/* 11. CDRs-Forwarded (AVP Code 20014) —— 已成功转发给计费中心的记录   */
/* ================================================================== */
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
/* 14. CDRs-Unknown (AVP Code 20015) —— 服务端不认识的 CDR ID 列表    */
/* ================================================================== */
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
/* 15. CDRs-Updated (AVP Code 20016) —— CDR 记录更新通知             */
/*     包含多个 CDR-Start-Stop-Pair (20018)                           */
/* ================================================================== */
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
/* 后续新增或修改 AVP，只需参考以上模板，复制粘贴即可，维护成本极低 */