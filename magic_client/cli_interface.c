/**
 * @file cli_interface.c
 * @brief MAGIC 客户端命令行界面实现。
 * @details 利用 GNU Readline
 * 库提供友好的交互环境，包括历史记录、动态提示符切换以及线程安全的命令分发机制。
 */

#include "cli_interface.h"
#include "magic_commands.h"
#include "session_manager.h"
#include <pthread.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <unistd.h>

/* ==================== 全局状态 ==================== */
/* 全局会话管理器 - 支持多并发会话 */
SessionManager g_session_manager;
static bool g_cli_running = false; // CLI主循环运行标志
static pthread_mutex_t g_cli_mutex =
    PTHREAD_MUTEX_INITIALIZER; // 互斥锁保护上述全局状态

/* 兼容旧代码的全局变量 (已废弃,使用 session_manager 代替) */
static char g_session_id[256] = {0};  // 当前 Diameter 会话ID
static bool g_is_registered = false;  // 注册状态标志
static bool g_session_active = false; // 通信会话活跃标志

/* ==================== ANSI 颜色代码 ==================== */
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BRIGHT_GREEN "\033[1;32m"

/* ==================== CLI 命令表 ==================== */
/*
 * 命令表驱动架构：
 * - 每个命令包含：主命令名、别名、处理函数、用法、描述
 * - 命令解析器通过此表实现统一的命令分发
 * - NULL结束标记用于遍历终止
 * - help命令特殊处理（handler为NULL），通过cli_print_help实现
 */
static cli_command_t g_commands[] = {
    {"mcar", "register", cmd_mcar,
     "mcar auth | mcar subscribe [level] | mcar connect [profile] [bw]",
     "客户端认证与订阅 - 向MAGIC服务器认证并订阅状态信息\n"
     "  auth: 仅执行认证\n"
     "  subscribe: 认证并订阅状态 (level: 0=无, 1=MAGIC, 2=DLM, 3=MAGIC_DLM, "
     "6=LINK, 7=ALL)\n"
     "  connect: 0-RTT 快速接入 (认证+建立通信会话)"},
    {"mccr", "comm", cmd_mccr,
     "mccr start [profile] [min_kbps] [max_kbps] | mccr modify [...] | mccr "
     "stop",
     "通信会话管理 - 提交 QoS 业务需求（MAGIC 自动选择最优链路）\n"
     "  start: 创建新会话（提交带宽/优先级/QoS需求）\n"
     "  modify: 修改 QoS 需求（MAGIC 自动重新评估链路）\n"
     "  stop: 释放会话（所有资源自动回收）\n"
     "  注意: 客户端不能指定具体链路（Satcom/LTE/WiFi），符合 ARINC 839 "
     "介质无关性原则"},
    {"msxr", "query", cmd_msxr, "msxr [type]",
     "状态查询 - 查询系统状态信息\n"
     "  type: 0=无 1=MAGIC 2=DLM 3=MAGIC_DLM 6=LINK 7=全部(默认)"},
    {"madr", "cdr", cmd_madr, "madr list | madr data <cdr_id>",
     "计费数据查询 - 查询计费记录(CDR)\n"
     "  list: 列出所有CDR\n"
     "  data: 查询指定CDR详细内容"},
    {"macr", "restart_cdr", cmd_macr, "macr restart <session_id>",
     "计费控制 - 重启指定会话的计费记录"},
    {"str", "terminate", cmd_str, "str [reason]",
     "会话终止 - 终止当前Diameter会话\n"
     "  reason: 0=正常终止 1=管理员强制 4=客户端请求"},
    {"status", "st", cmd_status, "status",
     "显示当前客户端状态（注册状态、会话信息、连接状态）"},
    {"session", "sess", cmd_session, "session list | session select <id>",
     "多会话管理 (v2.2)\n"
     "  list: 列出所有活跃会话\n"
     "  select <id>: 切换当前操作的会话"},
    {"config", "cfg", cmd_config, "config [show|reload]",
     "配置管理\n"
     "  show: 显示当前配置\n"
     "  reload: 重新加载配置文件"},
    {"help", "?",
     NULL, // 特殊处理
     "help [command]",
     "显示帮助信息\n"
     "  不带参数显示所有命令\n"
     "  指定命令名显示该命令详细帮助"},
    {"udp_test", "udp", cmd_udp_test,
     "udp_test [ip] [port] [message] [count] | udp_test echo [ip] [port] "
     "[count] [size]",
     "UDP 连通性测试 - 向指定地址发送 UDP 数据包\n"
     "  基本模式: udp_test <目标IP> [端口] [消息] [次数]\n"
     "  Echo模式: udp_test echo <目标IP> [端口] [次数] [大小]\n"
     "  示例: udp_test 192.168.1.100 5000 \"Hello\" 5\n"
     "  示例: udp_test echo 192.168.1.100 7 10 64"},
    {"show", "dlm", cmd_show_dlm, "show dlm | show",
     "显示 DLM 状态表 - 展示从 MSCR 收集的 DLM 硬件状态\n"
     "  包含: DLM 可用性、链路连接状态、信号强度、带宽分配\n"
     "  注意: 需先订阅状态通知 (mcar subscribe) 后才有数据"},
    {"quit", "exit", cmd_quit, "quit | exit | q", "退出MAGIC客户端程序"},
    {NULL, NULL, NULL, NULL, NULL} // 结束标记
};

/* ==================== 状态管理函数 ==================== */
/* 所有状态访问函数均使用 pthread_mutex 保证线程安全
 * 适用场景：CLI前台线程与freeDiameter后台线程并发访问 */

/**
 * 获取当前会话ID (线程安全)
 * @return Session-Id字符串指针，未注册时返回NULL
 * @note 返回的指针在下次调用cli_set_session_id前有效
 */
const char *cli_get_session_id(void) {
  pthread_mutex_lock(&g_cli_mutex); // 加锁保护
  const char *sid = g_session_id[0] ? g_session_id : NULL;
  pthread_mutex_unlock(&g_cli_mutex); // 解锁
  return sid;
}

/**
 * 设置当前会话ID (线程安全)
 * @param session_id 新的Session-Id字符串，NULL表示清空
 * @note MCAR成功后调用此函数保存服务器返回的Session-Id
 * @note STR成功后调用此函数(传NULL)清除会话ID
 */
void cli_set_session_id(const char *session_id) {
  pthread_mutex_lock(&g_cli_mutex);
  if (session_id) {
    // 安全拷贝，防止缓冲区溢出
    strncpy(g_session_id, session_id, sizeof(g_session_id) - 1);
    g_session_id[sizeof(g_session_id) - 1] = '\0'; // 强制null终止
  } else {
    // 清空会话ID
    g_session_id[0] = '\0';
  }
  pthread_mutex_unlock(&g_cli_mutex);
}

bool cli_is_registered(void) {
  pthread_mutex_lock(&g_cli_mutex);
  bool reg = g_is_registered;
  pthread_mutex_unlock(&g_cli_mutex);
  return reg;
}

void cli_set_registered(bool registered) {
  pthread_mutex_lock(&g_cli_mutex);
  g_is_registered = registered;
  pthread_mutex_unlock(&g_cli_mutex);
}

bool cli_has_active_session(void) {
  pthread_mutex_lock(&g_cli_mutex);
  bool active = g_session_active;
  pthread_mutex_unlock(&g_cli_mutex);
  return active;
}

void cli_set_session_active(bool active) {
  pthread_mutex_lock(&g_cli_mutex);
  g_session_active = active;
  pthread_mutex_unlock(&g_cli_mutex);
}

/* ==================== 输出辅助函数 ==================== */

void cli_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf(COLOR_CYAN "[INFO] " COLOR_RESET);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

void cli_warn(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf(COLOR_YELLOW "[WARN] " COLOR_RESET);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

void cli_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf(COLOR_RED "[ERROR] " COLOR_RESET);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

void cli_success(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf(COLOR_BRIGHT_GREEN "[SUCCESS] " COLOR_RESET);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

/* ==================== 帮助系统 ==================== */

void cli_print_help(const char *cmd_name) {
  if (cmd_name == NULL) {
    // 打印所有命令
    printf("\n" COLOR_GREEN
           "========== MAGIC Client Commands ==========" COLOR_RESET "\n\n");

    for (int i = 0; g_commands[i].name != NULL; i++) {
      printf(COLOR_CYAN "  %-15s" COLOR_RESET, g_commands[i].name);
      if (g_commands[i].alias) {
        printf(" (%-10s)", g_commands[i].alias);
      } else {
        printf("            ");
      }

      // 提取描述第一行
      const char *desc = g_commands[i].description;
      const char *newline = strchr(desc, '\n');
      if (newline) {
        printf(" - %.*s", (int)(newline - desc), desc);
      } else {
        printf(" - %s", desc);
      }
      printf("\n");
    }

    printf("\n输入 " COLOR_YELLOW "help <command>" COLOR_RESET
           " 查看详细帮助\n");
    printf("输入 " COLOR_YELLOW "quit" COLOR_RESET " 退出程序\n\n");

  } else {
    // 查找并打印指定命令
    for (int i = 0; g_commands[i].name != NULL; i++) {
      if (strcmp(g_commands[i].name, cmd_name) == 0 ||
          (g_commands[i].alias && strcmp(g_commands[i].alias, cmd_name) == 0)) {

        printf("\n" COLOR_GREEN "命令: " COLOR_RESET "%s", g_commands[i].name);
        if (g_commands[i].alias) {
          printf(" (别名: %s)", g_commands[i].alias);
        }
        printf("\n\n");

        printf(COLOR_GREEN "用法: " COLOR_RESET "%s\n\n", g_commands[i].usage);
        printf(COLOR_GREEN "描述: " COLOR_RESET "\n%s\n\n",
               g_commands[i].description);
        return;
      }
    }
    cli_error("未知命令: %s", cmd_name);
  }
}

/* ==================== 命令解析与执行 ==================== */

/**
 * 解析命令行字符串为 argc/argv 形式
 * @param cmdline 原始命令行字符串 (例如: "mccr create 5000000 1000000")
 * @param argc 输出参数：解析出的参数个数
 * @return argv数组指针 (需调用free_argv释放)，失败返回NULL
 *
 * 实现细节：
 * - 使用空格/tab/换行符作为分隔符
 * - 当前不支持引号包裹带空格的参数 (简化实现)
 * - 最多支持63个参数 (argv[63]保留为NULL)
 * - 每个参数都通过strdup独立分配内存
 */
static char **parse_command_line(const char *cmdline, int *argc) {
  if (!cmdline || !argc)
    return NULL;

  // 去除首尾空格
  while (*cmdline && isspace(*cmdline))
    cmdline++;
  if (*cmdline == '\0') {
    *argc = 0;
    return NULL;
  }

  // 简单的空格分割（不处理引号）
  // 注意：复杂场景(如参数包含空格)需使用更强大的解析器
  char *line_copy = strdup(cmdline);
  if (!line_copy)
    return NULL;

  char **argv = malloc(64 * sizeof(char *)); // 预分配64个指针槽位
  if (!argv) {
    free(line_copy);
    return NULL;
  }

  *argc = 0;
  char *token = strtok(line_copy, " \t\n"); // 第一次调用strtok
  while (token && *argc < 63) {             // 保留argv[63]为NULL
    argv[(*argc)++] = strdup(token);        // 为每个token独立分配内存
    token = strtok(NULL, " \t\n");          // 后续调用strtok(NULL, ...)
  }
  argv[*argc] = NULL; // NULL终止符，符合execv等系统调用约定

  free(line_copy); // 释放临时缓冲区
  return argv;
}

static void free_argv(char **argv, int argc) {
  if (!argv)
    return;
  for (int i = 0; i < argc; i++) {
    free(argv[i]);
  }
  free(argv);
}

/**
 * 执行单条命令 (命令分发核心)
 * @param cmdline 完整命令行字符串 (例如: "mcar" 或 "mccr create 5000000")
 * @return 0=成功执行 -1=失败
 *
 * 执行流程：
 * 1. 解析命令行为 argc/argv
 * 2. 提取命令名(argv[0])
 * 3. 特殊处理help命令
 * 4. 在命令表中查找匹配项(支持主名称和别名)
 * 5. 调用对应的handler函数
 * 6. 清理内存
 */
int cli_execute_command(const char *cmdline) {
  if (!cmdline || !*cmdline)
    return 0;

  // 步骤1: 解析命令行
  int argc = 0;
  char **argv = parse_command_line(cmdline, &argc);
  if (argc == 0) {
    free_argv(argv, argc);
    return 0;
  }

  // 步骤2: 提取命令名
  const char *cmd = argv[0];

  // 步骤3: 特殊处理 help 命令 (因为handler为NULL)
  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    if (argc > 1) {
      cli_print_help(argv[1]); // help <command>
    } else {
      cli_print_help(NULL); // help (显示所有)
    }
    free_argv(argv, argc);
    return 0;
  }

  // 步骤4-5: 查找并执行命令
  for (int i = 0; g_commands[i].name != NULL; i++) {
    // 匹配主命令名或别名
    if (strcmp(g_commands[i].name, cmd) == 0 ||
        (g_commands[i].alias && strcmp(g_commands[i].alias, cmd) == 0)) {

      // 安全检查：handler不应为NULL (除help外)
      if (g_commands[i].handler == NULL) {
        cli_error("命令 '%s' 未实现", cmd);
        free_argv(argv, argc);
        return -1;
      }

      // 调用命令处理函数
      int ret = g_commands[i].handler(argc, argv);
      free_argv(argv, argc);
      return ret;
    }
  }

  // 未找到匹配命令
  cli_error("未知命令: %s (输入 'help' 查看可用命令)", cmd);
  free_argv(argv, argc);
  return -1;
}

/* ==================== CLI 初始化与主循环 ==================== */

int cli_init(void) {
  // 初始化会话管理器
  session_manager_init(&g_session_manager);

  // 初始化 DLM 状态管理器 (v2.1: MSCR 解析支持)
  dlm_status_init();

  // 初始化 readline
  rl_attempted_completion_function = NULL; // TODO: 添加自动补全

  g_cli_running = false;
  g_is_registered = false;
  g_session_active = false;
  g_session_id[0] = '\0';

  printf("[CLI] Multi-session support enabled (max %d concurrent sessions)\n",
         MAX_CLIENT_SESSIONS);
  printf("[CLI] DLM status tracking enabled (v2.1 MSCR enhanced)\n");

  return 0;
}

/**
 * CLI 主交互循环 (运行在前台线程)
 * @return 0=正常退出 -1=异常
 *
 * 功能说明：
 * - 使用GNU readline库提供命令行编辑和历史记录功能
 * - 提示符根据当前状态动态显示颜色和文字：
 *   🔴 "MAGIC[未注册]>"         - 初始状态
 *   🟡 "MAGIC[已注册]>"         - MCAR成功后
 *   🟢 "MAGIC[已注册+通信中]>"  - MCCR Create成功后
 * - 支持Ctrl+D优雅退出
 * - 自动跳过空行
 * - 所有非空命令自动加入历史记录(支持↑↓键翻查)
 */
int cli_run_loop(void) {
  g_cli_running = true;

  printf("\n[DEBUG] 进入 cli_run_loop(), g_cli_running = %d\n", g_cli_running);
  fflush(stdout);

  // 打印欢迎横幅
  printf("\n");
  printf(COLOR_GREEN "╔══════════════════════════════════════════════╗\n");
  printf("║                                              ║\n");
  printf("║      MAGIC Client - ARINC 839-2014          ║\n");
  printf("║      航空电子 Diameter 通信客户端            ║\n");
  printf("║                                              ║\n");
  printf("╚══════════════════════════════════════════════╝" COLOR_RESET "\n\n");

  printf("输入 " COLOR_YELLOW "help" COLOR_RESET " 查看所有命令\n");
  printf("输入 " COLOR_YELLOW "mcar" COLOR_RESET " 开始客户端注册\n\n");
  fflush(stdout);

  printf("[DEBUG] 即将进入主循环\n");
  fflush(stdout);

  char *line = NULL;
  int loop_count = 0;
  while (g_cli_running) {
    loop_count++;
    if (loop_count == 1) {
      printf("[DEBUG] 开始循环迭代 #%d\n", loop_count);
      fflush(stdout);
    }

    // 构造状态感知的动态提示符
    // 颜色编码：绿色=正常运行，黄色=部分就绪，红色=未就绪
    char prompt[128];
    if (g_is_registered && g_session_active) {
      // 状态3: 已注册且有活跃通信会话 (全功能就绪)
      snprintf(prompt, sizeof(prompt),
               COLOR_GREEN "MAGIC[已注册+通信中]>" COLOR_RESET " ");
    } else if (g_is_registered) {
      // 状态2: 已注册但无通信会话 (可执行MCCR等命令)
      snprintf(prompt, sizeof(prompt),
               COLOR_YELLOW "MAGIC[已注册]>" COLOR_RESET " ");
    } else {
      // 状态1: 未注册 (只能执行MCAR或help/status)
      snprintf(prompt, sizeof(prompt),
               COLOR_RED "MAGIC[未注册]>" COLOR_RESET " ");
    }

    // 使用readline读取用户输入 (支持命令行编辑、历史记录、自动补全)
    if (loop_count == 1) {
      printf("[DEBUG] 调用 readline(\"%s\")\n", prompt);
      fflush(stdout);
    }
    line = readline(prompt);

    if (loop_count == 1) {
      printf("[DEBUG] readline 返回: %s\n", line ? line : "(NULL)");
      fflush(stdout);
    }

    // 处理EOF (Ctrl+D)
    if (line == NULL) {
      printf("\n[DEBUG] 收到 EOF (Ctrl+D)，退出循环\n");
      fflush(stdout);
      break;
    }

    // 跳过空行 (用户直接回车)
    if (line[0] == '\0') {
      free(line);
      continue;
    }

    // 添加到历史记录 (支持↑↓键翻查)
    add_history(line);

    // 执行命令
    cli_execute_command(line);

    // 释放readline分配的内存
    free(line);
  }

  printf("[DEBUG] 退出主循环，总循环次数: %d\n", loop_count);
  fflush(stdout);

  return 0;
}

void cli_cleanup(void) {
  g_cli_running = false;
  session_manager_cleanup(&g_session_manager);
  clear_history();
}

/* ==================== 会话管理器访问 ==================== */

SessionManager *cli_get_session_manager(void) { return &g_session_manager; }
