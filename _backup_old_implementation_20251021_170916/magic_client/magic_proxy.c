#include "magic_proxy.h"
#include "magic_client.h"
#include <stdarg.h>

// 内部函数声明
static int create_connection_to_server(const char *host, int port, const char *source_ip);
static void cleanup_connection(connection_info_t *conn);
static int transfer_data_bidirectional(int client_fd, int server_fd);

/**
 * 初始化外网访问管理器
 */
int magic_proxy_init(external_access_manager_t *manager, const char *assigned_ip) {
    if (!manager || !assigned_ip) {
        return -1;
    }
    
    memset(manager, 0, sizeof(external_access_manager_t));
    
    // 设置分配的IP
    strncpy(manager->assigned_ip, assigned_ip, sizeof(manager->assigned_ip) - 1);
    manager->assigned_ip[sizeof(manager->assigned_ip) - 1] = '\0';
    
    // 获取可用端口
    manager->proxy_port = magic_proxy_get_available_port(PROXY_PORT_START, PROXY_PORT_END);
    if (manager->proxy_port < 0) {
        return -1;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&manager->connections_mutex, NULL) != 0) {
        return -1;
    }
    
    // 初始化统计信息
    manager->stats.start_time = time(NULL);
    manager->is_running = false;
    
    magic_proxy_log_info("External access manager initialized");
    return 0;
}

/**
 * 清理外网访问管理器
 */
void magic_proxy_cleanup(external_access_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    // 停止代理服务
    magic_proxy_stop(manager);
    
    // 清理所有连接
    magic_proxy_cleanup_connections(manager);
    
    // 销毁互斥锁
    pthread_mutex_destroy(&manager->connections_mutex);
    
    magic_proxy_log_info("External access manager cleaned up");
}

/**
 * 启动代理服务
 */
int magic_proxy_start(external_access_manager_t *manager) {
    if (!manager || manager->is_running) {
        return -1;
    }
    
    // 创建代理服务器线程
    if (pthread_create(&manager->proxy_thread, NULL, magic_proxy_server_thread, manager) != 0) {
        magic_proxy_log_error("magic_proxy_start", errno, "Failed to create proxy thread");
        return -1;
    }
    
    manager->is_running = true;
    magic_proxy_log_info("Proxy server started on port %d", manager->proxy_port);
    return 0;
}

/**
 * 停止代理服务
 */
int magic_proxy_stop(external_access_manager_t *manager) {
    if (!manager || !manager->is_running) {
        return -1;
    }
    
    manager->is_running = false;
    
    // 等待代理线程结束
    if (pthread_join(manager->proxy_thread, NULL) != 0) {
        magic_proxy_log_error("magic_proxy_stop", errno, "Failed to join proxy thread");
    }
    
    magic_proxy_log_info("Proxy server stopped");
    return 0;
}

/**
 * 执行外网请求
 */
int magic_external_request(external_access_manager_t *manager, 
                          const external_request_t *request,
                          external_response_t *response) {
    if (!manager || !request || !response) {
        return -1;
    }
    
    magic_proxy_init_response(response);
    
    int sockfd = -1;
    char request_buffer[MAX_BUFFER_SIZE];
    char response_buffer[MAX_BUFFER_SIZE];
    
    // 创建到目标服务器的连接
    sockfd = create_connection_to_server(request->target_host, 
                                        request->target_port, 
                                        manager->assigned_ip);
    if (sockfd < 0) {
        magic_proxy_log_error("magic_external_request", -1, "Failed to connect to server");
        return -1;
    }
    
    // 构建HTTP请求
    if (magic_proxy_build_http_request(request, request_buffer, sizeof(request_buffer)) != 0) {
        close(sockfd);
        return -1;
    }
    
    // 发送请求
    clock_t start_time = clock();
    ssize_t sent = send(sockfd, request_buffer, strlen(request_buffer), 0);
    if (sent < 0) {
        magic_proxy_log_error("magic_external_request", errno, "Failed to send request");
        close(sockfd);
        return -1;
    }
    
    // 接收响应
    ssize_t received = recv(sockfd, response_buffer, sizeof(response_buffer) - 1, 0);
    if (received < 0) {
        magic_proxy_log_error("magic_external_request", errno, "Failed to receive response");
        close(sockfd);
        return -1;
    }
    
    response_buffer[received] = '\0';
    clock_t end_time = clock();
    response->response_time_ms = (int)((end_time - start_time) * 1000 / CLOCKS_PER_SEC);
    
    // 解析响应
    char *header_end = strstr(response_buffer, "\r\n\r\n");
    if (header_end) {
        *header_end = '\0';
        
        // 解析状态行和头部
        magic_proxy_parse_http_response(response_buffer, 
                                       &response->status_code,
                                       response->status_message);
        
        strncpy(response->headers, response_buffer, sizeof(response->headers) - 1);
        response->headers[sizeof(response->headers) - 1] = '\0';
        
        // 处理响应体
        char *body_start = header_end + 4;
        size_t body_length = received - (body_start - response_buffer);
        
        if (body_length > 0) {
            response->body = malloc(body_length + 1);
            if (response->body) {
                memcpy(response->body, body_start, body_length);
                response->body[body_length] = '\0';
                response->body_length = body_length;
            }
        }
    }
    
    close(sockfd);
    
    // 更新统计信息
    pthread_mutex_lock(&manager->connections_mutex);
    manager->stats.successful_connections++;
    manager->stats.total_bytes_sent += sent;
    manager->stats.total_bytes_received += received;
    pthread_mutex_unlock(&manager->connections_mutex);
    
    return 0;
}

/**
 * HTTP GET请求
 */
int magic_external_http_get(external_access_manager_t *manager,
                           const char *url, 
                           external_response_t *response) {
    if (!manager || !url || !response) {
        return -1;
    }
    
    external_request_t request;
    memset(&request, 0, sizeof(request));
    
    // 解析URL
    char protocol[16], path[512];
    if (magic_proxy_parse_url(url, protocol, request.target_host, 
                             &request.target_port, path) != 0) {
        return -1;
    }
    
    // 设置默认端口
    if (request.target_port == 0) {
        request.target_port = (strcmp(protocol, "https") == 0) ? 443 : 80;
    }
    
    request.protocol = (strcmp(protocol, "https") == 0) ? PROTOCOL_HTTPS : PROTOCOL_HTTP;
    strcpy(request.method, "GET");
    snprintf(request.url, sizeof(request.url), "%s", path);
    request.timeout_ms = 30000;
    
    return magic_external_request(manager, &request, response);
}

/**
 * HTTP POST请求
 */
int magic_external_http_post(external_access_manager_t *manager,
                            const char *url,
                            const char *data,
                            external_response_t *response) {
    if (!manager || !url || !response) {
        return -1;
    }
    
    external_request_t request;
    memset(&request, 0, sizeof(request));
    
    // 解析URL
    char protocol[16], path[512];
    if (magic_proxy_parse_url(url, protocol, request.target_host, 
                             &request.target_port, path) != 0) {
        return -1;
    }
    
    // 设置默认端口
    if (request.target_port == 0) {
        request.target_port = (strcmp(protocol, "https") == 0) ? 443 : 80;
    }
    
    request.protocol = (strcmp(protocol, "https") == 0) ? PROTOCOL_HTTPS : PROTOCOL_HTTP;
    strcpy(request.method, "POST");
    snprintf(request.url, sizeof(request.url), "%s", path);
    
    if (data) {
        strncpy(request.body, data, sizeof(request.body) - 1);
        request.body[sizeof(request.body) - 1] = '\0';
        
        snprintf(request.headers, sizeof(request.headers),
                "Content-Type: application/x-www-form-urlencoded\r\n"
                "Content-Length: %zu\r\n", strlen(data));
    }
    
    request.timeout_ms = 30000;
    
    return magic_external_request(manager, &request, response);
}

/**
 * TCP连接
 */
int magic_external_tcp_connect(external_access_manager_t *manager,
                              const char *host,
                              int port,
                              int *socket_fd) {
    if (!manager || !host || !socket_fd) {
        return -1;
    }
    
    *socket_fd = create_connection_to_server(host, port, manager->assigned_ip);
    if (*socket_fd < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * 代理服务器线程
 */
void* magic_proxy_server_thread(void *arg) {
    external_access_manager_t *manager = (external_access_manager_t*)arg;
    if (!manager) {
        return NULL;
    }
    
    int server_fd = magic_proxy_create_server(manager->proxy_port);
    if (server_fd < 0) {
        magic_proxy_log_error("magic_proxy_server_thread", -1, "Failed to create server");
        return NULL;
    }
    
    magic_proxy_log_info("Proxy server listening on port %d", manager->proxy_port);
    
    while (manager->is_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (manager->is_running) {
                magic_proxy_log_error("magic_proxy_server_thread", errno, "Accept failed");
            }
            continue;
        }
        
        // 创建连接信息
        connection_info_t *conn = malloc(sizeof(connection_info_t));
        if (!conn) {
            close(client_fd);
            continue;
        }
        
        memset(conn, 0, sizeof(connection_info_t));
        conn->client_fd = client_fd;
        conn->server_fd = -1;
        conn->state = CONN_STATE_CONNECTING;
        inet_ntop(AF_INET, &client_addr.sin_addr, conn->client_ip, INET_ADDRSTRLEN);
        conn->client_port = ntohs(client_addr.sin_port);
        conn->created_time = time(NULL);
        conn->last_activity = time(NULL);
        conn->is_active = true;
        
        // 添加到连接列表
        if (magic_proxy_add_connection(manager, conn) != 0) {
            cleanup_connection(conn);
            free(conn);
            continue;
        }
        
        // 创建处理线程
        if (pthread_create(&conn->thread_id, NULL, magic_proxy_connection_handler, conn) != 0) {
            magic_proxy_remove_connection(manager, client_fd);
            cleanup_connection(conn);
            free(conn);
        }
    }
    
    close(server_fd);
    return NULL;
}

/**
 * 连接处理线程
 */
void* magic_proxy_connection_handler(void *arg) {
    connection_info_t *conn = (connection_info_t*)arg;
    if (!conn) {
        return NULL;
    }
    
    // 处理HTTP请求
    if (magic_proxy_handle_http_request(conn) == 0) {
        // 转发数据
        transfer_data_bidirectional(conn->client_fd, conn->server_fd);
    }
    
    // 清理连接
    cleanup_connection(conn);
    free(conn);
    
    return NULL;
}

/**
 * 处理HTTP请求
 */
int magic_proxy_handle_http_request(connection_info_t *conn) {
    if (!conn) {
        return -1;
    }
    
    char buffer[MAX_BUFFER_SIZE];
    ssize_t received = recv(conn->client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return -1;
    }
    
    buffer[received] = '\0';
    
    // 解析HTTP请求
    char method[16], url[MAX_URL_LEN], version[16];
    if (magic_proxy_parse_http_request(buffer, method, url, version) != 0) {
        return -1;
    }
    
    // 解析目标主机和端口
    char protocol[16], host[MAX_HOSTNAME_LEN], path[512];
    int port;
    
    if (magic_proxy_parse_url(url, protocol, host, &port, path) != 0) {
        return -1;
    }
    
    if (port == 0) {
        port = (strcmp(protocol, "https") == 0) ? 443 : 80;
    }
    
    // 连接到目标服务器
    // 这里需要获取manager实例，简化处理
    extern external_access_manager_t *g_external_manager;
    conn->server_fd = create_connection_to_server(host, port, 
                                                 g_external_manager ? g_external_manager->assigned_ip : NULL);
    if (conn->server_fd < 0) {
        return -1;
    }
    
    strncpy(conn->server_ip, host, sizeof(conn->server_ip) - 1);
    conn->server_port = port;
    conn->state = CONN_STATE_CONNECTED;
    
    // 转发原始请求
    ssize_t sent = send(conn->server_fd, buffer, received, 0);
    if (sent != received) {
        return -1;
    }
    
    conn->bytes_sent += sent;
    conn->last_activity = time(NULL);
    
    return 0;
}

/**
 * 双向数据转发
 */
static int transfer_data_bidirectional(int client_fd, int server_fd) {
    fd_set read_fds;
    char buffer[MAX_BUFFER_SIZE];
    int max_fd = (client_fd > server_fd) ? client_fd : server_fd;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval timeout = {30, 0}; // 30秒超时
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            break;
        } else if (activity == 0) {
            // 超时
            break;
        }
        
        // 从客户端到服务器
        if (FD_ISSET(client_fd, &read_fds)) {
            ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                break;
            }
            
            if (send(server_fd, buffer, bytes, 0) != bytes) {
                break;
            }
        }
        
        // 从服务器到客户端
        if (FD_ISSET(server_fd, &read_fds)) {
            ssize_t bytes = recv(server_fd, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                break;
            }
            
            if (send(client_fd, buffer, bytes, 0) != bytes) {
                break;
            }
        }
    }
    
    return 0;
}

/**
 * 创建代理服务器
 */
int magic_proxy_create_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

/**
 * 创建到服务器的连接（使用指定源IP）
 */
static int create_connection_to_server(const char *host, int port, const char *source_ip) {
    if (!host) {
        return -1;
    }
    
    // 解析主机名
    char server_ip[INET_ADDRSTRLEN];
    if (magic_proxy_resolve_hostname(host, server_ip) != 0) {
        return -1;
    }
    
    // 创建套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    // 绑定源IP（如果指定）
    if (source_ip && strlen(source_ip) > 0) {
        struct sockaddr_in source_addr;
        memset(&source_addr, 0, sizeof(source_addr));
        source_addr.sin_family = AF_INET;
        inet_pton(AF_INET, source_ip, &source_addr.sin_addr);
        
        if (bind(sockfd, (struct sockaddr*)&source_addr, sizeof(source_addr)) < 0) {
            magic_proxy_log_error("create_connection_to_server", errno, "Failed to bind source IP");
            close(sockfd);
            return -1;
        }
    }
    
    // 连接到服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        magic_proxy_log_error("create_connection_to_server", errno, "Failed to connect to server");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/**
 * 清理连接
 */
static void cleanup_connection(connection_info_t *conn) {
    if (!conn) {
        return;
    }
    
    if (conn->client_fd >= 0) {
        close(conn->client_fd);
        conn->client_fd = -1;
    }
    
    if (conn->server_fd >= 0) {
        close(conn->server_fd);
        conn->server_fd = -1;
    }
    
    conn->is_active = false;
    conn->state = CONN_STATE_CLOSED;
}

// 其他工具函数的简化实现

int magic_proxy_resolve_hostname(const char *hostname, char *ip_str) {
    struct hostent *host_entry = gethostbyname(hostname);
    if (!host_entry) {
        return -1;
    }
    
    inet_ntop(AF_INET, host_entry->h_addr_list[0], ip_str, INET_ADDRSTRLEN);
    return 0;
}

int magic_proxy_parse_url(const char *url, char *protocol, char *host, int *port, char *path) {
    if (!url || !protocol || !host || !port || !path) {
        return -1;
    }
    
    // 简化的URL解析
    const char *proto_end = strstr(url, "://");
    if (proto_end) {
        size_t proto_len = proto_end - url;
        strncpy(protocol, url, proto_len);
        protocol[proto_len] = '\0';
        url = proto_end + 3;
    } else {
        strcpy(protocol, "http");
    }
    
    const char *path_start = strchr(url, '/');
    if (path_start) {
        strcpy(path, path_start);
        size_t host_len = path_start - url;
        strncpy(host, url, host_len);
        host[host_len] = '\0';
    } else {
        strcpy(path, "/");
        strcpy(host, url);
    }
    
    // 检查端口
    char *port_start = strchr(host, ':');
    if (port_start) {
        *port = atoi(port_start + 1);
        *port_start = '\0';
    } else {
        *port = 0;
    }
    
    return 0;
}

int magic_proxy_build_http_request(const external_request_t *request, char *buffer, size_t buffer_size) {
    if (!request || !buffer) {
        return -1;
    }
    
    int written = snprintf(buffer, buffer_size,
                          "%s %s HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "User-Agent: MAGIC-Client/1.0\r\n"
                          "Connection: close\r\n",
                          request->method,
                          request->url,
                          request->target_host);
    
    if (strlen(request->headers) > 0) {
        written += snprintf(buffer + written, buffer_size - written, "%s", request->headers);
    }
    
    if (strlen(request->body) > 0) {
        written += snprintf(buffer + written, buffer_size - written, "\r\n%s", request->body);
    } else {
        written += snprintf(buffer + written, buffer_size - written, "\r\n");
    }
    
    return 0;
}

int magic_proxy_parse_http_request(const char *request, char *method, char *url, char *version) {
    if (!request || !method || !url || !version) {
        return -1;
    }
    
    return sscanf(request, "%15s %1023s %15s", method, url, version) == 3 ? 0 : -1;
}

int magic_proxy_parse_http_response(const char *response, int *status_code, char *status_message) {
    if (!response || !status_code || !status_message) {
        return -1;
    }
    
    char version[16];
    return sscanf(response, "%15s %d %127[^\r\n]", version, status_code, status_message) >= 2 ? 0 : -1;
}

void magic_proxy_init_response(external_response_t *response) {
    if (!response) {
        return;
    }
    
    memset(response, 0, sizeof(external_response_t));
}

void magic_proxy_cleanup_response(external_response_t *response) {
    if (!response) {
        return;
    }
    
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }
    
    response->body_length = 0;
}

int magic_proxy_get_available_port(int start_port, int end_port) {
    for (int port = start_port; port <= end_port; port++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            continue;
        }
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sockfd);
            return port;
        }
        
        close(sockfd);
    }
    
    return -1;
}

int magic_proxy_add_connection(external_access_manager_t *manager, connection_info_t *conn) {
    if (!manager || !conn) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->connections_mutex);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!manager->connections[i].is_active) {
            memcpy(&manager->connections[i], conn, sizeof(connection_info_t));
            manager->stats.active_connections++;
            manager->stats.total_connections++;
            pthread_mutex_unlock(&manager->connections_mutex);
            return i;
        }
    }
    
    pthread_mutex_unlock(&manager->connections_mutex);
    return -1;
}

void magic_proxy_remove_connection(external_access_manager_t *manager, int connection_id) {
    if (!manager || connection_id < 0 || connection_id >= MAX_CONNECTIONS) {
        return;
    }
    
    pthread_mutex_lock(&manager->connections_mutex);
    
    if (manager->connections[connection_id].is_active) {
        manager->connections[connection_id].is_active = false;
        manager->stats.active_connections--;
    }
    
    pthread_mutex_unlock(&manager->connections_mutex);
}

void magic_proxy_cleanup_connections(external_access_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    pthread_mutex_lock(&manager->connections_mutex);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (manager->connections[i].is_active) {
            cleanup_connection(&manager->connections[i]);
            manager->connections[i].is_active = false;
        }
    }
    
    manager->stats.active_connections = 0;
    pthread_mutex_unlock(&manager->connections_mutex);
}

void magic_proxy_log_error(const char *function, int error_code, const char *message) {
    magic_client_log("ERROR", "[%s] Error %d: %s", function, error_code, message);
}

void magic_proxy_log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    magic_client_log("INFO", "%s", buffer);
    va_end(args);
}

void magic_proxy_log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    magic_client_log("DEBUG", "%s", buffer);
    va_end(args);
}

bool magic_proxy_is_valid_ip(const char *ip) {
    if (!ip) {
        return false;
    }
    
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

bool magic_proxy_is_valid_port(int port) {
    return port > 0 && port <= 65535;
}