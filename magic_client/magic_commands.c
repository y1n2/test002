/**
 * @file magic_commands.c
 * @brief MAGIC 协议各 Diameter 命令的处理实现。
 * @details 负责构建请求消息、调用传输接口并处理来自服务端的应答。
 *          涵盖了认证注册 (MCAR)、通信控制 (MCCR)、状态查询 (MSXR) 等
 *          ARINC 839 核心流程的手动触发场景。
 */

#include "magic_commands.h"
#include "add_avp.h"
#include "cli_interface.h"
#include "config.h"
#include "log.h"
#include "magic_dict_handles.h"
#include "magic_group_avp_add.h"
#include "session_manager.h"

/* 引用词典系统的错误码定义 */
#include "../extensions/dict_magic_839/dict_magic_codes.h"

/* Termination-Cause AVP 枚举值 (RFC 6733) */
#define DIAMETER_LOGOUT 1 /* 用户请求注销 */

#include <ctype.h>
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <string.h>
#include <unistd.h>

/* 外部全局变量 */
extern app_config_t g_cfg;
extern struct magic_dict_handles g_magic_dict;
extern struct std_diam_dict_handles g_std_dict;

/* ==================== 客户端状态跟踪 ==================== */
typedef enum {
  CLIENT_STATE_IDLE = 0,      /* 未认证 */
  CLIENT_STATE_AUTHENTICATED, /* 已认证，无活动通信 */
  CLIENT_STATE_ACTIVE,        /* 通信活跃中 */
  CLIENT_STATE_QUEUED         /* 请求已排队 */
} ClientState;

static ClientState g_client_state = CLIENT_STATE_IDLE;
static char g_assigned_link_id[64] = "";
static char g_gateway_ip[64] = "";
static uint64_t g_granted_bw = 0;
static uint64_t g_granted_ret_bw = 0;
static uint32_t g_session_timeout = 0;
static uint32_t g_bearer_id = 0;

/* 订阅状态跟踪 */
uint32_t g_requested_subscribe_level = 0; /* 请求的订阅级别 */
uint32_t g_granted_subscribe_level = 0;   /* 服务端授予的订阅级别 */

/* 前向声明 v2.1 增强解析函数 */
static void parse_dlm_info(struct avp *avp_dlm_info);
static void parse_dlm_list(struct avp *avp_dlm_list);

/* v2.1: MSXR 请求跟踪 (用于检测降级) */
static uint32_t g_last_msxr_requested_type = 0;

/* ==================== 订阅级别辅助函数 ==================== */

/**
 * 获取订阅级别的可读名称
 */
const char *magic_get_subscribe_level_name(uint32_t level) {
  switch (level) {
  case 0:
    return "No_Status (不订阅)";
  case 1:
    return "MAGIC_Status (系统状态)";
  case 2:
    return "DLM_Status (一般状态)";
  case 3:
    return "MAGIC_DLM_Status (综合状态)";
  case 6:
    return "DLM_Link_Status (详细链路)";
  case 7:
    return "All_Status (全部状态)";
  default:
    return "Unknown (未知)";
  }
}

/**
 * 验证订阅级别是否有效
 */
int magic_validate_subscribe_level(uint32_t level) {
  /* 有效值: 0, 1, 2, 3, 6, 7 (注意: 4, 5 是保留值) */
  return (level == 0 || level == 1 || level == 2 || level == 3 || level == 6 ||
          level == 7);
}

/* ==================== 辅助函数实现 ==================== */

/* 前向声明 - 应答解析函数（需要在回调函数中使用） */
void magic_print_status_info(struct msg *ans);
void magic_print_cdr_info(struct msg *ans);
void magic_print_macr_result(struct msg *ans);

/* 解析 Communication-Answer-Parameters 中的详细信息 */
static void parse_comm_answer_params(struct msg *ans) {
  struct avp *avp_comm_ans = NULL;
  struct avp_hdr *hdr = NULL;

  /* 查找 Communication-Answer-Parameters 复合 AVP */
  if (fd_msg_search_avp(ans, g_magic_dict.avp_comm_ans_params, &avp_comm_ans) !=
          0 ||
      !avp_comm_ans) {
    return;
  }

  cli_info("\n📋 Communication-Answer-Parameters:");

  /* 遍历子 AVP */
  struct avp *child = NULL;
  if (fd_msg_browse(avp_comm_ans, MSG_BRW_FIRST_CHILD, &child, NULL) != 0) {
    return;
  }

  while (child) {
    if (fd_msg_avp_hdr(child, &hdr) == 0 && hdr->avp_value) {
      /* 根据 AVP Code 解析不同字段 */
      switch (hdr->avp_code) {
      case 10001: /* Profile-Name */
        if (hdr->avp_value->os.data) {
          cli_info("  Profile-Name: %.*s", (int)hdr->avp_value->os.len,
                   (char *)hdr->avp_value->os.data);
        }
        break;

      case 10023: /* Selected-Link-ID */
        if (hdr->avp_value->os.data) {
          size_t len = hdr->avp_value->os.len;
          if (len >= sizeof(g_assigned_link_id))
            len = sizeof(g_assigned_link_id) - 1;
          memcpy(g_assigned_link_id, hdr->avp_value->os.data, len);
          g_assigned_link_id[len] = '\0';
          cli_info("  ✓ Selected-Link-ID: %s", g_assigned_link_id);
        }
        break;

      case 10024: /* Bearer-ID */
        g_bearer_id = hdr->avp_value->u32;
        cli_info("  Bearer-ID: %u", g_bearer_id);
        break;

      case 10051: /* Granted-Bandwidth (Float32) */
        g_granted_bw = (uint64_t)hdr->avp_value->f32;
        cli_info("  ✓ Granted-BW (↓Forward): %llu bps (%.2f kbps)",
                 (unsigned long long)g_granted_bw, g_granted_bw / 1000.0);
        break;

      case 10052: /* Granted-Return-Bandwidth (Float32) */
        g_granted_ret_bw = (uint64_t)hdr->avp_value->f32;
        cli_info("  ✓ Granted-Return-BW (↑Return): %llu bps (%.2f kbps)",
                 (unsigned long long)g_granted_ret_bw,
                 g_granted_ret_bw / 1000.0);
        break;

      case 10029: /* Gateway-IPAddress */
        if (hdr->avp_value->os.data) {
          size_t len = hdr->avp_value->os.len;
          if (len >= sizeof(g_gateway_ip))
            len = sizeof(g_gateway_ip) - 1;
          memcpy(g_gateway_ip, hdr->avp_value->os.data, len);
          g_gateway_ip[len] = '\0';
          cli_info("  ✓ Gateway-IPAddress: %s", g_gateway_ip);
        }
        break;

      case 291: /* Session-Timeout */
        g_session_timeout = hdr->avp_value->u32;
        cli_info("  Session-Timeout: %u 秒", g_session_timeout);
        break;

      case 10009: /* QoS-Level */
        cli_info("  QoS-Level: %u", hdr->avp_value->u32);
        break;

      case 10025: /* Keep-Request */
        cli_info("  Keep-Request: %u", hdr->avp_value->u32);
        break;

      case 10004: /* DLM-Name (used as Selected-Link-ID) */
        if (hdr->avp_value->os.data) {
          size_t len = hdr->avp_value->os.len;
          if (len >= sizeof(g_assigned_link_id))
            len = sizeof(g_assigned_link_id) - 1;
          memcpy(g_assigned_link_id, hdr->avp_value->os.data, len);
          g_assigned_link_id[len] = '\0';
          cli_info("  ✓ Selected Link (DLM-Name): %s", g_assigned_link_id);
        }
        break;

      case 10012: /* Link-Number (used as Bearer-ID) */
        g_bearer_id = hdr->avp_value->u32;
        cli_info("  ✓ Bearer-ID (Link-Number): %u", g_bearer_id);
        break;

      default:
        /* 其他 AVP */
        break;
      }
    }

    if (fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL) != 0) {
      break;
    }
  }
}

/* ==================== 应答处理回调函数 ==================== */

/**
 * 通用应答消息处理回调函数
 * @param ans 应答消息指针的指针 (freeDiameter传递，回调后自动释放)
 * @param avp 对端AVP (未使用)
 * @param data 用户自定义数据 (可传递请求类型标识)
 *
 * 工作原理：
 * - freeDiameter 收到应答后自动调用此回调
 * - 回调中解析 Result-Code 和业务AVP
 * - 回调返回后，freeDiameter 自动释放消息内存
 * - 支持所有 MAGIC 协议命令的应答处理
 */
static void magic_answer_callback(void *data, struct msg **ans) {
  struct msg *answer = *ans;
  struct msg_hdr *hdr = NULL;
  struct avp *avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  uint32_t result_code = 0;
  uint32_t magic_status_code = 0;
  char error_message[256] = "";
  int found_result = 0;

  if (!answer) {
    cli_error("应答消息为空");
    return;
  }

  // 获取消息头（包含命令代码）
  CHECK_FCT_DO(fd_msg_hdr(answer, &hdr), return);

  cli_info("\n╔══════════════════════════════════════════════╗");
  cli_info("║        收到 Diameter 应答消息               ║");
  cli_info("╚══════════════════════════════════════════════╝");
  cli_info("  Command-Code: %u", hdr->msg_code);

  // 1. 提取关键 AVP
  CHECK_FCT_DO(fd_msg_browse(answer, MSG_BRW_FIRST_CHILD, &avp, NULL), return);
  while (avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &avp_hdr), break);

    // Result-Code (268)
    if (avp_hdr->avp_code == 268 && !(avp_hdr->avp_flags & AVP_FLAG_VENDOR)) {
      if (avp_hdr->avp_value) {
        result_code = avp_hdr->avp_value->u32;
        found_result = 1;
      }
    }

    // MAGIC-Status-Code (Vendor-Specific)
    if (avp_hdr->avp_code == 10030 && (avp_hdr->avp_flags & AVP_FLAG_VENDOR)) {
      if (avp_hdr->avp_value) {
        magic_status_code = avp_hdr->avp_value->u32;
      }
    }

    // Error-Message (281)
    if (avp_hdr->avp_code == 281 && !(avp_hdr->avp_flags & AVP_FLAG_VENDOR)) {
      if (avp_hdr->avp_value && avp_hdr->avp_value->os.data) {
        size_t len = avp_hdr->avp_value->os.len;
        if (len >= sizeof(error_message))
          len = sizeof(error_message) - 1;
        memcpy(error_message, avp_hdr->avp_value->os.data, len);
        error_message[len] = '\0';
      }
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  if (!found_result) {
    cli_error("应答中未找到 Result-Code");
    goto cleanup;
  }

  // 2. 根据 Result-Code 判断成功或失败
  if (result_code == DIAMETER_SUCCESS) {
    cli_success("✓ Result-Code: %u (DIAMETER_SUCCESS)", result_code);

    // 3. 根据命令类型解析特定的业务AVP
    switch (hdr->msg_code) {
    case CMD_MCAR_CODE: // MCAA (100000)
      cli_info("\n📌 MCAR/MCAA 认证应答处理:");

      /* 检查 REQ-Status-Info 授权级别 */
      {
        struct avp *avp_status = NULL;
        if (fd_msg_search_avp(answer, g_magic_dict.avp_req_status_info,
                              &avp_status) == 0 &&
            avp_status) {
          struct avp_hdr *status_hdr = NULL;
          if (fd_msg_avp_hdr(avp_status, &status_hdr) == 0 &&
              status_hdr->avp_value) {
            g_granted_subscribe_level = status_hdr->avp_value->u32;

            cli_info("  订阅状态:");
            cli_info(
                "    请求级别: %u (%s)", g_requested_subscribe_level,
                magic_get_subscribe_level_name(g_requested_subscribe_level));
            cli_info("    授予级别: %u (%s)", g_granted_subscribe_level,
                     magic_get_subscribe_level_name(g_granted_subscribe_level));

            /* 检查是否被降级 */
            if (g_granted_subscribe_level < g_requested_subscribe_level) {
              cli_warn("  ⚠ 订阅级别被降级! 服务端可能限制了您的权限");
            } else if (g_granted_subscribe_level > 0) {
              cli_success("  ✓ 订阅成功！将接收 MSCR 状态推送");
            }
          }
        }
      }

      // 解析 Communication-Answer-Parameters
      parse_comm_answer_params(answer);

      if (g_assigned_link_id[0] && g_granted_bw > 0) {
        /* 场景 C: 0-RTT 接入成功，直接进入 ACTIVE 状态 */
        g_client_state = CLIENT_STATE_ACTIVE;
        cli_set_registered(true);
        cli_set_session_active(true);
        cli_success("  ✓ 0-RTT接入成功！状态: IDLE → ACTIVE");
        cli_info("  分配链路: %s, 带宽: %.2f/%.2f kbps", g_assigned_link_id,
                 g_granted_bw / 1000.0, g_granted_ret_bw / 1000.0);
      } else {
        /* 场景 A/B: 认证成功，进入 AUTHENTICATED 状态 */
        g_client_state = CLIENT_STATE_AUTHENTICATED;
        cli_set_registered(true);
        cli_success("  ✓ 认证成功！状态: IDLE → AUTHENTICATED");
        cli_info("  下一步可使用 'mccr start' 建立通信链路");
      }
      break;

    case CMD_MCCR_CODE: // MCCA (100001)
      cli_info("\n📌 MCCR/MCCA 通信控制应答处理:");

      // 解析 Communication-Answer-Parameters
      parse_comm_answer_params(answer);

      if (g_assigned_link_id[0] && g_granted_bw > 0) {
        /* 资源分配成功 → ACTIVE */
        g_client_state = CLIENT_STATE_ACTIVE;
        cli_set_session_active(true);
        cli_success("  ✓ 通信链路已建立！状态: → ACTIVE");
        cli_info("  链路: %s, 网关: %s", g_assigned_link_id,
                 g_gateway_ip[0] ? g_gateway_ip : "(未分配)");
        cli_info("  带宽: ↓%.2f kbps / ↑%.2f kbps", g_granted_bw / 1000.0,
                 g_granted_ret_bw / 1000.0);
      } else if (g_granted_bw == 0 && g_granted_ret_bw == 0) {
        /* 释放成功或排队中 */
        if (g_client_state == CLIENT_STATE_ACTIVE) {
          g_client_state = CLIENT_STATE_AUTHENTICATED;
          cli_set_session_active(false);
          g_assigned_link_id[0] = '\0';
          g_gateway_ip[0] = '\0';
          cli_success("  ✓ 通信链路已释放！状态: ACTIVE → AUTHENTICATED");
        } else if (magic_status_code == 0) {
          g_client_state = CLIENT_STATE_QUEUED;
          cli_info("  ⏳ 请求已排队，等待资源可用");
        }
      }
      break;

    case CMD_MSXR_CODE: // MSXA (100002)
      cli_info("\n📌 MSXR/MSXA 状态查询应答:");
      magic_print_status_info(answer);
      break;

    case CMD_MADR_CODE: // MADA (100003)
      cli_info("\n📌 MADR/MADA 计费数据应答:");
      magic_print_cdr_info(answer);
      break;

    case CMD_MACR_CODE: // MACA (100006)
      cli_info("\n📌 MACR/MACA 计费控制应答:");
      magic_print_macr_result(answer);
      break;

    case 275: // STA (Session-Termination-Answer)
      cli_info("\n📌 STR/STA 会话终止应答:");
      g_client_state = CLIENT_STATE_IDLE;
      cli_set_registered(false);
      cli_set_session_active(false);
      g_assigned_link_id[0] = '\0';
      g_gateway_ip[0] = '\0';
      g_granted_bw = 0;
      g_granted_ret_bw = 0;
      cli_success("  ✓ 会话已终止！状态: → IDLE");
      break;

    default:
      cli_info("  收到未知命令应答 (Code: %u)", hdr->msg_code);
      break;
    }
  } else {
    // 错误处理
    cli_error("✗ Result-Code: %u (失败)", result_code);

    if (magic_status_code > 0) {
      cli_error("  MAGIC-Status-Code: %u", magic_status_code);

      /* 解码 MAGIC 状态码 - 使用词典系统定义的常量 */
      const char *status_desc = magic_status_code_str(magic_status_code);
      if (status_desc) {
        cli_error("    → %s", status_desc);
      } else {
        cli_error("    → 未知错误码");
      }
    }

    if (error_message[0]) {
      cli_error("  Error-Message: %s", error_message);
    }

    /* 标准 Diameter 错误码说明 */
    switch (result_code) {
    case DIAMETER_UNABLE_TO_DELIVER:
      cli_error("    → DIAMETER_UNABLE_TO_DELIVER (无法送达)");
      break;
    case DIAMETER_REALM_NOT_SERVED:
      cli_error("    → DIAMETER_REALM_NOT_SERVED (Realm未提供服务)");
      break;
    case DIAMETER_AVP_UNSUPPORTED:
      cli_error("    → DIAMETER_AVP_UNSUPPORTED (不支持的AVP)");
      break;
    case DIAMETER_UNKNOWN_SESSION_ID:
      cli_error("    → DIAMETER_UNKNOWN_SESSION_ID (未知会话ID)");
      break;
    case DIAMETER_INVALID_AVP_VALUE:
      cli_error("    → DIAMETER_INVALID_AVP_VALUE (无效的AVP值)");
      break;
    case DIAMETER_MISSING_AVP:
      cli_error("    → DIAMETER_MISSING_AVP (缺少必需的AVP)");
      break;
    case DIAMETER_UNABLE_TO_COMPLY:
      cli_error("    → DIAMETER_UNABLE_TO_COMPLY (无法执行)");
      break;
    default:
      break;
    }

    /* 认证失败时清除状态 */
    if (hdr->msg_code == CMD_MCAR_CODE) {
      g_client_state = CLIENT_STATE_IDLE;
      cli_set_registered(false);
    }
  }

cleanup:
  // 释放应答消息
  fd_msg_free(answer);
  *ans = NULL;
}

/**
 * 发送 MAGIC 请求并注册应答回调 (异步版本)
 * @param req 请求消息指针的指针 (发送后所有权转移给freeDiameter)
 * @param ans 应答消息输出参数 (未使用，保留兼容性)
 * @param timeout_ms 超时时间(毫秒)，0=使用默认配置
 * @return 0=成功发送 -1=失败
 *
 * 实现说明：
 * - 使用 fd_msg_send() 的回调版本实现异步应答处理
 * - 回调函数 magic_answer_callback 在接收到应答后自动触发
 * - 无需手动等待或轮询，由 freeDiameter 核心线程调用回调
 */
int magic_send_request(struct msg **req, struct msg **ans,
                       uint32_t timeout_ms) {
  int ret;

  // 参数校验
  if (!req || !*req) {
    cli_error("请求消息为空");
    return -1;
  }

  cli_info("发送请求...");

  // 发送请求到 freeDiameter 核心 (带回调版本)
  // 参数说明：
  //   req: 请求消息，所有权转移给 freeDiameter
  //   magic_answer_callback: 应答到达时的回调函数
  //   NULL: 用户数据指针 (可传递请求上下文)
  ret = fd_msg_send(req, magic_answer_callback, NULL);
  if (ret != 0) {
    cli_error("发送请求失败: %d", ret);
    return -1;
  }

  cli_success("请求已发送，等待服务器应答...");
  return 0;
}

int magic_get_result_code(struct msg *ans, uint32_t *result_code) {
  struct avp *avp = NULL;
  struct avp_hdr *hdr = NULL;

  if (!ans || !result_code)
    return -1;

  // 查找 Result-Code AVP
  CHECK_FCT(fd_msg_search_avp(ans, g_std_dict.avp_result_code, &avp));
  if (!avp) {
    cli_error("应答中缺少 Result-Code");
    return -1;
  }

  CHECK_FCT(fd_msg_avp_hdr(avp, &hdr));
  *result_code = hdr->avp_value->u32;

  return 0;
}

/**
 * 打印 MSXA 应答中的状态信息
 * v2.1: 增加权限降级检测
 * @param ans MSXA 应答消息
 */
void magic_print_status_info(struct msg *ans) {
  struct avp *avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  uint32_t granted_status_type = 0;
  bool found_status_type = false;

  if (!ans)
    return;

  cli_info("=== 系统状态信息 (MSXA v2.1) ===");

  // 第一遍: 查找 Status-Type AVP (10003)
  CHECK_FCT_DO(fd_msg_browse(ans, MSG_BRW_FIRST_CHILD, &avp, NULL), return);
  while (avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &avp_hdr), break);

    // Status-Type (10003)
    if (avp_hdr->avp_code == 10003 && (avp_hdr->avp_flags & AVP_FLAG_VENDOR)) {
      if (avp_hdr->avp_value) {
        granted_status_type = avp_hdr->avp_value->u32;
        found_status_type = true;
        cli_info("  Status-Type: %u (%s)", granted_status_type,
                 magic_get_subscribe_level_name(granted_status_type));

        /* v2.1: 检测权限降级 */
        if (g_last_msxr_requested_type > 0 &&
            granted_status_type < g_last_msxr_requested_type) {
          cli_warn("  ⚠ 权限降级! 请求=%u (%s) → 授予=%u (%s)",
                   g_last_msxr_requested_type,
                   magic_get_subscribe_level_name(g_last_msxr_requested_type),
                   granted_status_type,
                   magic_get_subscribe_level_name(granted_status_type));
          cli_warn("    您可能没有查看详细链路状态的权限");
        }
      }
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  // 第二遍: 遍历所有 AVP 查找状态相关信息
  CHECK_FCT_DO(fd_msg_browse(ans, MSG_BRW_FIRST_CHILD, &avp, NULL), return);
  while (avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &avp_hdr), break);

    switch (avp_hdr->avp_code) {
    case 20007: /* DLM-List (v2.1 标准) */
      parse_dlm_list(avp);
      break;

    case 20008: /* DLM-Info (v2.1 嵌套结构) */
      parse_dlm_info(avp);
      break;

    case 10041: /* Registered-Clients (10041) */
      if (avp_hdr->avp_value) {
        cli_info("  Registered-Clients: %.*s", (int)avp_hdr->avp_value->os.len,
                 (char *)avp_hdr->avp_value->os.data);
      }
      break;

    /* 兼容旧版或简单 AVP */
    case 10004: /* DLM-Name */
      if (avp_hdr->avp_value) {
        cli_info("  DLM-Name: %.*s", (int)avp_hdr->avp_value->os.len,
                 (char *)avp_hdr->avp_value->os.data);
      }
      break;

    case 10021: /* Link-Status (10021) */
      if (avp_hdr->avp_value) {
        uint32_t status = avp_hdr->avp_value->u32;
        cli_info("  Link-Status: %u (%s)", status,
                 status == 0   ? "离线"
                 : status == 1 ? "在线"
                 : status == 2 ? "连接中"
                               : "未知");
      }
      break;

    case 10006: /* DLM-Max-Bandwidth (10006) */
      if (avp_hdr->avp_value) {
        cli_info("  DLM-Max-BW: %llu bps",
                 (unsigned long long)avp_hdr->avp_value->u64);
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  cli_info("==================");
}

/**
 * @brief 解析 CDR-Info (AVP 20017) Grouped AVP
 * 内含 CDR-ID (10046) 和 CDR-Content (10047)
 */
static void parse_cdr_info(struct avp *cdr_info_avp, const char *status_label) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;
  uint32_t cdr_id = 0;
  char cdr_content[512] = {0};

  if (!cdr_info_avp)
    return;

  CHECK_FCT_DO(fd_msg_browse(cdr_info_avp, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    switch (hdr->avp_code) {
    case 10046: /* CDR-ID */
      if (hdr->avp_value) {
        cdr_id = hdr->avp_value->u32;
      }
      break;
    case 10047: /* CDR-Content */
      if (hdr->avp_value && hdr->avp_value->os.data) {
        size_t len = hdr->avp_value->os.len;
        if (len >= sizeof(cdr_content))
          len = sizeof(cdr_content) - 1;
        memcpy(cdr_content, hdr->avp_value->os.data, len);
        cdr_content[len] = '\0';
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  cli_info("  │ [%s] CDR-ID: %u", status_label, cdr_id);
  if (cdr_content[0]) {
    cli_info("  │     Content: %s", cdr_content);
  }
}

/**
 * @brief 解析 CDRs-Active/Finished/Forwarded/Unknown Grouped AVP
 * 内含多个 CDR-Info (20017) 子 AVP
 */
static void parse_cdrs_group(struct avp *cdrs_avp, const char *group_name,
                             const char *status_label) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;
  int count = 0;

  if (!cdrs_avp)
    return;

  cli_info("  ├─ %s:", group_name);

  CHECK_FCT_DO(fd_msg_browse(cdrs_avp, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    if (hdr->avp_code == 20017) { /* CDR-Info */
      parse_cdr_info(child, status_label);
      count++;
    } else if (hdr->avp_code == 10046) { /* CDRs-Unknown 直接包含 CDR-ID */
      if (hdr->avp_value) {
        cli_info("  │ [%s] CDR-ID: %u (unknown)", status_label,
                 hdr->avp_value->u32);
        count++;
      }
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  if (count == 0) {
    cli_info("  │   (无记录)");
  }
}

/**
 * 打印 MADA 应答中的 CDR 信息
 * @param ans MADA 应答消息
 *
 * v2.1 修复: 正确解析 Grouped AVP 层级结构
 * MADA → CDRs-Active(20012) → CDR-Info(20017) → CDR-ID(10046) /
 * CDR-Content(10047)
 */
void magic_print_cdr_info(struct msg *ans) {
  struct avp *avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  uint32_t cdr_type = 0, cdr_level = 0;

  if (!ans)
    return;

  cli_info("┌─────────────────────────────────────────────────────────┐");
  cli_info("│                    CDR 计费信息                         │");
  cli_info("├─────────────────────────────────────────────────────────┤");

  /* 遍历所有 AVP */
  CHECK_FCT_DO(fd_msg_browse(ans, MSG_BRW_FIRST_CHILD, &avp, NULL), return);

  while (avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &avp_hdr), break);

    switch (avp_hdr->avp_code) {
    case 10042: /* CDR-Type */
      if (avp_hdr->avp_value) {
        cdr_type = avp_hdr->avp_value->u32;
        cli_info("  CDR-Type: %u (%s)", cdr_type,
                 cdr_type == 1 ? "LIST_REQUEST" : "DATA_REQUEST");
      }
      break;

    case 10043: /* CDR-Level */
      if (avp_hdr->avp_value) {
        cdr_level = avp_hdr->avp_value->u32;
        cli_info("  CDR-Level: %u (%s)", cdr_level,
                 cdr_level == 1 ? "ALL"
                                : (cdr_level == 2 ? "USER_DEPENDENT"
                                                  : "SESSION_DEPENDENT"));
      }
      break;

    case 10044: /* CDR-Request-Identifier */
      if (avp_hdr->avp_value && avp_hdr->avp_value->os.data) {
        cli_info("  CDR-Request-Id: %.*s", (int)avp_hdr->avp_value->os.len,
                 (char *)avp_hdr->avp_value->os.data);
      }
      break;

    case 20012: /* CDRs-Active */
      parse_cdrs_group(avp, "CDRs-Active", "\033[32mACTIVE\033[0m");
      break;

    case 20013: /* CDRs-Finished */
      parse_cdrs_group(avp, "CDRs-Finished", "\033[33mFINISHED\033[0m");
      break;

    case 20014: /* CDRs-Forwarded */
      parse_cdrs_group(avp, "CDRs-Forwarded", "\033[34mFORWARDED\033[0m");
      break;

    case 20015: /* CDRs-Unknown */
      parse_cdrs_group(avp, "CDRs-Unknown", "\033[31mUNKNOWN\033[0m");
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  cli_info("└─────────────────────────────────────────────────────────┘");
}

/*
 * 解析 MACR/MACA 计费控制应答
 * 解析 CDRs-Updated → CDR-Start-Stop-Pair → CDR-Stopped/CDR-Started
 */
void magic_print_macr_result(struct msg *ans) {
  struct avp *avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  char restart_session_id[128] = "";
  uint32_t cdr_stopped = 0;
  uint32_t cdr_started = 0;
  bool found_cdrs_updated = false;

  if (!ans)
    return;

  cli_info("┌─────────────────────────────────────────────────────────┐");
  cli_info("│                 CDR 计费控制结果                        │");
  cli_info("├─────────────────────────────────────────────────────────┤");

  /* 遍历所有 AVP */
  CHECK_FCT_DO(fd_msg_browse(ans, MSG_BRW_FIRST_CHILD, &avp, NULL), return);

  while (avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &avp_hdr), break);

    switch (avp_hdr->avp_code) {
    case 10048: /* CDR-Restart-Session-Id */
      if (avp_hdr->avp_value && avp_hdr->avp_value->os.data) {
        size_t len = avp_hdr->avp_value->os.len;
        if (len >= sizeof(restart_session_id))
          len = sizeof(restart_session_id) - 1;
        memcpy(restart_session_id, avp_hdr->avp_value->os.data, len);
        restart_session_id[len] = '\0';
        cli_info("  目标会话: %s", restart_session_id);
      }
      break;

    case 20016: /* CDRs-Updated */
      found_cdrs_updated = true;
      {
        /* 遍历 CDRs-Updated 内的 CDR-Start-Stop-Pair */
        struct avp *pair_avp = NULL;
        CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_FIRST_CHILD, &pair_avp, NULL),
                     break);

        int pair_count = 0;
        while (pair_avp) {
          struct avp_hdr *pair_hdr = NULL;
          CHECK_FCT_DO(fd_msg_avp_hdr(pair_avp, &pair_hdr), break);

          if (pair_hdr->avp_code == 20018) { /* CDR-Start-Stop-Pair */
            pair_count++;
            cli_info("  ├── CDR 切分对 #%d:", pair_count);

            /* 遍历 CDR-Start-Stop-Pair 内的 CDR-Stopped 和 CDR-Started */
            struct avp *child_avp = NULL;
            CHECK_FCT_DO(
                fd_msg_browse(pair_avp, MSG_BRW_FIRST_CHILD, &child_avp, NULL),
                break);

            while (child_avp) {
              struct avp_hdr *child_hdr = NULL;
              CHECK_FCT_DO(fd_msg_avp_hdr(child_avp, &child_hdr), break);

              if (child_hdr->avp_value) {
                if (child_hdr->avp_code == 10049) { /* CDR-Stopped */
                  cdr_stopped = child_hdr->avp_value->u32;
                  cli_info("  │   ├── \033[33m旧CDR (已关闭)\033[0m: ID=%u",
                           cdr_stopped);
                } else if (child_hdr->avp_code == 10050) { /* CDR-Started */
                  cdr_started = child_hdr->avp_value->u32;
                  cli_info("  │   └── \033[32m新CDR (已启动)\033[0m: ID=%u",
                           cdr_started);
                }
              }

              CHECK_FCT_DO(
                  fd_msg_browse(child_avp, MSG_BRW_NEXT, &child_avp, NULL),
                  break);
            }
          }

          CHECK_FCT_DO(fd_msg_browse(pair_avp, MSG_BRW_NEXT, &pair_avp, NULL),
                       break);
        }

        if (pair_count > 0) {
          cli_success("  ✓ CDR 切分完成! 共 %d 对", pair_count);
        }
      }
      break;

    case 10001: /* MAGIC-Status-Code */
      if (avp_hdr->avp_value) {
        uint32_t status = avp_hdr->avp_value->u32;
        const char *status_desc = "";
        switch (status) {
        case 0:
          status_desc = "OK";
          break;
        case 1002:
          status_desc = "UNKNOWN_SESSION";
          break;
        case 1023:
          status_desc = "CDR_ACCESS_DENIED";
          break;
        default:
          status_desc = "UNKNOWN";
          break;
        }
        if (status != 0) {
          cli_error("  MAGIC-Status-Code: %u (%s)", status, status_desc);
        }
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  if (!found_cdrs_updated) {
    cli_warn("  (未找到 CDRs-Updated，切分可能失败)");
  }

  cli_info("└─────────────────────────────────────────────────────────┘");
}

/* ==================== MCAR 命令 (三场景测试版) ==================== */

/**
 * @brief MCAR 命令 - 客户端认证注册
 *
 * 支持三种场景:
 *   场景 A: mcar auth           - 纯认证 (仅 Client-Credentials)
 *   场景 B: mcar subscribe <n>  - 认证+订阅 (Client-Credentials +
 * REQ-Status-Info) 场景 C: mcar connect [profile] [bw_kbps] - 0-RTT接入
 * (Client-Credentials + Comm-Req-Params)
 */
int cmd_mcar(int argc, char **argv) {
  struct msg *req = NULL;
  struct session *sess = NULL;
  os0_t sid = NULL;
  size_t sid_len = 0;
  int ret;

  /* 确定请求场景 */
  typedef enum {
    MCAR_SCENARIO_AUTH = 0,  /* 场景 A: 纯认证 */
    MCAR_SCENARIO_SUBSCRIBE, /* 场景 B: 认证+订阅 */
    MCAR_SCENARIO_CONNECT    /* 场景 C: 0-RTT接入 */
  } McarScenario;

  McarScenario scenario = MCAR_SCENARIO_AUTH;
  uint32_t subscribe_level = 0;
  bool has_subscribe = false; /* 标记是否同时订阅 (B+C 组合) */

  /* 重置订阅状态 */
  g_requested_subscribe_level = 0;
  g_granted_subscribe_level = 0;

  /* 解析子命令 */
  if (argc >= 2) {
    if (strcmp(argv[1], "auth") == 0) {
      scenario = MCAR_SCENARIO_AUTH;
    } else if (strcmp(argv[1], "subscribe") == 0) {
      scenario = MCAR_SCENARIO_SUBSCRIBE;
      if (argc >= 3) {
        subscribe_level = atoi(argv[2]);
        /* 验证订阅级别 */
        if (!magic_validate_subscribe_level(subscribe_level)) {
          cli_error("无效的订阅级别: %u", subscribe_level);
          cli_info("有效值: 0=不订阅, 1=MAGIC, 2=DLM, 3=MAGIC_DLM, 6=DLM_LINK, "
                   "7=全部");
          return -1;
        }
      } else {
        subscribe_level = 3; /* 默认: MAGIC_DLM_Status */
      }
      has_subscribe = true;
    } else if (strcmp(argv[1], "connect") == 0) {
      scenario = MCAR_SCENARIO_CONNECT;
      int param_idx = 2;

      /* 解析 0-RTT 参数 */
      if (argc >= 3) {
        strncpy(g_cfg.profile_name, argv[2], sizeof(g_cfg.profile_name) - 1);
        param_idx = 3;
      }
      if (argc >= 4) {
        g_cfg.requested_bw =
            strtoull(argv[3], NULL, 10) * 1000;         /* kbps -> bps */
        g_cfg.requested_return_bw = g_cfg.requested_bw; /* 默认对称带宽 */
        param_idx = 4;
      }
      if (argc >= 5 && strcmp(argv[4], "subscribe") != 0) {
        g_cfg.requested_return_bw = strtoull(argv[4], NULL, 10) * 1000;
        param_idx = 5;
      }

      /* 检查是否有 subscribe 子参数 (B+C 组合场景) */
      for (int i = param_idx; i < argc; i++) {
        if (strcmp(argv[i], "subscribe") == 0) {
          has_subscribe = true;
          if (i + 1 < argc) {
            subscribe_level = atoi(argv[i + 1]);
            if (!magic_validate_subscribe_level(subscribe_level)) {
              cli_error("无效的订阅级别: %u", subscribe_level);
              cli_info("有效值: 0=不订阅, 1=MAGIC, 2=DLM, 3=MAGIC_DLM, "
                       "6=DLM_LINK, 7=全部");
              return -1;
            }
          } else {
            subscribe_level = 3; /* 默认 */
          }
          break;
        }
      }
    } else if (strcmp(argv[1], "create_session") == 0) {
      /* 兼容旧命令 */
      scenario = MCAR_SCENARIO_CONNECT;
    } else {
      cli_error("未知子命令: %s", argv[1]);
      goto show_usage;
    }
  }

  /* 记录请求的订阅级别 */
  if (has_subscribe) {
    g_requested_subscribe_level = subscribe_level;
  }

  /* 打印场景信息 */
  cli_info("╔══════════════════════════════════════════════╗");
  cli_info("║        MCAR - 客户端认证注册                ║");
  cli_info("╚══════════════════════════════════════════════╝");

  switch (scenario) {
  case MCAR_SCENARIO_AUTH:
    cli_info("📌 场景 A: 纯认证 (Auth Only)");
    cli_info("   → 仅携带 Client-Credentials");
    cli_info("   → 服务端: IDLE → AUTHENTICATED");
    break;
  case MCAR_SCENARIO_SUBSCRIBE:
    cli_info("📌 场景 B: 认证+订阅 (Auth + Subscribe)");
    cli_info("   → 携带 Client-Credentials + REQ-Status-Info");
    cli_info("   → 订阅级别: %u (%s)", subscribe_level,
             magic_get_subscribe_level_name(subscribe_level));
    cli_info("   → 服务端: IDLE → AUTHENTICATED, 后续接收 MSCR 推送");
    break;
  case MCAR_SCENARIO_CONNECT:
    if (has_subscribe) {
      cli_info("📌 场景 B+C: 0-RTT接入 + 订阅 (Zero-RTT + Subscribe)");
      cli_info(
          "   → 携带 Client-Credentials + Comm-Req-Params + REQ-Status-Info");
      cli_info("   → 订阅级别: %u (%s)", subscribe_level,
               magic_get_subscribe_level_name(subscribe_level));
    } else {
      cli_info("📌 场景 C: 0-RTT接入 (Zero-RTT Access)");
      cli_info(
          "   → 携带 Client-Credentials + Communication-Request-Parameters");
    }
    cli_info("   → 服务端: IDLE → AUTHENTICATED → ACTIVE (一步到位)");
    cli_info("   → Profile: %s, BW: ↓%llu/↑%llu kbps", g_cfg.profile_name,
             (unsigned long long)(g_cfg.requested_bw / 1000),
             (unsigned long long)(g_cfg.requested_return_bw / 1000));
    break;
  }
  cli_info("");

  SessionManager *mgr = cli_get_session_manager();

  /* 检查会话数量限制 */
  int active_count = session_manager_count_active(mgr);
  if (active_count >= MAX_CLIENT_SESSIONS) {
    cli_error("已达到最大会话数限制 (%d/%d)", active_count,
              MAX_CLIENT_SESSIONS);
    cli_info("请先终止某个会话: str <session_id>");
    return -1;
  }

  /* 1. 生成新的 Session-Id */
  char new_session_id[MAX_SESSION_ID_LEN];
  ret =
      session_manager_generate_id(mgr, new_session_id, sizeof(new_session_id));
  if (ret != 0) {
    cli_error("生成 Session-Id 失败");
    return -1;
  }

  cli_info("  新会话 Session-Id: %s", new_session_id);
  cli_info("  当前活动会话数: %d/%d", active_count + 1, MAX_CLIENT_SESSIONS);

  /* 2. 在会话管理器中创建会话记录 */
  ClientSessionRecord *session_rec =
      session_manager_create(mgr, new_session_id);
  if (!session_rec) {
    cli_error("创建会话记录失败");
    return -1;
  }

  /* 3. 创建 MCAR 请求消息 */
  cli_info("创建 MCAR 请求消息...");
  ret = fd_msg_new(g_magic_dict.cmd_mcar, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 MCAR 消息失败: %d", ret);
    session_manager_delete(mgr, new_session_id);
    return -1;
  }

  /* 4. 创建 Diameter 会话对象 */
  cli_info("创建 Diameter 会话对象...");
  ret = fd_msg_new_session(req, (os0_t) "magic", strlen("magic"));
  if (ret != 0) {
    cli_error("创建会话对象失败: %d", ret);
    fd_msg_free(req);
    session_manager_delete(mgr, new_session_id);
    return -1;
  }

  /* 5. 从消息中提取会话句柄 */
  ret = fd_msg_sess_get(fd_g_config->cnf_dict, req, &sess, NULL);
  if (ret != 0) {
    cli_error("获取会话句柄失败: %d", ret);
    fd_msg_free(req);
    session_manager_delete(mgr, new_session_id);
    return -1;
  }

  /* 6. 验证 Session-Id */
  ret = fd_sess_getsid(sess, &sid, &sid_len);
  if (ret == 0 && sid != NULL) {
    cli_info("  Diameter Session-Id: %s", (const char *)sid);
    strncpy(session_rec->session_id, (const char *)sid,
            sizeof(session_rec->session_id) - 1);
    session_manager_set_current(mgr, (const char *)sid);
    cli_set_session_id((const char *)sid);
  }

  /* 7. 添加必需的 Diameter 协议 AVP */
  cli_info("添加必需 AVP...");

  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);
  ADD_AVP_U32(req, g_std_dict.avp_auth_application_id, g_cfg.auth_app_id);

  if (g_cfg.destination_host[0] != '\0') {
    ADD_AVP_STR(req, g_std_dict.avp_destination_host, g_cfg.destination_host);
  }

  ADD_AVP_U32(req, g_std_dict.avp_auth_session_state,
              1); /* NO_STATE_MAINTAINED */

  /* 8. 添加 Client-Credentials (所有场景都需要) */
  if (g_cfg.client_password[0] != '\0') {
    cli_info("添加客户端凭证 (Client-Credentials)...");
    ret = add_client_credentials(req);
    if (ret != 0) {
      cli_warn("添加客户端凭证失败");
    }
  }

  /* 9. 场景 B 或 B+C: 添加 REQ-Status-Info */
  if (has_subscribe && subscribe_level > 0) {
    cli_info("添加状态订阅请求 (REQ-Status-Info = %u: %s)...", subscribe_level,
             magic_get_subscribe_level_name(subscribe_level));
    ADD_AVP_U32_V(req, g_magic_dict.avp_req_status_info, subscribe_level,
                  MAGIC_VENDOR_ID);
  }

  /* 10. 场景 C 或 B+C: 添加 Communication-Request-Parameters */
  if (scenario == MCAR_SCENARIO_CONNECT) {
    cli_info("添加通信请求参数 (Communication-Request-Parameters)...");
    ret = add_comm_req_params(req);
    if (ret != 0) {
      cli_warn("添加通信参数失败");
    }
  }

  /* 11. 发送请求 */
  cli_info("\n发送 MCAR 请求到服务器...");
  cli_info("  Origin-Host: %s", g_cfg.origin_host);
  cli_info("  Destination-Realm: %s", g_cfg.destination_realm);
  if (has_subscribe) {
    cli_info("  订阅级别: %u (%s)", subscribe_level,
             magic_get_subscribe_level_name(subscribe_level));
  }

  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    cli_success("MCAR 请求已发送！");
    cli_info("等待服务器应答...");
    if (has_subscribe) {
      cli_info("  (认证成功后将开始接收 MSCR 状态推送)");
    }
  } else {
    if (sess) {
      fd_sess_destroy(&sess);
    }
  }

  return ret;

show_usage:
  cli_info("");
  cli_info("╔══════════════════════════════════════════════════════════════╗");
  cli_info("║                 MCAR 命令使用说明                           ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║                                                              ║");
  cli_info("║ 场景 A: 纯认证                                              ║");
  cli_info("║   mcar auth                                                 ║");
  cli_info("║   → 仅携带 Client-Credentials                               ║");
  cli_info("║   → 服务端: IDLE → AUTHENTICATED                            ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 B: 认证+订阅                                           ║");
  cli_info("║   mcar subscribe <level>                                    ║");
  cli_info("║   → 订阅级别:                                               ║");
  cli_info("║     1 = MAGIC_Status (系统状态)                             ║");
  cli_info("║     2 = DLM_Status (DLM一般状态)                            ║");
  cli_info("║     3 = MAGIC_DLM_Status (综合状态) [默认]                  ║");
  cli_info("║     6 = DLM_Link_Status (详细链路状态)                      ║");
  cli_info("║     7 = All_Status (全部状态)                               ║");
  cli_info("║   → 服务端: IDLE → AUTHENTICATED, 后续接收 MSCR 推送       ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 C: 0-RTT快速接入                                       ║");
  cli_info("║   mcar connect <profile> <bw_kbps> [<ret_bw_kbps>]          ║");
  cli_info("║   示例: mcar connect IP_DATA 5000      (5Mbps对称)          ║");
  cli_info("║   示例: mcar connect VOICE 512 256     (非对称)             ║");
  cli_info("║   → 服务端: IDLE → ACTIVE (一步到位)                        ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 B+C: 0-RTT接入 + 订阅                                  ║");
  cli_info("║   mcar connect <profile> <bw> subscribe <level>             ║");
  cli_info("║   示例: mcar connect IP_DATA 5000 subscribe 3               ║");
  cli_info("║   → 同时建立通信并订阅状态推送                              ║");
  cli_info("║                                                              ║");
  cli_info("╚══════════════════════════════════════════════════════════════╝");
  return -1;
}

/* ==================== MCCR 命令 (四场景测试版) ==================== */

/**
 * @brief MCCR 命令 - 通信控制请求
 *
 * 支持四种场景:
 *   场景 A: mccr start [profile] [min_bw] [max_bw] [priority] [qos]
 *       - 前提: 会话状态 AUTHENTICATED
 *       - 服务端: AUTHENTICATED → ACTIVE
 *
 *   场景 B: mccr modify [min_bw] [max_bw] [priority] [qos]
 *       - 前提: 会话状态 ACTIVE
 *       - 服务端: ACTIVE → ACTIVE (参数变更)
 *
 *   场景 C: mccr stop
 *       - 前提: 会话状态 ACTIVE
 *       - 服务端: ACTIVE → AUTHENTICATED (释放链路)
 *
 *   场景 D: mccr queue [min_bw] [max_bw] [priority]
 *       - 设置 Keep-Request=1
 *       - 服务端: 资源不足时排队等待
 */
int cmd_mccr(int argc, char **argv) {
  struct msg *req = NULL;
  int ret;

  /* 确定请求场景 */
  typedef enum {
    MCCR_SCENARIO_START = 0, /* 场景 A: 启动通信 */
    MCCR_SCENARIO_MODIFY,    /* 场景 B: 修改参数 */
    MCCR_SCENARIO_STOP,      /* 场景 C: 停止通信 */
    MCCR_SCENARIO_QUEUE      /* 场景 D: 排队请求 */
  } MccxScenario;

  MccxScenario scenario = MCCR_SCENARIO_START;

  if (argc < 2) {
    goto show_usage;
  }

  /* 检查是否已注册 */
  if (!cli_is_registered()) {
    cli_error("客户端未注册！请先执行 'mcar' 命令注册");
    return -1;
  }

  const char *action = argv[1];

  /* 解析子命令 */
  if (strcmp(action, "start") == 0 || strcmp(action, "create") == 0) {
    scenario = MCCR_SCENARIO_START;

    /* 检查状态 */
    if (g_client_state == CLIENT_STATE_ACTIVE) {
      cli_warn("当前已有活动通信，将自动切换为 modify 模式");
      scenario = MCCR_SCENARIO_MODIFY;
    }

    /* 解析参数 */
    if (argc > 2) {
      strncpy(g_cfg.profile_name, argv[2], sizeof(g_cfg.profile_name) - 1);
    }
    if (argc > 3) {
      g_cfg.required_bw = strtoull(argv[3], NULL, 10) * 1000; /* kbps -> bps */
    }
    if (argc > 4) {
      g_cfg.requested_bw = strtoull(argv[4], NULL, 10) * 1000;
    } else {
      g_cfg.requested_bw =
          g_cfg.required_bw * 2; /* 默认请求带宽 = 2x 最低带宽 */
    }
    if (argc > 5) {
      g_cfg.priority_class = atoi(argv[5]);
    }
    if (argc > 6) {
      g_cfg.qos_level = atoi(argv[6]);
    }

    /* 设置上行带宽 (默认对称) */
    if (g_cfg.requested_return_bw == 0) {
      g_cfg.requested_return_bw = g_cfg.requested_bw;
      g_cfg.required_return_bw = g_cfg.required_bw;
    }

    /* Keep-Request = 0 (不排队) */
    g_cfg.keep_request = false;

  } else if (strcmp(action, "modify") == 0) {
    scenario = MCCR_SCENARIO_MODIFY;

    /* 检查状态 */
    if (g_client_state != CLIENT_STATE_ACTIVE) {
      cli_error("当前无活动通信！请先执行 'mccr start'");
      return -1;
    }

    /* 解析修改参数 */
    if (argc > 2) {
      g_cfg.required_bw = strtoull(argv[2], NULL, 10) * 1000;
    }
    if (argc > 3) {
      g_cfg.requested_bw = strtoull(argv[3], NULL, 10) * 1000;
    }
    if (argc > 4) {
      g_cfg.priority_class = atoi(argv[4]);
    }
    if (argc > 5) {
      g_cfg.qos_level = atoi(argv[5]);
    }

    g_cfg.keep_request = false;

  } else if (strcmp(action, "stop") == 0 || strcmp(action, "release") == 0) {
    scenario = MCCR_SCENARIO_STOP;

    /* 检查状态 */
    if (g_client_state != CLIENT_STATE_ACTIVE &&
        g_client_state != CLIENT_STATE_QUEUED) {
      cli_warn("当前无活动通信或排队请求");
    }

    /* 带宽设为 0 表示释放 */
    g_cfg.requested_bw = 0;
    g_cfg.requested_return_bw = 0;
    g_cfg.required_bw = 0;
    g_cfg.required_return_bw = 0;
    g_cfg.keep_request = false;

  } else if (strcmp(action, "queue") == 0) {
    scenario = MCCR_SCENARIO_QUEUE;

    /* 解析参数 */
    if (argc > 2) {
      g_cfg.required_bw = strtoull(argv[2], NULL, 10) * 1000;
    }
    if (argc > 3) {
      g_cfg.requested_bw = strtoull(argv[3], NULL, 10) * 1000;
    } else {
      g_cfg.requested_bw = g_cfg.required_bw;
    }
    if (argc > 4) {
      g_cfg.priority_class = atoi(argv[4]);
    }

    g_cfg.requested_return_bw = g_cfg.requested_bw;
    g_cfg.required_return_bw = g_cfg.required_bw;

    /* Keep-Request = 1 (允许排队) */
    g_cfg.keep_request = true;

  } else {
    cli_error("未知操作: %s", action);
    goto show_usage;
  }

  /* 打印场景信息 */
  cli_info("╔══════════════════════════════════════════════╗");
  cli_info("║        MCCR - 通信控制请求                  ║");
  cli_info("╚══════════════════════════════════════════════╝");

  switch (scenario) {
  case MCCR_SCENARIO_START:
    cli_info("📌 场景 A: OpenLink (启动通信)");
    cli_info("   → 当前状态: %s",
             g_client_state == CLIENT_STATE_AUTHENTICATED ? "AUTHENTICATED"
             : g_client_state == CLIENT_STATE_ACTIVE      ? "ACTIVE"
                                                          : "OTHER");
    cli_info("   → 目标状态: AUTHENTICATED → ACTIVE");
    cli_info("   → Profile: %s", g_cfg.profile_name);
    cli_info("   → 请求带宽: ↓%llu/↑%llu kbps (最低: ↓%llu/↑%llu kbps)",
             (unsigned long long)(g_cfg.requested_bw / 1000),
             (unsigned long long)(g_cfg.requested_return_bw / 1000),
             (unsigned long long)(g_cfg.required_bw / 1000),
             (unsigned long long)(g_cfg.required_return_bw / 1000));
    cli_info("   → 优先级: %u, QoS: %u", g_cfg.priority_class, g_cfg.qos_level);
    break;

  case MCCR_SCENARIO_MODIFY:
    cli_info("📌 场景 B: ChangeLink (修改参数)");
    cli_info("   → 当前状态: ACTIVE (链路: %s)", g_assigned_link_id);
    cli_info("   → 目标状态: ACTIVE (参数变更)");
    cli_info("   → 原带宽: ↓%.2f/↑%.2f kbps", g_granted_bw / 1000.0,
             g_granted_ret_bw / 1000.0);
    cli_info("   → 新请求: ↓%llu/↑%llu kbps",
             (unsigned long long)(g_cfg.requested_bw / 1000),
             (unsigned long long)(g_cfg.requested_return_bw / 1000));
    cli_info("   → 优先级: %u, QoS: %u", g_cfg.priority_class, g_cfg.qos_level);
    break;

  case MCCR_SCENARIO_STOP:
    cli_info("📌 场景 C: CloseLink (停止通信)");
    cli_info("   → 当前状态: %s",
             g_client_state == CLIENT_STATE_ACTIVE   ? "ACTIVE"
             : g_client_state == CLIENT_STATE_QUEUED ? "QUEUED"
                                                     : "OTHER");
    cli_info("   → 目标状态: ACTIVE → AUTHENTICATED");
    cli_info("   → 释放链路: %s",
             g_assigned_link_id[0] ? g_assigned_link_id : "(无)");
    break;

  case MCCR_SCENARIO_QUEUE:
    cli_info("📌 场景 D: QueueLink (排队请求)");
    cli_info("   → 设置 Keep-Request = 1 (允许排队等待)");
    cli_info("   → 请求带宽: ↓%llu/↑%llu kbps",
             (unsigned long long)(g_cfg.requested_bw / 1000),
             (unsigned long long)(g_cfg.requested_return_bw / 1000));
    cli_info("   → 优先级: %u (数字越高越优先)", g_cfg.priority_class);
    cli_info("   → 如果资源不足将进入排队队列");
    break;
  }
  cli_info("");

  /* 创建 MCCR 请求 */
  ret = fd_msg_new(g_magic_dict.cmd_mccr, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 MCCR 消息失败: %d", ret);
    return -1;
  }

  /* 添加必需 AVP */
  ADD_AVP_STR(req, g_std_dict.avp_session_id, cli_get_session_id());
  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);

  /* 添加 Communication-Request-Parameters */
  cli_info("添加 Communication-Request-Parameters...");
  cli_info("  Keep-Request: %s", g_cfg.keep_request ? "是" : "否");

  ret = add_comm_req_params(req);
  if (ret != 0) {
    cli_error("添加通信参数失败");
    fd_msg_free(req);
    return -1;
  }

  /* 发送请求 */
  cli_info("\n发送 MCCR 请求到服务器...");
  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    cli_success("MCCR 请求已发送！");
    cli_info("等待服务器应答...");
    cli_info("  (MAGIC 策略引擎将自动选择最优链路)");
  }

  return ret;

show_usage:
  cli_info("");
  cli_info("╔══════════════════════════════════════════════════════════════╗");
  cli_info("║               MCCR 命令使用说明                             ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║                                                              ║");
  cli_info("║ 📋 ARINC 839 介质无关性原则:                                ║");
  cli_info("║    客户端只提交业务需求，不能指定物理链路                   ║");
  cli_info("║    MAGIC 策略引擎将自动选择最优链路（Satcom/LTE/WiFi）      ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 A: 启动通信 (OpenLink)                                 ║");
  cli_info("║   mccr start [profile] [min_bw] [max_bw] [priority] [qos]   ║");
  cli_info("║   示例: mccr start IP_DATA 512 5000 2 1                     ║");
  cli_info("║         (数据业务,最小512kbps,最大5Mbps,优先级2,QoS1)       ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 B: 修改参数 (ChangeLink)                               ║");
  cli_info("║   mccr modify [min_bw] [max_bw] [priority] [qos]            ║");
  cli_info("║   示例: mccr modify 1024 10000 3 0                          ║");
  cli_info("║         (修改为最小1Mbps,最大10Mbps,优先级3,QoS0)           ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 C: 停止通信 (CloseLink)                                ║");
  cli_info("║   mccr stop                                                 ║");
  cli_info("║   (释放当前会话,所有资源自动回收)                           ║");
  cli_info("║                                                              ║");
  cli_info("╠══════════════════════════════════════════════════════════════╣");
  cli_info("║ 场景 D: 排队请求 (QueueLink)                                ║");
  cli_info("║   mccr queue [min_bw] [max_bw] [priority]                   ║");
  cli_info("║   示例: mccr queue 2000 4000 5                              ║");
  cli_info("║   (设置 Keep-Request=1,资源不足时排队等待)                  ║");
  cli_info("║                                                              ║");
  cli_info("╚══════════════════════════════════════════════════════════════╝");
  return -1;
}

/* ==================== MSXR 命令 ==================== */

int cmd_msxr(int argc, char **argv) {
  struct msg *req = NULL;
  int ret;
  uint32_t status_type = 7; // 默认查询全部状态 (All_Status)

  if (!cli_is_registered()) {
    cli_error("客户端未注册！请先执行 'mcar' 命令注册");
    return -1;
  }

  if (argc > 1) {
    status_type = atoi(argv[1]);
  }

  /* v2.1: 保存请求的 Status-Type 用于检测降级 */
  g_last_msxr_requested_type = status_type;

  cli_info("查询系统状态 (MSXR v2.1)...");
  cli_info("  Status-Type: %u (%s)", status_type,
           magic_get_subscribe_level_name(status_type));

  // 创建 MSXR 请求
  ret = fd_msg_new(g_magic_dict.cmd_msxr, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 MSXR 消息失败: %d", ret);
    return -1;
  }

  // 添加必需 AVP
  ADD_AVP_STR(req, g_std_dict.avp_session_id, cli_get_session_id());
  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);

  // Status-Type (REQUIRED)
  ADD_AVP_U32_V(req, g_magic_dict.avp_status_type, status_type,
                MAGIC_VENDOR_ID);

  // 发送请求
  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    cli_success("MSXR 请求已发送！");
    cli_info("等待服务器返回状态信息...");
  }

  return ret;
}

/* ==================== MADR 命令 ==================== */

int cmd_madr(int argc, char **argv) {
  struct msg *req = NULL;
  int ret;

  if (!cli_is_registered()) {
    cli_error("客户端未注册！请先执行 'mcar' 命令注册");
    return -1;
  }

  if (argc < 2) {
    cli_error("用法: madr list | madr data <cdr_id>");
    return -1;
  }

  const char *action = argv[1];
  uint32_t cdr_type = 1;  // 1=LIST_REQUEST, 2=DATA_REQUEST
  uint32_t cdr_level = 1; // 1=ALL, 2=USER_DEPENDENT, 3=SESSION_DEPENDENT

  if (strcmp(action, "list") == 0) {
    cli_info("查询 CDR 列表 (MADR List)...");
    cdr_type = 1;
    cdr_level = 1;
  } else if (strcmp(action, "data") == 0) {
    if (argc < 3) {
      cli_error("请指定 CDR ID");
      return -1;
    }
    cli_info("查询 CDR 详细数据 (MADR Data)...");
    cli_info("  CDR-ID: %s", argv[2]);
    cdr_type = 2;
    cdr_level = 3;
  } else {
    cli_error("未知操作: %s", action);
    return -1;
  }

  // 创建 MADR 请求
  ret = fd_msg_new(g_magic_dict.cmd_madr, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 MADR 消息失败: %d", ret);
    return -1;
  }

  // 添加必需 AVP
  ADD_AVP_STR(req, g_std_dict.avp_session_id, cli_get_session_id());
  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);

  // CDR-Type (REQUIRED)
  ADD_AVP_U32_V(req, g_magic_dict.avp_cdr_type, cdr_type, MAGIC_VENDOR_ID);

  // CDR-Level (REQUIRED)
  ADD_AVP_U32_V(req, g_magic_dict.avp_cdr_level, cdr_level, MAGIC_VENDOR_ID);

  // CDR-Request-Identifier (OPTIONAL)
  if (strcmp(action, "data") == 0 && argc >= 3) {
    ADD_AVP_STR_V(req, g_magic_dict.avp_cdr_req_id, argv[2], MAGIC_VENDOR_ID);
  }

  // 发送请求
  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    cli_success("MADR 请求已发送！");
  }

  return ret;
}

/* ==================== MACR 命令 ==================== */

int cmd_macr(int argc, char **argv) {
  struct msg *req = NULL;
  int ret;

  if (!cli_is_registered()) {
    cli_error("客户端未注册！请先执行 'mcar' 命令注册");
    return -1;
  }

  if (argc < 3 || strcmp(argv[1], "restart") != 0) {
    cli_error("用法: macr restart <session_id>");
    return -1;
  }

  const char *restart_sid = argv[2];

  cli_info("重启 CDR (MACR)...");
  cli_info("  Session-Id: %s", restart_sid);

  // 创建 MACR 请求
  ret = fd_msg_new(g_magic_dict.cmd_macr, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 MACR 消息失败: %d", ret);
    return -1;
  }

  // 添加必需 AVP
  ADD_AVP_STR(req, g_std_dict.avp_session_id, cli_get_session_id());
  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);

  if (g_cfg.destination_host[0] != '\0') {
    ADD_AVP_STR(req, g_std_dict.avp_destination_host, g_cfg.destination_host);
  }

  // CDR-Restart-Session-Id (REQUIRED)
  ADD_AVP_STR_V(req, g_magic_dict.avp_cdr_restart_sess_id, restart_sid,
                MAGIC_VENDOR_ID);

  // 发送请求
  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    cli_success("MACR 请求已发送！");
  }

  return ret;
}

/* ==================== STR 命令 ==================== */

int cmd_str(int argc, char **argv) {
  struct msg *req = NULL;
  int ret;
  uint32_t termination_cause = DIAMETER_LOGOUT;
  char target_session_id[MAX_SESSION_ID_LEN] = {0};

  if (!cli_is_registered()) {
    cli_warn("客户端未注册，无需终止会话");
    return 0;
  }

  /* 解析参数: str [session_id] [cause] 或 str [cause] */
  if (argc > 1) {
    /* 检查第一个参数是否为数字 (cause) */
    if (isdigit(argv[1][0])) {
      termination_cause = atoi(argv[1]);
      /* 使用当前会话 */
      const char *curr = session_manager_get_current(&g_session_manager);
      if (curr)
        strncpy(target_session_id, curr, sizeof(target_session_id) - 1);
    } else {
      /* 第一个参数是 session_id */
      strncpy(target_session_id, argv[1], sizeof(target_session_id) - 1);
      if (argc > 2) {
        termination_cause = atoi(argv[2]);
      }
    }
  } else {
    /* 无参数，使用当前会话 */
    const char *curr = session_manager_get_current(&g_session_manager);
    if (curr)
      strncpy(target_session_id, curr, sizeof(target_session_id) - 1);
  }

  if (target_session_id[0] == '\0') {
    cli_error("未指定会话ID，且当前无活动会话");
    return -1;
  }

  cli_info("终止 Diameter 会话 (STR)...");
  cli_info("  Termination-Cause: %u", termination_cause);
  cli_info("  Session-Id: %s", target_session_id);

  // 创建 STR 请求（标准 Diameter 基本协议命令）
  struct dict_object *cmd_str = NULL;
  struct dict_cmd_data cmd_data = {275, // STR Command-Code
                                   "Session-Termination-Request",
                                   CMD_FLAG_REQUEST, CMD_FLAG_REQUEST};

  // 查找或创建 STR 命令对象
  ret = fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME,
                       "Session-Termination-Request", &cmd_str, ENOENT);
  if (ret != 0) {
    cli_error("查找 STR 命令失败");
    return -1;
  }

  ret = fd_msg_new(cmd_str, MSGFL_ALLOC_ETEID, &req);
  if (ret != 0) {
    cli_error("创建 STR 消息失败: %d", ret);
    return -1;
  }

  // 设置消息头 Application-ID (MAGIC Application ID: 16777300)
  struct msg_hdr *hdr = NULL;
  ret = fd_msg_hdr(req, &hdr);
  if (ret == 0 && hdr) {
    hdr->msg_appl = g_cfg.auth_app_id; // 16777300
  }

  // 添加必需 AVP
  ADD_AVP_STR(req, g_std_dict.avp_session_id, target_session_id);
  ADD_AVP_STR(req, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(req, g_std_dict.avp_origin_realm, g_cfg.origin_realm);
  ADD_AVP_STR(req, g_std_dict.avp_destination_realm, g_cfg.destination_realm);
  ADD_AVP_U32(req, g_std_dict.avp_auth_application_id, g_cfg.auth_app_id);

  // Termination-Cause (REQUIRED)
  ADD_AVP_U32(req, g_std_dict.avp_termination_cause, termination_cause);

  // 发送请求
  ret = magic_send_request(&req, NULL, 0);
  if (ret == 0) {
    /* 从管理器中删除会话 */
    session_manager_delete(&g_session_manager, target_session_id);

    /* 检查是否还有其他会话 */
    if (session_manager_count_active(&g_session_manager) == 0) {
      cli_set_registered(false);
      cli_set_session_active(false);
      cli_set_session_id(NULL);
    } else {
      /* 如果当前会话被删除了（变为空），尝试自动切换到第一个活跃会话 */
      const char *curr = session_manager_get_current(&g_session_manager);
      if (!curr || curr[0] == '\0') {
        /* 遍历查找第一个活跃会话 */
        for (int i = 0; i < MAX_CLIENT_SESSIONS; i++) {
          if (g_session_manager.sessions[i].in_use) {
            session_manager_set_current(
                &g_session_manager, g_session_manager.sessions[i].session_id);
            cli_set_session_id(g_session_manager.sessions[i].session_id);
            cli_info("自动切换当前会话为: %s",
                     g_session_manager.sessions[i].session_id);
            break;
          }
        }
      }
    }
    cli_success("STR 请求已发送！会话已终止");
  }

  return ret;
}

/* ==================== SESSION 命令 (v2.2) ==================== */

int cmd_session(int argc, char **argv) {
  if (argc < 2) {
    cli_error("用法: session list | session select <id>");
    return -1;
  }

  if (strcmp(argv[1], "list") == 0) {
    session_manager_list_active(&g_session_manager);
  } else if (strcmp(argv[1], "select") == 0) {
    if (argc < 3) {
      cli_error("请指定 Session-Id");
      return -1;
    }
    if (session_manager_set_current(&g_session_manager, argv[2]) == 0) {
      cli_success("当前会话已切换为: %s", argv[2]);
      cli_set_session_id(argv[2]); /* 同步到旧的全局变量 */
    } else {
      cli_error("找不到会话: %s", argv[2]);
    }
  } else {
    cli_error("未知子命令: %s", argv[1]);
  }
  return 0;
}

/* ==================== SHOW DLM 命令 (v2.1) ==================== */

/**
 * cmd_show_dlm - 显示 DLM 状态表
 * 用法: show dlm | show
 *
 * 显示从 MSCR 收集的 DLM 硬件状态信息，包括：
 * - DLM 可用性 (Available/Unavailable)
 * - 链路连接状态 (Connected/Disconnected)
 * - 信号强度 (dBm)
 * - 带宽分配情况
 */
int cmd_show_dlm(int argc, char **argv) {
  (void)argc;
  (void)argv;

  cli_info("DLM 状态表 (数据来自 MSCR 推送):");
  dlm_status_print_all();

  return 0;
}

/* ==================== STATUS 命令 (增强版) ==================== */

int cmd_status(int argc, char **argv) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════╗\n");
  printf("║              MAGIC Client Status                        ║\n");
  printf("╚══════════════════════════════════════════════════════════╝\n\n");

  /* 客户端状态 */
  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 客户端状态                                              │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");

  const char *state_str = "UNKNOWN";
  const char *state_color = "\033[33m"; /* 黄色 */
  switch (g_client_state) {
  case CLIENT_STATE_IDLE:
    state_str = "IDLE (未认证)";
    state_color = "\033[31m"; /* 红色 */
    break;
  case CLIENT_STATE_AUTHENTICATED:
    state_str = "AUTHENTICATED (已认证)";
    state_color = "\033[33m"; /* 黄色 */
    break;
  case CLIENT_STATE_ACTIVE:
    state_str = "ACTIVE (通信中)";
    state_color = "\033[32m"; /* 绿色 */
    break;
  case CLIENT_STATE_QUEUED:
    state_str = "QUEUED (排队中)";
    state_color = "\033[36m"; /* 青色 */
    break;
  }
  printf("│ 状态: %s%s\033[0m\n", state_color, state_str);

  if (cli_is_registered()) {
    printf("│ Session-Id: %s\n", cli_get_session_id());
  }
  printf("└─────────────────────────────────────────────────────────┘\n\n");

  /* 通信链路信息 */
  if (g_client_state == CLIENT_STATE_ACTIVE || g_assigned_link_id[0]) {
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│ 通信链路信息                                            │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    printf("│ 链路 ID: \033[32m%s\033[0m\n",
           g_assigned_link_id[0] ? g_assigned_link_id : "(无)");
    printf("│ Bearer-ID: %u\n", g_bearer_id);
    printf("│ 网关 IP: %s\n", g_gateway_ip[0] ? g_gateway_ip : "(未分配)");
    printf("│ 授予带宽: ↓%.2f kbps / ↑%.2f kbps\n", g_granted_bw / 1000.0,
           g_granted_ret_bw / 1000.0);
    if (g_session_timeout > 0) {
      printf("│ 会话超时: %u 秒\n", g_session_timeout);
    }
    printf("└─────────────────────────────────────────────────────────┘\n\n");
  }

  /* 客户端身份 */
  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 客户端身份                                              │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");
  printf("│ Client-ID: %s\n", g_cfg.client_id);
  printf("│ Origin-Host: %s\n", g_cfg.origin_host);
  printf("│ Origin-Realm: %s\n", g_cfg.origin_realm);
  if (g_cfg.tail_number[0]) {
    printf("│ Tail-Number: %s\n", g_cfg.tail_number);
  }
  if (g_cfg.aircraft_type[0]) {
    printf("│ Aircraft-Type: %s\n", g_cfg.aircraft_type);
  }
  printf("└─────────────────────────────────────────────────────────┘\n\n");

  /* 当前 QoS 配置 */
  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 当前 QoS 配置                                           │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");
  printf("│ Profile-Name: %s\n", g_cfg.profile_name);
  printf("│ 请求带宽: ↓%llu / ↑%llu kbps\n",
         (unsigned long long)(g_cfg.requested_bw / 1000),
         (unsigned long long)(g_cfg.requested_return_bw / 1000));
  printf("│ 最低带宽: ↓%llu / ↑%llu kbps\n",
         (unsigned long long)(g_cfg.required_bw / 1000),
         (unsigned long long)(g_cfg.required_return_bw / 1000));
  printf("│ QoS-Level: %u (0=尽力, 1=保证, 2=实时, 3=控制)\n", g_cfg.qos_level);
  printf("│ Priority-Class: %u (1=最高, 8=最低)\n", g_cfg.priority_class);
  printf("│ Keep-Request: %s\n", g_cfg.keep_request ? "是 (允许排队)" : "否");
  printf("└─────────────────────────────────────────────────────────┘\n\n");

  /* 可用命令提示 */
  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 下一步操作建议                                          │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");
  switch (g_client_state) {
  case CLIENT_STATE_IDLE:
    printf("│ → 使用 'mcar auth' 进行纯认证                          │\n");
    printf("│ → 使用 'mcar connect IP_DATA 5000' 进行 0-RTT 接入    │\n");
    break;
  case CLIENT_STATE_AUTHENTICATED:
    printf("│ → 使用 'mccr start IP_DATA 512 5000' 建立通信链路     │\n");
    printf("│ → 使用 'str' 终止认证会话                             │\n");
    break;
  case CLIENT_STATE_ACTIVE:
    printf("│ → 使用 'mccr modify 1024 10000' 修改带宽              │\n");
    printf("│ → 使用 'mccr stop' 释放通信链路                       │\n");
    printf("│ → 使用 'msxr' 查询系统状态                            │\n");
    break;
  case CLIENT_STATE_QUEUED:
    printf("│ → 等待资源可用...                                      │\n");
    printf("│ → 使用 'mccr stop' 取消排队                           │\n");
    break;
  }
  printf("└─────────────────────────────────────────────────────────┘\n\n");

  return 0;
}

/* ==================== CONFIG 命令 ==================== */

int cmd_config(int argc, char **argv) {
  if (argc < 2 || strcmp(argv[1], "show") == 0) {
    // 显示配置（与 status 类似但更详细）
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║          Configuration Details              ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("Diameter 配置:\n");
    printf("  Vendor-ID: %u\n", g_cfg.vendor_id);
    printf("  Auth-App-ID: %u\n", g_cfg.auth_app_id);
    printf("  Destination-Realm: %s\n", g_cfg.destination_realm);
    if (g_cfg.destination_host[0]) {
      printf("  Destination-Host: %s\n", g_cfg.destination_host);
    }

    printf("\n带宽配置:\n");
    printf("  Requested: ↓%llu / ↑%llu bps\n",
           (unsigned long long)g_cfg.requested_bw,
           (unsigned long long)g_cfg.requested_return_bw);
    printf("  Required: ↓%llu / ↑%llu bps\n",
           (unsigned long long)g_cfg.required_bw,
           (unsigned long long)g_cfg.required_return_bw);

    printf("\n策略配置:\n");
    printf("  QoS-Level: %u\n", g_cfg.qos_level);
    printf("  Priority-Type: %u\n", g_cfg.priority_type);
    printf("  Priority-Class: %u\n", g_cfg.priority_class);
    printf("  Accounting: %s\n", g_cfg.accounting_enabled ? "启用" : "禁用");

    printf("\n会话配置:\n");
    printf("  Timeout: %u 秒\n", g_cfg.timeout);
    printf("  Keep-Request: %s\n", g_cfg.keep_request ? "是" : "否");
    printf("  Auto-Detect: %s\n", g_cfg.auto_detect ? "是" : "否");

    printf("\n");
  } else if (strcmp(argv[1], "reload") == 0) {
    cli_info("重新加载配置文件...");
    // TODO: 实现配置重载
    cli_warn("配置重载功能尚未实现");
  } else {
    cli_error("未知操作: %s", argv[1]);
    cli_info("用法: config [show|reload]");
    return -1;
  }

  return 0;
}

/* ==================== QUIT 命令 ==================== */

int cmd_quit(int argc, char **argv) {
  cli_info("退出 MAGIC 客户端...");

  // 如果有活跃会话，先终止
  if (cli_is_registered()) {
    cli_info("检测到活跃会话，正在发送 STR...");
    cmd_str(0, NULL);
    sleep(1);
  }

  cli_success("再见！");
  exit(0);
  return 0;
}

/* ============================================================================
 *                      MSCR/MNTR 服务器推送消息处理器
 * ============================================================================
 *
 * MSCR (MAGIC-Status-Change-Report): 服务器推送状态变更通知
 *   - 当订阅了状态通知后(mcar subscribe)，服务器会推送此消息
 *   - 包含 DLM-List, Link-List, Communication-Report-Parameters 等
 *   - 客户端需发送 MSCA (Answer) 确认
 *
 * MNTR (MAGIC-Notification-Request): 服务器推送会话通知 (ARINC 839 §4.1.3.3)
 *   - 服务器主动通知客户端会话状态变更
 *   - 根据 ARINC 839 规范，MNTR 使用 MAGIC-Status-Code 标识通知原因:
 *       0    = SUCCESS (成功/带宽增加，通过 Granted-Bandwidth 传递新值)
 *       1016 = NO_FREE_BANDWIDTH (带宽不足/被抢占)
 *       2007 = LINK_ERROR (链路错误/丢失)
 *       2010 = FORCED_REROUTING (链路切换/强制重路由)
 *   - 包含 Communication-Report-Parameters 传递变更后的参数
 *   - 客户端需发送 MNTA (Answer) 确认
 *
 * v2.2: 符合 ARINC 839 规范 - 使用 MAGIC-Status-Code 替代自定义
 * Notification-Type
 * ============================================================================
 */

#include "session_manager.h" /* DLMStatusRecord, g_dlm_status_mgr */

/* MSCR Command Code: 100003 */
#define CMD_MSCR_CODE 100003
/* MNTR Command Code: 100002 */
#define CMD_MNTR_CODE 100002

/* 存储上一次收到的 DLM 状态信息 */
static uint32_t g_last_dlm_status = 0;
static uint32_t g_last_link_count = 0;
static char g_last_status_time[64] = {0};

/* ==================== MSCR v2.1 增强解析函数 ==================== */

/**
 * 解析 Link-Status-Group (AVP 20011) 嵌套结构
 * @param avp_link_group  Link-Status-Group AVP
 * @param dlm_rec         DLM 状态记录 (输出到此)
 */
static void parse_link_status_group(struct avp *avp_link_group,
                                    DLMStatusRecord *dlm_rec) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;

  if (!avp_link_group || !dlm_rec)
    return;
  if (dlm_rec->link_count >= MAX_LINKS_PER_DLM) {
    cli_warn("  警告: 链路数超过最大限制 (%u)", MAX_LINKS_PER_DLM);
    return;
  }

  LinkStatusRecord *lnk = &dlm_rec->links[dlm_rec->link_count];
  memset(lnk, 0, sizeof(LinkStatusRecord));

  CHECK_FCT_DO(fd_msg_browse(avp_link_group, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    switch (hdr->avp_code) {
    case 10054: /* Link-Name */
      if (hdr->avp_value->os.data && hdr->avp_value->os.len > 0) {
        size_t len = hdr->avp_value->os.len < 63 ? hdr->avp_value->os.len : 63;
        memcpy(lnk->link_name, hdr->avp_value->os.data, len);
        lnk->link_name[len] = '\0';
      }
      break;
    case 10012: /* Link-Number */
      lnk->link_number = hdr->avp_value->u32;
      break;
    case 10013: /* Link-Available */
      lnk->link_available = hdr->avp_value->u32;
      break;
    case 10014: /* Link-Connection-Status */
      lnk->link_conn_status = hdr->avp_value->u32;
      break;
    case 10015: /* Link-Login-Status */
      lnk->link_login_status = hdr->avp_value->u32;
      break;
    case 10020: /* Link-Error-String */
      if (hdr->avp_value->os.data && hdr->avp_value->os.len > 0) {
        size_t len =
            hdr->avp_value->os.len < 127 ? hdr->avp_value->os.len : 127;
        memcpy(lnk->error_string, hdr->avp_value->os.data, len);
        lnk->error_string[len] = '\0';
      }
      break;
    case 10016: /* Max-Bandwidth */
      lnk->max_bw_kbps = hdr->avp_value->u64;
      break;
    case 10018: /* Allocated-Bandwidth */
      lnk->alloc_bw_kbps = hdr->avp_value->u64;
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  dlm_rec->link_count++;

  /* 详细日志输出 */
  const char *conn_str =
      (lnk->link_conn_status == 0)   ? "\033[31mDISCONNECTED\033[0m"
      : (lnk->link_conn_status == 1) ? "\033[32mCONNECTED\033[0m"
      : (lnk->link_conn_status == 2) ? "\033[33mFORCED_OFF\033[0m"
                                     : "UNKNOWN";
  cli_info("  │   Link[%u] %-16s %s", lnk->link_number,
           lnk->link_name[0] ? lnk->link_name : "unnamed", conn_str);
  if (lnk->error_string[0]) {
    cli_info("  │     Error: %s", lnk->error_string);
  }
}

/**
 * 解析 DLM-Link-Status-List (AVP 20010) 容器
 * 内含多个 Link-Status-Group
 */
static void parse_dlm_link_status_list(struct avp *avp_list,
                                       DLMStatusRecord *dlm_rec) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;

  if (!avp_list || !dlm_rec)
    return;

  CHECK_FCT_DO(fd_msg_browse(avp_list, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    if (hdr->avp_code == 20011) { /* Link-Status-Group */
      parse_link_status_group(child, dlm_rec);
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }
}

/**
 * 解析 DLM-Info (AVP 20008) 嵌套结构
 * 包含 DLM 元信息及 Link-Status-List
 */
static void parse_dlm_info(struct avp *avp_dlm_info) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;
  char dlm_name[64] = {0};

  if (!avp_dlm_info)
    return;

  cli_info("  ┌─ DLM-Info ─────────────────────────────────────────────┐");

  /* 第一遍: 获取 DLM-Name 以创建/查找记录 */
  CHECK_FCT_DO(fd_msg_browse(avp_dlm_info, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);
  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);
    if (hdr->avp_code == 10004) { /* DLM-Name */
      size_t len = hdr->avp_value->os.len < 63 ? hdr->avp_value->os.len : 63;
      memcpy(dlm_name, hdr->avp_value->os.data, len);
      dlm_name[len] = '\0';
      break;
    }
    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  if (dlm_name[0] == '\0') {
    strncpy(dlm_name, "Unknown-DLM", 63);
  }

  DLMStatusRecord *dlm_rec = dlm_status_find_or_create(dlm_name);
  if (!dlm_rec) {
    cli_warn("  无法创建 DLM 状态记录: %s", dlm_name);
    return;
  }

  /* 重置链路列表准备更新 */
  dlm_rec->link_count = 0;
  memset(dlm_rec->links, 0, sizeof(dlm_rec->links));

  /* 第二遍: 解析所有字段 */
  CHECK_FCT_DO(fd_msg_browse(avp_dlm_info, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);
  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    switch (hdr->avp_code) {
    case 10004: /* DLM-Name */
      cli_info("  │ DLM-Name: %s", dlm_name);
      break;

    case 10005: /* DLM-Available */
    {
      uint32_t avail = hdr->avp_value->u32;
      bool changed = dlm_status_update_available(dlm_name, avail);
      const char *avail_str = (avail == 0)   ? "\033[32mAVAILABLE\033[0m"
                              : (avail == 1) ? "\033[31mUNAVAILABLE\033[0m"
                                             : "UNKNOWN";
      cli_info("  │ DLM-Available: %s%s", avail_str,
               changed ? " (CHANGED!)" : "");
    } break;

    case 10010: /* DLM-Max-Links */
      dlm_rec->dlm_max_links = hdr->avp_value->u32;
      cli_info("  │ Max-Links: %u", dlm_rec->dlm_max_links);
      break;

    case 10011: /* DLM-Allocated-Links */
      dlm_rec->dlm_alloc_links = hdr->avp_value->u32;
      cli_info("  │ Allocated-Links: %u", dlm_rec->dlm_alloc_links);
      break;

    case 10006: /* DLM-Max-Forward-Bandwidth */
      dlm_rec->dlm_max_bw_kbps = (float)hdr->avp_value->u64 / 1000.0f;
      cli_info("  │ Max-Forward-BW: %.1f kbps", dlm_rec->dlm_max_bw_kbps);
      break;

    case 10007: /* DLM-Allocated-Forward-Bandwidth */
      dlm_rec->dlm_alloc_bw_kbps = (float)hdr->avp_value->u64 / 1000.0f;
      cli_info("  │ Allocated-Forward-BW: %.1f kbps",
               dlm_rec->dlm_alloc_bw_kbps);
      break;

    case 10008: /* DLM-Max-Return-Bandwidth */
      dlm_rec->dlm_max_ret_bw_kbps = (float)hdr->avp_value->u64 / 1000.0f;
      break;

    case 10009: /* DLM-Allocated-Return-Bandwidth */
      dlm_rec->dlm_alloc_ret_bw_kbps = (float)hdr->avp_value->u64 / 1000.0f;
      break;

    case 20010: /* DLM-Link-Status-List */
      cli_info("  │ Link-Status-List:");
      parse_dlm_link_status_list(child, dlm_rec);
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  dlm_rec->last_update = time(NULL);
  cli_info("  └──────────────────────────────────────────────────────────┘");
}

/**
 * 解析并显示 DLM-List AVP 内容 (兼容旧版格式)
 */
static void parse_dlm_list(struct avp *avp_dlm_list) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;
  uint32_t dlm_id = 0;
  uint32_t dlm_status = 0;
  char dlm_name[64] = {0};

  cli_info("  ┌─ DLM-List ─────────────────────────────────────────────┐");

  CHECK_FCT_DO(fd_msg_browse(avp_dlm_list, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    if (hdr->avp_code == 10010) { /* DLM-Max-Links (used as ID in legacy) */
      dlm_id = hdr->avp_value->u32;
    } else if (hdr->avp_code == 10004) { /* DLM-Name */
      size_t len = hdr->avp_value->os.len < 63 ? hdr->avp_value->os.len : 63;
      memcpy(dlm_name, hdr->avp_value->os.data, len);
      dlm_name[len] = '\0';
    } else if (hdr->avp_code == 10005) { /* DLM-Available */
      dlm_status = hdr->avp_value->u32;
      const char *status_str = "UNKNOWN";
      const char *color = "\033[33m";
      switch (dlm_status) {
      case 0:
        status_str = "OFFLINE/UNAVAILABLE";
        color = "\033[31m";
        break;
      case 1:
        status_str = "ONLINE/AVAILABLE";
        color = "\033[32m";
        break;
      }
      cli_info("  │ DLM %s: %s%s\033[0m", dlm_name[0] ? dlm_name : "-", color,
               status_str);
      g_last_dlm_status = dlm_status;

      /* 更新到 DLM 状态管理器 */
      if (dlm_name[0]) {
        dlm_status_update_available(dlm_name, (dlm_status == 1) ? 1 : 0);
      }
    } else if (hdr->avp_code == 20008) { /* DLM-Info (嵌套结构) */
      parse_dlm_info(child);
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  cli_info("  └──────────────────────────────────────────────────────────┘");
}

/**
 * 解析并显示 Link-List AVP 内容
 */
static void parse_link_list(struct avp *avp_link_list) {
  struct avp *child = NULL;
  struct avp_hdr *hdr = NULL;
  uint32_t link_count = 0;

  cli_info("  ┌─ Link-List ────────────────────────────────────────────┐");

  CHECK_FCT_DO(fd_msg_browse(avp_link_list, MSG_BRW_FIRST_CHILD, &child, NULL),
               return);

  while (child) {
    CHECK_FCT_DO(fd_msg_avp_hdr(child, &hdr), break);

    if (hdr->avp_code == 10020) { /* Link-ID */
      char link_id[65] = {0};
      size_t len = hdr->avp_value->os.len < 64 ? hdr->avp_value->os.len : 64;
      memcpy(link_id, hdr->avp_value->os.data, len);

      const char *status = "\033[32m在线\033[0m"; /* 默认在线 */
      cli_info("  │ Link #%u: %s [%s]", ++link_count, link_id, status);
    } else if (hdr->avp_code == 10021) { /* Link-Status */
      uint32_t link_status = hdr->avp_value->u32;
      const char *status_str = "UNKNOWN";
      switch (link_status) {
      case 0:
        status_str = "DOWN";
        break;
      case 1:
        status_str = "UP";
        break;
      case 2:
        status_str = "CONGESTED";
        break;
      }
      cli_info("  │   Status: %s", status_str);
    } else if (hdr->avp_code == 10022) { /* Available-Bandwidth */
      uint64_t bw = hdr->avp_value->u64;
      cli_info("  │   Available BW: %.2f kbps", bw / 1000.0);
    }

    CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
  }

  g_last_link_count = link_count;
  cli_info("  │ 总链路数: %u", link_count);
  cli_info("  └──────────────────────────────────────────────────────────┘");
}

/**
 * 发送 MSCA 响应 (自动应答)
 *
 * @param mscr_req_ptr 指向 MSCR 请求消息指针的指针
 * @return 0 成功，非0失败
 *
 * 注意：此函数会修改 *mscr_req_ptr，将其置为 NULL
 */
static int send_msca_response(struct msg **mscr_req_ptr) {
  struct msg *ans = NULL;
  struct msg_hdr *hdr = NULL;
  int ret;

  if (!mscr_req_ptr || !*mscr_req_ptr) {
    cli_error("MSCR 请求消息为空，无法发送应答");
    return -1;
  }

  /* 创建应答消息 */
  ret = fd_msg_new_answer_from_req(fd_g_config->cnf_dict, mscr_req_ptr, 0);
  if (ret != 0) {
    cli_error("创建 MSCA 应答失败: %d", ret);
    return ret;
  }

  ans = *mscr_req_ptr; /* 注意: new_answer_from_req 会修改指针 */

  /* 添加 Result-Code = 2001 (SUCCESS) */
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);

  /* 添加 Origin-Host 和 Origin-Realm */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, g_cfg.origin_realm);

  /* 发送应答 */
  ret = fd_msg_send(&ans, NULL, NULL);
  if (ret != 0) {
    cli_error("发送 MSCA 应答失败: %d", ret);
    if (ans) {
      fd_msg_free(ans);
      *mscr_req_ptr = NULL;
    }
    return ret;
  }

  /* fd_msg_send 成功后会自动设置 ans 为 NULL */
  *mscr_req_ptr = NULL;

  cli_success("→ 已发送 MSCA 确认应答 (Result-Code=2001)");
  return 0;
}

/**
 * MSCR (MAGIC-Status-Change-Report) 处理回调
 * v2.1: 增强版本，支持 DLM-Info/Registered-Clients/Link-Status-List
 */
static int mscr_handler_callback(struct msg **msg, struct avp *avp,
                                 struct session *session, void *opaque,
                                 enum disp_action *act) {
  struct msg_hdr *hdr = NULL;
  struct avp *cur_avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);

  (void)avp;
  (void)session;
  (void)opaque;

  strftime(g_last_status_time, sizeof(g_last_status_time), "%Y-%m-%d %H:%M:%S",
           tm_info);

  /* 更新 MSCR 接收时间戳 */
  g_dlm_status_mgr.last_mscr_time = now;

  cli_info("%s", "");
  cli_info("╔══════════════════════════════════════════════════════════════╗");
  cli_info("║  📡 收到 MSCR 状态变更推送 (v2.1)                            ║");
  cli_info("║  时间: %s                              ║", g_last_status_time);
  cli_info("╠══════════════════════════════════════════════════════════════╣");

  /* 获取消息头 */
  CHECK_FCT_DO(fd_msg_hdr(*msg, &hdr), goto send_answer);
  cli_info("║ Command-Code: %u, Application-ID: %u", hdr->msg_code,
           hdr->msg_appl);
  cli_info("╠══════════════════════════════════════════════════════════════╣");

  /* 遍历 AVP */
  CHECK_FCT_DO(fd_msg_browse(*msg, MSG_BRW_FIRST_CHILD, &cur_avp, NULL),
               goto send_answer);

  while (cur_avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(cur_avp, &avp_hdr), break);

    switch (avp_hdr->avp_code) {
    case 10002: /* REQ-Status-Info (订阅级别) */
      cli_info("║ REQ-Status-Info: %u (%s)", avp_hdr->avp_value->u32,
               magic_get_subscribe_level_name(avp_hdr->avp_value->u32));
      break;

    case 20007: /* DLM-List (v2.1 标准) */
      cli_info("║ DLM 链路列表:");
      {
        struct avp *dlm_info_avp = NULL;
        /* 遍历 DLM-List 中的所有 DLM-Info */
        CHECK_FCT_DO(
            fd_msg_browse(cur_avp, MSG_BRW_FIRST_CHILD, &dlm_info_avp, NULL),
            break);
        while (dlm_info_avp) {
          struct avp_hdr *dlm_info_hdr = NULL;
          CHECK_FCT_DO(fd_msg_avp_hdr(dlm_info_avp, &dlm_info_hdr), break);

          if (dlm_info_hdr->avp_code == 20008) { /* DLM-Info */
            parse_dlm_info(dlm_info_avp);
          }

          CHECK_FCT_DO(
              fd_msg_browse(dlm_info_avp, MSG_BRW_NEXT, &dlm_info_avp, NULL),
              break);
        }
      }
      break;

    case 10005: /* DLM-Available (单个枚举) */
      /* 检查是否是 Grouped AVP (旧版 DLM-List) */
      {
        struct avp *test_child = NULL;
        if (fd_msg_browse(cur_avp, MSG_BRW_FIRST_CHILD, &test_child, NULL) ==
                0 &&
            test_child) {
          /* 是 Grouped AVP -> DLM-List */
          cli_info("║ DLM 状态信息:");
          parse_dlm_list(cur_avp);
        } else {
          /* 是简单 AVP -> DLM-Available */
          uint32_t avail = avp_hdr->avp_value->u32;
          cli_info("║ DLM-Available: %u (%s)", avail,
                   avail == 0 ? "AVAILABLE" : "UNAVAILABLE");
        }
      }
      break;

    case 20008: /* DLM-Info (v2.1 嵌套结构) */
      cli_info("║ DLM 详细信息:");
      parse_dlm_info(cur_avp);
      break;

    case 10006: /* Link-List */
      cli_info("║ 链路状态信息:");
      parse_link_list(cur_avp);
      break;

    case 20010: /* DLM-Link-Status-List (v2.1) */
      cli_info("║ DLM 链路状态列表:");
      {
        /* 需要找到关联的 DLM 记录来存储 */
        DLMStatusRecord *dlm_rec = NULL;
        if (g_dlm_status_mgr.count > 0) {
          dlm_rec = &g_dlm_status_mgr.records[0]; /* 使用第一个作为默认 */
        }
        if (dlm_rec) {
          parse_dlm_link_status_list(cur_avp, dlm_rec);
        }
      }
      break;

    case 10041: /* Registered-Clients (v2.1) */
      g_dlm_status_mgr.registered_clients = avp_hdr->avp_value->u32;
      cli_info("║ Registered-Clients: %u", g_dlm_status_mgr.registered_clients);
      break;

    case 10030: /* MAGIC-System-Status */
    {
      uint32_t sys_status = avp_hdr->avp_value->u32;
      const char *status_str = "UNKNOWN";
      switch (sys_status) {
      case 0:
        status_str = "NORMAL";
        break;
      case 1:
        status_str = "WARNING";
        break;
      case 2:
        status_str = "CRITICAL";
        break;
      case 3:
        status_str = "OFFLINE";
        break;
      }
      cli_info("║ MAGIC-System-Status: %u (%s)", sys_status, status_str);
    } break;

    case 263: /* Session-Id */
      if (avp_hdr->avp_value->os.data && avp_hdr->avp_value->os.len > 0) {
        char sess_id[128] = {0};
        size_t len =
            avp_hdr->avp_value->os.len < 127 ? avp_hdr->avp_value->os.len : 127;
        memcpy(sess_id, avp_hdr->avp_value->os.data, len);
        cli_info("║ Session-Id: %s", sess_id);
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(cur_avp, MSG_BRW_NEXT, &cur_avp, NULL), break);
  }

  cli_info("╚══════════════════════════════════════════════════════════════╝");

send_answer:
  /* 发送 MSCA 自动确认应答 */
  if (send_msca_response(msg) == 0) {
    /* 发送成功，send_msca_response 已将 *msg 置为 NULL */
    /* 返回 DISP_ACT_CONT，框架会识别到消息已处理 */
    *act = DISP_ACT_CONT;
  } else {
    /* 发送失败，让框架处理 */
    cli_error("MSCA 应答发送失败，让框架处理");
    *act = DISP_ACT_ERROR;
  }

  return 0;
}

/**
 * 注册 MSCR 消息处理器
 */
int magic_mscr_handler_init(void) {
  struct disp_when when;
  int ret;

  memset(&when, 0, sizeof(when));

  /* 按 Application-ID 和 Command-Code 匹配 */
  when.app = g_magic_dict.app;
  when.command = g_magic_dict.cmd_mscr; /* 需要在字典中定义 */

  /* 如果没有专门的 MSCR 命令对象，使用通用方式 */
  if (!when.command) {
    cli_warn("MSCR 命令对象未定义，使用 Application 级别分发");
    when.command = NULL;
  }

  ret = fd_disp_register(mscr_handler_callback, DISP_HOW_CC, &when, NULL, NULL);
  if (ret != 0) {
    cli_error("注册 MSCR 处理器失败: %d", ret);
    return ret;
  }

  cli_success("MSCR 状态推送处理器已注册 (Command-Code=%d)", CMD_MSCR_CODE);
  return 0;
}

/* ==================== MNTR 处理器回调 ==================== */

/**
 * 根据 MAGIC-Status-Code 获取状态名称 (符合 ARINC 839 §1.3.2)
 *
 * MNTR 使用 MAGIC-Status-Code 来标识通知原因，而不是单独的 Notification-Type
 * AVP
 */
static const char *get_magic_status_name(uint32_t code) {
  switch (code) {
  /* 成功 */
  case 0:
    return "SUCCESS";

  /* 错误码 1000-1999 */
  case 1000:
    return "MISSING_AVP";
  case 1001:
    return "AUTHENTICATION_FAILED";
  case 1002:
    return "UNKNOWN_SESSION";
  case 1003:
    return "MAGIC_NOT_RUNNING";
  case 1008:
    return "MALFORMED_DATA_LINK_STRING (数据链路字符串格式错误)";
  case 1016:
    return "NO_FREE_BANDWIDTH (带宽不足/被抢占)";
  case 1019:
    return "CLIENT_UNREGISTRATION";
  case 1024:
    return "SESSION_TIMEOUT";
  case 1025:
    return "MAGIC_SHUTDOWN";

  /* 系统错误码 2000-2010 */
  case 2007:
    return "LINK_ERROR (链路错误/丢失)";
  case 2010:
    return "FORCED_REROUTING (链路切换)";

  /* 错误码 2008-2009 */
  case 2008:
    return "CLOSE_LINK_FAILED";
  case 2009:
    return "MAGIC_FAILURE";

  /* 错误码 3000+ */
  case 3000:
    return "UNKNOWN_ISSUE";
  case 3001:
    return "AVIONICSDATA_MISSING";

  default:
    return "UNKNOWN";
  }
}

/**
 * 发送 MNTA 响应 (自动应答)
 */
static int send_mnta_response(struct msg *mntr_req) {
  struct msg *ans = NULL;
  int ret;

  /* 创建应答消息 */
  ret = fd_msg_new_answer_from_req(fd_g_config->cnf_dict, &mntr_req, 0);
  if (ret != 0) {
    cli_error("创建 MNTA 应答失败: %d", ret);
    return ret;
  }

  ans = mntr_req;

  /* 添加 Result-Code = 2001 (SUCCESS) */
  ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);

  /* 添加 Origin-Host 和 Origin-Realm */
  ADD_AVP_STR(ans, g_std_dict.avp_origin_host, g_cfg.origin_host);
  ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, g_cfg.origin_realm);

  /* 发送应答 */
  ret = fd_msg_send(&ans, NULL, NULL);
  if (ret != 0) {
    cli_error("发送 MNTA 应答失败: %d", ret);
    return ret;
  }

  cli_success("→ 已发送 MNTA 确认应答 (Result-Code=2001)");
  return 0;
}

/**
 * MNTR (MAGIC-Notification-Request) 处理回调
 *
 * 当服务器推送会话通知时调用此函数
 */
static int mntr_handler_callback(struct msg **msg, struct avp *avp,
                                 struct session *session, void *opaque,
                                 enum disp_action *act) {
  struct msg_hdr *hdr = NULL;
  struct avp *cur_avp = NULL;
  struct avp_hdr *avp_hdr = NULL;
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];

  (void)avp;
  (void)session;
  (void)opaque;

  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

  cli_info("%s", "");
  cli_info("╔══════════════════════════════════════════════════════════════╗");
  cli_info("║  🔔 收到 MNTR 会话通知                                       ║");
  cli_info("║  时间: %s                              ║", time_str);
  cli_info("╠══════════════════════════════════════════════════════════════╣");

  /* 获取消息头 */
  CHECK_FCT_DO(fd_msg_hdr(*msg, &hdr), goto send_answer);
  cli_info("║ Command-Code: %u, Application-ID: %u", hdr->msg_code,
           hdr->msg_appl);

  /* 遍历 AVP */
  CHECK_FCT_DO(fd_msg_browse(*msg, MSG_BRW_FIRST_CHILD, &cur_avp, NULL),
               goto send_answer);

  uint32_t magic_status_code = 0; /* MAGIC-Status-Code 决定通知类型 */

  while (cur_avp) {
    CHECK_FCT_DO(fd_msg_avp_hdr(cur_avp, &avp_hdr), break);

    switch (avp_hdr->avp_code) {
    case 10009: /* MAGIC-Status-Code (ARINC 839 §1.3.2) */
      magic_status_code = avp_hdr->avp_value->u32;
      cli_info("║ MAGIC-Status-Code: %u (%s)", magic_status_code,
               get_magic_status_name(magic_status_code));

      /* 根据 MAGIC-Status-Code 更新客户端状态 */
      switch (magic_status_code) {
      case 2007: /* LINK_ERROR - 链路错误/丢失 */
      case 1024: /* SESSION_TIMEOUT */
      case 1025: /* MAGIC_SHUTDOWN */
        cli_warn("║ ⚠ 链路/资源已释放，状态将变为 AUTHENTICATED");
        g_client_state = CLIENT_STATE_AUTHENTICATED;
        g_assigned_link_id[0] = '\0';
        g_bearer_id = 0;
        break;

      case 0: /* SUCCESS - 链路恢复/带宽增加 */
        cli_success("║ ✓ 操作成功，检查 Granted-Bandwidth 获取新带宽");
        break;

      case 1016: /* NO_FREE_BANDWIDTH - 带宽不足/被抢占 */
        cli_warn("║ ⚠ 带宽不足/被抢占，带宽可能降低");
        break;

      case 2010: /* FORCED_REROUTING - 链路切换 */
        cli_success("║ ✓ 链路切换完成，请更新网关配置");
        /* 链路切换时保持 ACTIVE 状态，但需要更新网关 */
        break;
      }
      break;

    case 263: /* Session-Id */
      if (avp_hdr->avp_value->os.data && avp_hdr->avp_value->os.len > 0) {
        char sess_id[128] = {0};
        size_t len =
            avp_hdr->avp_value->os.len < 127 ? avp_hdr->avp_value->os.len : 127;
        memcpy(sess_id, avp_hdr->avp_value->os.data, len);
        cli_info("║ Session-Id: %s", sess_id);
      }
      break;

    case 10050: /* Communication-Report-Parameters (Grouped) */
      cli_info("║ 通信报告参数:");
      {
        struct avp *child = NULL;
        struct avp_hdr *child_hdr = NULL;
        CHECK_FCT_DO(fd_msg_browse(cur_avp, MSG_BRW_FIRST_CHILD, &child, NULL),
                     break);
        while (child) {
          CHECK_FCT_DO(fd_msg_avp_hdr(child, &child_hdr), break);

          if (child_hdr->avp_code == 10051) { /* Granted-Bandwidth */
            uint64_t bw = child_hdr->avp_value->u64;
            cli_info("║   授予带宽: %.2f kbps", bw / 1000.0);
            g_granted_bw = bw;
          } else if (child_hdr->avp_code ==
                     10052) { /* Granted-Return-Bandwidth */
            uint64_t ret_bw = child_hdr->avp_value->u64;
            cli_info("║   授予上行带宽: %.2f kbps", ret_bw / 1000.0);
          } else if (child_hdr->avp_code == 10029) { /* Gateway-IPAddress */
            if (child_hdr->avp_value->os.data &&
                child_hdr->avp_value->os.len > 0) {
              char gateway_ip[64] = {0};
              size_t len = child_hdr->avp_value->os.len < 63
                               ? child_hdr->avp_value->os.len
                               : 63;
              memcpy(gateway_ip, child_hdr->avp_value->os.data, len);
              cli_info("║   网关地址: %s", gateway_ip);
              /* TODO: 更新本地路由表，将默认网关设为新地址 */
            }
          } else if (child_hdr->avp_code == 10040) { /* Profile-Name */
            if (child_hdr->avp_value->os.data &&
                child_hdr->avp_value->os.len > 0) {
              char profile[64] = {0};
              size_t len = child_hdr->avp_value->os.len < 63
                               ? child_hdr->avp_value->os.len
                               : 63;
              memcpy(profile, child_hdr->avp_value->os.data, len);
              cli_info("║   配置文件: %s", profile);
            }
          }

          CHECK_FCT_DO(fd_msg_browse(child, MSG_BRW_NEXT, &child, NULL), break);
        }
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(cur_avp, MSG_BRW_NEXT, &cur_avp, NULL), break);
  }

  cli_info("╚══════════════════════════════════════════════════════════════╝");

send_answer:
  /* 发送 MNTA 自动确认应答 */
  if (send_mnta_response(*msg) == 0) {
    /* 成功发送应答，告诉freeDiameter我们已经处理完成 */
    *msg = NULL;          /* 消息已被转换为应答并发送，置空避免double free */
    *act = DISP_ACT_CONT; /* 消息已设为NULL,框架不会再处理 */
  } else {
    /* 发送失败,返回错误 */
    *act = DISP_ACT_ERROR;
  }

  return 0;
}

/**
 * 注册 MNTR 消息处理器
 */
int magic_mntr_handler_init(void) {
  struct disp_when when;
  int ret;

  memset(&when, 0, sizeof(when));

  /* 按 Application-ID 和 Command-Code 匹配 */
  when.app = g_magic_dict.app;
  when.command = g_magic_dict.cmd_mntr; /* 需要在字典中定义 */

  if (!when.command) {
    cli_warn("MNTR 命令对象未定义，使用 Application 级别分发");
    when.command = NULL;
  }

  ret = fd_disp_register(mntr_handler_callback, DISP_HOW_CC, &when, NULL, NULL);
  if (ret != 0) {
    cli_error("注册 MNTR 处理器失败: %d", ret);
    return ret;
  }

  cli_success("MNTR 会话通知处理器已注册 (Command-Code=%d)", CMD_MNTR_CODE);
  return 0;
}

/**
 * 清理推送消息处理器
 */
void magic_push_handlers_cleanup(void) {
  /* freeDiameter 会在关闭时自动清理分发处理器 */
  cli_info("推送消息处理器已清理");
}
