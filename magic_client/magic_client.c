/**
 * @file magic_client.c
 * @brief MAGIC 客户端主程序入口。
 * @details 负责初始化 freeDiameter 核心库、加载业务配置、注册消息处理器并启动
 * CLI 交互界面。
 */

#include "cli_interface.h"
#include "config.h"
#include "magic_commands.h"
#include "magic_dict_handles.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/** @brief freeDiameter 预设配置文件路径。 */
#define DEFAULT_CONFIG_FILE                                                    \
  "/home/zhuwuhui/freeDiameter/magic_client/magic_client.conf"
/** @brief MAGIC 业务层预设配置文件路径。 */
#define DEFAULT_CONFIG_MAGIC_FILE                                              \
  "/home/zhuwuhui/freeDiameter/magic_client/EFB_magic.conf"

static void print_usage(const char *prog_name) {
  printf("用法: %s [选项]\n", prog_name);
  printf("选项:\n");
  printf("  -c, --config <文件>       指定 freeDiameter 配置文件 (默认: %s)\n",
         DEFAULT_CONFIG_FILE);
  printf("  -m, --magic-config <文件> 指定 MAGIC 业务配置文件 (默认: %s)\n",
         DEFAULT_CONFIG_MAGIC_FILE);
  printf("  -h, --help                显示此帮助信息\n");
}

/* 全局变量声明 */
extern app_config_t g_cfg;
extern struct magic_dict_handles g_magic_dict;
extern struct std_diam_dict_handles g_std_dict;

/* freeDiameter 核心线程函数 */
static void *fd_core_thread(void *arg) {
  // 等待 freeDiameter 核心关闭
  fd_core_wait_shutdown_complete();
  return NULL;
}

int main(int argc, char *argv[]) {
  int ret = 0;
  pthread_t fd_thread;
  char *fd_conf = DEFAULT_CONFIG_FILE;
  char *magic_conf = DEFAULT_CONFIG_MAGIC_FILE;
  int opt;

  static struct option long_options[] = {
      {"config", required_argument, 0, 'c'},
      {"magic-config", required_argument, 0, 'm'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "c:m:h", long_options, NULL)) != -1) {
    switch (opt) {
    case 'c':
      fd_conf = optarg;
      break;
    case 'm':
      magic_conf = optarg;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  printf("\n=================================================\n");
  printf("  MAGIC Client - ARINC 839-2014 航空通信客户端\n");
  printf("=================================================\n\n");

  // 1. 初始化 freeDiameter 核心库
  printf("[1/7] 初始化 freeDiameter 核心...\n");
  ret = fd_core_initialize();
  if (ret != 0) {
    fprintf(stderr, "ERROR: freeDiameter 核心初始化失败: %d\n", ret);
    return ret;
  }

  // 2. 解析 freeDiameter 主配置文件
  printf("[2/7] 加载 freeDiameter 配置: %s\n", fd_conf);
  ret = fd_core_parseconf(fd_conf);
  if (ret != 0) {
    fprintf(stderr, "ERROR: 配置文件解析失败: %d\n", ret);
    fd_core_shutdown();
    return ret;
  }

  // 3. 初始化 MAGIC 字典
  printf("[3/7] 初始化 MAGIC 协议字典...\n");
  ret = magic_dict_init();
  if (ret != 0) {
    fprintf(stderr, "ERROR: MAGIC 字典初始化失败: %d\n", ret);
    fd_core_shutdown();
    return ret;
  }

  // 4. 解析 MAGIC 客户端配置文件
  printf("[4/7] 加载 MAGIC 客户端配置: %s\n", magic_conf);
  ret = magic_conf_parse(magic_conf);
  if (ret != 0) {
    fprintf(stderr, "ERROR: MAGIC 配置文件解析失败: %d\n", ret);
    fd_core_shutdown();
    return ret;
  }

  // 5. 注册服务器推送消息处理器 (MSCR/MNTR)
  printf("[5/7] 注册服务器推送消息处理器...\n");
  ret = magic_mscr_handler_init();
  if (ret != 0) {
    fprintf(stderr, "WARNING: MSCR 处理器注册失败 (可能缺少字典定义): %d\n",
            ret);
    // 不致命，继续运行
  }
  ret = magic_mntr_handler_init();
  if (ret != 0) {
    fprintf(stderr, "WARNING: MNTR 处理器注册失败 (可能缺少字典定义): %d\n",
            ret);
    // 不致命，继续运行
  }

  // 6. 启动 freeDiameter 核心（在独立线程）
  printf("[6/7] 启动 freeDiameter 核心服务...\n");
  ret = fd_core_start();
  if (ret != 0) {
    fprintf(stderr, "ERROR: freeDiameter 核心启动失败: %d\n", ret);
    fd_core_shutdown();
    return ret;
  }

  // 创建 freeDiameter 后台线程
  pthread_create(&fd_thread, NULL, fd_core_thread, NULL);
  pthread_detach(fd_thread);

  // 7. 初始化并启动 CLI 界面
  printf("[7/7] 初始化命令行界面...\n\n");
  ret = cli_init();
  if (ret != 0) {
    fprintf(stderr, "ERROR: CLI 初始化失败: %d\n", ret);
    fd_core_shutdown();
    return ret;
  }

  // 给 freeDiameter 一点时间完成初始化
  sleep(1);

  // 进入 CLI 主循环（阻塞）
  printf("启动命令行界面...\n");
  fflush(stdout);

  ret = cli_run_loop();

  printf("\nCLI 退出，返回值: %d\n", ret);

  // 清理资源
  magic_push_handlers_cleanup();
  cli_cleanup();
  fd_core_shutdown();
  fd_core_wait_shutdown_complete();

  return 0;
}
