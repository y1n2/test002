/**
 * MAGIC 客户端接口控制器(CIC)模块
 * 基于freeDiameter扩展实现ARINC839应用
 */

#include "../../include/magic_common.h"
#include "../../include/cm/magic_cm.h"

/* 处理MCAR(Mobile Client Authentication Request)请求 */
static int magic_cic_mcar_cb(struct msg **msg, struct avp *avp, struct session *sess, void *data, enum disp_action *act)
{
    struct msg *ans, *qry;
    struct avp *avp_res;
    union avp_value value;
    int ret = 0;
    
    MAGIC_LOG(FD_LOG_NOTICE, "收到MCAR请求");
    
    /* 获取请求消息 */
    qry = *msg;
    
    /* 创建应答消息 */
    CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0));
    ans = *msg;
    
    /* 设置结果代码AVP */
    {
        struct dict_object *avp_result_code;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &avp_result_code, ENOENT));
        CHECK_FCT(fd_msg_avp_new(avp_result_code, 0, &avp_res));
    }
    value.i32 = ER_DIAMETER_SUCCESS;
    CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    CHECK_FCT(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, avp_res));
    
    /* 调用CM模块进行认证和链路选择 */
    ret = cm_authenticate_client(qry, ans);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("客户端认证失败: %d", ret);
        value.i32 = ER_DIAMETER_AUTHENTICATION_REJECTED;
        CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    }
    
    /* 发送应答 */
    CHECK_FCT(fd_msg_send(msg, NULL, NULL));
    
    return 0;
}

/* 处理LSR(Link Selection Request)请求 */
static int magic_cic_lsr_cb(struct msg **msg, struct avp *avp, struct session *sess, void *data, enum disp_action *act)
{
    struct msg *ans, *qry;
    struct avp *avp_res;
    union avp_value value;
    int ret = 0;
    
    MAGIC_LOG(FD_LOG_NOTICE, "收到LSR请求");
    
    /* 获取请求消息 */
    qry = *msg;
    
    /* 创建应答消息 */
    CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0));
    ans = *msg;
    
    /* 设置结果代码AVP */
    {
        struct dict_object *avp_result_code;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &avp_result_code, ENOENT));
        CHECK_FCT(fd_msg_avp_new(avp_result_code, 0, &avp_res));
    }
    value.i32 = ER_DIAMETER_SUCCESS;
    CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    CHECK_FCT(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, avp_res));
    
    /* 调用CM模块进行链路选择 */
    ret = cm_select_link(qry, ans);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("链路选择失败: %d", ret);
        value.i32 = ER_DIAMETER_UNABLE_TO_COMPLY;
        CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    }
    
    /* 发送应答 */
    CHECK_FCT(fd_msg_send(msg, NULL, NULL));
    
    return 0;
}

/* 处理EUR(Environment Update Request)请求 */
static int magic_cic_eur_cb(struct msg **msg, struct avp *avp, struct session *sess, void *data, enum disp_action *act)
{
    struct msg *ans, *qry;
    struct avp *avp_res;
    union avp_value value;
    int ret = 0;
    
    MAGIC_LOG(FD_LOG_NOTICE, "收到EUR请求");
    
    /* 获取请求消息 */
    qry = *msg;
    
    /* 创建应答消息 */
    CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0));
    ans = *msg;
    
    /* 设置结果代码AVP */
    {
        struct dict_object *avp_result_code;
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &avp_result_code, ENOENT));
        CHECK_FCT(fd_msg_avp_new(avp_result_code, 0, &avp_res));
    }
    value.i32 = ER_DIAMETER_SUCCESS;
    CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    CHECK_FCT(fd_msg_avp_add(ans, MSG_BRW_LAST_CHILD, avp_res));
    
    /* 调用CM模块处理环境更新 */
    ret = cm_update_environment(qry, ans);
    if (ret != MAGIC_OK) {
        MAGIC_ERROR("环境更新失败: %d", ret);
        value.i32 = ER_DIAMETER_UNABLE_TO_COMPLY;
        CHECK_FCT(fd_msg_avp_setvalue(avp_res, &value));
    }
    
    /* 发送应答 */
    CHECK_FCT(fd_msg_send(msg, NULL, NULL));
    
    return 0;
}

/* 注册命令回调函数 */
static int magic_cic_register_callbacks(void)
{
    struct disp_when data;
    
    /* 注册 MCAR 命令回调 */
    {
        uint32_t app_id = ARINC839_APP_ID;
        memset(&data, 0, sizeof(data));
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_ID, &app_id, &data.app, ENOENT));
        {
            struct dict_object *cmd_mcar = NULL;
            uint32_t cmd_code = 100000; /* 使用dict_arinc839中定义的MCAR命令代码 */
            
            /* 先尝试查找现有的MCAR命令 */
            int ret = fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, 
                                   CMD_BY_CODE_R, &cmd_code, 
                                   &cmd_mcar, ENOENT);
            
            if (ret == 0) {
                /* 命令已存在，使用现有对象 */
                TRACE_DEBUG(INFO, "MCAR命令已存在，使用现有对象");
            } else {
                /* 命令不存在，创建新对象 */
                struct dict_cmd_data cmd_data = { cmd_code, "MCAR", CMD_FLAG_REQUEST, CMD_FLAG_REQUEST };
                CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &cmd_data, data.app, &cmd_mcar));
                TRACE_DEBUG(INFO, "创建新的MCAR命令对象");
            }
            data.command = cmd_mcar;
        } /* MCAR命令代码 */
        CHECK_FCT(fd_disp_register(magic_cic_mcar_cb, DISP_HOW_CC, &data, NULL, NULL));
    }
    
    /* 注册 LSR 命令回调 */
    {
        uint32_t app_id = ARINC839_APP_ID;
        memset(&data, 0, sizeof(data));
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_ID, &app_id, &data.app, ENOENT));
        {
            struct dict_object *cmd_lsr = NULL;
            uint32_t cmd_code = 100010; /* 使用dict_arinc839中定义的Link-Resource命令代码 */
            
            /* 先尝试查找现有的LSR命令 */
            int ret = fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, 
                                   CMD_BY_CODE_R, &cmd_code, 
                                   &cmd_lsr, ENOENT);
            
            if (ret == 0) {
                /* 命令已存在，使用现有对象 */
                TRACE_DEBUG(INFO, "LSR命令已存在，使用现有对象");
            } else {
                /* 命令不存在，创建新对象 */
                struct dict_cmd_data cmd_data = { cmd_code, "LSR", CMD_FLAG_REQUEST, CMD_FLAG_REQUEST };
                CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &cmd_data, data.app, &cmd_lsr));
                TRACE_DEBUG(INFO, "创建新的LSR命令对象");
            }
            data.command = cmd_lsr;
        } /* LSR命令代码 */
        /* 不需要再次查找命令，已经在上面创建了 */
        CHECK_FCT(fd_disp_register(magic_cic_lsr_cb, DISP_HOW_CC, &data, NULL, NULL));
    }
    
    /* 注册 EUR 命令回调 */
    {
        uint32_t app_id = ARINC839_APP_ID;
        memset(&data, 0, sizeof(data));
        CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_ID, &app_id, &data.app, ENOENT));
        {
            struct dict_object *cmd_eur = NULL;
            uint32_t cmd_code = 100015; /* 使用dict_arinc839中定义的Link-Selection命令代码 */
            
            /* 先尝试查找现有的EUR命令 */
            int ret = fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, 
                                   CMD_BY_CODE_R, &cmd_code, 
                                   &cmd_eur, ENOENT);
            
            if (ret == 0) {
                /* 命令已存在，使用现有对象 */
                TRACE_DEBUG(INFO, "EUR命令已存在，使用现有对象");
            } else {
                /* 命令不存在，创建新对象 */
                struct dict_cmd_data cmd_data = { cmd_code, "EUR", CMD_FLAG_REQUEST, CMD_FLAG_REQUEST };
                CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &cmd_data, data.app, &cmd_eur));
                TRACE_DEBUG(INFO, "创建新的EUR命令对象");
            }
            data.command = cmd_eur;
        } /* EUR命令代码 */
        /* 不需要再次查找命令，已经在上面创建了 */
        CHECK_FCT(fd_disp_register(magic_cic_eur_cb, DISP_HOW_CC, &data, NULL, NULL));
    }
    
    return 0;
}

/* 扩展模块入口函数 */
static int magic_cic_entry(char *conffile)
{
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC CIC模块初始化开始");
    
    /* 初始化CM模块 */
    if (cm_init(conffile) != MAGIC_OK) {
        MAGIC_ERROR("CM模块初始化失败");
        return EINVAL;
    }
    
    /* 注册命令回调函数 */
    CHECK_FCT(magic_cic_register_callbacks());
    
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC CIC模块初始化完成");
    return 0;
}

/* CIC模块清理函数 */
void cic_cleanup(void)
{
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC CIC模块退出");
    
    /* 清理CM模块 */
    cm_cleanup();
}

/* CIC模块初始化函数 */
int cic_init(char *conffile)
{
    MAGIC_LOG(FD_LOG_NOTICE, "MAGIC CIC模块初始化");
    
    /* 调用入口函数进行初始化 */
    return magic_cic_entry(conffile);
}

/* CIC模块初始化和清理函数已在头文件中声明 */