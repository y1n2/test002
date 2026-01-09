#ifndef _MAGIC_NETMGMT_H
#define _MAGIC_NETMGMT_H

#include "../magic_common.h"

/* 网络管理API */
int netmgmt_init(void);
void netmgmt_cleanup(void);

int netmgmt_add_route(const char *client_ip, int link_id);
int netmgmt_remove_route(const char *client_ip);
int netmgmt_add_firewall_rule(const char *client_ip, int link_id);
int netmgmt_remove_firewall_rule(const char *client_ip);

#endif /* _MAGIC_NETMGMT_H */