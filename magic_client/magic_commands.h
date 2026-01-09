/**
 * @file magic_commands.h
 * @brief MAGIC 协议 Diameter 命令处理函数库。
 * @details 包含 MCAR, MCCR, MSXR 等核心命令的封装与处理逻辑。
 *          支持 ARINC 839 协议规定的多种交互场景（认证、订阅、会话管理等）。
 */

#ifndef _MAGIC_COMMANDS_H_
#define _MAGIC_COMMANDS_H_

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

/**
 * @name Status-Info 订阅级别定义
 * @{
 */
#define STATUS_LEVEL_NONE 0  /**< 不订阅 */
#define STATUS_LEVEL_MAGIC 1 /**< 仅订阅 MAGIC 系统状态 (Registered-Clients) \
                              */
#define STATUS_LEVEL_DLM 2   /**< 仅订阅 DLM 一般状态 */
#define STATUS_LEVEL_MAGIC_DLM 3 /**< 订阅 MAGIC + DLM 综合状态 */
#define STATUS_LEVEL_DLM_LINK 6  /**< 订阅 DLM 详细链路状态 */
#define STATUS_LEVEL_ALL 7       /**< 订阅全部状态 */
/** @} */

/** @brief 请求的订阅级别全局变量。 */
extern uint32_t g_requested_subscribe_level;
/** @brief 服务端实际授予的订阅级别。 */
extern uint32_t g_granted_subscribe_level;

/* ==================== MAGIC 命令处理函数 ==================== */

/**
 * @brief MCAR (MAGIC-Client-Authentication-Registration) 命令处理。
 * @details 实现 ARINC 839 场景 A (纯认证), B (订阅), C (0-RTT 接入)。
 * @param argc 命令行参数个数。
 * @param argv 命令行参数数组。
 * @return int 成功返回 0，失败返回 -1。
 */
int cmd_mcar(int argc, char **argv);

/**
 * @brief MCCR (MAGIC-Communication-Control-Request) 命令处理。
 * @details 处理通信会话的创建 (start)、修改 (modify) 和释放 (stop/release)。
 * @param argc 参数个数。
 * @param argv 参数数组。
 * @return int 成功返回 0。
 */
int cmd_mccr(int argc, char **argv);

/**
 * @brief MSXR (MAGIC-Status-eXchange-Request) 命令处理。
 * @details 主动查询系统状态，如 DLM 状态、链路状态、客户端列表等。
 * @param argc 参数个数。
 * @param argv 参数数组。
 * @return int 成功返回 0。
 */
int cmd_msxr(int argc, char **argv);

/**
 * @brief MADR (MAGIC-Accounting-Data-Request) 命令处理。
 * @details 获取计费数据 (CDR) 列表或特定记录内容。
 * @param argc 参数个数。
 * @param argv 参数数组。
 * @return int 成功返回 0。
 */
int cmd_madr(int argc, char **argv);

/**
 * @brief MACR (MAGIC-Accounting-Control-Request) 命令处理。
 * @details 执行计费会话控制操作，如重启 CDR 会话。
 * @param argc 参数个数。
 * @param argv 参数数组。
 * @return int 成功返回 0。
 */
int cmd_macr(int argc, char **argv);

/**
 * @brief STR (Session-Termination-Request) 标准 Diameter 会话终止命令。
 * @param argc 参数个数。
 * @param argv 参数数组。
 * @return int 成功返回 0。
 */
int cmd_str(int argc, char **argv);

/**
 * @brief 打印当前客户端运行状态。
 */
int cmd_status(int argc, char **argv);

/**
 * @brief 显示缓存的 DLM 状态表 (v2.1 增强)。
 */
int cmd_show_dlm(int argc, char **argv);

/**
 * @brief 配置管理命令。
 * @details 支持 show (查看) 和 reload (重载) 配置项。
 */
int cmd_config(int argc, char **argv);

/**
 * @brief 退出交互式 CLI 程序。
 */
int cmd_quit(int argc, char **argv);

/**
 * @brief UDP 连通性测试命令。
 */
int cmd_udp_test(int argc, char **argv);

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 发送异步 Diameter 请求并注册应答回调。
 * @param req 请求消息指针，成功发送后所有权归核心。
 * @param ans 保留参数，恒为 NULL。
 * @param timeout_ms 超时（毫秒）。
 * @return int 0 已发送，-1 发送失败。
 */
int magic_send_request(struct msg **req, struct msg **ans, uint32_t timeout_ms);

/**
 * @brief 从应答消息中安全提取标准 Diameter Result-Code。
 * @param ans 应答消息。
 * @param result_code 指向存储结果的 uint32 指针。
 * @return int 成功返回 0。
 */
int magic_get_result_code(struct msg *ans, uint32_t *result_code);

/** @brief 格式化展示 DLM 列表。 */
void magic_print_dlm_list(struct msg *ans);

/** @brief 格式化展示链路详细状态。 */
void magic_print_link_status(struct msg *ans);

/** @brief 解析并高亮显示 MSXA 返回的各类系统状态。 */
void magic_print_status_info(struct msg *ans);

/** @brief 格式化解析并展示计费信息。 */
void magic_print_cdr_info(struct msg *ans);

/* ==================== 服务端推送消息处理器 ==================== */

/**
 * @brief 注册并初始化 MSCR (Status-Change-Report) 消息处理器。
 * @details 实现服务端通知的实时接收（如 Registered-Clients 变更）。
 * @return int 0 成功。
 */
int magic_mscr_handler_init(void);

/**
 * @brief 注册并初始化 MNTR (Notification-Request) 消息处理器。
 * @details 实现会话层级的重要通知处理（如强制下线、带宽变更）。
 * @return int 0 成功。
 */
int magic_mntr_handler_init(void);

/**
 * @brief 注销并清理所有推送消息处理器。
 */
void magic_push_handlers_cleanup(void);

/** @brief 获取订阅级别数值对应的可读名称字符串。 */
const char *magic_get_subscribe_level_name(uint32_t level);

/** @brief 校验输入的订阅级别是否符合协议规范。 */
int magic_validate_subscribe_level(uint32_t level);

/**
 * @brief 会话切换与管理命令 (用于多会话并发测试)。
 */
int cmd_session(int argc, char **argv);

#endif /* _MAGIC_COMMANDS_H_ */
