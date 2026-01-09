#include "../../include/cm/magic_cm.h"
#include "../../include/dlm/magic_dlm.h"
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* 全局CM实例 */
typedef struct {
    magic_client_t *clients;
    int client_count;
    int max_clients;
    
    magic_datalink_t *links;
    int link_count;
    
    magic_central_policy_t policy;
    
    uint32_t total_bandwidth_used;
    time_t utc_time;
    
    pthread_mutex_t mutex;
    
    int dlm_sockets[4]; /* 对应4种链路类型的socket */
    pthread_t monitor_thread;
    int monitor_running;
} magic_cm_t;

static magic_cm_t g_cm;

/* 初始化CM模块 */
int cm_init(const char *config_file)
{
    MAGIC_LOG(FD_LOG_NOTICE, "初始化CM模块");
    
    memset(&g_cm, 0, sizeof(g_cm));
    
    /* 初始化互斥锁 */
    pthread_mutex_init(&g_cm.mutex, NULL);
    
    /* 分配客户端和链路数组 */
    g_cm.max_clients = 100;
    g_cm.clients = (magic_client_t *)malloc(g_cm.max_clients * sizeof(magic_client_t));
    if (!g_cm.clients) {
        MAGIC_ERROR("无法分配客户端数组内存");
        return MAGIC_ERROR_MEMORY;
    }
    
    /* 初始化链路 */
    g_cm.links = (magic_datalink_t *)malloc(4 * sizeof(magic_datalink_t));
    if (!g_cm.links) {
        MAGIC_ERROR("无法分配链路数组内存");
        free(g_cm.clients);
        return MAGIC_ERROR_MEMORY;
    }
    
    /* 配置默认链路 */
    g_cm.link_count = 4;
    
    /* 以太网链路 */
    g_cm.links[0].link_id = 1;
    g_cm.links[0].link_type = LINK_TYPE_ETHERNET;
    g_cm.links[0].max_bandwidth = 100000000; /* 100 Mbps */
    g_cm.links[0].status = LINK_STATUS_UP;
    g_cm.links[0].latency = 10; /* 10ms */
    g_cm.links[0].reliability = 0.99;
    g_cm.links[0].signal_strength = 90;
    
    /* WiFi链路 */
    g_cm.links[1].link_id = 2;
    g_cm.links[1].link_type = LINK_TYPE_WIFI;
    g_cm.links[1].max_bandwidth = 54000000; /* 54 Mbps */
    g_cm.links[1].status = LINK_STATUS_UP;
    g_cm.links[1].latency = 20; /* 20ms */
    g_cm.links[1].reliability = 0.95;
    g_cm.links[1].signal_strength = 80;
    
    /* 蜂窝链路 */
    g_cm.links[2].link_id = 3;
    g_cm.links[2].link_type = LINK_TYPE_CELLULAR;
    g_cm.links[2].max_bandwidth = 10000000; /* 10 Mbps */
    g_cm.links[2].status = LINK_STATUS_UP;
    g_cm.links[2].latency = 100; /* 100ms */
    g_cm.links[2].reliability = 0.90;
    g_cm.links[2].signal_strength = 70;
    
    /* 卫星链路 */
    g_cm.links[3].link_id = 4;
    g_cm.links[3].link_type = LINK_TYPE_SATCOM;
    g_cm.links[3].max_bandwidth = 1000000; /* 1 Mbps */
    g_cm.links[3].status = LINK_STATUS_UP;
    g_cm.links[3].latency = 500; /* 500ms */
    g_cm.links[3].reliability = 0.85;
    g_cm.links[3].signal_strength = 60;
    
    /* 设置默认策略 */
    g_cm.policy.global_bandwidth_limit = 150000000; /* 150 Mbps */
    g_cm.policy.require_tls = 1;
    g_cm.policy.default_priority_bandwidth = 1000000; /* 1 Mbps */
    
    /* 启动链路监控 */
    cm_start_link_monitoring();
    
    MAGIC_LOG(FD_LOG_NOTICE, "CM模块初始化完成");
    return MAGIC_OK;
}

/* 清理CM模块 */
void cm_cleanup(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "清理CM模块");
    
    /* 停止链路监控 */
    cm_stop_link_monitoring();
    
    /* 释放资源 */
    pthread_mutex_destroy(&g_cm.mutex);
    
    if (g_cm.clients) {
        free(g_cm.clients);
        g_cm.clients = NULL;
    }
    
    if (g_cm.links) {
        free(g_cm.links);
        g_cm.links = NULL;
    }
    
    /* 关闭所有DLM套接字 */
    for (int i = 0; i < 4; i++) {
        if (g_cm.dlm_sockets[i] > 0) {
            close(g_cm.dlm_sockets[i]);
            g_cm.dlm_sockets[i] = 0;
        }
    }
}

/* 客户端认证 */
int cm_authenticate_client(struct msg *request, struct msg *answer)
{
    struct avp *avp;
    struct avp_hdr *hdr;
    char client_id[128] = {0};
    char client_ip[64] = {0};
    int service_type = 0;
    int priority = 0;
    int ret;
    
    MAGIC_LOG(FD_LOG_NOTICE, "处理客户端认证请求");
    
    /* 从请求中提取客户端ID */
    {
        struct dict_object *avp_origin_host;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &avp_origin_host, ENOENT));
        ret = fd_msg_search_avp(request, avp_origin_host, &avp);
    }
    if (ret == 0 && avp) {
        ret = fd_msg_avp_hdr(avp, &hdr);
        if (ret == 0) {
            strncpy(client_id, (char *)hdr->avp_value->os.data, 
                    hdr->avp_value->os.len > 127 ? 127 : hdr->avp_value->os.len);
        }
    }
    
    /* 从请求中提取客户端IP */
    {
        struct dict_object *avp_host_ip;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Host-IP-Address", &avp_host_ip, ENOENT));
        ret = fd_msg_search_avp(request, avp_host_ip, &avp);
    }
    if (ret == 0 && avp) {
        ret = fd_msg_avp_hdr(avp, &hdr);
        if (ret == 0) {
            strncpy(client_ip, (char *)hdr->avp_value->os.data, 
                    hdr->avp_value->os.len > 63 ? 63 : hdr->avp_value->os.len);
        }
    }
    
    /* 从请求中提取服务类型 */
    /* 注意：这里假设有一个名为MAGIC_SERVICE_TYPE的AVP */
    {
        struct dict_object *avp_user_name;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "User-Name", &avp_user_name, ENOENT));
        ret = fd_msg_search_avp(request, avp_user_name, &avp);
    }
    if (ret == 0 && avp) {
        ret = fd_msg_avp_hdr(avp, &hdr);
        if (ret == 0) {
            service_type = 1; /* 默认服务类型 */
        }
    }
    
    /* 从请求中提取优先级 */
    /* 注意：这里假设有一个名为MAGIC_PRIORITY的AVP */
    priority = 5; /* 默认优先级 */
    
    /* 锁定CM */
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 检查客户端是否已存在 */
    for (int i = 0; i < g_cm.client_count; i++) {
        if (strcmp(g_cm.clients[i].client_id, client_id) == 0) {
            /* 更新现有客户端 */
            strncpy(g_cm.clients[i].ip_addr, client_ip, sizeof(g_cm.clients[i].ip_addr) - 1);
            g_cm.clients[i].service_type = service_type;
            g_cm.clients[i].priority = priority;
            g_cm.clients[i].last_activity = time(NULL);
            
            pthread_mutex_unlock(&g_cm.mutex);
            
            /* 为现有客户端重新授予网络访问权限 */
            network_access_t access_mask = NETWORK_ACCESS_ALL;
            
            /* 根据服务类型和优先级调整访问权限 */
            if (priority >= 8) {
                access_mask = NETWORK_ACCESS_ALL;
            } else if (priority >= 5) {
                access_mask = NETWORK_ACCESS_ETHERNET | NETWORK_ACCESS_WIFI | NETWORK_ACCESS_CELLULAR;
            } else {
                access_mask = NETWORK_ACCESS_ETHERNET | NETWORK_ACCESS_WIFI;
            }
            
            ret = dlm_grant_network_access(client_id, access_mask);
            if (ret != MAGIC_OK) {
                MAGIC_ERROR("更新客户端 %s 网络访问权限失败: %d", client_id, ret);
            } else {
                MAGIC_LOG(FD_LOG_NOTICE, "已更新客户端 %s 网络访问权限: 0x%x", client_id, access_mask);
            }
            
            MAGIC_LOG(FD_LOG_NOTICE, "更新现有客户端: %s", client_id);
            return MAGIC_OK;
        }
    }
    
    /* 添加新客户端 */
    if (g_cm.client_count < g_cm.max_clients) {
        strncpy(g_cm.clients[g_cm.client_count].client_id, client_id, sizeof(g_cm.clients[g_cm.client_count].client_id) - 1);
        strncpy(g_cm.clients[g_cm.client_count].ip_addr, client_ip, sizeof(g_cm.clients[g_cm.client_count].ip_addr) - 1);
        g_cm.clients[g_cm.client_count].service_type = service_type;
        g_cm.clients[g_cm.client_count].priority = priority;
        g_cm.clients[g_cm.client_count].allocated_bandwidth = 0;
        g_cm.clients[g_cm.client_count].last_activity = time(NULL);
        g_cm.client_count++;
        
        pthread_mutex_unlock(&g_cm.mutex);
        
        /* 认证成功后，授予网络访问权限 */
        network_access_t access_mask = NETWORK_ACCESS_ALL; /* 默认授予所有链路访问权限 */
        
        /* 根据服务类型和优先级调整访问权限 */
        if (priority >= 8) {
            /* 高优先级客户端可访问所有链路 */
            access_mask = NETWORK_ACCESS_ALL;
        } else if (priority >= 5) {
            /* 中等优先级客户端可访问以太网、WiFi和蜂窝 */
            access_mask = NETWORK_ACCESS_ETHERNET | NETWORK_ACCESS_WIFI | NETWORK_ACCESS_CELLULAR;
        } else {
            /* 低优先级客户端只能访问以太网和WiFi */
            access_mask = NETWORK_ACCESS_ETHERNET | NETWORK_ACCESS_WIFI;
        }
        
        ret = dlm_grant_network_access(client_id, access_mask);
        if (ret != MAGIC_OK) {
            MAGIC_ERROR("授予客户端 %s 网络访问权限失败: %d", client_id, ret);
            /* 认证成功但网络权限授予失败，仍然返回成功，但记录错误 */
        } else {
            MAGIC_LOG(FD_LOG_NOTICE, "已授予客户端 %s 网络访问权限: 0x%x", client_id, access_mask);
        }
        
        MAGIC_LOG(FD_LOG_NOTICE, "添加新客户端: %s", client_id);
        return MAGIC_OK;
    }
    
    pthread_mutex_unlock(&g_cm.mutex);
    MAGIC_ERROR("客户端数量已达上限");
    return MAGIC_ERROR_GENERAL;
}

/* 链路选择 */
int cm_select_link(struct msg *request, struct msg *answer)
{
    struct avp *avp;
    struct avp_hdr *hdr;
    char client_id[128] = {0};
    int service_type = 0;
    int priority = 0;
    int selected_link = 0;
    uint32_t requested_bandwidth = 1000000; /* 默认1Mbps */
    uint32_t allocated_bandwidth = 0;
    int ret;
    
    MAGIC_LOG(FD_LOG_NOTICE, "处理链路选择请求");
    
    /* 从请求中提取客户端ID */
    {
        struct dict_object *avp_origin_host;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &avp_origin_host, ENOENT));
        ret = fd_msg_search_avp(request, avp_origin_host, &avp);
    }
    if (ret == 0 && avp) {
        ret = fd_msg_avp_hdr(avp, &hdr);
        if (ret == 0) {
            strncpy(client_id, (char *)hdr->avp_value->os.data, 
                    hdr->avp_value->os.len > 127 ? 127 : hdr->avp_value->os.len);
        }
    }
    
    /* 从请求中提取请求带宽 */
    /* 注意：这里假设有一个名为MAGIC_REQUESTED_BANDWIDTH的AVP */
    
    /* 查找客户端信息 */
    pthread_mutex_lock(&g_cm.mutex);
    
    for (int i = 0; i < g_cm.client_count; i++) {
        if (strcmp(g_cm.clients[i].client_id, client_id) == 0) {
            service_type = g_cm.clients[i].service_type;
            priority = g_cm.clients[i].priority;
            g_cm.clients[i].last_activity = time(NULL);
            break;
        }
    }
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    /* 选择最优链路 */
    ret = cm_get_optimal_link(priority, service_type, &selected_link);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("无法选择最优链路");
        return ret;
    }
    
    /* 分配带宽 */
    ret = cm_allocate_bandwidth(client_id, requested_bandwidth, &allocated_bandwidth);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("无法分配带宽");
        return ret;
    }
    
    /* 添加选定链路信息到应答 */
    /* 注意：这里假设有名为MAGIC_SELECTED_LINK和MAGIC_ALLOCATED_BANDWIDTH的AVP */
    
    MAGIC_LOG(FD_LOG_NOTICE, "选择链路 %d，分配带宽 %u bps", selected_link, allocated_bandwidth);
    return MAGIC_OK;
}

/* 环境更新 */
int cm_update_environment(struct msg *request, struct msg *answer)
{
    struct avp *avp;
    struct avp_hdr *hdr;
    int env_state = ENV_STATE_UNKNOWN;
    int ret;
    
    MAGIC_LOG(FD_LOG_NOTICE, "处理环境更新请求");
    
    /* 从请求中提取环境状态 */
    /* 注意：这里假设有一个名为MAGIC_ENVIRONMENT_STATE的AVP */
    
    /* 更新链路状态 */
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 根据环境状态调整链路优先级和可用性 */
    switch (env_state) {
        case ENV_STATE_GROUND:
            /* 地面模式：优先使用以太网和WiFi */
            g_cm.links[0].status = LINK_STATUS_UP; /* 以太网 */
            g_cm.links[1].status = LINK_STATUS_UP; /* WiFi */
            break;
            
        case ENV_STATE_AIR:
            /* 空中模式：优先使用卫星和蜂窝 */
            g_cm.links[2].status = LINK_STATUS_UP; /* 蜂窝 */
            g_cm.links[3].status = LINK_STATUS_UP; /* 卫星 */
            break;
            
        case ENV_STATE_TRANSITION:
            /* 过渡模式：所有链路可用 */
            for (int i = 0; i < g_cm.link_count; i++) {
                g_cm.links[i].status = LINK_STATUS_UP;
            }
            break;
            
        default:
            /* 未知状态：保持当前配置 */
            break;
    }
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "环境状态更新为: %d", env_state);
    return MAGIC_OK;
}

/* 分配带宽 */
int cm_allocate_bandwidth(const char *client_id, uint32_t requested_bandwidth, uint32_t *allocated_bandwidth)
{
    int client_index = -1;
    
    if (!client_id || !allocated_bandwidth) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 查找客户端 */
    for (int i = 0; i < g_cm.client_count; i++) {
        if (strcmp(g_cm.clients[i].client_id, client_id) == 0) {
            client_index = i;
            break;
        }
    }
    
    if (client_index == -1) {
        pthread_mutex_unlock(&g_cm.mutex);
        return MAGIC_ERROR_NOT_FOUND;
    }
    
    /* 检查全局带宽限制 */
    uint32_t available_bandwidth = g_cm.policy.global_bandwidth_limit - g_cm.total_bandwidth_used;
    
    /* 根据优先级调整可用带宽 */
    int priority = g_cm.clients[client_index].priority;
    uint32_t priority_bandwidth = g_cm.policy.default_priority_bandwidth * priority;
    
    /* 计算实际分配的带宽 */
    *allocated_bandwidth = requested_bandwidth;
    
    if (*allocated_bandwidth > available_bandwidth) {
        *allocated_bandwidth = available_bandwidth;
    }
    
    if (*allocated_bandwidth > priority_bandwidth) {
        *allocated_bandwidth = priority_bandwidth;
    }
    
    /* 更新客户端和全局带宽使用量 */
    g_cm.total_bandwidth_used += *allocated_bandwidth - g_cm.clients[client_index].allocated_bandwidth;
    g_cm.clients[client_index].allocated_bandwidth = *allocated_bandwidth;
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "为客户端 %s 分配带宽 %u bps", client_id, *allocated_bandwidth);
    return MAGIC_OK;
}

/* 释放带宽 */
int cm_release_bandwidth(const char *client_id)
{
    int client_index = -1;
    
    if (!client_id) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 查找客户端 */
    for (int i = 0; i < g_cm.client_count; i++) {
        if (strcmp(g_cm.clients[i].client_id, client_id) == 0) {
            client_index = i;
            break;
        }
    }
    
    if (client_index == -1) {
        pthread_mutex_unlock(&g_cm.mutex);
        return MAGIC_ERROR_NOT_FOUND;
    }
    
    /* 更新全局带宽使用量 */
    g_cm.total_bandwidth_used -= g_cm.clients[client_index].allocated_bandwidth;
    g_cm.clients[client_index].allocated_bandwidth = 0;
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    MAGIC_LOG(FD_LOG_NOTICE, "释放客户端 %s 的带宽", client_id);
    return MAGIC_OK;
}

/* 获取最优链路 */
int cm_get_optimal_link(int client_priority, int service_type, int *selected_link)
{
    int best_link = -1;
    int best_score = -1;
    
    if (!selected_link) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 评估每个链路 */
    for (int i = 0; i < g_cm.link_count; i++) {
        if (g_cm.links[i].status != LINK_STATUS_UP) {
            continue;
        }
        
        /* 计算链路得分 */
        int score = 0;
        
        /* 带宽得分 */
        score += g_cm.links[i].max_bandwidth / 1000000;
        
        /* 延迟得分（越低越好） */
        score += (1000 - g_cm.links[i].latency) / 10;
        
        /* 可靠性得分 */
        score += (int)(g_cm.links[i].reliability * 100);
        
        /* 信号强度得分 */
        score += g_cm.links[i].signal_strength;
        
        /* 根据服务类型调整得分 */
        switch (service_type) {
            case 1: /* 实时服务 */
                /* 优先考虑低延迟 */
                score += (1000 - g_cm.links[i].latency) / 5;
                break;
                
            case 2: /* 高带宽服务 */
                /* 优先考虑高带宽 */
                score += g_cm.links[i].max_bandwidth / 500000;
                break;
                
            case 3: /* 关键服务 */
                /* 优先考虑高可靠性 */
                score += (int)(g_cm.links[i].reliability * 200);
                break;
                
            default:
                /* 默认平衡评分 */
                break;
        }
        
        /* 更新最佳链路 */
        if (score > best_score) {
            best_score = score;
            best_link = g_cm.links[i].link_id;
        }
    }
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    if (best_link == -1) {
        MAGIC_ERROR("没有可用的链路");
        return MAGIC_ERROR_NOT_FOUND;
    }
    
    *selected_link = best_link;
    MAGIC_LOG(FD_LOG_NOTICE, "选择最优链路: %d，得分: %d", best_link, best_score);
    return MAGIC_OK;
}

/* 发送数据到链路 */
int cm_send_to_link(int link_id, const void *data, size_t data_len)
{
    int link_index = -1;
    
    if (!data || data_len == 0) {
        return MAGIC_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cm.mutex);
    
    /* 查找链路 */
    for (int i = 0; i < g_cm.link_count; i++) {
        if (g_cm.links[i].link_id == link_id) {
            link_index = i;
            break;
        }
    }
    
    if (link_index == -1 || g_cm.links[link_index].status != LINK_STATUS_UP) {
        pthread_mutex_unlock(&g_cm.mutex);
        return MAGIC_ERROR_NOT_FOUND;
    }
    
    /* 检查DLM套接字 */
    if (g_cm.dlm_sockets[link_index] <= 0) {
        pthread_mutex_unlock(&g_cm.mutex);
        return MAGIC_ERROR_COMMUNICATION;
    }
    
    /* 发送数据 */
    ssize_t sent = send(g_cm.dlm_sockets[link_index], data, data_len, 0);
    
    pthread_mutex_unlock(&g_cm.mutex);
    
    if (sent != (ssize_t)data_len) {
        MAGIC_ERROR("发送数据到链路 %d 失败", link_id);
        return MAGIC_ERROR_COMMUNICATION;
    }
    
    MAGIC_LOG(FD_LOG_DEBUG, "发送 %zu 字节数据到链路 %d", data_len, link_id);
    return MAGIC_OK;
}

/* 链路监控线程 */
static void *link_monitor_thread(void *arg)
{
    while (g_cm.monitor_running) {
        /* 更新链路状态 */
        pthread_mutex_lock(&g_cm.mutex);
        
        for (int i = 0; i < g_cm.link_count; i++) {
            /* 模拟链路状态检查 */
            /* 在实际实现中，这里应该与DLM通信获取真实状态 */
        }
        
        /* 清理不活跃的客户端 */
        time_t now = time(NULL);
        for (int i = 0; i < g_cm.client_count; i++) {
            if (now - g_cm.clients[i].last_activity > 3600) { /* 1小时不活跃 */
                /* 释放带宽 */
                g_cm.total_bandwidth_used -= g_cm.clients[i].allocated_bandwidth;
                
                /* 移除客户端 */
                if (i < g_cm.client_count - 1) {
                    memmove(&g_cm.clients[i], &g_cm.clients[i + 1], 
                            (g_cm.client_count - i - 1) * sizeof(magic_client_t));
                }
                g_cm.client_count--;
                i--; /* 调整索引 */
            }
        }
        
        pthread_mutex_unlock(&g_cm.mutex);
        
        /* 每10秒检查一次 */
        sleep(10);
    }
    
    return NULL;
}

/* 启动链路监控 */
int cm_start_link_monitoring(void)
{
    if (g_cm.monitor_running) {
        return MAGIC_OK; /* 已经在运行 */
    }
    
    g_cm.monitor_running = 1;
    
    int ret = pthread_create(&g_cm.monitor_thread, NULL, link_monitor_thread, NULL);
    if (ret != 0) {
        MAGIC_ERROR("创建链路监控线程失败: %d", ret);
        g_cm.monitor_running = 0;
        return MAGIC_ERROR_GENERAL;
    }
    
    MAGIC_LOG(FD_LOG_NOTICE, "链路监控已启动");
    return MAGIC_OK;
}

/* 停止链路监控 */
int cm_stop_link_monitoring(void)
{
    if (!g_cm.monitor_running) {
        return MAGIC_OK; /* 已经停止 */
    }
    
    g_cm.monitor_running = 0;
    
    /* 等待线程结束 */
    pthread_join(g_cm.monitor_thread, NULL);
    
    MAGIC_LOG(FD_LOG_NOTICE, "链路监控已停止");
    return MAGIC_OK;
}