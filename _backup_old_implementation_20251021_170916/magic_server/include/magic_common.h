#ifndef _MAGIC_COMMON_H
#define _MAGIC_COMMON_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <freeDiameter/extension.h>

/* ARINC839 应用ID */
#define ARINC839_APP_ID 100000

/* 链路类型定义 */
#define LINK_TYPE_ETHERNET 1
#define LINK_TYPE_WIFI     2
#define LINK_TYPE_CELLULAR 3
#define LINK_TYPE_SATCOM   4

/* 链路索引定义 */
#define MAGIC_LINK_ETHERNET  1
#define MAGIC_LINK_WIFI      2
#define MAGIC_LINK_CELLULAR  3
#define MAGIC_LINK_SATELLITE 4

/* 最大链路数量 */
#define MAGIC_LINK_MAX     4

/* 环境状态定义 */
#define ENV_STATE_GROUND     1
#define ENV_STATE_AIR        2
#define ENV_STATE_TRANSITION 3
#define ENV_STATE_UNKNOWN    0

/* 链路状态定义 */
#define LINK_STATUS_UP       1
#define LINK_STATUS_DOWN     0
#define LINK_STATUS_DEGRADED 2

/* 日志宏定义 */
#define MAGIC_LOG(level, format, ...) \
    fd_log_debug("%s: " format, __FUNCTION__, ##__VA_ARGS__)

#define MAGIC_ERROR(format, ...) \
    fd_log_error("%s: " format, __FUNCTION__, ##__VA_ARGS__)

/* IPC通信相关定义 */
#define IPC_SOCKET_PATH "/tmp/magic_ipc.sock"
#define IPC_MAX_MSG_SIZE 4096

/* 通用错误码 */
typedef enum {
    MAGIC_OK = 0,
    MAGIC_ERROR_GENERAL = -1,
    MAGIC_ERROR_MEMORY = -2,
    MAGIC_ERROR_INVALID_PARAM = -3,
    MAGIC_ERROR_NOT_FOUND = -4,
    MAGIC_ERROR_ALREADY_EXISTS = -5,
    MAGIC_ERROR_COMMUNICATION = -6,
    MAGIC_ERROR_CONFIG = -7,
    MAGIC_ERROR_RESOURCE_LIMIT = -8,
    MAGIC_ERROR_ACCESS_DENIED = -9
} magic_error_t;

/* CIC模块函数声明 */
int cic_init(char *conffile);
void cic_cleanup(void);

#endif /* _MAGIC_COMMON_H */