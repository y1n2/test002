#ifndef _MAGIC_DLM_H
#define _MAGIC_DLM_H

#include "../magic_common.h"

/* 网络访问权限类型 */
typedef enum {
    NETWORK_ACCESS_DENIED = 0,
    NETWORK_ACCESS_ETHERNET = 1,
    NETWORK_ACCESS_WIFI = 2,
    NETWORK_ACCESS_CELLULAR = 4,
    NETWORK_ACCESS_SATELLITE = 8,
    NETWORK_ACCESS_ALL = 15
} network_access_t;

/* DLM API - 网络管理模式 */
int dlm_init(void);
void dlm_cleanup(void);

/* 网络访问控制 */
int dlm_grant_network_access(const char *client_id, network_access_t access_mask);
int dlm_revoke_network_access(const char *client_id);
int dlm_check_network_access(const char *client_id, int link_type);

/* 链路信息查询 */
int dlm_get_link_info(int link_id, char *ip, int *port);
int dlm_get_available_links(int *link_mask);

/* 兼容性接口（已废弃，仅用于向后兼容） */
int dlm_open_link(int link_id) __attribute__((deprecated));
int dlm_close_link(int link_id) __attribute__((deprecated));
int dlm_get_link_status(int link_id, int *status) __attribute__((deprecated));
int dlm_send_data(int link_id, const void *data, size_t data_len) __attribute__((deprecated));

#endif /* _MAGIC_DLM_H */