#include "magic_client.h"
#include "magic_auth.h"
#include "magic_network.h"
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <stdarg.h>

// 全局回调函数指针
static state_change_callback_t g_state_callback = NULL;
static network_config_callback_t g_network_callback = NULL;
static error_callback_t g_error_callback = NULL;

// 内部函数声明
static void* heartbeat_thread_func(void *arg);
static void* network_monitor_thread_func(void *arg);
static void magic_client_set_state(magic_client_t *client, client_state_t new_state);
static void magic_client_handle_error(magic_client_t *client, const char *error_msg);

/**
 * 创建MAGIC客户端实例
 */
magic_client_t* magic_client_create(const void *config) {
    magic_client_t *client = malloc(sizeof(magic_client_t));
    if (!client) {
        magic_client_log("ERROR", "Failed to allocate memory for client");
        return NULL;
    }
    
    if (magic_client_init(client, (const client_config_t*)config) != 0) {
        free(client);
        return NULL;
    }
    
    return client;
}

/**
 * 销毁MAGIC客户端实例
 */
void magic_client_destroy(magic_client_t *client) {
    if (!client) {
        return;
    }
    
    magic_client_cleanup(client);
    free(client);
}

/**
 * 获取状态字符串
 */
const char* magic_client_get_state_string(client_state_t state) {
    switch (state) {
        case CLIENT_STATE_DISCONNECTED: return "Disconnected";
        case CLIENT_STATE_CONNECTING: return "Connecting";
        case CLIENT_STATE_AUTHENTICATING: return "Authenticating";
        case CLIENT_STATE_AUTHENTICATED: return "Authenticated";
        case CLIENT_STATE_NETWORK_CONFIGURED: return "Network Configured";
        case CLIENT_STATE_READY: return "Ready";
        case CLIENT_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}

/**
 * 初始化MAGIC客户端
 */
int magic_client_init(magic_client_t *client, const client_config_t *config) {
    if (!client || !config) {
        return -1;
    }
    
    // 清零客户端结构
    memset(client, 0, sizeof(magic_client_t));
    
    // 复制配置
    memcpy(&client->config, config, sizeof(client_config_t));
    
    // 初始化状态
    client->state = CLIENT_STATE_DISCONNECTED;
    client->server_socket = -1;
    client->running = false;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&client->state_mutex, NULL) != 0) {
        magic_client_log("ERROR", "Failed to initialize state mutex");
        return -1;
    }
    
    // 初始化认证模块
    if (magic_auth_init() != 0) {
        magic_client_log("ERROR", "Failed to initialize authentication module");
        pthread_mutex_destroy(&client->state_mutex);
        return -1;
    }
    
    magic_client_log("INFO", "MAGIC client initialized successfully");
    return 0;
}

/**
 * 连接到MAGIC服务端
 */
int magic_client_connect(magic_client_t *client) {
    if (!client) {
        return -1;
    }
    
    magic_client_set_state(client, CLIENT_STATE_CONNECTING);
    
    // 创建套接字
    client->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client->server_socket < 0) {
        magic_client_handle_error(client, "Failed to create socket");
        return -1;
    }
    
    // 设置套接字选项
    int opt = 1;
    setsockopt(client->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 设置连接超时
    struct timeval timeout;
    timeout.tv_sec = client->config.auth_timeout;
    timeout.tv_usec = 0;
    setsockopt(client->server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->server_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // 连接服务端
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->config.server_port);
    
    if (inet_pton(AF_INET, client->config.server_address, &server_addr.sin_addr) <= 0) {
        magic_client_handle_error(client, "Invalid server address");
        close(client->server_socket);
        client->server_socket = -1;
        return -1;
    }
    
    if (connect(client->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to connect to server: %s", strerror(errno));
        magic_client_handle_error(client, error_msg);
        close(client->server_socket);
        client->server_socket = -1;
        return -1;
    }
    
    // 记录连接时间
    client->stats.connect_time = time(NULL);
    client->stats.reconnect_count++;
    
    magic_client_log("INFO", "Connected to MAGIC server %s:%d", 
                     client->config.server_address, client->config.server_port);
    
    return 0;
}

/**
 * 执行客户端认证
 */
int magic_client_authenticate(magic_client_t *client) {
    if (!client || client->server_socket < 0) {
        return -1;
    }
    
    magic_client_set_state(client, CLIENT_STATE_AUTHENTICATING);
    
    // 执行认证流程
    int result = magic_auth_perform_authentication(client);
    if (result == 0) {
        magic_client_set_state(client, CLIENT_STATE_AUTHENTICATED);
        magic_client_log("INFO", "Authentication successful");
        
        // 配置网络
        if (magic_client_configure_network(client) == 0) {
            magic_client_set_state(client, CLIENT_STATE_READY);
            client->running = true;
            
            // 启动心跳线程
            if (pthread_create(&client->heartbeat_thread, NULL, heartbeat_thread_func, client) != 0) {
                magic_client_log("WARNING", "Failed to start heartbeat thread");
            }
            
            // 启动网络监控线程
            if (pthread_create(&client->network_monitor_thread, NULL, network_monitor_thread_func, client) != 0) {
                magic_client_log("WARNING", "Failed to start network monitor thread");
            }
        } else {
            magic_client_handle_error(client, "Failed to configure network");
            return -1;
        }
    } else {
        magic_client_handle_error(client, "Authentication failed");
        return -1;
    }
    
    return 0;
}

/**
 * 配置网络
 */
int magic_client_configure_network(magic_client_t *client) {
    if (!client || !client->network.is_configured) {
        return -1;
    }
    
    network_manager_t network_manager;
    if (magic_network_init(&network_manager) != 0) {
        magic_client_log("ERROR", "Failed to initialize network manager");
        return -1;
    }
    
    // 备份当前网络配置
    if (magic_network_backup_current_config(&network_manager) != 0) {
        magic_client_log("WARNING", "Failed to backup current network configuration");
    }
    
    // 应用新的网络配置
    if (magic_network_apply_config(&network_manager, &client->network) != 0) {
        magic_client_log("ERROR", "Failed to apply network configuration");
        magic_network_cleanup(&network_manager);
        return -1;
    }
    
    // 测试网络连通性
    if (magic_network_connectivity_test(&client->network) != 0) {
        magic_client_log("WARNING", "Network connectivity test failed");
    }
    
    magic_client_set_state(client, CLIENT_STATE_NETWORK_CONFIGURED);
    
    // 调用网络配置回调
    if (g_network_callback) {
        g_network_callback(&client->network);
    }
    
    magic_client_log("INFO", "Network configured: IP=%s, Gateway=%s", 
                     client->network.assigned_ip, client->network.gateway);
    
    magic_network_cleanup(&network_manager);
    return 0;
}

/**
 * 恢复网络配置
 */
int magic_client_restore_network(magic_client_t *client) {
    if (!client) {
        return -1;
    }
    
    network_manager_t network_manager;
    if (magic_network_init(&network_manager) != 0) {
        return -1;
    }
    
    int result = magic_network_restore_config(&network_manager);
    magic_network_cleanup(&network_manager);
    
    if (result == 0) {
        magic_client_log("INFO", "Network configuration restored");
    } else {
        magic_client_log("ERROR", "Failed to restore network configuration");
    }
    
    return result;
}

/**
 * 断开连接
 */
int magic_client_disconnect(magic_client_t *client) {
    if (!client) {
        return -1;
    }
    
    client->running = false;
    
    // 等待线程结束
    if (client->heartbeat_thread) {
        pthread_join(client->heartbeat_thread, NULL);
    }
    if (client->network_monitor_thread) {
        pthread_join(client->network_monitor_thread, NULL);
    }
    
    // 关闭套接字
    if (client->server_socket >= 0) {
        close(client->server_socket);
        client->server_socket = -1;
    }
    
    // 恢复网络配置
    magic_client_restore_network(client);
    
    magic_client_set_state(client, CLIENT_STATE_DISCONNECTED);
    magic_client_log("INFO", "Disconnected from MAGIC server");
    
    return 0;
}

/**
 * 清理客户端资源
 */
void magic_client_cleanup(magic_client_t *client) {
    if (!client) {
        return;
    }
    
    // 断开连接
    magic_client_disconnect(client);
    
    // 清理认证模块
    magic_auth_cleanup();
    
    // 销毁互斥锁
    pthread_mutex_destroy(&client->state_mutex);
    
    magic_client_log("INFO", "MAGIC client cleanup completed");
}

/**
 * 获取客户端状态
 */
client_state_t magic_client_get_state(magic_client_t *client) {
    if (!client) {
        return CLIENT_STATE_ERROR;
    }
    
    pthread_mutex_lock(&client->state_mutex);
    client_state_t state = client->state;
    pthread_mutex_unlock(&client->state_mutex);
    
    return state;
}

/**
 * 状态转换为字符串
 */
const char* magic_client_state_to_string(client_state_t state) {
    switch (state) {
        case CLIENT_STATE_DISCONNECTED: return "DISCONNECTED";
        case CLIENT_STATE_CONNECTING: return "CONNECTING";
        case CLIENT_STATE_AUTHENTICATING: return "AUTHENTICATING";
        case CLIENT_STATE_AUTHENTICATED: return "AUTHENTICATED";
        case CLIENT_STATE_NETWORK_CONFIGURED: return "NETWORK_CONFIGURED";
        case CLIENT_STATE_READY: return "READY";
        case CLIENT_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * 检查是否已连接
 */
bool magic_client_is_connected(magic_client_t *client) {
    client_state_t state = magic_client_get_state(client);
    return state >= CLIENT_STATE_CONNECTING && state != CLIENT_STATE_ERROR;
}

/**
 * 检查是否已认证
 */
bool magic_client_is_authenticated(magic_client_t *client) {
    client_state_t state = magic_client_get_state(client);
    return state >= CLIENT_STATE_AUTHENTICATED && state != CLIENT_STATE_ERROR;
}

/**
 * 获取统计信息
 */
void magic_client_get_stats(magic_client_t *client, connection_stats_t *stats) {
    if (!client || !stats) {
        return;
    }
    
    pthread_mutex_lock(&client->state_mutex);
    memcpy(stats, &client->stats, sizeof(connection_stats_t));
    pthread_mutex_unlock(&client->state_mutex);
}

/**
 * 重置统计信息
 */
void magic_client_reset_stats(magic_client_t *client) {
    if (!client) {
        return;
    }
    
    pthread_mutex_lock(&client->state_mutex);
    memset(&client->stats, 0, sizeof(connection_stats_t));
    pthread_mutex_unlock(&client->state_mutex);
}

/**
 * 设置状态变化回调
 */
void magic_client_set_state_callback(state_change_callback_t callback) {
    g_state_callback = callback;
}

/**
 * 设置网络配置回调
 */
void magic_client_set_network_callback(network_config_callback_t callback) {
    g_network_callback = callback;
}

/**
 * 设置错误回调
 */
void magic_client_set_error_callback(error_callback_t callback) {
    g_error_callback = callback;
}

/**
 * 初始化日志系统
 */
void magic_client_log_init(void) {
    // 这里可以初始化日志文件、设置日志级别等
    // 目前使用简单的控制台输出
}

/**
 * 清理日志系统
 */
void magic_client_log_cleanup(void) {
    // 这里可以关闭日志文件等
}

/**
 * 日志记录函数
 */
void magic_client_log(const char *level, const char *format, ...) {
    va_list args;
    time_t now;
    struct tm *tm_info;
    char timestamp[64];
    char message[MAX_LOG_MSG_LEN];
    
    // 获取当前时间
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 格式化消息
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // 输出日志
    printf("[%s] [%s] %s\n", timestamp, level, message);
    fflush(stdout);
}

// 内部函数实现

/**
 * 设置客户端状态
 */
static void magic_client_set_state(magic_client_t *client, client_state_t new_state) {
    if (!client) {
        return;
    }
    
    pthread_mutex_lock(&client->state_mutex);
    client_state_t old_state = client->state;
    client->state = new_state;
    pthread_mutex_unlock(&client->state_mutex);
    
    // 调用状态变化回调
    if (g_state_callback && old_state != new_state) {
        g_state_callback(old_state, new_state);
    }
    
    magic_client_log("INFO", "State changed: %s -> %s", 
                     magic_client_state_to_string(old_state),
                     magic_client_state_to_string(new_state));
}

/**
 * 处理错误
 */
static void magic_client_handle_error(magic_client_t *client, const char *error_msg) {
    if (!client || !error_msg) {
        return;
    }
    
    strncpy(client->last_error, error_msg, sizeof(client->last_error) - 1);
    client->last_error[sizeof(client->last_error) - 1] = '\0';
    
    magic_client_set_state(client, CLIENT_STATE_ERROR);
    magic_client_log("ERROR", "%s", error_msg);
    
    // 调用错误回调
    if (g_error_callback) {
        g_error_callback(error_msg);
    }
}

/**
 * 心跳线程函数
 */
static void* heartbeat_thread_func(void *arg) {
    magic_client_t *client = (magic_client_t*)arg;
    
    while (client->running) {
        sleep(client->config.heartbeat_interval);
        
        if (!client->running) {
            break;
        }
        
        // 检查会话是否有效
        if (!magic_auth_is_session_valid(&client->auth)) {
            magic_client_log("WARNING", "Session expired, attempting to refresh");
            if (magic_auth_refresh_session(client) != 0) {
                magic_client_handle_error(client, "Failed to refresh session");
                break;
            }
        }
        
        // 发送心跳包（这里可以实现具体的心跳逻辑）
        magic_client_log("DEBUG", "Heartbeat sent");
    }
    
    return NULL;
}

/**
 * 网络监控线程函数
 */
static void* network_monitor_thread_func(void *arg) {
    magic_client_t *client = (magic_client_t*)arg;
    
    while (client->running) {
        sleep(30); // 每30秒检查一次
        
        if (!client->running) {
            break;
        }
        
        // 检查网络连通性
        if (magic_network_ping_test(client->network.gateway, 5000) != 0) {
            magic_client_log("WARNING", "Gateway ping test failed");
        }
        
        // 更新网络统计信息（这里可以实现具体的统计逻辑）
        magic_client_log("DEBUG", "Network monitoring check completed");
    }
    
    return NULL;
}