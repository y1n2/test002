#ifndef _MAGIC_CM_H
#define _MAGIC_CM_H

#include "../magic_common.h"

/* 客户端信息结构 */
typedef struct {
    char client_id[128];
    char ip_addr[64];
    int service_type;
    int priority;
    uint32_t allocated_bandwidth;
    time_t last_activity;
} magic_client_t;

/* 数据链路配置结构 */
typedef struct {
    int link_id;
    int link_type;
    uint32_t max_bandwidth;
    int status;
    uint32_t latency;
    float reliability;
    int signal_strength;
} magic_datalink_t;

/* 中央策略结构 */
typedef struct {
    uint32_t global_bandwidth_limit;
    int require_tls;
    uint32_t default_priority_bandwidth;
} magic_central_policy_t;

/* CM模块API */
int cm_init(const char *config_file);
void cm_cleanup(void);

/* 客户端管理API */
int cm_authenticate_client(struct msg *request, struct msg *answer);
int cm_select_link(struct msg *request, struct msg *answer);
int cm_update_environment(struct msg *request, struct msg *answer);

/* 带宽管理API */
int cm_allocate_bandwidth(const char *client_id, uint32_t requested_bandwidth, uint32_t *allocated_bandwidth);
int cm_release_bandwidth(const char *client_id);

/* 链路管理API */
int cm_get_optimal_link(int client_priority, int service_type, int *selected_link);
int cm_send_to_link(int link_id, const void *data, size_t data_len);
int cm_start_link_monitoring(void);
int cm_stop_link_monitoring(void);

#endif /* _MAGIC_CM_H */