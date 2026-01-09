#include "../../include/dlm/magic_dlm.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <sys/wait.h>

/* 静态函数前向声明 */
static int init_routing_tables(void);
static int cleanup_routing_tables(void);
static int init_firewall_rules(void);
static int cleanup_firewall_rules(void);

/* 链路模拟器连接信息 */
typedef struct {
    int link_id;
    int link_type;
    int socket;
    int status;
    pthread_t monitor_thread;
    int monitor_running;
} dlm_link_t;

#define MAX_LINKS 4
static dlm_link_t g_links[MAX_LINKS];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 客户端访问权限管理 */
typedef struct {
    char client_id[64];
    network_access_t access_mask;
    time_t granted_time;
    int active;
} client_access_t;

#define MAX_CLIENTS 100
static client_access_t g_client_access[MAX_CLIENTS];
static pthread_mutex_t g_access_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 链路模拟器端口配置 */
static const struct {
    const char *ip;
    int port;
} link_simulators[MAX_LINKS] = {
    { "127.0.0.1", 9001 }, /* 以太网 */
    { "127.0.0.1", 9002 }, /* WiFi */
    { "127.0.0.1", 9003 }, /* 蜂窝 */
    { "127.0.0.1", 9004 }  /* 卫星 */
};

/* 链路监控线程 */
static void *link_monitor_thread(void *arg)
{
    dlm_link_t *link = (dlm_link_t *)arg;
    char buffer[128];
    
    while (link->monitor_running) {
        /* 发送心跳 */
        snprintf(buffer, sizeof(buffer), "HEARTBEAT %d", link->link_id);
        send(link->socket, buffer, strlen(buffer), 0);
        
        /* 等待响应 */
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(link->socket, &readfds);
        
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        int ret = select(link->socket + 1, &readfds, NULL, NULL, &tv);
        
        pthread_mutex_lock(&g_mutex);
        
        if (ret <= 0) {
            /* 超时或错误 */
            link->status = LINK_STATUS_DOWN;
            MAGIC_ERROR("链路 %d 心跳超时", link->link_id);
            
            /* 尝试重连 */
            close(link->socket);
            
            struct sockaddr_in addr;
            link->socket = socket(AF_INET, SOCK_STREAM, 0);
            
            if (link->socket < 0) {
                MAGIC_ERROR("无法创建套接字");
                pthread_mutex_unlock(&g_mutex);
                sleep(10);
                continue;
            }
            
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(link_simulators[link->link_id - 1].port);
            inet_pton(AF_INET, link_simulators[link->link_id - 1].ip, &addr.sin_addr);
            
            if (connect(link->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                MAGIC_ERROR("无法连接到链路模拟器 %d", link->link_id);
                close(link->socket);
                link->socket = -1;
                pthread_mutex_unlock(&g_mutex);
                sleep(10);
                continue;
            }
            
            link->status = LINK_STATUS_UP;
            MAGIC_LOG(FD_LOG_NOTICE, "链路 %d 重新连接成功", link->link_id);
        } else if (FD_ISSET(link->socket, &readfds)) {
            /* 接收响应 */
            memset(buffer, 0, sizeof(buffer));
            ret = recv(link->socket, buffer, sizeof(buffer) - 1, 0);
            
            if (ret <= 0) {
                /* 连接关闭或错误 */
                link->status = LINK_STATUS_DOWN;
                MAGIC_ERROR("链路 %d 连接关闭", link->link_id);
                close(link->socket);
                link->socket = -1;
            } else {
                /* 检查响应 */
                if (strstr(buffer, "OK") != NULL) {
                    link->status = LINK_STATUS_UP;
                } else if (strstr(buffer, "DEGRADED") != NULL) {
                    link->status = LINK_STATUS_DEGRADED;
                } else {
                    link->status = LINK_STATUS_DOWN;
                }
            }
        }
        
        pthread_mutex_unlock(&g_mutex);
        
        /* 每10秒检查一次 */
        sleep(10);
    }
    
    return NULL;
}

/* 初始化DLM模块 */
int dlm_init(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "初始化DLM模块");
    
    memset(g_links, 0, sizeof(g_links));
    
    /* 初始化链路配置信息 */
    for (int i = 0; i < MAX_LINKS; i++) {
        g_links[i].link_id = i + 1;
        g_links[i].link_type = i + 1; /* 链路类型与ID对应 */
        g_links[i].socket = -1;
        g_links[i].status = LINK_STATUS_DOWN;
        g_links[i].monitor_running = 0;
    }
    
    /* 初始化防火墙规则 */
    int firewall_result = init_firewall_rules();
    if (firewall_result != MAGIC_OK) {
        MAGIC_LOG(FD_LOG_WARNING, "防火墙规则初始化失败，但继续启动DLM模块");
    }
    
    /* 初始化路由表 */
    int routing_result = init_routing_tables();
    if (routing_result != MAGIC_OK) {
        MAGIC_LOG(FD_LOG_WARNING, "路由表初始化失败，但继续启动DLM模块");
    }
    
    /* 初始化客户端访问权限表 */
    memset(g_client_access, 0, sizeof(g_client_access));
    
    /* 注意：根据ARINC 839规范，DLM模块不应主动连接链路模拟器
     * 而是管理网络路由和防火墙规则，允许认证后的客户端访问链路模拟器 */
    MAGIC_LOG(FD_LOG_NOTICE, "DLM模块初始化完成 - 网络管理模式");
    return MAGIC_OK;
}

/* 清理DLM模块 */
void dlm_cleanup(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "清理DLM模块");
    
    pthread_mutex_lock(&g_mutex);
    
    /* 关闭所有链路 */
    for (int i = 0; i < MAX_LINKS; i++) {
        if (g_links[i].socket >= 0) {
            /* 停止监控线程 */
            if (g_links[i].monitor_running) {
                g_links[i].monitor_running = 0;
                pthread_mutex_unlock(&g_mutex);
                pthread_join(g_links[i].monitor_thread, NULL);
                pthread_mutex_lock(&g_mutex);
            }
            
            /* 关闭套接字 */
            close(g_links[i].socket);
            g_links[i].socket = -1;
            g_links[i].status = LINK_STATUS_DOWN;
        }
    }
    
    pthread_mutex_unlock(&g_mutex);
    
    /* 清理防火墙规则 */
    cleanup_firewall_rules();
    
    /* 清理路由表 */
    cleanup_routing_tables();
    
    /* 清理客户端访问权限表 */
    pthread_mutex_lock(&g_access_mutex);
    memset(g_client_access, 0, sizeof(g_client_access));
    pthread_mutex_unlock(&g_access_mutex);
    
    pthread_mutex_destroy(&g_mutex);
    pthread_mutex_destroy(&g_access_mutex);
}

/* 打开链路 */
int dlm_open_link(int link_id)
{
    struct sockaddr_in addr;
    int idx;
    
    if (link_id < 1 || link_id > MAX_LINKS) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    idx = link_id - 1;
    
    pthread_mutex_lock(&g_mutex);
    
    /* 检查链路是否已打开 */
    if (g_links[idx].socket >= 0) {
        pthread_mutex_unlock(&g_mutex);
        return MAGIC_OK;
    }
    
    /* 创建套接字 */
    g_links[idx].socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (g_links[idx].socket < 0) {
        MAGIC_ERROR("无法创建套接字");
        pthread_mutex_unlock(&g_mutex);
        return MAGIC_ERROR_GENERAL;
    }
    
    /* 设置套接字为非阻塞模式 */
    int flags = fcntl(g_links[idx].socket, F_GETFL, 0);
    fcntl(g_links[idx].socket, F_SETFL, flags | O_NONBLOCK);
    
    /* 连接到链路模拟器 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(link_simulators[idx].port);
    inet_pton(AF_INET, link_simulators[idx].ip, &addr.sin_addr);
    
    int connect_result = connect(g_links[idx].socket, (struct sockaddr *)&addr, sizeof(addr));
    
    if (connect_result < 0) {
        if (errno == EINPROGRESS) {
            /* 非阻塞连接正在进行，使用select等待连接完成 */
            fd_set writefds;
            struct timeval tv;
            
            FD_ZERO(&writefds);
            FD_SET(g_links[idx].socket, &writefds);
            
            tv.tv_sec = 1;  /* 1秒超时 */
            tv.tv_usec = 0;
            
            int select_result = select(g_links[idx].socket + 1, NULL, &writefds, NULL, &tv);
            
            if (select_result <= 0) {
                /* 连接超时或失败 */
                MAGIC_LOG(FD_LOG_NOTICE, "连接到链路模拟器 %d 超时，将在后台重试连接", link_id);
                close(g_links[idx].socket);
                g_links[idx].socket = -1;
                g_links[idx].status = LINK_STATUS_DOWN;
                pthread_mutex_unlock(&g_mutex);
                
                /* 启动监控线程进行重连尝试 */
                if (!g_links[idx].monitor_running) {
                    g_links[idx].monitor_running = 1;
                    int ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
                    if (ret != 0) {
                        MAGIC_LOG(FD_LOG_NOTICE, "创建链路监控线程失败: %d，但不影响系统启动", ret);
                        g_links[idx].monitor_running = 0;
                    }
                }
                
                return MAGIC_OK; /* 返回成功，允许系统继续启动 */
            }
            
            /* 检查连接是否真的成功 */
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(g_links[idx].socket, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                MAGIC_LOG(FD_LOG_NOTICE, "连接到链路模拟器 %d 失败，将在后台重试连接", link_id);
                close(g_links[idx].socket);
                g_links[idx].socket = -1;
                g_links[idx].status = LINK_STATUS_DOWN;
                pthread_mutex_unlock(&g_mutex);
                
                /* 启动监控线程进行重连尝试 */
                if (!g_links[idx].monitor_running) {
                    g_links[idx].monitor_running = 1;
                    int ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
                    if (ret != 0) {
                        MAGIC_LOG(FD_LOG_NOTICE, "创建链路监控线程失败: %d，但不影响系统启动", ret);
                        g_links[idx].monitor_running = 0;
                    }
                }
                
                return MAGIC_OK; /* 返回成功，允许系统继续启动 */
            }
        } else {
            /* 立即连接失败 */
            MAGIC_LOG(FD_LOG_NOTICE, "无法连接到链路模拟器 %d，将在后台重试连接", link_id);
            close(g_links[idx].socket);
            g_links[idx].socket = -1;
            g_links[idx].status = LINK_STATUS_DOWN;
            pthread_mutex_unlock(&g_mutex);
            
            /* 启动监控线程进行重连尝试 */
            if (!g_links[idx].monitor_running) {
                g_links[idx].monitor_running = 1;
                int ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
                if (ret != 0) {
                    MAGIC_LOG(FD_LOG_NOTICE, "创建链路监控线程失败: %d，但不影响系统启动", ret);
                    g_links[idx].monitor_running = 0;
                }
            }
            
            return MAGIC_OK; /* 返回成功，允许系统继续启动 */
        }
    }
    
    /* 连接成功，恢复阻塞模式 */
    flags = fcntl(g_links[idx].socket, F_GETFL, 0);
    fcntl(g_links[idx].socket, F_SETFL, flags & ~O_NONBLOCK);
    
    /* 发送OPEN命令 */
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "OPEN %d", link_id);
    send(g_links[idx].socket, buffer, strlen(buffer), 0);
    
    /* 接收响应，设置超时 */
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(g_links[idx].socket, &readfds);
    
    tv.tv_sec = 2;  /* 2秒超时 */
    tv.tv_usec = 0;
    
    memset(buffer, 0, sizeof(buffer));
    int ret = select(g_links[idx].socket + 1, &readfds, NULL, NULL, &tv);
    
    if (ret <= 0) {
        /* 超时或错误 */
        MAGIC_LOG(FD_LOG_NOTICE, "链路 %d 响应超时，但不影响系统启动", link_id);
        close(g_links[idx].socket);
        g_links[idx].socket = -1;
        g_links[idx].status = LINK_STATUS_DOWN;
        pthread_mutex_unlock(&g_mutex);
        
        /* 启动监控线程进行重连尝试 */
        if (!g_links[idx].monitor_running) {
            g_links[idx].monitor_running = 1;
            int ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
            if (ret != 0) {
                MAGIC_LOG(FD_LOG_NOTICE, "创建链路监控线程失败: %d，但不影响系统启动", ret);
                g_links[idx].monitor_running = 0;
            }
        }
        
        return MAGIC_OK; /* 返回成功，允许系统继续启动 */
    }
    
    ret = recv(g_links[idx].socket, buffer, sizeof(buffer) - 1, 0);
    
    if (ret <= 0 || strstr(buffer, "OK") == NULL) {
        MAGIC_LOG(FD_LOG_NOTICE, "链路 %d OPEN命令失败，但不影响系统启动", link_id);
        close(g_links[idx].socket);
        g_links[idx].socket = -1;
        g_links[idx].status = LINK_STATUS_DOWN;
        pthread_mutex_unlock(&g_mutex);
        
        /* 启动监控线程进行重连尝试 */
        if (!g_links[idx].monitor_running) {
            g_links[idx].monitor_running = 1;
            int ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
            if (ret != 0) {
                MAGIC_LOG(FD_LOG_NOTICE, "创建链路监控线程失败: %d，但不影响系统启动", ret);
                g_links[idx].monitor_running = 0;
            }
        }
        
        return MAGIC_OK; /* 返回成功，允许系统继续启动 */
    }
    
    /* 更新链路状态 */
    g_links[idx].status = LINK_STATUS_UP;
    
    /* 启动监控线程 */
    if (!g_links[idx].monitor_running) {
        g_links[idx].monitor_running = 1;
        
        ret = pthread_create(&g_links[idx].monitor_thread, NULL, link_monitor_thread, &g_links[idx]);
        if (ret != 0) {
            MAGIC_ERROR("创建链路监控线程失败: %d", ret);
            g_links[idx].monitor_running = 0;
        }
    }
    
    pthread_mutex_unlock(&g_mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "链路 %d 打开成功", link_id);
    return MAGIC_OK;
}

/* 关闭链路 */
int dlm_close_link(int link_id)
{
    int idx;
    
    if (link_id < 1 || link_id > MAX_LINKS) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    idx = link_id - 1;
    
    pthread_mutex_lock(&g_mutex);
    
    /* 检查链路是否已关闭 */
    if (g_links[idx].socket < 0) {
        pthread_mutex_unlock(&g_mutex);
        return MAGIC_OK;
    }
    
    /* 发送CLOSE命令 */
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "CLOSE %d", link_id);
    send(g_links[idx].socket, buffer, strlen(buffer), 0);
    
    /* 停止监控线程 */
    if (g_links[idx].monitor_running) {
        g_links[idx].monitor_running = 0;
        pthread_mutex_unlock(&g_mutex);
        pthread_join(g_links[idx].monitor_thread, NULL);
        pthread_mutex_lock(&g_mutex);
    }
    
    /* 关闭套接字 */
    close(g_links[idx].socket);
    g_links[idx].socket = -1;
    g_links[idx].status = LINK_STATUS_DOWN;
    
    pthread_mutex_unlock(&g_mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "链路 %d 关闭成功", link_id);
    return MAGIC_OK;
}

/* 获取链路状态 */
int dlm_get_link_status(int link_id, int *status)
{
    int idx;
    
    if (link_id < 1 || link_id > MAX_LINKS || !status) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    idx = link_id - 1;
    
    pthread_mutex_lock(&g_mutex);
    *status = g_links[idx].status;
    pthread_mutex_unlock(&g_mutex);
    
    return MAGIC_OK;
}

/* 发送数据 */
int dlm_send_data(int link_id, const void *data, size_t data_len)
{
    int idx;
    
    if (link_id < 1 || link_id > MAX_LINKS || !data || data_len == 0) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    idx = link_id - 1;
    
    pthread_mutex_lock(&g_mutex);
    
    /* 检查链路状态 */
    if (g_links[idx].socket < 0 || g_links[idx].status != LINK_STATUS_UP) {
        pthread_mutex_unlock(&g_mutex);
        return MAGIC_ERROR_COMMUNICATION;
    }
    
    /* 发送数据 */
    ssize_t sent = send(g_links[idx].socket, data, data_len, 0);
    
    pthread_mutex_unlock(&g_mutex);
    
    if (sent != (ssize_t)data_len) {
        MAGIC_ERROR("发送数据到链路 %d 失败", link_id);
        return MAGIC_ERROR_COMMUNICATION;
    }
    
    MAGIC_LOG(FD_LOG_DEBUG, "发送 %zu 字节数据到链路 %d", data_len, link_id);
    return MAGIC_OK;
}

/* ========== 新的网络管理功能 ========== */

/* 客户端网络访问权限管理 */

/* 防火墙规则配置函数 */
static int configure_firewall_rules(const char *client_id, network_access_t access_mask, int add_rule)
{
    char command[512];
    char client_ip[64];
    
    /* 从client_id中提取IP地址（假设client_id格式为 "client_ip:port" 或直接是IP） */
    strncpy(client_ip, client_id, sizeof(client_ip) - 1);
    client_ip[sizeof(client_ip) - 1] = '\0';
    
    /* 如果client_id包含端口号，去掉端口号部分 */
    char *colon_pos = strchr(client_ip, ':');
    if (colon_pos) {
        *colon_pos = '\0';
    }
    
    /* 验证IP地址格式 */
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, client_ip, &(sa.sin_addr)) != 1) {
        MAGIC_LOG(FD_LOG_WARNING, "无效的客户端IP地址: %s", client_ip);
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    /* 为每个授权的链路类型配置防火墙规则 */
    for (int link_type = 1; link_type <= MAX_LINKS; link_type++) {
        network_access_t required_access = 1 << (link_type - 1);
        
        if (access_mask & required_access) {
            int link_port = link_simulators[link_type - 1].port;
            const char *link_ip = link_simulators[link_type - 1].ip;
            
            if (add_rule) {
                /* 添加防火墙规则：允许客户端访问链路模拟器 */
                snprintf(command, sizeof(command),
                    "iptables -C MAGIC_FORWARD -s %s -d %s -p tcp --dport %d -j ACCEPT 2>/dev/null || "
                    "iptables -I MAGIC_FORWARD 1 -s %s -d %s -p tcp --dport %d -j ACCEPT",
                    client_ip, link_ip, link_port,
                    client_ip, link_ip, link_port);
                
                MAGIC_LOG(FD_LOG_DEBUG, "添加防火墙规则: %s -> %s:%d", client_ip, link_ip, link_port);
            } else {
                /* 删除防火墙规则：禁止客户端访问链路模拟器 */
                snprintf(command, sizeof(command),
                    "iptables -D MAGIC_FORWARD -s %s -d %s -p tcp --dport %d -j ACCEPT 2>/dev/null",
                    client_ip, link_ip, link_port);
                
                MAGIC_LOG(FD_LOG_DEBUG, "删除防火墙规则: %s -> %s:%d", client_ip, link_ip, link_port);
            }
            
            /* 执行iptables命令 */
            int result = system(command);
            if (result != 0) {
                MAGIC_LOG(FD_LOG_WARNING, "执行iptables命令失败: %s (返回码: %d)", command, result);
                /* 继续处理其他规则，不立即返回错误 */
            }
        }
    }
    
    /* 如果是添加规则，还需要确保客户端可以建立连接 */
    if (add_rule) {
        /* 允许已建立的连接返回数据 */
        snprintf(command, sizeof(command),
            "iptables -C FORWARD -d %s -m state --state ESTABLISHED,RELATED -j ACCEPT 2>/dev/null || "
            "iptables -A FORWARD -d %s -m state --state ESTABLISHED,RELATED -j ACCEPT",
            client_ip, client_ip);
        
        int result = system(command);
        if (result != 0) {
            MAGIC_LOG(FD_LOG_WARNING, "配置连接状态规则失败: %s", command);
        }
    }
    
    return MAGIC_OK;
}

/* 路由表管理函数 */
static int configure_routing_rules(const char *client_id, network_access_t access_mask, int add_rule)
{
    char command[512];
    char client_ip[64];
    
    /* 从client_id中提取IP地址 */
    strncpy(client_ip, client_id, sizeof(client_ip) - 1);
    client_ip[sizeof(client_ip) - 1] = '\0';
    
    /* 如果client_id包含端口号，去掉端口号部分 */
    char *colon_pos = strchr(client_ip, ':');
    if (colon_pos) {
        *colon_pos = '\0';
    }
    
    /* 验证IP地址格式 */
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, client_ip, &(sa.sin_addr)) != 1) {
        MAGIC_LOG(FD_LOG_WARNING, "无效的客户端IP地址: %s", client_ip);
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    /* 为每个授权的链路类型配置路由规则 */
    for (int link_type = 1; link_type <= MAX_LINKS; link_type++) {
        network_access_t required_access = 1 << (link_type - 1);
        
        if (access_mask & required_access) {
            int link_port = link_simulators[link_type - 1].port;
            const char *link_ip = link_simulators[link_type - 1].ip;
            
            if (add_rule) {
                /* 添加路由规则：为客户端创建到链路模拟器的路由 */
                
                /* 1. 创建自定义路由表（如果不存在） */
                snprintf(command, sizeof(command),
                    "grep -q 'magic_table_%d' /etc/iproute2/rt_tables || "
                    "echo '10%d magic_table_%d' >> /etc/iproute2/rt_tables",
                    link_type, link_type, link_type);
                system(command);
                
                /* 2. 在自定义表中添加到链路模拟器的路由 */
                snprintf(command, sizeof(command),
                    "ip route add %s/32 dev lo table magic_table_%d 2>/dev/null || "
                    "ip route replace %s/32 dev lo table magic_table_%d",
                    link_ip, link_type, link_ip, link_type);
                
                int result = system(command);
                if (result != 0) {
                    MAGIC_LOG(FD_LOG_WARNING, "添加路由规则失败: %s", command);
                }
                
                /* 3. 添加策略路由：来自客户端到链路模拟器的流量使用自定义表 */
                snprintf(command, sizeof(command),
                    "ip rule add from %s to %s table magic_table_%d priority 100 2>/dev/null || true",
                    client_ip, link_ip, link_type);
                
                result = system(command);
                if (result != 0) {
                    MAGIC_LOG(FD_LOG_WARNING, "添加策略路由失败: %s", command);
                }
                
                MAGIC_LOG(FD_LOG_DEBUG, "添加路由规则: %s -> %s (表: magic_table_%d)", 
                         client_ip, link_ip, link_type);
                
            } else {
                /* 删除路由规则 */
                
                /* 1. 删除策略路由 */
                snprintf(command, sizeof(command),
                    "ip rule del from %s to %s table magic_table_%d priority 100 2>/dev/null || true",
                    client_ip, link_ip, link_type);
                
                int result = system(command);
                if (result != 0) {
                    MAGIC_LOG(FD_LOG_DEBUG, "删除策略路由: %s (可能不存在)", command);
                }
                
                MAGIC_LOG(FD_LOG_DEBUG, "删除路由规则: %s -> %s (表: magic_table_%d)", 
                         client_ip, link_ip, link_type);
            }
        }
    }
    
    /* 刷新路由缓存 */
    system("ip route flush cache 2>/dev/null || true");
    
    return MAGIC_OK;
}

/* 初始化路由表 */
static int init_routing_tables(void)
{
    char command[512];
    
    MAGIC_LOG(FD_LOG_NOTICE, "初始化MAGIC系统路由表");
    
    /* 为每个链路类型创建专用路由表 */
    for (int i = 1; i <= MAX_LINKS; i++) {
        /* 确保路由表在 /etc/iproute2/rt_tables 中定义 */
        snprintf(command, sizeof(command),
            "grep -q 'magic_table_%d' /etc/iproute2/rt_tables || "
            "echo '10%d magic_table_%d' >> /etc/iproute2/rt_tables",
            i, i, i);
        system(command);
        
        /* 清空路由表（如果存在） */
        snprintf(command, sizeof(command),
            "ip route flush table magic_table_%d 2>/dev/null || true", i);
        system(command);
        
        /* 在路由表中添加默认的本地路由 */
        snprintf(command, sizeof(command),
            "ip route add local %s dev lo table magic_table_%d 2>/dev/null || true",
            link_simulators[i-1].ip, i);
        system(command);
    }
    
    /* 清理可能存在的旧策略路由 */
    system("ip rule del priority 100 2>/dev/null || true");
    
    MAGIC_LOG(FD_LOG_NOTICE, "路由表初始化完成");
    return MAGIC_OK;
}

/* 清理路由表 */
static int cleanup_routing_tables(void)
{
    char command[512];
    
    MAGIC_LOG(FD_LOG_NOTICE, "清理MAGIC系统路由表");
    
    /* 删除所有MAGIC相关的策略路由 */
    system("ip rule del priority 100 2>/dev/null || true");
    
    /* 清空所有MAGIC路由表 */
    for (int i = 1; i <= MAX_LINKS; i++) {
        snprintf(command, sizeof(command),
            "ip route flush table magic_table_%d 2>/dev/null || true", i);
        system(command);
    }
    
    /* 刷新路由缓存 */
    system("ip route flush cache 2>/dev/null || true");
    
    MAGIC_LOG(FD_LOG_NOTICE, "路由表清理完成");
    return MAGIC_OK;
}

/* 初始化防火墙规则 */
static int init_firewall_rules(void)
{
    char command[512];
    
    MAGIC_LOG(FD_LOG_NOTICE, "初始化MAGIC系统防火墙规则");
    
    /* 创建MAGIC专用的iptables链 */
    snprintf(command, sizeof(command), 
        "iptables -t filter -N MAGIC_FORWARD 2>/dev/null || true");
    system(command);
    
    /* 将MAGIC链插入到FORWARD链的开头 */
    snprintf(command, sizeof(command),
        "iptables -C FORWARD -j MAGIC_FORWARD 2>/dev/null || "
        "iptables -I FORWARD 1 -j MAGIC_FORWARD");
    system(command);
    
    /* 默认拒绝所有到链路模拟器的连接 */
    for (int i = 0; i < MAX_LINKS; i++) {
        snprintf(command, sizeof(command),
            "iptables -C MAGIC_FORWARD -d %s -p tcp --dport %d -j DROP 2>/dev/null || "
            "iptables -A MAGIC_FORWARD -d %s -p tcp --dport %d -j DROP",
            link_simulators[i].ip, link_simulators[i].port,
            link_simulators[i].ip, link_simulators[i].port);
        
        int result = system(command);
        if (result != 0) {
            MAGIC_LOG(FD_LOG_WARNING, "设置默认拒绝规则失败: %s", command);
        }
    }
    
    /* 允许本地回环接口通信 */
    snprintf(command, sizeof(command),
        "iptables -C MAGIC_FORWARD -i lo -j ACCEPT 2>/dev/null || "
        "iptables -I MAGIC_FORWARD 1 -i lo -j ACCEPT");
    system(command);
    
    MAGIC_LOG(FD_LOG_NOTICE, "防火墙规则初始化完成");
    return MAGIC_OK;
}

/* 清理防火墙规则 */
static int cleanup_firewall_rules(void)
{
    char command[512];
    
    MAGIC_LOG(FD_LOG_NOTICE, "清理MAGIC系统防火墙规则");
    
    /* 从FORWARD链中移除MAGIC链的引用 */
    snprintf(command, sizeof(command),
        "iptables -D FORWARD -j MAGIC_FORWARD 2>/dev/null || true");
    system(command);
    
    /* 清空并删除MAGIC链 */
    snprintf(command, sizeof(command),
        "iptables -F MAGIC_FORWARD 2>/dev/null || true");
    system(command);
    
    snprintf(command, sizeof(command),
        "iptables -X MAGIC_FORWARD 2>/dev/null || true");
    system(command);
    
    MAGIC_LOG(FD_LOG_NOTICE, "防火墙规则清理完成");
    return MAGIC_OK;
}

/* 授予网络访问权限 */
int dlm_grant_network_access(const char *client_id, network_access_t access_mask)
{
    if (!client_id || strlen(client_id) == 0) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_access_mutex);
    
    /* 查找现有记录或空闲位置 */
    int idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_client_access[i].active && strcmp(g_client_access[i].client_id, client_id) == 0) {
            idx = i;
            break;
        }
        if (!g_client_access[i].active && idx == -1) {
            idx = i;
        }
    }
    
    if (idx == -1) {
        pthread_mutex_unlock(&g_access_mutex);
        MAGIC_ERROR("客户端访问权限表已满");
        return MAGIC_ERROR_RESOURCE_LIMIT;
    }
    
    /* 设置访问权限 */
    strncpy(g_client_access[idx].client_id, client_id, sizeof(g_client_access[idx].client_id) - 1);
    g_client_access[idx].client_id[sizeof(g_client_access[idx].client_id) - 1] = '\0';
    g_client_access[idx].access_mask = access_mask;
    g_client_access[idx].granted_time = time(NULL);
    g_client_access[idx].active = 1;
    
    pthread_mutex_unlock(&g_access_mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "授予客户端 %s 网络访问权限: 0x%x", client_id, access_mask);
    
    /* 配置实际的防火墙规则，允许客户端访问指定的链路模拟器端口 */
    int result = configure_firewall_rules(client_id, access_mask, 1); /* 1 = 添加规则 */
    if (result != MAGIC_OK) {
        MAGIC_LOG(FD_LOG_WARNING, "配置防火墙规则失败，但继续授予权限");
    }
    
    /* 配置路由规则 */
    result = configure_routing_rules(client_id, access_mask, 1); /* 1 = 添加规则 */
    if (result != MAGIC_OK) {
        MAGIC_LOG(FD_LOG_WARNING, "配置路由规则失败，但继续授予权限");
    }
    
    return MAGIC_OK;
}

/* 撤销网络访问权限 */
int dlm_revoke_network_access(const char *client_id)
{
    if (!client_id || strlen(client_id) == 0) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_access_mutex);
    
    /* 查找客户端记录 */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_client_access[i].active && strcmp(g_client_access[i].client_id, client_id) == 0) {
            /* 保存访问权限掩码，用于删除规则 */
            network_access_t old_access = g_client_access[i].access_mask;
            
            /* 清除记录 */
            g_client_access[i].active = 0;
            memset(&g_client_access[i], 0, sizeof(g_client_access[i]));
            
            pthread_mutex_unlock(&g_access_mutex);
            
            MAGIC_LOG(FD_LOG_NOTICE, "撤销客户端 %s 的网络访问权限", client_id);
            
            /* 删除相应的防火墙规则 */
            int result = configure_firewall_rules(client_id, old_access, 0); /* 0 = 删除规则 */
            if (result != MAGIC_OK) {
                MAGIC_LOG(FD_LOG_WARNING, "删除防火墙规则失败");
            }
            
            /* 删除相应的路由规则 */
            result = configure_routing_rules(client_id, old_access, 0); /* 0 = 删除规则 */
            if (result != MAGIC_OK) {
                MAGIC_LOG(FD_LOG_WARNING, "删除路由规则失败");
            }
            
            return MAGIC_OK;
        }
    }
    
    pthread_mutex_unlock(&g_access_mutex);
    
    MAGIC_LOG(FD_LOG_WARNING, "未找到客户端 %s 的访问权限记录", client_id);
    return MAGIC_ERROR_NOT_FOUND;
}

/* 检查网络访问权限 */
int dlm_check_network_access(const char *client_id, int link_type)
{
    if (!client_id || strlen(client_id) == 0 || link_type < 1 || link_type > 4) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_access_mutex);
    
    /* 查找客户端记录 */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_client_access[i].active && strcmp(g_client_access[i].client_id, client_id) == 0) {
            network_access_t required_access = 1 << (link_type - 1);
            int has_access = (g_client_access[i].access_mask & required_access) != 0;
            
            pthread_mutex_unlock(&g_access_mutex);
            
            MAGIC_LOG(FD_LOG_DEBUG, "客户端 %s 访问链路 %d: %s", 
                     client_id, link_type, has_access ? "允许" : "拒绝");
            
            return has_access ? MAGIC_OK : MAGIC_ERROR_ACCESS_DENIED;
        }
    }
    
    pthread_mutex_unlock(&g_access_mutex);
    
    MAGIC_LOG(FD_LOG_DEBUG, "客户端 %s 无访问权限记录", client_id);
    return MAGIC_ERROR_ACCESS_DENIED;
}

/* 获取链路信息 */
int dlm_get_link_info(int link_id, char *ip, int *port)
{
    if (link_id < 1 || link_id > MAX_LINKS || !ip || !port) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    int idx = link_id - 1;
    strcpy(ip, link_simulators[idx].ip);
    *port = link_simulators[idx].port;
    
    MAGIC_LOG(FD_LOG_DEBUG, "链路 %d 信息: %s:%d", link_id, ip, *port);
    return MAGIC_OK;
}

/* 获取可用链路掩码 */
int dlm_get_available_links(int *link_mask)
{
    if (!link_mask) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    /* 所有链路都可用（链路模拟器独立运行） */
    *link_mask = NETWORK_ACCESS_ALL;
    
    MAGIC_LOG(FD_LOG_DEBUG, "可用链路掩码: 0x%x", *link_mask);
    return MAGIC_OK;
}