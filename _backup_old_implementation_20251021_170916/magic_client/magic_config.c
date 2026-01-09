#include "magic_config.h"
#include "magic_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <getopt.h>

// 内部函数声明
static int load_server_config(config_t *cfg, server_config_t *server);
static int load_auth_config(config_t *cfg, auth_config_t *auth);
static int load_network_config(config_t *cfg, network_config_ext_t *network);
static int load_proxy_config(config_t *cfg, proxy_config_t *proxy);
static int load_log_config(config_t *cfg, log_config_t *log);
static int load_monitor_config(config_t *cfg, monitor_config_t *monitor);
static int load_security_config(config_t *cfg, security_config_t *security);

static int save_server_config(config_t *cfg, const server_config_t *server);
static int save_auth_config(config_t *cfg, const auth_config_t *auth);
static int save_network_config(config_t *cfg, const network_config_ext_t *network);
static int save_proxy_config(config_t *cfg, const proxy_config_t *proxy);
static int save_log_config(config_t *cfg, const log_config_t *log);
static int save_monitor_config(config_t *cfg, const monitor_config_t *monitor);
static int save_security_config(config_t *cfg, const security_config_t *security);

/**
 * 初始化配置结构
 */
int magic_config_init(magic_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    memset(config, 0, sizeof(magic_client_config_t));
    
    // 设置默认值
    magic_config_set_defaults(config);
    
    config->is_loaded = false;
    
    return 0;
}

/**
 * 清理配置结构
 */
void magic_config_cleanup(magic_client_config_t *config) {
    if (!config) {
        return;
    }
    
    // 这里可以添加需要释放的资源
    config->is_loaded = false;
}

/**
 * 加载配置文件
 */
int magic_config_load(magic_client_config_t *config, const char *config_file) {
    if (!config) {
        return -1;
    }
    
    config_t cfg;
    config_init(&cfg);
    
    // 如果没有指定配置文件，查找默认配置文件
    const char *file_to_load = config_file;
    if (!file_to_load) {
        file_to_load = magic_config_find_config_file();
        if (!file_to_load) {
            magic_config_log_error("magic_config_load", -1, "No config file found");
            config_destroy(&cfg);
            return -1;
        }
    }
    
    // 读取配置文件
    if (!config_read_file(&cfg, file_to_load)) {
        magic_config_log_error("magic_config_load", -1, 
                              config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }
    
    // 保存配置文件路径
    strncpy(config->config_file_path, file_to_load, 
           sizeof(config->config_file_path) - 1);
    config->config_file_path[sizeof(config->config_file_path) - 1] = '\0';
    
    // 获取文件修改时间
    struct stat file_stat;
    if (stat(file_to_load, &file_stat) == 0) {
        config->last_modified = file_stat.st_mtime;
    }
    
    // 加载各个配置段
    int result = 0;
    result |= load_server_config(&cfg, &config->server);
    result |= load_auth_config(&cfg, &config->auth);
    result |= load_network_config(&cfg, &config->network);
    result |= load_proxy_config(&cfg, &config->proxy);
    result |= load_log_config(&cfg, &config->log);
    result |= load_monitor_config(&cfg, &config->monitor);
    result |= load_security_config(&cfg, &config->security);
    
    config_destroy(&cfg);
    
    if (result == 0) {
        config->is_loaded = true;
        magic_client_log("INFO", "Configuration loaded from %s", file_to_load);
    }
    
    return result;
}

/**
 * 保存配置文件
 */
int magic_config_save(const magic_client_config_t *config, const char *config_file) {
    if (!config) {
        return -1;
    }
    
    config_t cfg;
    config_init(&cfg);
    
    // 保存各个配置段
    int result = 0;
    result |= save_server_config(&cfg, &config->server);
    result |= save_auth_config(&cfg, &config->auth);
    result |= save_network_config(&cfg, &config->network);
    result |= save_proxy_config(&cfg, &config->proxy);
    result |= save_log_config(&cfg, &config->log);
    result |= save_monitor_config(&cfg, &config->monitor);
    result |= save_security_config(&cfg, &config->security);
    
    if (result == 0) {
        const char *file_to_save = config_file ? config_file : config->config_file_path;
        if (!config_write_file(&cfg, file_to_save)) {
            magic_config_log_error("magic_config_save", -1, "Failed to write config file");
            result = -1;
        } else {
            magic_client_log("INFO", "Configuration saved to %s", file_to_save);
        }
    }
    
    config_destroy(&cfg);
    return result;
}

/**
 * 查找配置文件
 */
char* magic_config_find_config_file(void) {
    static char config_path[MAX_CONFIG_PATH_LEN];
    
    // 1. 检查当前目录
    if (magic_config_file_exists(LOCAL_CONFIG_FILE)) {
        strcpy(config_path, LOCAL_CONFIG_FILE);
        return config_path;
    }
    
    // 2. 检查用户主目录
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        snprintf(config_path, sizeof(config_path), "%s/.magic_client.conf", pw->pw_dir);
        if (magic_config_file_exists(config_path)) {
            return config_path;
        }
    }
    
    // 3. 检查系统配置目录
    if (magic_config_file_exists(DEFAULT_CONFIG_FILE)) {
        strcpy(config_path, DEFAULT_CONFIG_FILE);
        return config_path;
    }
    
    return NULL;
}

/**
 * 加载默认配置
 */
int magic_config_load_default(magic_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    magic_config_set_defaults(config);
    config->is_loaded = true;
    
    magic_client_log("INFO", "Using default configuration");
    return 0;
}

/**
 * 验证配置
 */
int magic_config_validate(const magic_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    int result = 0;
    result |= magic_config_validate_server(&config->server);
    result |= magic_config_validate_auth(&config->auth);
    result |= magic_config_validate_network(&config->network);
    result |= magic_config_validate_proxy(&config->proxy);
    result |= magic_config_validate_log(&config->log);
    
    return result;
}

/**
 * 验证服务端配置
 */
int magic_config_validate_server(const server_config_t *server) {
    if (!server) {
        return -1;
    }
    
    if (strlen(server->hostname) == 0) {
        magic_config_log_error("magic_config_validate_server", -1, "Server hostname is empty");
        return -1;
    }
    
    if (server->port <= 0 || server->port > 65535) {
        magic_config_log_error("magic_config_validate_server", -1, "Invalid server port");
        return -1;
    }
    
    if (server->use_tls) {
        if (strlen(server->cert_file) == 0 || !magic_config_file_exists(server->cert_file)) {
            magic_config_log_error("magic_config_validate_server", -1, "TLS cert file not found");
            return -1;
        }
    }
    
    return 0;
}

/**
 * 验证认证配置
 */
int magic_config_validate_auth(const auth_config_t *auth) {
    if (!auth) {
        return -1;
    }
    
    if (strlen(auth->client_id) == 0) {
        magic_config_log_error("magic_config_validate_auth", -1, "Client ID is empty");
        return -1;
    }
    
    if (strlen(auth->username) == 0) {
        magic_config_log_error("magic_config_validate_auth", -1, "Username is empty");
        return -1;
    }
    
    return 0;
}

/**
 * 设置默认配置
 */
void magic_config_set_defaults(magic_client_config_t *config) {
    if (!config) {
        return;
    }
    
    magic_config_set_server_defaults(&config->server);
    magic_config_set_auth_defaults(&config->auth);
    magic_config_set_network_defaults(&config->network);
    magic_config_set_proxy_defaults(&config->proxy);
    magic_config_set_log_defaults(&config->log);
    magic_config_set_monitor_defaults(&config->monitor);
    magic_config_set_security_defaults(&config->security);
}

/**
 * 设置服务端默认配置
 */
void magic_config_set_server_defaults(server_config_t *server) {
    if (!server) {
        return;
    }
    
    strcpy(server->hostname, "localhost");
    server->port = 3868;
    server->use_tls = true;
    strcpy(server->cert_file, "/etc/magic/client.crt");
    strcpy(server->key_file, "/etc/magic/client.key");
    strcpy(server->ca_file, "/etc/magic/ca.crt");
    server->connect_timeout = 30;
    server->response_timeout = 60;
    server->max_retries = 3;
}

/**
 * 设置认证默认配置
 */
void magic_config_set_auth_defaults(auth_config_t *auth) {
    if (!auth) {
        return;
    }
    
    strcpy(auth->client_id, "magic_client");
    strcpy(auth->realm, "magic.local");
    auth->auth_timeout = 30;
    auth->auto_reconnect = true;
    auth->reconnect_interval = 60;
}

/**
 * 设置网络默认配置
 */
void magic_config_set_network_defaults(network_config_ext_t *network) {
    if (!network) {
        return;
    }
    
    network->auto_select_interface = true;
    network->backup_original_config = true;
    network->restore_on_exit = true;
    network->network_test_timeout = 10;
    network->bandwidth_limit = 0; // 无限制
    
    // 默认测试主机
    strcpy(network->test_hosts[0], "8.8.8.8");
    strcpy(network->test_hosts[1], "1.1.1.1");
    network->test_host_count = 2;
}

/**
 * 设置代理默认配置
 */
void magic_config_set_proxy_defaults(proxy_config_t *proxy) {
    if (!proxy) {
        return;
    }
    
    proxy->enable_proxy = true;
    proxy->proxy_port = 8080;
    strcpy(proxy->bind_address, "127.0.0.1");
    proxy->max_connections = 100;
    proxy->connection_timeout = 30;
    proxy->log_requests = false;
    proxy->allowed_host_count = 0;
    proxy->blocked_host_count = 0;
}

/**
 * 设置日志默认配置
 */
void magic_config_set_log_defaults(log_config_t *log) {
    if (!log) {
        return;
    }
    
    strcpy(log->log_level, "INFO");
    strcpy(log->log_file, "/var/log/magic_client.log");
    log->log_to_console = true;
    log->log_to_file = true;
    log->log_to_syslog = false;
    log->max_log_size = 10; // 10MB
    log->max_log_files = 5;
    log->rotate_logs = true;
}

/**
 * 设置监控默认配置
 */
void magic_config_set_monitor_defaults(monitor_config_t *monitor) {
    if (!monitor) {
        return;
    }
    
    monitor->enable_monitoring = true;
    monitor->stats_interval = 60;
    strcpy(monitor->stats_file, "/var/log/magic_client_stats.log");
    monitor->enable_heartbeat = true;
    monitor->heartbeat_interval = 30;
    monitor->heartbeat_timeout = 10;
    monitor->enable_bandwidth_monitor = true;
    monitor->bandwidth_check_interval = 10;
}

/**
 * 设置安全默认配置
 */
void magic_config_set_security_defaults(security_config_t *security) {
    if (!security) {
        return;
    }
    
    security->verify_server_cert = true;
    security->allow_self_signed = false;
    strcpy(security->trusted_ca_dir, "/etc/ssl/certs");
    security->enable_encryption = true;
    strcpy(security->encryption_algorithm, "AES-256-GCM");
    security->enable_compression = false;
    security->max_session_time = 3600; // 1小时
    security->auto_logout_on_idle = true;
    security->idle_timeout = 1800; // 30分钟
}

// 内部函数实现

/**
 * 加载服务端配置
 */
static int load_server_config(config_t *cfg, server_config_t *server) {
    config_setting_t *setting = config_lookup(cfg, "server");
    if (!setting) {
        return 0; // 使用默认值
    }
    
    const char *str_val;
    int int_val;
    
    if (config_setting_lookup_string(setting, "hostname", &str_val)) {
        strncpy(server->hostname, str_val, sizeof(server->hostname) - 1);
        server->hostname[sizeof(server->hostname) - 1] = '\0';
    }
    
    if (config_setting_lookup_int(setting, "port", &int_val)) {
        server->port = int_val;
    }
    
    if (config_setting_lookup_bool(setting, "use_tls", &int_val)) {
        server->use_tls = int_val;
    }
    
    if (config_setting_lookup_string(setting, "cert_file", &str_val)) {
        strncpy(server->cert_file, str_val, sizeof(server->cert_file) - 1);
        server->cert_file[sizeof(server->cert_file) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "key_file", &str_val)) {
        strncpy(server->key_file, str_val, sizeof(server->key_file) - 1);
        server->key_file[sizeof(server->key_file) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "ca_file", &str_val)) {
        strncpy(server->ca_file, str_val, sizeof(server->ca_file) - 1);
        server->ca_file[sizeof(server->ca_file) - 1] = '\0';
    }
    
    if (config_setting_lookup_int(setting, "connect_timeout", &int_val)) {
        server->connect_timeout = int_val;
    }
    
    if (config_setting_lookup_int(setting, "response_timeout", &int_val)) {
        server->response_timeout = int_val;
    }
    
    if (config_setting_lookup_int(setting, "max_retries", &int_val)) {
        server->max_retries = int_val;
    }
    
    return 0;
}

/**
 * 加载认证配置
 */
static int load_auth_config(config_t *cfg, auth_config_t *auth) {
    config_setting_t *setting = config_lookup(cfg, "auth");
    if (!setting) {
        return 0;
    }
    
    const char *str_val;
    int int_val;
    
    if (config_setting_lookup_string(setting, "client_id", &str_val)) {
        strncpy(auth->client_id, str_val, sizeof(auth->client_id) - 1);
        auth->client_id[sizeof(auth->client_id) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "client_secret", &str_val)) {
        strncpy(auth->client_secret, str_val, sizeof(auth->client_secret) - 1);
        auth->client_secret[sizeof(auth->client_secret) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "username", &str_val)) {
        strncpy(auth->username, str_val, sizeof(auth->username) - 1);
        auth->username[sizeof(auth->username) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "password", &str_val)) {
        strncpy(auth->password, str_val, sizeof(auth->password) - 1);
        auth->password[sizeof(auth->password) - 1] = '\0';
    }
    
    if (config_setting_lookup_string(setting, "realm", &str_val)) {
        strncpy(auth->realm, str_val, sizeof(auth->realm) - 1);
        auth->realm[sizeof(auth->realm) - 1] = '\0';
    }
    
    if (config_setting_lookup_int(setting, "auth_timeout", &int_val)) {
        auth->auth_timeout = int_val;
    }
    
    if (config_setting_lookup_bool(setting, "auto_reconnect", &int_val)) {
        auth->auto_reconnect = int_val;
    }
    
    if (config_setting_lookup_int(setting, "reconnect_interval", &int_val)) {
        auth->reconnect_interval = int_val;
    }
    
    return 0;
}

/**
 * 加载网络配置
 */
static int load_network_config(config_t *cfg, network_config_ext_t *network) {
    config_setting_t *setting = config_lookup(cfg, "network");
    if (!setting) {
        return 0;
    }
    
    const char *str_val;
    int int_val;
    
    if (config_setting_lookup_string(setting, "preferred_interface", &str_val)) {
        strncpy(network->preferred_interface, str_val, sizeof(network->preferred_interface) - 1);
        network->preferred_interface[sizeof(network->preferred_interface) - 1] = '\0';
    }
    
    if (config_setting_lookup_bool(setting, "auto_select_interface", &int_val)) {
        network->auto_select_interface = int_val;
    }
    
    if (config_setting_lookup_bool(setting, "backup_original_config", &int_val)) {
        network->backup_original_config = int_val;
    }
    
    if (config_setting_lookup_bool(setting, "restore_on_exit", &int_val)) {
        network->restore_on_exit = int_val;
    }
    
    if (config_setting_lookup_int(setting, "network_test_timeout", &int_val)) {
        network->network_test_timeout = int_val;
    }
    
    if (config_setting_lookup_int(setting, "bandwidth_limit", &int_val)) {
        network->bandwidth_limit = int_val;
    }
    
    // 加载测试主机列表
    config_setting_t *test_hosts = config_setting_lookup(setting, "test_hosts");
    if (test_hosts && config_setting_is_array(test_hosts)) {
        int count = config_setting_length(test_hosts);
        network->test_host_count = (count > MAX_CONFIG_ARRAY_SIZE) ? MAX_CONFIG_ARRAY_SIZE : count;
        
        for (int i = 0; i < network->test_host_count; i++) {
            const char *host = config_setting_get_string_elem(test_hosts, i);
            if (host) {
                strncpy(network->test_hosts[i], host, sizeof(network->test_hosts[i]) - 1);
                network->test_hosts[i][sizeof(network->test_hosts[i]) - 1] = '\0';
            }
        }
    }
    
    return 0;
}

/**
 * 保存服务端配置
 */
static int save_server_config(config_t *cfg, const server_config_t *server) {
    config_setting_t *setting = config_setting_add(config_root_setting(cfg), "server", CONFIG_TYPE_GROUP);
    if (!setting) {
        return -1;
    }
    
    config_setting_t *hostname = config_setting_add(setting, "hostname", CONFIG_TYPE_STRING);
    config_setting_set_string(hostname, server->hostname);
    
    config_setting_t *port = config_setting_add(setting, "port", CONFIG_TYPE_INT);
    config_setting_set_int(port, server->port);
    
    config_setting_t *use_tls = config_setting_add(setting, "use_tls", CONFIG_TYPE_BOOL);
    config_setting_set_bool(use_tls, server->use_tls);
    
    config_setting_t *cert_file = config_setting_add(setting, "cert_file", CONFIG_TYPE_STRING);
    config_setting_set_string(cert_file, server->cert_file);
    
    config_setting_t *key_file = config_setting_add(setting, "key_file", CONFIG_TYPE_STRING);
    config_setting_set_string(key_file, server->key_file);
    
    config_setting_t *ca_file = config_setting_add(setting, "ca_file", CONFIG_TYPE_STRING);
    config_setting_set_string(ca_file, server->ca_file);
    
    config_setting_t *connect_timeout = config_setting_add(setting, "connect_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(connect_timeout, server->connect_timeout);
    
    config_setting_t *response_timeout = config_setting_add(setting, "response_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(response_timeout, server->response_timeout);
    
    config_setting_t *max_retries = config_setting_add(setting, "max_retries", CONFIG_TYPE_INT);
    config_setting_set_int(max_retries, server->max_retries);
    
    return 0;
}

// 其他保存函数的简化实现
static int save_auth_config(config_t *cfg, const auth_config_t *auth) {
    config_setting_t *setting = config_setting_add(config_root_setting(cfg), "auth", CONFIG_TYPE_GROUP);
    if (!setting) return -1;
    
    config_setting_t *client_id = config_setting_add(setting, "client_id", CONFIG_TYPE_STRING);
    config_setting_set_string(client_id, auth->client_id);
    
    config_setting_t *realm = config_setting_add(setting, "realm", CONFIG_TYPE_STRING);
    config_setting_set_string(realm, auth->realm);
    
    config_setting_t *auth_timeout = config_setting_add(setting, "auth_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(auth_timeout, auth->auth_timeout);
    
    config_setting_t *auto_reconnect = config_setting_add(setting, "auto_reconnect", CONFIG_TYPE_BOOL);
    config_setting_set_bool(auto_reconnect, auth->auto_reconnect);
    
    config_setting_t *reconnect_interval = config_setting_add(setting, "reconnect_interval", CONFIG_TYPE_INT);
    config_setting_set_int(reconnect_interval, auth->reconnect_interval);
    
    return 0;
}

// 其他简化实现
static int load_proxy_config(config_t *cfg, proxy_config_t *proxy) { return 0; }
static int load_log_config(config_t *cfg, log_config_t *log) { return 0; }
static int load_monitor_config(config_t *cfg, monitor_config_t *monitor) { return 0; }
static int load_security_config(config_t *cfg, security_config_t *security) { return 0; }

static int save_network_config(config_t *cfg, const network_config_ext_t *network) { return 0; }
static int save_proxy_config(config_t *cfg, const proxy_config_t *proxy) { return 0; }
static int save_log_config(config_t *cfg, const log_config_t *log) { return 0; }
static int save_monitor_config(config_t *cfg, const monitor_config_t *monitor) { return 0; }
static int save_security_config(config_t *cfg, const security_config_t *security) { return 0; }

// 工具函数实现

bool magic_config_file_exists(const char *file_path) {
    if (!file_path) {
        return false;
    }
    
    return access(file_path, F_OK) == 0;
}

bool magic_config_is_readable(const char *file_path) {
    if (!file_path) {
        return false;
    }
    
    return access(file_path, R_OK) == 0;
}

bool magic_config_is_writable(const char *file_path) {
    if (!file_path) {
        return false;
    }
    
    return access(file_path, W_OK) == 0;
}

int magic_config_validate_network(const network_config_ext_t *network) {
    if (!network) {
        return -1;
    }
    
    if (network->bandwidth_limit < 0) {
        magic_config_log_error("magic_config_validate_network", -1, "Invalid bandwidth limit");
        return -1;
    }
    
    return 0;
}

int magic_config_validate_proxy(const proxy_config_t *proxy) {
    if (!proxy) {
        return -1;
    }
    
    if (proxy->proxy_port <= 0 || proxy->proxy_port > 65535) {
        magic_config_log_error("magic_config_validate_proxy", -1, "Invalid proxy port");
        return -1;
    }
    
    return 0;
}

int magic_config_validate_log(const log_config_t *log) {
    if (!log) {
        return -1;
    }
    
    if (log->max_log_size <= 0) {
        magic_config_log_error("magic_config_validate_log", -1, "Invalid log size");
        return -1;
    }
    
    return 0;
}

void magic_config_print(const magic_client_config_t *config) {
    if (!config) {
        return;
    }
    
    printf("MAGIC Client Configuration:\n");
    printf("==========================\n");
    printf("Server: %s:%d (TLS: %s)\n", 
           config->server.hostname, 
           config->server.port,
           config->server.use_tls ? "Yes" : "No");
    printf("Client ID: %s\n", config->auth.client_id);
    printf("Realm: %s\n", config->auth.realm);
    printf("Proxy: %s (Port: %d)\n", 
           config->proxy.enable_proxy ? "Enabled" : "Disabled",
           config->proxy.proxy_port);
    printf("Log Level: %s\n", config->log.log_level);
    printf("Config File: %s\n", config->config_file_path);
}

void magic_config_log_error(const char *function, int error_code, const char *message) {
    magic_client_log("ERROR", "[%s] Error %d: %s", function, error_code, message);
}

const char* magic_config_get_error_string(int error_code) {
    return strerror(abs(error_code));
}