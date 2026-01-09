#include "magic_auth.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <freeDiameter/libfdproto.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 全局变量
static bool auth_initialized = false;
static struct dict_object *dict_app_arinc839 = NULL;
static struct dict_object *dict_cmd_mcar = NULL;
static struct dict_object *dict_cmd_mcaa = NULL;
static struct dict_object *dict_avp_client_credential = NULL;
static struct dict_object *dict_avp_mcar_message_id = NULL;
static struct dict_object *dict_avp_mcar_message_type = NULL;
static struct dict_object *dict_avp_mcar_message_content = NULL;
static struct dict_object *dict_avp_result_code = NULL;
static struct dict_object *dict_avp_session_id = NULL;
static struct dict_object *dict_avp_origin_host = NULL;
static struct dict_object *dict_avp_origin_realm = NULL;
static struct dict_object *dict_avp_destination_realm = NULL;

/**
 * 初始化认证模块
 */
int magic_auth_init(void) {
    if (auth_initialized) {
        return 0;
    }
    
    // 初始化freeDiameter库
    if (fd_libproto_init() != 0) {
        return -1;
    }
    
    // 检查全局配置是否可用
    if (!fd_g_config || !fd_g_config->cnf_dict) {
        // 创建基本的字典配置
        struct fd_config *config = calloc(1, sizeof(struct fd_config));
        if (!config) {
            return -1;
        }
        
        // 初始化字典
        if (fd_dict_init(&config->cnf_dict) != 0) {
            free(config);
            return -1;
        }
        
        // 加载基础字典
        if (fd_dict_base_protocol(config->cnf_dict) != 0) {
            fd_dict_fini(&config->cnf_dict);
            free(config);
            return -1;
        }
        
        fd_g_config = config;
    }
    
    // 定义ARINC-839应用
    {
        struct dict_application_data data = { ARINC839_APPLICATION_ID, "ARINC-839" };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_APPLICATION, &data, NULL, &dict_app_arinc839));
    }
    
    // 定义自定义AVP
    {
        struct dict_avp_data data = { 
            AVP_CLIENT_CREDENTIAL, 
            0, 
            "Client-Credential", 
            AVP_FLAG_MANDATORY, 
            AVP_FLAG_MANDATORY,
            AVP_TYPE_OCTETSTRING 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_AVP, &data, NULL, &dict_avp_client_credential));
    }
    
    // 定义MCAR命令
    {
        struct dict_cmd_data data = { 
            DIAMETER_MSG_MCAR, 
            "MCAR", 
            CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE, 
            CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &data, dict_app_arinc839, &dict_cmd_mcar));
    }
    
    // 定义MCAA命令
    {
        struct dict_cmd_data data = { 
            DIAMETER_MSG_MCAA, 
            "MCAA", 
            CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE, 
            CMD_FLAG_PROXIABLE 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &data, dict_app_arinc839, &dict_cmd_mcaa));
    }
    
    // 定义其他需要的AVP
    {
        struct dict_avp_data data = { 
            AVP_MCAR_MESSAGE_ID, 
            0, 
            "MCAR-Message-ID", 
            AVP_FLAG_MANDATORY, 
            AVP_FLAG_MANDATORY,
            AVP_TYPE_OCTETSTRING 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_AVP, &data, NULL, &dict_avp_mcar_message_id));
    }
    
    {
        struct dict_avp_data data = { 
            AVP_MCAR_MESSAGE_TYPE, 
            0, 
            "MCAR-Message-Type", 
            AVP_FLAG_MANDATORY, 
            AVP_FLAG_MANDATORY,
            AVP_TYPE_UNSIGNED32 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_AVP, &data, NULL, &dict_avp_mcar_message_type));
    }
    
    {
        struct dict_avp_data data = { 
            AVP_MCAR_MESSAGE_CONTENT, 
            0, 
            "MCAR-Message-Content", 
            AVP_FLAG_MANDATORY, 
            AVP_FLAG_MANDATORY,
            AVP_TYPE_OCTETSTRING 
        };
        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_AVP, &data, NULL, &dict_avp_mcar_message_content));
    }
    
    // 查找标准AVP（这些应该已经在基础字典中）
    uint32_t avp_code = AC_RESULT_CODE;
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, 
                            AVP_BY_CODE, &avp_code, &dict_avp_result_code, ENOENT));
    
    avp_code = AC_SESSION_ID;
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, 
                            AVP_BY_CODE, &avp_code, &dict_avp_session_id, ENOENT));
    
    avp_code = AC_ORIGIN_HOST;
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, 
                            AVP_BY_CODE, &avp_code, &dict_avp_origin_host, ENOENT));
    
    avp_code = AC_ORIGIN_REALM;
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, 
                            AVP_BY_CODE, &avp_code, &dict_avp_origin_realm, ENOENT));
    
    avp_code = AC_DESTINATION_REALM;
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, 
                            AVP_BY_CODE, &avp_code, &dict_avp_destination_realm, ENOENT));
    
    auth_initialized = true;
    return 0;
}

/**
 * 清理认证模块
 */
void magic_auth_cleanup(void) {
    auth_initialized = false;
}

/**
 * 创建MCAR消息
 */
int magic_auth_create_mcar_message(const auth_request_t *request, struct msg **msg) {
    if (!request || !msg) {
        return -1;
    }
    
    struct avp *avp = NULL;
    union avp_value val;
    char session_id[128];
    
    // 创建MCAR消息
    CHECK_FCT(fd_msg_new(dict_cmd_mcar, MSGFL_ALLOC_ETEID, msg));
    
    // 生成会话ID
    if (magic_auth_generate_session_id(session_id, sizeof(session_id)) != 0) {
        fd_msg_free(*msg);
        return -1;
    }
    
    // 添加Session-Id AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_session_id, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)session_id;
    val.os.len = strlen(session_id);
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加Origin-Host AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_origin_host, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)request->client_id;
    val.os.len = strlen(request->client_id);
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加Origin-Realm AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_origin_realm, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)"magic.local";
    val.os.len = strlen("magic.local");
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加Destination-Realm AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_destination_realm, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)"arinc839.local";
    val.os.len = strlen("arinc839.local");
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加Client-Credential AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_client_credential, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)request->password;
    val.os.len = strlen(request->password);
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加MCAR-Message-ID AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_mcar_message_id, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.os.data = (uint8_t*)session_id;
    val.os.len = strlen(session_id);
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加MCAR-Message-Type AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_mcar_message_type, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    val.u32 = 1; // 认证请求类型
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    // 添加MCAR-Message-Content AVP
    CHECK_FCT_DO(fd_msg_avp_new(dict_avp_mcar_message_content, 0, &avp), goto error);
    memset(&val, 0, sizeof(val));
    char content[256];
    snprintf(content, sizeof(content), "client_id=%s;service_type=%u;priority=%u", 
             request->client_id, request->service_type, request->priority);
    val.os.data = (uint8_t*)content;
    val.os.len = strlen(content);
    CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &val), goto error);
    CHECK_FCT_DO(fd_msg_avp_add(*msg, MSG_BRW_LAST_CHILD, avp), goto error);
    avp = NULL;
    
    return 0;
    
error:
    if (avp) fd_msg_free(avp);
    if (*msg) fd_msg_free(*msg);
    *msg = NULL;
    return -1;
}

/**
 * 解析MCAA消息
 */
int magic_auth_parse_mcaa_message(struct msg *msg, auth_response_t *response) {
    if (!msg || !response) {
        return -1;
    }
    
    struct avp *avp = NULL;
    struct avp_hdr *hdr = NULL;
    
    memset(response, 0, sizeof(auth_response_t));
    
    // 提取Result-Code AVP
    CHECK_FCT(fd_msg_search_avp(msg, dict_avp_result_code, &avp));
    if (avp) {
        CHECK_FCT(fd_msg_avp_hdr(avp, &hdr));
        response->result_code = hdr->avp_value->u32;
    } else {
        response->result_code = RESULT_CODE_AUTHENTICATION_FAILED;
        return -1;
    }
    
    // 提取Session-Id AVP
    CHECK_FCT(fd_msg_search_avp(msg, dict_avp_session_id, &avp));
    if (avp) {
        CHECK_FCT(fd_msg_avp_hdr(avp, &hdr));
        size_t len = hdr->avp_value->os.len;
        if (len >= sizeof(response->session_id)) {
            len = sizeof(response->session_id) - 1;
        }
        memcpy(response->session_id, hdr->avp_value->os.data, len);
        response->session_id[len] = '\0';
    }
    
    // 如果认证成功，设置网络配置
    if (response->result_code == RESULT_CODE_SUCCESS) {
        response->network_config.is_configured = true;
        strcpy(response->network_config.assigned_ip, "192.168.1.100");
        strcpy(response->network_config.gateway, "192.168.1.1");
        strcpy(response->network_config.netmask, "255.255.255.0");
        strcpy(response->network_config.dns_primary, "8.8.8.8");
        strcpy(response->network_config.dns_secondary, "8.8.4.4");
        response->network_config.bandwidth_limit = 1000000; // 1Mbps
        response->session_timeout = 3600; // 1小时
    }
    
    return response->result_code == RESULT_CODE_SUCCESS ? 0 : -1;
}

/**
 * 发送认证请求
 */
int magic_auth_send_request(magic_client_t *client, const auth_request_t *request) {
    if (!client || !request) {
        return -1;
    }
    
    struct msg *msg = NULL;
    
    // 创建MCAR消息
    if (magic_auth_create_mcar_message(request, &msg) != 0) {
        return -1;
    }
    
    // 发送消息（这里需要实现实际的发送逻辑）
    // 在实际实现中，应该使用fd_msg_send或类似的freeDiameter API
    printf("发送MCAR认证请求消息\n");
    
    // 清理消息
    fd_msg_free(msg);
    
    client->stats.packets_sent++;
    return 0;
}

/**
 * 接收认证响应
 */
int magic_auth_receive_response(magic_client_t *client, auth_response_t *response) {
    if (!client || !response) {
        return -1;
    }
    
    // 这里应该实现实际的消息接收逻辑
    // 在实际实现中，应该从freeDiameter消息队列中接收MCAA消息
    printf("接收MCAA认证响应消息\n");
    
    // 模拟成功响应
    memset(response, 0, sizeof(auth_response_t));
    response->result_code = RESULT_CODE_SUCCESS;
    strcpy(response->session_id, "test-session-123");
    // 启用安全的网络配置用于链路模拟器连接
    response->network_config.is_configured = true;
    // 使用172.20.0.0/24子网避免与现有网络冲突
    strcpy(response->network_config.assigned_ip, "172.20.0.100");
    strcpy(response->network_config.gateway, "172.20.0.1");
    strcpy(response->network_config.netmask, "255.255.255.0");
    strcpy(response->network_config.dns_primary, "8.8.8.8");
    strcpy(response->network_config.dns_secondary, "8.8.4.4");
    response->network_config.bandwidth_limit = 0; // 无带宽限制
    response->session_timeout = 3600;
    
    client->stats.packets_received++;
    return 0;
}

/**
 * 执行认证流程
 */
int magic_auth_perform_authentication(magic_client_t *client) {
    if (!client) {
        return -1;
    }
    
    client->stats.auth_attempts++;
    
    // 准备认证请求
    auth_request_t request;
    memset(&request, 0, sizeof(request));
    
    strncpy(request.client_id, client->config.client_id, sizeof(request.client_id) - 1);
    strncpy(request.password, client->config.password, sizeof(request.password) - 1);
    request.service_type = client->config.service_type;
    request.priority = client->config.priority;
    
    // 生成会话ID
    if (magic_auth_generate_session_id(request.session_id, sizeof(request.session_id)) != 0) {
        return -1;
    }
    
    // 发送认证请求
    if (magic_auth_send_request(client, &request) != 0) {
        client->stats.auth_attempts++;
        return -1;
    }
    
    // 接收认证响应
    auth_response_t response;
    if (magic_auth_receive_response(client, &response) != 0) {
        client->stats.auth_attempts++;
        return -1;
    }
    
    // 检查认证结果
    if (response.result_code != RESULT_CODE_SUCCESS) {
        client->stats.auth_attempts++;
        return -1;
    }
    
    // 保存认证信息
    client->auth.is_authenticated = true;
    strncpy(client->auth.session_id, response.session_id, sizeof(client->auth.session_id) - 1);
    client->auth.auth_time = time(NULL);
    client->auth.expire_time = client->auth.auth_time + response.session_timeout;
    
    // 保存网络配置
    if (response.network_config.is_configured) {
        client->network = response.network_config;
    }
    
    // 认证成功，增加认证尝试计数
    client->stats.auth_attempts++;
    return 0;
}

/**
 * 生成会话ID
 */
int magic_auth_generate_session_id(char *session_id, size_t len) {
    if (!session_id || len < 37) {
        return -1;
    }
    
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, session_id);
    
    return 0;
}

/**
 * 检查会话是否有效
 */
bool magic_auth_is_session_valid(const auth_info_t *auth) {
    if (!auth || !auth->is_authenticated) {
        return false;
    }
    
    time_t current_time = time(NULL);
    return current_time < auth->expire_time;
}

/**
 * 刷新会话
 */
int magic_auth_refresh_session(magic_client_t *client) {
    if (!client || !client->auth.is_authenticated) {
        return -1;
    }
    
    // 重新执行认证流程
    return magic_auth_perform_authentication(client);
}

/**
 * 获取错误字符串
 */
const char* magic_auth_get_error_string(unsigned int result_code) {
    switch (result_code) {
        case RESULT_CODE_SUCCESS:
            return "认证成功";
        case RESULT_CODE_AUTHENTICATION_FAILED:
            return "认证失败";
        case RESULT_CODE_AUTHORIZATION_FAILED:
            return "授权失败";
        case RESULT_CODE_INVALID_CREDENTIALS:
            return "无效凭据";
        case RESULT_CODE_SERVICE_UNAVAILABLE:
            return "服务不可用";
        case RESULT_CODE_INSUFFICIENT_RESOURCES:
            return "资源不足";
        default:
            return "未知错误";
    }
}

/**
 * 记录错误日志
 */
void magic_auth_log_error(const char *function, int error_code, const char *message) {
    printf("ERROR [%s]: %s (code: %d)\n", function, message, error_code);
}