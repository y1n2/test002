#ifndef MAGIC_CLIENT_H
#define MAGIC_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>

// MAGIC客户端版本信息
#define MAGIC_CLIENT_VERSION "1.0.0"
#define MAGIC_CLIENT_NAME "MAGIC Aviation Client"

// 配置常量
#define MAX_SERVER_ADDR_LEN 256
#define MAX_CLIENT_ID_LEN 64
#define MAX_PASSWORD_LEN 128
#define MAX_SERVICE_TYPE_LEN 32
#define MAX_IP_ADDR_LEN 16
#define MAX_LOG_MSG_LEN 1024

// 客户端状态枚举
typedef enum {
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_AUTHENTICATING,
    CLIENT_STATE_AUTHENTICATED,
    CLIENT_STATE_NETWORK_CONFIGURED,
    CLIENT_STATE_READY,
    CLIENT_STATE_ERROR
} client_state_t;

// 服务类型枚举
typedef enum {
    SERVICE_TYPE_BASIC = 1,
    SERVICE_TYPE_PREMIUM = 2,
    SERVICE_TYPE_EMERGENCY = 3
} service_type_t;

// 优先级枚举
typedef enum {
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_EMERGENCY = 4
} priority_level_t;

// 网络配置信息
typedef struct {
    char assigned_ip[MAX_IP_ADDR_LEN];      // 服务端分配的IP地址
    char gateway[MAX_IP_ADDR_LEN];          // 网关地址
    char netmask[MAX_IP_ADDR_LEN];          // 子网掩码
    char dns_primary[MAX_IP_ADDR_LEN];      // 主DNS服务器
    char dns_secondary[MAX_IP_ADDR_LEN];    // 备用DNS服务器
    int bandwidth_limit;                     // 带宽限制(Kbps)
    bool is_configured;                      // 网络是否已配置
} network_config_t;

// 客户端配置
typedef struct {
    char server_address[MAX_SERVER_ADDR_LEN];  // MAGIC服务端地址
    int server_port;                           // 服务端端口
    char client_id[MAX_CLIENT_ID_LEN];         // 客户端ID
    char password[MAX_PASSWORD_LEN];           // 认证密码
    service_type_t service_type;               // 服务类型
    priority_level_t priority;                 // 优先级
    int heartbeat_interval;                    // 心跳间隔(秒)
    int auth_timeout;                          // 认证超时(秒)
    bool auto_reconnect;                       // 自动重连
    char log_file[256];                        // 日志文件路径
} client_config_t;

// 认证信息
typedef struct {
    char session_id[64];                       // 会话ID
    time_t auth_time;                          // 认证时间
    time_t expire_time;                        // 过期时间
    bool is_authenticated;                     // 是否已认证
} auth_info_t;

// 连接统计信息
typedef struct {
    time_t connect_time;                       // 连接时间
    unsigned long bytes_sent;                  // 发送字节数
    unsigned long bytes_received;              // 接收字节数
    unsigned int packets_sent;                 // 发送包数
    unsigned int packets_received;             // 接收包数
    unsigned int auth_attempts;                // 认证尝试次数
    unsigned int reconnect_count;              // 重连次数
} connection_stats_t;

// MAGIC客户端主结构
typedef struct {
    client_config_t config;                    // 客户端配置
    client_state_t state;                      // 当前状态
    network_config_t network;                  // 网络配置
    auth_info_t auth;                          // 认证信息
    connection_stats_t stats;                  // 连接统计
    
    int server_socket;                         // 服务端连接套接字
    pthread_t heartbeat_thread;                // 心跳线程
    pthread_t network_monitor_thread;          // 网络监控线程
    pthread_mutex_t state_mutex;               // 状态互斥锁
    
    bool running;                              // 运行标志
    char last_error[MAX_LOG_MSG_LEN];          // 最后错误信息
} magic_client_t;

// 回调函数类型定义
typedef void (*state_change_callback_t)(client_state_t old_state, client_state_t new_state);
typedef void (*network_config_callback_t)(const network_config_t *config);
typedef void (*error_callback_t)(const char *error_msg);

// 核心API函数
magic_client_t* magic_client_create(const void *config);
void magic_client_destroy(magic_client_t *client);
int magic_client_init(magic_client_t *client, const client_config_t *config);
int magic_client_connect(magic_client_t *client);
int magic_client_authenticate(magic_client_t *client);
int magic_client_disconnect(magic_client_t *client);
void magic_client_cleanup(magic_client_t *client);
const char* magic_client_get_state_string(client_state_t state);

// 网络配置函数
int magic_client_configure_network(magic_client_t *client);
int magic_client_restore_network(magic_client_t *client);

// 状态查询函数
client_state_t magic_client_get_state(magic_client_t *client);
const char* magic_client_state_to_string(client_state_t state);
bool magic_client_is_connected(magic_client_t *client);
bool magic_client_is_authenticated(magic_client_t *client);

// 统计信息函数
void magic_client_get_stats(magic_client_t *client, connection_stats_t *stats);
void magic_client_reset_stats(magic_client_t *client);

// 配置管理函数
int magic_client_load_config(const char *config_file, client_config_t *config);
int magic_client_save_config(const char *config_file, const client_config_t *config);
void magic_client_set_default_config(client_config_t *config);

// 回调函数注册
void magic_client_set_state_callback(state_change_callback_t callback);
void magic_client_set_network_callback(network_config_callback_t callback);
void magic_client_set_error_callback(error_callback_t callback);

// 日志函数
void magic_client_log_init(void);
void magic_client_log_cleanup(void);
void magic_client_log(const char *level, const char *format, ...);

#endif // MAGIC_CLIENT_H