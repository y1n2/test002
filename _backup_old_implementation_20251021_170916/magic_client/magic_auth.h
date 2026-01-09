#ifndef MAGIC_AUTH_H
#define MAGIC_AUTH_H

#include "magic_client.h"

// ARINC-839标准消息类型
#define DIAMETER_MSG_MCAR 100000  // Mobile Client Authentication Request
#define DIAMETER_MSG_MCAA 100000  // Mobile Client Authentication Answer
#define DIAMETER_MSG_MADR 100005  // Mobile Application Data Request
#define DIAMETER_MSG_MADA 100005  // Mobile Application Data Answer
#define DIAMETER_MSG_LSR  100015  // Link Selection Request
#define DIAMETER_MSG_LSA  100015  // Link Selection Answer
#define DIAMETER_MSG_EUR  100020  // Environment Update Request
#define DIAMETER_MSG_EUA  100020  // Environment Update Answer

// ARINC-839标准AVP代码
#define AVP_CLIENT_CREDENTIAL 1000019    // Client-Credential
#define AVP_MCAR_MESSAGE_ID 1001         // MCAR-Message-ID
#define AVP_MCAR_MESSAGE_TYPE 1002       // MCAR-Message-Type
#define AVP_MCAR_MESSAGE_CONTENT 1003    // MCAR-Message-Content
#define AVP_CDR_ID 100046                // CDR-Id
#define AVP_BEARER_IDENTIFIER 1000047    // BearerIdentifier
#define AVP_QOS_PARAMETERS 100048        // QoSParameters
#define AVP_ENVIRONMENT_STATE 100050     // Environment-State
#define AVP_LINK_TYPE 100051             // Link-Type
#define AVP_LINK_STATUS 100052           // Link-Status
#define AVP_SELECTED_LINK 100062         // Selected-Link
#define AVP_BACKUP_LINK 100063           // Backup-Link
#define AVP_SESSION_ID 263               // 标准Diameter Session-Id AVP
#define AVP_RESULT_CODE 268              // 标准Diameter Result-Code AVP
#define AVP_ERROR_MESSAGE 281            // 标准Diameter Error-Message AVP

// ARINC-839应用ID
#define ARINC839_APPLICATION_ID 100000

// 结果代码
#define RESULT_CODE_SUCCESS 2001
#define RESULT_CODE_AUTHENTICATION_FAILED 4001  // 认证失败
#define RESULT_CODE_AUTHORIZATION_FAILED 4002   // 授权失败
#define RESULT_CODE_INVALID_CREDENTIALS 4003    // 无效凭据
#define RESULT_CODE_SERVICE_UNAVAILABLE 4004    // 服务不可用
#define RESULT_CODE_INSUFFICIENT_RESOURCES 4005 // 资源不足
#define RESULT_CODE_INVALID_REQUEST 4006        // 无效请求

// 兼容性别名
#define RESULT_CODE_AUTH_FAILED RESULT_CODE_AUTHENTICATION_FAILED

// Diameter消息头结构
typedef struct {
    unsigned char version;
    unsigned int length;
    unsigned char flags;
    unsigned int command_code;
    unsigned int application_id;
    unsigned int hop_by_hop_id;
    unsigned int end_to_end_id;
} diameter_header_t;

// Diameter AVP结构
typedef struct {
    unsigned int code;
    unsigned char flags;
    unsigned int length;
    unsigned int vendor_id;
    char data[];
} diameter_avp_t;

// 认证请求结构
typedef struct {
    char client_id[MAX_CLIENT_ID_LEN];
    char password[MAX_PASSWORD_LEN];
    service_type_t service_type;
    priority_level_t priority;
    char session_id[64];
} auth_request_t;

// 认证响应结构
typedef struct {
    unsigned int result_code;
    char session_id[64];
    network_config_t network_config;
    int session_timeout;
    char error_message[256];
} auth_response_t;

// 认证模块函数声明
int magic_auth_init(void);
void magic_auth_cleanup(void);

// Diameter消息处理
int magic_auth_create_lsr_message(const auth_request_t *request, char **message, size_t *message_len);
int magic_auth_parse_lsa_message(const char *message, size_t message_len, auth_response_t *response);
int magic_auth_create_eur_message(const char *session_id, char **message, size_t *message_len);
int magic_auth_parse_eua_message(const char *message, size_t message_len, auth_response_t *response);

// 认证流程函数
int magic_auth_send_request(magic_client_t *client, const auth_request_t *request);
int magic_auth_receive_response(magic_client_t *client, auth_response_t *response);
int magic_auth_perform_authentication(magic_client_t *client);
int magic_auth_request_emergency_access(magic_client_t *client);

// 会话管理
int magic_auth_generate_session_id(char *session_id, size_t len);
bool magic_auth_is_session_valid(const auth_info_t *auth);
int magic_auth_refresh_session(magic_client_t *client);

// Diameter消息工具函数
int diameter_create_header(diameter_header_t *header, unsigned int command_code, 
                          unsigned int app_id, unsigned int hop_id, unsigned int end_id);
int diameter_add_avp_string(char **message, size_t *offset, size_t *capacity, 
                           unsigned int avp_code, const char *value);
int diameter_add_avp_uint32(char **message, size_t *offset, size_t *capacity, 
                           unsigned int avp_code, unsigned int value);
int diameter_get_avp_string(const char *message, size_t message_len, 
                           unsigned int avp_code, char *value, size_t value_len);
int diameter_get_avp_uint32(const char *message, size_t message_len, 
                           unsigned int avp_code, unsigned int *value);

// 错误处理
const char* magic_auth_get_error_string(unsigned int result_code);
void magic_auth_log_error(const char *function, int error_code, const char *message);

#endif // MAGIC_AUTH_H