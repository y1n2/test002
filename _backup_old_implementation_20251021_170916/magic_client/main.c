#include "magic_client.h"
#include "magic_config.h"
#include "magic_auth.h"
#include "magic_network.h"
#include "magic_proxy.h"
#include <signal.h>
#include <getopt.h>
#include <sys/wait.h>

// 全局变量
static magic_client_t *g_client = NULL;
static volatile bool g_running = true;
static volatile bool g_restart_requested = false;
external_access_manager_t *g_external_manager = NULL;

// 函数声明
static void signal_handler(int sig);
static void print_usage(const char *program_name);
static void print_version(void);
static int parse_command_line(int argc, char *argv[], magic_client_config_t *config);
static int run_client(magic_client_t *client);

static void print_status(const magic_client_t *client);
static void print_statistics(const magic_client_t *client);
static int handle_interactive_commands(magic_client_t *client);
static void print_help_commands(void);

/**
 * 信号处理函数
 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            magic_client_log("INFO", "Received signal %d, shutting down gracefully...", sig);
            g_running = false;
            break;
        case SIGHUP:
            magic_client_log("INFO", "Received SIGHUP, restarting...");
            g_restart_requested = true;
            break;
        case SIGUSR1:
            if (g_client) {
                print_status(g_client);
            }
            break;
        case SIGUSR2:
            if (g_client) {
                print_statistics(g_client);
            }
            break;
        default:
            break;
    }
}

/**
 * 主函数
 */
int main(int argc, char *argv[]) {
    int exit_code = 0;
    magic_client_config_t config;
    magic_client_t *client = NULL;
    
    // 初始化日志系统
    magic_client_log_init();
    magic_client_log("INFO", "MAGIC Client v%s starting...", MAGIC_CLIENT_VERSION);
    
    // 初始化配置
    if (magic_config_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize configuration\n");
        exit_code = 1;
        goto cleanup;
    }
    
    // 解析命令行参数
    if (parse_command_line(argc, argv, &config) != 0) {
        exit_code = 1;
        goto cleanup;
    }
    
    // 验证配置
    if (magic_config_validate(&config) != 0) {
        fprintf(stderr, "Configuration validation failed\n");
        exit_code = 1;
        goto cleanup;
    }
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    // 主循环 - 支持重启
    do {
        g_restart_requested = false;
        g_running = true;
        
        // 创建客户端实例
        client = magic_client_create(&config);
        if (!client) {
            fprintf(stderr, "Failed to create client instance\n");
            exit_code = 1;
            break;
        }
        
        g_client = client;
        
        // 运行客户端
        magic_client_log("INFO", "Starting MAGIC client...");
        int run_result = run_client(client);
        
        if (run_result != 0 && !g_restart_requested) {
            exit_code = run_result;
        }
        
        // 清理客户端实例
        magic_client_destroy(client);
        g_client = NULL;
        
        if (g_restart_requested) {
            magic_client_log("INFO", "Restarting client...");
            sleep(2); // 短暂延迟
        }
        
    } while (g_restart_requested && g_running);

cleanup:
    magic_config_cleanup(&config);
    magic_client_log("INFO", "MAGIC Client shutdown complete");
    magic_client_log_cleanup();
    
    return exit_code;
}

/**
 * 运行客户端主循环
 */
static int run_client(magic_client_t *client) {
    if (!client) {
        return -1;
    }
    
    // 连接到服务端
    magic_client_log("INFO", "Connecting to server...");
    if (magic_client_connect(client) != 0) {
        magic_client_log("ERROR", "Failed to connect to server");
        return -1;
    }
    
    // 执行认证
    magic_client_log("INFO", "Authenticating...");
    if (magic_client_authenticate(client) != 0) {
        magic_client_log("ERROR", "Authentication failed");
        magic_client_disconnect(client);
        return -1;
    }
    
    magic_client_log("INFO", "Client connected and authenticated successfully");
    
    // 主事件循环
    while (g_running && !g_restart_requested) {
        // 检查连接状态
        if (magic_client_get_state(client) != CLIENT_STATE_READY) {
            magic_client_log("WARNING", "Connection lost, attempting to reconnect...");
            
            if (magic_client_connect(client) != 0 || 
                magic_client_authenticate(client) != 0) {
                magic_client_log("ERROR", "Reconnection failed");
                sleep(5);
                continue;
            }
            
            magic_client_log("INFO", "Reconnected successfully");
        }
        
        // 处理交互式命令（如果启用）
        if (client->config.log_file[0] != '\0') {
            handle_interactive_commands(client);
        }
        
        // 短暂休眠
        usleep(100000); // 100ms
    }
    
    // 断开连接
    magic_client_log("INFO", "Disconnecting from server...");
    magic_client_disconnect(client);
    
    return 0;
}

/**
 * 解析命令行参数
 */
static int parse_command_line(int argc, char *argv[], magic_client_config_t *config) {
    int opt;
    bool config_loaded = false;
    
    static struct option long_options[] = {
        {"config",      required_argument, 0, 'c'},
        {"server",      required_argument, 0, 's'},
        {"port",        required_argument, 0, 'p'},
        {"username",    required_argument, 0, 'u'},
        {"password",    required_argument, 0, 'P'},
        {"client-id",   required_argument, 0, 'i'},
        {"realm",       required_argument, 0, 'r'},
        {"verbose",     no_argument,       0, 'v'},
        {"quiet",       no_argument,       0, 'q'},
        {"daemon",      no_argument,       0, 'd'},
        {"no-tls",      no_argument,       0, 'n'},
        {"help",        no_argument,       0, 'h'},
        {"version",     no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "c:s:p:u:P:i:r:vqdnhV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                if (magic_config_load(config, optarg) == 0) {
                    config_loaded = true;
                } else {
                    fprintf(stderr, "Failed to load config file: %s\n", optarg);
                    return -1;
                }
                break;
                
            case 's':
                strncpy(config->server.hostname, optarg, sizeof(config->server.hostname) - 1);
                config->server.hostname[sizeof(config->server.hostname) - 1] = '\0';
                break;
                
            case 'p':
                config->server.port = atoi(optarg);
                if (config->server.port <= 0 || config->server.port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return -1;
                }
                break;
                
            case 'u':
                strncpy(config->auth.username, optarg, sizeof(config->auth.username) - 1);
                config->auth.username[sizeof(config->auth.username) - 1] = '\0';
                break;
                
            case 'P':
                strncpy(config->auth.password, optarg, sizeof(config->auth.password) - 1);
                config->auth.password[sizeof(config->auth.password) - 1] = '\0';
                // 清除命令行中的密码
                memset(optarg, '*', strlen(optarg));
                break;
                
            case 'i':
                strncpy(config->auth.client_id, optarg, sizeof(config->auth.client_id) - 1);
                config->auth.client_id[sizeof(config->auth.client_id) - 1] = '\0';
                break;
                
            case 'r':
                strncpy(config->auth.realm, optarg, sizeof(config->auth.realm) - 1);
                config->auth.realm[sizeof(config->auth.realm) - 1] = '\0';
                break;
                
            case 'v':
                strcpy(config->log.log_level, "DEBUG");
                break;
                
            case 'q':
                strcpy(config->log.log_level, "ERROR");
                config->log.log_to_console = false;
                break;
                
            case 'd':
                config->log.log_to_console = false;
                // 这里可以添加守护进程化的代码
                break;
                
            case 'n':
                config->server.use_tls = false;
                break;
                
            case 'h':
                print_usage(argv[0]);
                exit(0);
                break;
                
            case 'V':
                print_version();
                exit(0);
                break;
                
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    // 如果没有加载配置文件，尝试加载默认配置
    if (!config_loaded) {
        char *default_config = magic_config_find_config_file();
        if (default_config) {
            if (magic_config_load(config, default_config) != 0) {
                magic_config_load_default(config);
            }
        } else {
            magic_config_load_default(config);
        }
    }
    
    return 0;
}

/**
 * 打印使用说明
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nMAGIC Client - Secure network access client\n");
    printf("\nOptions:\n");
    printf("  -c, --config FILE       Configuration file path\n");
    printf("  -s, --server HOST       Server hostname or IP address\n");
    printf("  -p, --port PORT         Server port (default: 3868)\n");
    printf("  -u, --username USER     Username for authentication\n");
    printf("  -P, --password PASS     Password for authentication\n");
    printf("  -i, --client-id ID      Client identifier\n");
    printf("  -r, --realm REALM       Authentication realm\n");
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("  -q, --quiet             Quiet mode (errors only)\n");
    printf("  -d, --daemon            Run as daemon\n");
    printf("  -n, --no-tls            Disable TLS encryption\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -V, --version           Show version information\n");
    printf("\nSignals:\n");
    printf("  SIGINT/SIGTERM          Graceful shutdown\n");
    printf("  SIGHUP                  Restart client\n");
    printf("  SIGUSR1                 Print status\n");
    printf("  SIGUSR2                 Print statistics\n");
    printf("\nInteractive Commands (when running in console mode):\n");
    printf("  status                  Show connection status\n");
    printf("  stats                   Show statistics\n");
    printf("  reconnect               Force reconnection\n");
    printf("  quit                    Graceful shutdown\n");
    printf("  help                    Show command help\n");
}

/**
 * 打印版本信息
 */
static void print_version(void) {
    printf("MAGIC Client v%s\n", MAGIC_CLIENT_VERSION);
    printf("Build date: %s %s\n", __DATE__, __TIME__);
    printf("Copyright (c) 2024 MAGIC Project\n");
}

/**
 * 处理交互式命令
 */
static int handle_interactive_commands(magic_client_t *client) {
    fd_set readfds;
    struct timeval timeout;
    char command[256];
    
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0) {
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(command, sizeof(command), stdin)) {
                // 移除换行符
                command[strcspn(command, "\n")] = '\0';
                
                if (strcmp(command, "status") == 0) {
                    print_status(client);
                } else if (strcmp(command, "stats") == 0) {
                    print_statistics(client);
                } else if (strcmp(command, "reconnect") == 0) {
                    magic_client_log("INFO", "Forcing reconnection...");
                    magic_client_disconnect(client);
                } else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
                    g_running = false;
                } else if (strcmp(command, "help") == 0) {
                    print_help_commands();
                } else if (strlen(command) > 0) {
                    printf("Unknown command: %s (type 'help' for available commands)\n", command);
                }
            }
        }
    }
    
    return 0;
}

/**
 * 打印命令帮助
 */
static void print_help_commands(void) {
    printf("\nAvailable commands:\n");
    printf("  status      - Show connection status\n");
    printf("  stats       - Show connection statistics\n");
    printf("  reconnect   - Force reconnection to server\n");
    printf("  quit/exit   - Graceful shutdown\n");
    printf("  help        - Show this help\n");
    printf("\n");
}

/**
 * 打印客户端状态
 */
static void print_status(const magic_client_t *client) {
    if (!client) {
        printf("Client not initialized\n");
        return;
    }
    
    printf("\n=== MAGIC Client Status ===\n");
    printf("State: %s\n", magic_client_state_to_string(magic_client_get_state((magic_client_t*)client)));
    printf("Server: %s:%d\n", client->config.server_address, client->config.server_port);
    printf("Client ID: %s\n", client->config.client_id);
    
    if (magic_client_get_state((magic_client_t*)client) == CLIENT_STATE_READY) {
        printf("Assigned IP: %s\n", client->network.assigned_ip);
        printf("Session ID: %s\n", client->auth.session_id);
        printf("Connected since: %s", ctime(&client->stats.connect_time));
        printf("Service Type: %d\n", client->config.service_type);
        printf("Priority: %d\n", client->config.priority);
    }
    printf("===========================\n\n");
}

/**
 * 打印统计信息
 */
static void print_statistics(const magic_client_t *client) {
    if (!client) {
        printf("Client not initialized\n");
        return;
    }
    
    connection_stats_t stats;
    magic_client_get_stats((magic_client_t*)client, &stats);
    
    printf("\n=== Connection Statistics ===\n");
    printf("Bytes sent: %lu\n", stats.bytes_sent);
    printf("Bytes received: %lu\n", stats.bytes_received);
    printf("Packets sent: %u\n", stats.packets_sent);
    printf("Packets received: %u\n", stats.packets_received);
    printf("Authentication attempts: %u\n", stats.auth_attempts);
    printf("Reconnect count: %u\n", stats.reconnect_count);
    printf("Last error: %s\n", client->last_error[0] ? client->last_error : "None");
    printf("Uptime: %ld seconds\n", time(NULL) - stats.connect_time);
    printf("=============================\n\n");
}