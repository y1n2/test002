#include <freeDiameter/extension.h>
#include "../include/magic_common.h"
#include "../include/utils/magic_config.h"
#include "../include/cic/magic_cic.h"
#include "../include/cm/magic_cm.h"
#include "../include/dlm/magic_dlm.h"
#include "../include/netmgmt/magic_netmgmt.h"

/* 扩展入口点 */
static int magic_server_entry(char *conffile)
{
    int ret;
    
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC服务器扩展启动");
    
    /* 初始化配置 */
    ret = magic_config_init(conffile);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("配置初始化失败");
        return EINVAL;
    }
    
    /* 初始化网络管理 */
    ret = netmgmt_init();
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("网络管理初始化失败");
        goto error_netmgmt;
    }
    
    /* 初始化数据链路模块 */
    ret = dlm_init();
    if (ret != MAGIC_OK) {
        MAGIC_LOG(FD_LOG_ERROR, "数据链路模块初始化失败，但继续启动（链路模拟器可能未运行）");
        /* 不返回错误，允许扩展继续加载 */
    }
    
    /* 初始化中央管理模块 */
    ret = cm_init(conffile);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("中央管理模块初始化失败");
        goto error_cm;
    }
    
    /* 初始化客户端接口控制器 */
    ret = cic_init(conffile);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("客户端接口控制器初始化失败");
        goto error_cic;
    }
    
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC服务器扩展初始化成功");
    return 0;
    
error_cic:
    cm_cleanup();
error_cm:
    /* DLM 可能初始化失败，但仍需要清理 */
    dlm_cleanup();
    netmgmt_cleanup();
error_netmgmt:
    magic_config_cleanup();
    return EINVAL;
}

/* 扩展清理函数 */
void fd_ext_fini(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC服务器扩展关闭");
    
    /* 按照初始化的相反顺序清理 */
    cic_cleanup();
    cm_cleanup();
    dlm_cleanup();
    netmgmt_cleanup();
    magic_config_cleanup();
}

/* 扩展注册 */
EXTENSION_ENTRY("magic_server", magic_server_entry);