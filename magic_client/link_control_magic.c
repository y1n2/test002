/**
 * @file link_control_magic.c
 * @brief MAGIC 链路控制自动化流水线 (Sequence Control)。
 * @details 演示如何利用 MCCR 命令在“打开 (Open)”、“修改 (Modify)”和“切换
 * (Switch)”三种业务形态间进行自动转换。 遵循 ARINC 839
 * 规范中定义的介质无关性原则。
 */

#include "add_avp.h"
#include "config.h"
#include "magic_dict_handles.h"

/* 引用词典系统的错误码定义 */
#include "../extensions/dict_magic_839/dict_magic_codes.h"

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <gnutls/gnutls.h>

/* --- 外部声明 --- */
extern app_config_t g_cfg;
extern struct std_diam_dict_handles g_std_dict;
extern struct magic_dict_handles g_magic_dict;
int add_client_credentials_group(struct msg *req);
static int add_mandatory_diameter_avps(struct msg *req, const char *dest_realm);

/* --- 协议常量 --- */
#define MAGIC_APP_ID 1094202169
#define CMD_STR_CODE 275

/* --- 全局命令代码缓存 (Method B) --- */
// 仅缓存 Request Code，Answer Code 总是 Request Code + 1
static uint32_t CMD_MCAR_R_CODE =
    0; // MAGIC-Client-Authentication-Request (350)
static uint32_t CMD_MCCR_R_CODE = 0; // MAGIC-Communication-Change-Request (351)
static uint32_t CMD_MSXR_R_CODE = 0; // MAGIC-Status-Request (354)
static uint32_t CMD_MCCA_R_CODE = 0;

/* --- MCCR 请求参数结构体 --- */
/* 用于灵活控制 MCCR 的行为 (Open vs Modify vs Switch) */
typedef struct {
  const char *profile_name; // 必需
  uint64_t req_bw;          // 请求上行带宽
  uint64_t req_return_bw;   // 请求下行带宽
  const char *dlm_name;     // 指定链路名称 (用于切换链路)
  uint32_t qos_level;       // QoS 等级
  const char *description;  // 日志描述用
} mccr_params_t;

/* --- 状态机定义 --- */
typedef enum {
  STATE_INIT = 0,
  STATE_REGISTERED,    // MCAR 完成
  STATE_LINK_OPENED,   // MCCR(Open) 完成
  STATE_LINK_MODIFIED, // MCCR(Modify) 完成
  STATE_LINK_SWITCHED, // MCCR(Switch) 完成
  STATE_DONE
} client_state_t;

static client_state_t g_client_state = STATE_INIT;
/* 全局会话句柄 (定义在文件顶部) */
static struct fd_session *g_session = NULL;
static const char *g_dest_realm = "magic-server.example.com";

/* ============================================================================
 * 0. 辅助函数：命令代码初始化 (使用 fd_dict_getval)
 * ============================================================================
 */
static int cache_command_codes(void) {
  struct dict_cmd_data data = {0};

  LOG_I("SEQ: 缓存命令代码 (使用 fd_dict_getval API)...");

  // 获取 MCAR (Request) Code
  CHECK_FCT(fd_dict_getval(g_magic_dict.cmd_mcar, &data));
  CMD_MCAR_R_CODE = data.cmd_code;

  // 获取 MCCR (Request) Code
  CHECK_FCT(fd_dict_getval(g_magic_dict.cmd_mccr, &data));
  CMD_MCCR_R_CODE = data.cmd_code;

  // 获取 MSXR (Request) Code
  CHECK_FCT(fd_dict_getval(g_magic_dict.cmd_msxr, &data));
  CMD_MSXR_R_CODE = data.cmd_code;

  // 检查是否成功获取 (350, 351, 354)
  if (CMD_MCAR_R_CODE != 350 || CMD_MCCR_R_CODE != 351) {
    LOG_E("命令代码获取失败或不匹配。MCAR:%u, MCCR:%u", CMD_MCAR_R_CODE,
          CMD_MCCR_R_CODE);
    return -1;
  }
  return 0;
}

/* ============================================================================
 * 0. 辅助函数：添加 Communication-Request-Parameters (Grouped 20001)
 * ============================================================================
 */
/**
 * @brief 根据传入的 params 结构体动态构造 MCCR 参数组
 */
static int add_dynamic_comm_req_params(struct msg *req,
                                       const mccr_params_t *p) {
  struct avp *group_avp = NULL;
  int ret = 0;

  // 1. 创建 Grouped AVP
  CHECK_FCT_DO(fd_msg_avp_new(g_magic_dict.avp_comm_req_params, AVP_FLAG_VENDOR,
                              &group_avp),
               return -1);

  // 2. Profile-Name (必需)
  CHECK_FCT_DO(ADD_AVP_STR_V(group_avp, g_magic_dict.avp_profile_name,
                             p->profile_name, MAGIC_VENDOR_ID),
               ret = -1;
               goto cleanup);

  // 3. 带宽参数 (可选/修改项)
  if (p->req_bw > 0) {
    CHECK_FCT_DO(ADD_AVP_U64_V(group_avp, g_magic_dict.avp_link_max_bw,
                               p->req_bw, MAGIC_VENDOR_ID),
                 ret = -1;
                 goto cleanup);
  }
  if (p->req_return_bw > 0) {
    CHECK_FCT_DO(ADD_AVP_U64_V(group_avp, g_magic_dict.avp_link_max_return_bw,
                               p->req_return_bw, MAGIC_VENDOR_ID),
                 ret = -1;
                 goto cleanup);
  }

  // 4. DLM-Name (用于切换链路)
  if (p->dlm_name && p->dlm_name[0] != '\0') {
    CHECK_FCT_DO(ADD_AVP_STR_V(group_avp, g_magic_dict.avp_dlm_name,
                               p->dlm_name, MAGIC_VENDOR_ID),
                 ret = -1;
                 goto cleanup);
  }

  // 5. QoS Level
  if (p->qos_level > 0) {
    CHECK_FCT_DO(ADD_AVP_U32_V(group_avp, g_magic_dict.avp_qos_level,
                               p->qos_level, MAGIC_VENDOR_ID),
                 ret = -1;
                 goto cleanup);
  }

  // 6. 添加到消息
  CHECK_FCT_DO(fd_msg_avp_add(req, MSG_BRW_LAST_CHILD, group_avp), ret = -1;
               goto cleanup);
  return 0;

cleanup:
  if (group_avp)
    __fd_avp_cleanup(group_avp);
  return ret;
}

/* ============================================================================
 * 核心辅助函数：添加 Diameter 协议强制 AVP (Origin/Session)
 * ============================================================================
 */

/**
 * @brief 添加所有标准的 Diameter 强制性 AVP (Session-Id, Origin-Host/Realm,
 * Auth-App-Id, Dest-Realm)。 这些 AVP 是所有应用消息的通用基础。
 * @param req 请求消息句柄
 * @param dest_realm 目标 Realm 字符串 (通常从 g_cfg.destination_realm 获取)
 * @return 0 成功
 */
static int add_mandatory_diameter_avps(struct msg *req,
                                       const char *dest_realm) {
  // Origin-Host (必需) - 从 freeDiameter 核心配置获取
  CHECK_FCT_DO(
      ADD_AVP_STR(req, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid),
      return -1);

  // Origin-Realm (必需) - 从 freeDiameter 核心配置获取
  CHECK_FCT_DO(
      ADD_AVP_STR(req, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm),
      return -1);

  // Destination-Realm (必需) - 使用传入参数
  CHECK_FCT_DO(ADD_AVP_STR(req, g_std_dict.avp_destination_realm, dest_realm),
               return -1);

  // Auth-Application-Id (必需)
  CHECK_FCT_DO(ADD_AVP_U32_V(req, g_std_dict.avp_auth_application_id,
                             g_cfg.auth_app_id, 0),
               return -1);

  return 0;
}

/* ============================================================================
 * 1. 核心发送函数
 * ============================================================================
 */
/**
 * @brief 发送 MCAR 应用注册
 */
static int send_app_registration(const char *dest_realm,
                                 void (*response_handler)(struct msg **)) {
  struct msg *req = NULL;
  int ret = 0;

  // 1. 创建 MCAR 请求消息
  CHECK_FCT_DO(fd_msg_new(g_magic_dict.cmd_mcar, MSGFL_ALLOC_ETEID, &req),
               return -1);

  // 2. 为消息创建新会话(自动创建会话并添加 Session-Id AVP)
  CHECK_FCT_DO(fd_msg_new_session(req, (os0_t) "magic", strlen("magic")),
               ret = -1;
               goto cleanup);

  // 【关键步骤】从消息中提取新创建的 Session 句柄并保存到全局变量 g_session
  // fd_msg_session_get(req) 返回消息关联的会话对象
  // fd_msg_sess_get(...) 获取实际的 session 结构体指针
  CHECK_FCT_DO(fd_msg_sess_get(fd_g_config->cnf_dict, req, &g_session, NULL),
               ret = -1;
               goto cleanup);

  LOG_I("SEQ: 新会话已创建，Session ID stored in g_session.");

  // 2. 添加强制性的 Diameter 协议 AVP
  // (注意：Session-Id AVP 已经由 fd_msg_new_session 自动添加，这里不需要再加)
  CHECK_FCT_DO(add_mandatory_diameter_avps(req, dest_realm), ret = -1;
               goto cleanup);

  // 3. 添加 Client-Credentials (Grouped AVP 20019)
  if (add_client_credentials_group(req) != 0) { // 假设这是封装好的辅助函数
    ret = -1;
    goto cleanup;
  }

  LOG_N("Sending MCAR to %s...", dest_realm);

  // 4. 发送消息
  ret = fd_msg_send(&req, response_handler, NULL);

cleanup:
  if (ret != 0 && req != NULL) {
    fd_msg_free(req);
    // 如果发送失败，可能需要清理 g_session，视具体逻辑而定
    // fd_sess_destroy(&g_session);
    // g_session = NULL;
  }
  return ret;
}

/**
 * @brief 发送 MCCR 通用函数 (支持 打开/修改/切换)
 */
/* * 发送 MCCR 通用函数 (支持 打开/修改/切换)
 * * 逻辑说明：
 * MCCR 不负责创建会话。它必须复用由 MCAR 建立的全局会话 (g_session)。
 * 如果 g_session 为空，说明并未完成注册，不能发送 MCCR。
 */
static int send_mccr_generic(const mccr_params_t *p) {
  struct msg *req = NULL;
  struct avp *avp = NULL;
  os0_t sid;
  int ret = 0;

  // 1. 严格检查：确保会话已由 MCAR 创建
  if (g_session == NULL) {
    LOG_E("SEQ Error: 尝试发送 MCCR，但全局会话 (g_session) 为空。请先执行 "
          "MCAR。");
    return -1;
  }

  LOG_I("SEQ: 发送 MCCR (%s) - BW: %llu, DLM: %s", p->description,
        (unsigned long long)p->req_bw, p->dlm_name ? p->dlm_name : "Default");

  // 2. 创建消息并关联现有会话
  // 2. 创建消息 (不创建新会话)
  CHECK_FCT_DO(fd_msg_new(g_magic_dict.cmd_mccr, MSGFL_ALLOC_ETEID, &req),
               return -1);

  // 3. 手动添加现有会话的 Session-Id
  CHECK_FCT_DO(fd_sess_getsid(g_session, &sid, NULL), goto cleanup);

  CHECK_FCT_DO(ADD_AVP_STR(req, g_std_dict.avp_session_id, sid), return -1);

  // 3. 添加标准 Diameter AVP (Origin-Host, Origin-Realm, Destination-Realm,
  // Auth-App-Id)
  CHECK_FCT_DO(add_mandatory_diameter_avps(req, g_dest_realm), ret = -1;
               goto cleanup);

  // 4. 使用传入的参数构造 MCCR 业务 AVP (Communication-Request-Parameters)
  CHECK_FCT_DO(add_dynamic_comm_req_params(req, p), ret = -1; goto cleanup);

  // 5. 发送消息
  // 回调函数为 NULL，响应将在通用的 client_resp_handler 中处理
  CHECK_FCT_DO(fd_msg_send(&req, NULL, NULL), ret = -1; goto cleanup);

  return 0;

cleanup:
  // 如果发送失败，释放消息内存
  if (req)
    __fd_avp_cleanup(req);
  return ret;
}
/* ============================================================================
 * 2. 业务逻辑封装
 * ============================================================================
 */

// 场景1：打开链路 (使用默认配置)
static void action_open_link(void) {
  mccr_params_t p = {0};
  p.description = "Open Link";
  p.profile_name = "Std-Profile";
  p.req_bw = 5000000; // 5 Mbps
  p.req_return_bw = 1000000;
  p.qos_level = 2;   // Silver
  p.dlm_name = NULL; // 默认链路

  send_mccr_generic(&p);
}

// 场景2：修改参数 (增加带宽)
static void action_modify_params(void) {
  mccr_params_t p = {0};
  p.description = "Modify Params (Increase BW)";
  p.profile_name = "Std-Profile";
  p.req_bw = 10000000; // 升至 10 Mbps
  p.req_return_bw = 2000000;
  p.qos_level = 3; // Gold
  // DLM 为空表示保留当前链路，仅修改参数

  send_mccr_generic(&p);
}

// 场景3：切换链路 (指定 SATCOM)
static void action_switch_link(void) {
  mccr_params_t p = {0};
  p.description = "Switch Link (to SATCOM)";
  p.profile_name = "High-Rel-Profile";
  p.req_bw = 2000000; // 卫星链路带宽可能较低
  p.req_return_bw = 500000;
  p.qos_level = 4;              // Platinum
  p.dlm_name = "SATCOM-Link-1"; // *** 关键：指定新的 DLM 名称以触发切换 ***

  send_mccr_generic(&p);
}

/* ============================================================================
 * 3. 响应处理状态机
 * ============================================================================
 */

static int client_resp_handler(struct msg **ans, struct avp *peer_avp,
                               void *data) {
  struct msg *msg = *ans;
  uint32_t command_code, result_code;
  struct msg_hdr *cmd_data;
  struct avp *avp;

  if (msg == NULL)
    return 0;

  CHECK_FCT_DO(fd_msg_hdr(msg, &cmd_data), return 0);
  command_code = cmd_data->msg_code;
  if (command_code == 280)
    return 0; // Ignore DWA

  // 搜索 Result-Code AVP
  CHECK_FCT_DO(fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL), return 0);
  while (avp) {
    struct avp_hdr *ahdr;
    CHECK_FCT_DO(fd_msg_avp_hdr(avp, &ahdr), break);

    if ((ahdr->avp_code == AC_RESULT_CODE) &&
        !(ahdr->avp_flags & AVP_FLAG_VENDOR)) {
      // 解析 Result-Code AVP
      CHECK_FCT(fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL));
      if (ahdr->avp_value) {
        result_code = ahdr->avp_value->u32;
      }
      break;
    }

    CHECK_FCT_DO(fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), break);
  }

  if (result_code != DIAMETER_SUCCESS) {
    LOG_E("SEQ: 收到错误应答 (Code %u)，流程终止。", result_code);
    g_client_state = STATE_DONE;
    return 0;
  }

  switch (g_client_state) {
  case STATE_INIT: // Waiting for MCAA
    if (command_code == CMD_MCAR_R_CODE) {
      LOG_N(">>> MCAR 成功 (Registered)。准备打开链路...");
      g_client_state = STATE_REGISTERED;
      // 触发动作：打开链路
      action_open_link();
    }
    break;

  case STATE_REGISTERED: // Waiting for MCCA (Open)
    if (command_code == g_magic_dict.cmd_mcar->data.cmd.cmd_code) {
      LOG_N(">>> MCCA 成功 (Link Opened)。准备修改参数...");
      g_client_state = STATE_LINK_OPENED;
      // 触发动作：修改参数 (模拟延时)
      action_modify_params();
    }
    break;

  case STATE_LINK_OPENED: // Waiting for MCCA (Modify)
    if (command_code == g_magic_dict.cmd_mcar->data.cmd.cmd_code) {
      LOG_N(">>> MCCA 成功 (Params Modified)。准备切换链路...");
      g_client_state = STATE_LINK_MODIFIED;
      // 触发动作：切换链路
      action_switch_link();
    }
    break;

  case STATE_LINK_MODIFIED: // Waiting for MCCA (Switch)
    if (command_code == CMD_MCAR_R_CODE) {
      LOG_N(">>> MCCA 成功 (Link Switched)。流程演示结束。");
      g_client_state = STATE_LINK_SWITCHED;
      g_client_state = STATE_DONE;
    }
    break;

  default:
    break;
  }
  return 0;
}

/* ============================================================================
 * 4. 启动入口
 * ============================================================================
 */
static void client_app_start(void *data) {
  LOG_I("SEQ: 连接就绪，启动应用注册 (MCAR)...");
  send_app_registration();
}

int magic_client_sequence_init() {
  CHECK_FCT(fd_disp_register(client_resp_handler, 0, 0, MAGIC_APP_ID, 0, NULL,
                             g_magic_dict.app));
  CHECK_FCT(fd_peer_iterate(client_app_start, NULL, NULL));
  return 0;
}