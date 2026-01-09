/**
 * @file cli_interface.h
 * @brief MAGIC 客户端命令行界面 (CLI) 定义。
 * @details 提供交互式 Shell 环境，支持用户手动执行 Diameter
 * 协议命令、管理会话及查询系统状态。
 */

#ifndef _CLI_INTERFACE_H_
#define _CLI_INTERFACE_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CLI 命令处理回调函数原型。
 * @param argc 参数个数。
 * @param argv 参数字符串数组。
 * @return int 执行结果，0 表示成功。
 */
typedef int (*cli_cmd_handler_t)(int argc, char **argv);

/**
 * @brief CLI 命令元数据结构。
 * @details 定义一个命令的名称、快捷方式及相关的处理逻辑。
 */
typedef struct {
  const char *name;          ///< 命令全称。
  const char *alias;         ///< 命令别名 (可选)。
  cli_cmd_handler_t handler; ///< 指向处理该命令的具体 C 函数。
  const char *usage;         ///< 命令语法说明。
  const char *description;   ///< 命令的功能详细描述信息。
} cli_command_t;

/* ==================== CLI 核心控制 API ==================== */

/**
 * @brief 初始化 CLI 系统。
 * @details 负责初始化 readline 库、内部状态锁以及命令分发器。
 * @return int 0 成功，-1 失败。
 */
int cli_init(void);

/**
 * @brief 启动 CLI 交互主循环。
 * @details
 * 此函数为阻塞式调用，通常由主线程执行。它负责读取输入、解析并分发命令。
 * @return int 交互结束后的退出码。
 */
int cli_run_loop(void);

/**
 * @brief 释放 CLI 占用的所有系统资源。
 */
void cli_cleanup(void);

/**
 * @brief 在非交互模式下直接执行一条命令行字符串。
 * @param cmdline 类似 "mcar connect" 的完整字符串。
 * @return int 执行状态。
 */
int cli_execute_command(const char *cmdline);

/**
 * @brief 显示系统或具体命令的帮助信息。
 * @param cmd_name 指定命令名称，若为 NULL 则显示全局帮助。
 */
void cli_print_help(const char *cmd_name);

/* ==================== 状态缓存管理 API ==================== */

/**
 * @brief 获取当前活动 Diameter 会话的标识符。
 * @return const char* Session-Id 字符串指针。
 */
const char *cli_get_session_id(void);

/**
 * @brief 缓存当前活动的 Diameter 会话 ID。
 * @param session_id 来自 MCAA 应答的 Session-Id。
 */
void cli_set_session_id(const char *session_id);

/**
 * @brief 查询客户端当前是否已通过 MCAR 流程注册。
 */
bool cli_is_registered(void);

/**
 * @brief 更新客户端的注册状态。
 */
void cli_set_registered(bool registered);

/**
 * @brief 查询当前是否存在活跃的数据传输会话 (MCCR 已成功)。
 */
bool cli_has_active_session(void);

/**
 * @brief 更新数据传输会话的激活状态。
 */
void cli_set_session_active(bool active);

/* ==================== 界面格式化输出 API ==================== */

/**
 * @brief 输出普通信息日志 (蓝色高亮)。
 */
void cli_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief 输出警告信息 (黄色高亮)。
 */
void cli_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief 输出错误提示 (红色高亮)。
 */
void cli_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief 输出操作成功反馈 (亮绿色高亮)。
 */
void cli_success(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ==================== 全局资源访问 ==================== */

typedef struct SessionManager SessionManager;

/**
 * @brief 获取全局会话管理器实例。
 * @return SessionManager* 管理器指针。
 */
SessionManager *cli_get_session_manager(void);

#endif /* _CLI_INTERFACE_H_ */
