#ifndef MAGIC_CONFIG_H
#define MAGIC_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libconfig.h>

// 配置文件路径
#define DEFAULT_CONFIG_FILE "/etc/magic_client.conf"
#define USER_CONFIG_FILE "~/.magic_client.conf"
#define LOCAL_CONFIG_FILE "./magic_client.conf"

// 配置项最大长度
#define MAX_CONFIG_STRING_LEN 256
#define MAX_CONFIG_PATH_LEN 512
#define MAX_CONFIG_ARRAY_SIZE 10

// 服务端配置
typedef struct {
    char hostname[MAX_CONFIG_STRING_LEN];
    int port;
    bool use_tls;
    char cert_file[MAX_CONFIG_PATH_LEN];
    char key_file[MAX_CONFIG_PATH_LEN];
    char ca_file[MAX_CONFIG_PATH_LEN];
    int connect_timeout;
    int response_timeout;
    int max_retries;
} server_config_t;

// 认证配置
typedef struct {
    char client_id[MAX_CONFIG_STRING_LEN];
    char client_secret[MAX_CONFIG_STRING_LEN];
    char username[MAX_CONFIG_STRING_LEN];
    char password[MAX_CONFIG_STRING_LEN];
    char realm[MAX_CONFIG_STRING_LEN];
    int auth_timeout;
    bool auto_reconnect;
    int reconnect_interval;
} auth_config_t;

// 网络配置
typedef struct {
    char preferred_interface[MAX_CONFIG_STRING_LEN];
    bool auto_select_interface;
    bool backup_original_config;
    bool restore_on_exit;
    int network_test_timeout;
    char test_hosts[MAX_CONFIG_ARRAY_SIZE][MAX_CONFIG_STRING_LEN];
    int test_host_count;
    int bandwidth_limit;  // KB/s, 0表示无限制
} network_config_ext_t;

// 代理配置
typedef struct {
    bool enable_proxy;
    int proxy_port;
    char bind_address[MAX_CONFIG_STRING_LEN];
    int max_connections;
    int connection_timeout;
    bool log_requests;
    char allowed_hosts[MAX_CONFIG_ARRAY_SIZE][MAX_CONFIG_STRING_LEN];
    int allowed_host_count;
    char blocked_hosts[MAX_CONFIG_ARRAY_SIZE][MAX_CONFIG_STRING_LEN];
    int blocked_host_count;
} proxy_config_t;

// 日志配置
typedef struct {
    char log_level[16];  // DEBUG, INFO, WARN, ERROR
    char log_file[MAX_CONFIG_PATH_LEN];
    bool log_to_console;
    bool log_to_file;
    bool log_to_syslog;
    int max_log_size;  // MB
    int max_log_files;
    bool rotate_logs;
} log_config_t;

// 监控配置
typedef struct {
    bool enable_monitoring;
    int stats_interval;  // 秒
    char stats_file[MAX_CONFIG_PATH_LEN];
    bool enable_heartbeat;
    int heartbeat_interval;  // 秒
    int heartbeat_timeout;   // 秒
    bool enable_bandwidth_monitor;
    int bandwidth_check_interval;  // 秒
} monitor_config_t;

// 安全配置
typedef struct {
    bool verify_server_cert;
    bool allow_self_signed;
    char trusted_ca_dir[MAX_CONFIG_PATH_LEN];
    bool enable_encryption;
    char encryption_algorithm[32];
    bool enable_compression;
    int max_session_time;  // 秒
    bool auto_logout_on_idle;
    int idle_timeout;  // 秒
} security_config_t;

// 完整的客户端配置
typedef struct {
    server_config_t server;
    auth_config_t auth;
    network_config_ext_t network;
    proxy_config_t proxy;
    log_config_t log;
    monitor_config_t monitor;
    security_config_t security;
    
    // 配置文件信息
    char config_file_path[MAX_CONFIG_PATH_LEN];
    time_t last_modified;
    bool is_loaded;
} magic_client_config_t;

// 配置管理函数
int magic_config_init(magic_client_config_t *config);
void magic_config_cleanup(magic_client_config_t *config);

// 配置文件操作
int magic_config_load(magic_client_config_t *config, const char *config_file);
int magic_config_save(const magic_client_config_t *config, const char *config_file);
int magic_config_reload(magic_client_config_t *config);
bool magic_config_is_modified(const magic_client_config_t *config);

// 配置查找和加载
char* magic_config_find_config_file(void);
int magic_config_load_default(magic_client_config_t *config);
int magic_config_load_from_args(magic_client_config_t *config, int argc, char *argv[]);

// 配置验证
int magic_config_validate(const magic_client_config_t *config);
int magic_config_validate_server(const server_config_t *server);
int magic_config_validate_auth(const auth_config_t *auth);
int magic_config_validate_network(const network_config_ext_t *network);
int magic_config_validate_proxy(const proxy_config_t *proxy);
int magic_config_validate_log(const log_config_t *log);

// 默认配置
void magic_config_set_defaults(magic_client_config_t *config);
void magic_config_set_server_defaults(server_config_t *server);
void magic_config_set_auth_defaults(auth_config_t *auth);
void magic_config_set_network_defaults(network_config_ext_t *network);
void magic_config_set_proxy_defaults(proxy_config_t *proxy);
void magic_config_set_log_defaults(log_config_t *log);
void magic_config_set_monitor_defaults(monitor_config_t *monitor);
void magic_config_set_security_defaults(security_config_t *security);

// 配置项获取和设置
const char* magic_config_get_string(const magic_client_config_t *config, 
                                   const char *section, 
                                   const char *key);
int magic_config_get_int(const magic_client_config_t *config, 
                        const char *section, 
                        const char *key);
bool magic_config_get_bool(const magic_client_config_t *config, 
                          const char *section, 
                          const char *key);

int magic_config_set_string(magic_client_config_t *config, 
                           const char *section, 
                           const char *key, 
                           const char *value);
int magic_config_set_int(magic_client_config_t *config, 
                        const char *section, 
                        const char *key, 
                        int value);
int magic_config_set_bool(magic_client_config_t *config, 
                         const char *section, 
                         const char *key, 
                         bool value);

// 配置合并和覆盖
int magic_config_merge(magic_client_config_t *dest, const magic_client_config_t *src);
int magic_config_override_from_env(magic_client_config_t *config);
int magic_config_override_from_args(magic_client_config_t *config, int argc, char *argv[]);

// 配置导出和导入
int magic_config_export_to_json(const magic_client_config_t *config, const char *json_file);
int magic_config_import_from_json(magic_client_config_t *config, const char *json_file);
int magic_config_export_to_xml(const magic_client_config_t *config, const char *xml_file);
int magic_config_import_from_xml(magic_client_config_t *config, const char *xml_file);

// 配置打印和调试
void magic_config_print(const magic_client_config_t *config);
void magic_config_print_section(const magic_client_config_t *config, const char *section);
void magic_config_dump_to_file(const magic_client_config_t *config, const char *dump_file);

// 配置监控和通知
typedef void (*config_change_callback_t)(const magic_client_config_t *config, 
                                        const char *section, 
                                        const char *key, 
                                        const char *old_value, 
                                        const char *new_value);

int magic_config_watch_file(magic_client_config_t *config, config_change_callback_t callback);
void magic_config_stop_watching(magic_client_config_t *config);

// 配置模板和示例
int magic_config_create_template(const char *template_file);
int magic_config_create_example(const char *example_file);

// 错误处理
const char* magic_config_get_error_string(int error_code);
void magic_config_log_error(const char *function, int error_code, const char *message);

// 工具函数
bool magic_config_file_exists(const char *file_path);
bool magic_config_is_readable(const char *file_path);
bool magic_config_is_writable(const char *file_path);
char* magic_config_expand_path(const char *path);
int magic_config_backup_file(const char *file_path);
int magic_config_restore_backup(const char *file_path);

// 配置加密和解密（用于敏感信息）
int magic_config_encrypt_string(const char *plaintext, char *encrypted, size_t encrypted_size);
int magic_config_decrypt_string(const char *encrypted, char *plaintext, size_t plaintext_size);
bool magic_config_is_encrypted(const char *value);

#endif // MAGIC_CONFIG_H