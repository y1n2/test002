#ifndef MAGIC_PROXY_H
#define MAGIC_PROXY_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>

// 常量定义
#define MAX_BUFFER_SIZE 8192
#define MAX_HOSTNAME_LEN 256
#define MAX_URL_LEN 1024
#define MAX_CONNECTIONS 100
#define CONNECTION_TIMEOUT 30
#define PROXY_PORT_START 8080
#define PROXY_PORT_END 8090

// 协议类型
typedef enum {
    PROTOCOL_HTTP = 0,
    PROTOCOL_HTTPS,
    PROTOCOL_TCP,
    PROTOCOL_UDP
} protocol_type_t;

// 连接状态
typedef enum {
    CONN_STATE_IDLE = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_TRANSFERRING,
    CONN_STATE_CLOSING,
    CONN_STATE_CLOSED
} connection_state_t;

// 外网访问请求
typedef struct {
    char target_host[MAX_HOSTNAME_LEN];
    int target_port;
    protocol_type_t protocol;
    char url[MAX_URL_LEN];
    char method[16];  // GET, POST, etc.
    char headers[2048];
    char body[4096];
    int timeout_ms;
} external_request_t;

// 外网访问响应
typedef struct {
    int status_code;
    char status_message[128];
    char headers[2048];
    char *body;
    size_t body_length;
    int response_time_ms;
} external_response_t;

// 连接信息
typedef struct {
    int client_fd;
    int server_fd;
    connection_state_t state;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    char server_ip[INET_ADDRSTRLEN];
    int server_port;
    time_t created_time;
    time_t last_activity;
    size_t bytes_sent;
    size_t bytes_received;
    pthread_t thread_id;
    bool is_active;
} connection_info_t;

// 代理统计信息
typedef struct {
    int total_connections;
    int active_connections;
    int successful_connections;
    int failed_connections;
    size_t total_bytes_sent;
    size_t total_bytes_received;
    time_t start_time;
    double average_response_time;
} proxy_stats_t;

// 外网访问管理器
typedef struct {
    char assigned_ip[INET_ADDRSTRLEN];  // 服务端分配的IP
    int proxy_port;                     // 本地代理端口
    bool is_running;
    pthread_t proxy_thread;
    pthread_mutex_t connections_mutex;
    connection_info_t connections[MAX_CONNECTIONS];
    proxy_stats_t stats;
    
    // 回调函数
    void (*on_connection_established)(const char *target_host, int target_port);
    void (*on_connection_closed)(const char *target_host, int target_port);
    void (*on_data_transferred)(size_t bytes);
    void (*on_error)(const char *error_message);
} external_access_manager_t;

// 核心功能函数
int magic_proxy_init(external_access_manager_t *manager, const char *assigned_ip);
void magic_proxy_cleanup(external_access_manager_t *manager);
int magic_proxy_start(external_access_manager_t *manager);
int magic_proxy_stop(external_access_manager_t *manager);

// 外网访问函数
int magic_external_request(external_access_manager_t *manager, 
                          const external_request_t *request,
                          external_response_t *response);
int magic_external_http_get(external_access_manager_t *manager,
                           const char *url, 
                           external_response_t *response);
int magic_external_http_post(external_access_manager_t *manager,
                            const char *url,
                            const char *data,
                            external_response_t *response);
int magic_external_tcp_connect(external_access_manager_t *manager,
                              const char *host,
                              int port,
                              int *socket_fd);

// 代理服务器函数
int magic_proxy_create_server(int port);
void* magic_proxy_server_thread(void *arg);
void* magic_proxy_connection_handler(void *arg);
int magic_proxy_handle_http_request(connection_info_t *conn);
int magic_proxy_forward_data(int from_fd, int to_fd);

// 连接管理函数
int magic_proxy_add_connection(external_access_manager_t *manager, 
                              connection_info_t *conn);
void magic_proxy_remove_connection(external_access_manager_t *manager, 
                                  int connection_id);
connection_info_t* magic_proxy_find_connection(external_access_manager_t *manager,
                                              int client_fd);
void magic_proxy_cleanup_connections(external_access_manager_t *manager);

// 网络工具函数
int magic_proxy_resolve_hostname(const char *hostname, char *ip_str);
int magic_proxy_create_socket_with_source_ip(const char *source_ip);
int magic_proxy_connect_with_timeout(int sockfd, 
                                    const struct sockaddr *addr,
                                    socklen_t addrlen, 
                                    int timeout_ms);
int magic_proxy_set_socket_nonblocking(int sockfd);
int magic_proxy_set_socket_blocking(int sockfd);

// HTTP协议处理函数
int magic_proxy_parse_http_request(const char *request, 
                                  char *method, 
                                  char *url, 
                                  char *version);
int magic_proxy_parse_http_response(const char *response,
                                   int *status_code,
                                   char *status_message);
int magic_proxy_build_http_request(const external_request_t *request,
                                  char *buffer,
                                  size_t buffer_size);
int magic_proxy_parse_url(const char *url,
                         char *protocol,
                         char *host,
                         int *port,
                         char *path);

// 统计和监控函数
void magic_proxy_update_stats(external_access_manager_t *manager,
                             connection_info_t *conn);
void magic_proxy_get_stats(external_access_manager_t *manager,
                          proxy_stats_t *stats);
void magic_proxy_reset_stats(external_access_manager_t *manager);
void magic_proxy_print_stats(external_access_manager_t *manager);

// 回调函数注册
void magic_proxy_set_connection_callback(external_access_manager_t *manager,
                                        void (*callback)(const char*, int));
void magic_proxy_set_close_callback(external_access_manager_t *manager,
                                   void (*callback)(const char*, int));
void magic_proxy_set_data_callback(external_access_manager_t *manager,
                                  void (*callback)(size_t));
void magic_proxy_set_error_callback(external_access_manager_t *manager,
                                   void (*callback)(const char*));

// 响应处理函数
void magic_proxy_init_response(external_response_t *response);
void magic_proxy_cleanup_response(external_response_t *response);
int magic_proxy_copy_response(const external_response_t *src,
                             external_response_t *dst);

// 错误处理和日志函数
void magic_proxy_log_error(const char *function, int error_code, const char *message);
void magic_proxy_log_info(const char *format, ...);
void magic_proxy_log_debug(const char *format, ...);
const char* magic_proxy_get_error_string(int error_code);

// 工具函数
bool magic_proxy_is_valid_ip(const char *ip);
bool magic_proxy_is_valid_port(int port);
int magic_proxy_get_available_port(int start_port, int end_port);
void magic_proxy_format_bytes(size_t bytes, char *buffer, size_t buffer_size);
double magic_proxy_get_uptime(time_t start_time);

#endif // MAGIC_PROXY_H