#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// 简化的网络配置结构
typedef struct {
    char assigned_ip[16];
    char netmask[16];
    char gateway[16];
    char dns_primary[16];
    char dns_secondary[16];
    int bandwidth_limit;
} test_network_config_t;

// 执行系统命令并返回结果
int execute_command_with_output(const char *command, char *output, size_t output_len) {
    printf("执行命令: %s\n", command);
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        printf("ERROR: 无法执行命令\n");
        return -1;
    }
    
    if (output && output_len > 0) {
        if (fgets(output, output_len, fp) == NULL) {
            output[0] = '\0';
        }
        // 移除换行符
        char *newline = strchr(output, '\n');
        if (newline) *newline = '\0';
    }
    
    int result = pclose(fp);
    int exit_code = WEXITSTATUS(result);
    
    if (exit_code == 0) {
        printf("SUCCESS: 命令执行成功\n");
        if (output && strlen(output) > 0) {
            printf("输出: %s\n", output);
        }
    } else {
        printf("ERROR: 命令执行失败，退出码: %d\n", exit_code);
    }
    
    return exit_code;
}

// 测试接口发现
int test_interface_discovery() {
    printf("\n=== 测试接口发现 ===\n");
    
    char output[1024];
    return execute_command_with_output("ip link show | grep -E '^[0-9]+:' | head -5", output, sizeof(output));
}

// 测试接口选择
int test_interface_selection(char *selected_interface, size_t interface_len) {
    printf("\n=== 测试接口选择 ===\n");
    
    char output[1024];
    int result = execute_command_with_output("ip link show | grep -E '^[0-9]+: (eth|ens|enp)' | head -1 | cut -d: -f2 | tr -d ' '", output, sizeof(output));
    
    if (result == 0 && strlen(output) > 0) {
        strncpy(selected_interface, output, interface_len - 1);
        selected_interface[interface_len - 1] = '\0';
        printf("选择的接口: %s\n", selected_interface);
    } else {
        // 如果没有找到以太网接口，使用lo接口进行测试
        strcpy(selected_interface, "lo");
        printf("使用回环接口进行测试: %s\n", selected_interface);
    }
    
    return 0;
}

// 测试IP地址设置
int test_ip_address_setting(const char *interface, const test_network_config_t *config) {
    printf("\n=== 测试IP地址设置 ===\n");
    
    char command[256];
    int result = 0;
    
    // 首先检查接口状态
    snprintf(command, sizeof(command), "ip link show %s", interface);
    result = execute_command_with_output(command, NULL, 0);
    if (result != 0) {
        printf("ERROR: 接口 %s 不存在\n", interface);
        return -1;
    }
    
    // 备份当前IP配置
    printf("备份当前IP配置...\n");
    snprintf(command, sizeof(command), "ip addr show %s | grep 'inet ' | head -1", interface);
    char backup_ip[256];
    execute_command_with_output(command, backup_ip, sizeof(backup_ip));
    
    // 清除现有IP地址（仅对非lo接口）
    if (strcmp(interface, "lo") != 0) {
        snprintf(command, sizeof(command), "ip addr flush dev %s", interface);
        result = execute_command_with_output(command, NULL, 0);
        if (result != 0) {
            printf("WARNING: 清除IP地址失败\n");
        }
    }
    
    // 设置新IP地址
    snprintf(command, sizeof(command), "ip addr add %s/%s dev %s", 
             config->assigned_ip, config->netmask, interface);
    result = execute_command_with_output(command, NULL, 0);
    
    // 启用接口
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    int up_result = execute_command_with_output(command, NULL, 0);
    
    // 验证IP地址设置
    printf("验证IP地址设置...\n");
    snprintf(command, sizeof(command), "ip addr show %s | grep %s", interface, config->assigned_ip);
    int verify_result = execute_command_with_output(command, NULL, 0);
    
    if (verify_result == 0) {
        printf("SUCCESS: IP地址设置成功\n");
    } else {
        printf("ERROR: IP地址设置验证失败\n");
        result = -1;
    }
    
    return result;
}

// 测试网关设置
int test_gateway_setting(const test_network_config_t *config) {
    printf("\n=== 测试网关设置 ===\n");
    
    char command[256];
    
    // 备份当前默认路由
    printf("备份当前默认路由...\n");
    execute_command_with_output("ip route show default", NULL, 0);
    
    // 删除默认路由
    printf("删除现有默认路由...\n");
    execute_command_with_output("ip route del default", NULL, 0);
    
    // 添加新的默认路由
    snprintf(command, sizeof(command), "ip route add default via %s", config->gateway);
    int result = execute_command_with_output(command, NULL, 0);
    
    // 验证路由设置
    printf("验证路由设置...\n");
    snprintf(command, sizeof(command), "ip route show default | grep %s", config->gateway);
    int verify_result = execute_command_with_output(command, NULL, 0);
    
    if (verify_result == 0) {
        printf("SUCCESS: 网关设置成功\n");
    } else {
        printf("ERROR: 网关设置验证失败\n");
        result = -1;
    }
    
    return result;
}

// 测试DNS设置
int test_dns_setting(const test_network_config_t *config) {
    printf("\n=== 测试DNS设置 ===\n");
    
    // 备份当前DNS配置
    printf("备份当前DNS配置...\n");
    execute_command_with_output("cat /etc/resolv.conf", NULL, 0);
    
    // 创建新的DNS配置
    FILE *resolv_file = fopen("/etc/resolv.conf.test", "w");
    if (!resolv_file) {
        printf("ERROR: 无法创建测试DNS配置文件\n");
        return -1;
    }
    
    fprintf(resolv_file, "nameserver %s\n", config->dns_primary);
    if (strlen(config->dns_secondary) > 0) {
        fprintf(resolv_file, "nameserver %s\n", config->dns_secondary);
    }
    fclose(resolv_file);
    
    printf("SUCCESS: DNS测试配置文件创建成功\n");
    execute_command_with_output("cat /etc/resolv.conf.test", NULL, 0);
    
    // 清理测试文件
    unlink("/etc/resolv.conf.test");
    
    return 0;
}

// 测试带宽限制
int test_bandwidth_limit(const char *interface, const test_network_config_t *config) {
    printf("\n=== 测试带宽限制 ===\n");
    
    if (config->bandwidth_limit <= 0) {
        printf("跳过带宽限制测试（未设置限制）\n");
        return 0;
    }
    
    char command[256];
    
    // 检查tc命令是否可用
    int tc_available = execute_command_with_output("which tc", NULL, 0);
    if (tc_available != 0) {
        printf("WARNING: tc命令不可用，跳过带宽限制测试\n");
        return 0;
    }
    
    // 设置带宽限制
    snprintf(command, sizeof(command), "tc qdisc add dev %s root handle 1: htb default 30", interface);
    int result = execute_command_with_output(command, NULL, 0);
    
    if (result == 0) {
        snprintf(command, sizeof(command), "tc class add dev %s parent 1: classid 1:1 htb rate %dkbit", 
                 interface, config->bandwidth_limit);
        result = execute_command_with_output(command, NULL, 0);
    }
    
    // 验证带宽限制
    if (result == 0) {
        printf("验证带宽限制设置...\n");
        snprintf(command, sizeof(command), "tc qdisc show dev %s", interface);
        execute_command_with_output(command, NULL, 0);
    }
    
    // 清理带宽限制
    snprintf(command, sizeof(command), "tc qdisc del dev %s root", interface);
    execute_command_with_output(command, NULL, 0);
    
    return result;
}

// 主测试函数
int main() {
    printf("MAGIC Client 网络配置调试程序\n");
    printf("==============================\n");
    
    // 检查是否以root权限运行
    if (getuid() != 0) {
        printf("WARNING: 未以root权限运行，某些网络配置可能失败\n");
    }
    
    // 设置测试网络配置
    test_network_config_t config;
    strcpy(config.assigned_ip, "192.168.1.100");
    strcpy(config.netmask, "255.255.255.0");
    strcpy(config.gateway, "192.168.1.1");
    strcpy(config.dns_primary, "8.8.8.8");
    strcpy(config.dns_secondary, "8.8.4.4");
    config.bandwidth_limit = 1000;
    
    printf("\n测试网络配置:\n");
    printf("IP地址: %s/%s\n", config.assigned_ip, config.netmask);
    printf("网关: %s\n", config.gateway);
    printf("DNS: %s, %s\n", config.dns_primary, config.dns_secondary);
    printf("带宽限制: %d kbps\n", config.bandwidth_limit);
    
    int total_tests = 0;
    int failed_tests = 0;
    
    // 测试接口发现
    total_tests++;
    if (test_interface_discovery() != 0) {
        failed_tests++;
    }
    
    // 测试接口选择
    char selected_interface[32];
    total_tests++;
    if (test_interface_selection(selected_interface, sizeof(selected_interface)) != 0) {
        failed_tests++;
    }
    
    // 测试IP地址设置
    total_tests++;
    if (test_ip_address_setting(selected_interface, &config) != 0) {
        failed_tests++;
    }
    
    // 测试网关设置（仅在非WSL环境下）
    char *wsl_env = getenv("WSL_DISTRO_NAME");
    if (!wsl_env) {
        total_tests++;
        if (test_gateway_setting(&config) != 0) {
            failed_tests++;
        }
    } else {
        printf("\n跳过网关设置测试（WSL环境）\n");
    }
    
    // 测试DNS设置
    total_tests++;
    if (test_dns_setting(&config) != 0) {
        failed_tests++;
    }
    
    // 测试带宽限制
    total_tests++;
    if (test_bandwidth_limit(selected_interface, &config) != 0) {
        failed_tests++;
    }
    
    // 输出测试结果
    printf("\n==============================\n");
    printf("测试完成\n");
    printf("总测试数: %d\n", total_tests);
    printf("失败测试数: %d\n", failed_tests);
    printf("成功率: %.1f%%\n", (total_tests - failed_tests) * 100.0 / total_tests);
    
    if (failed_tests == 0) {
        printf("所有网络配置测试通过！\n");
        return 0;
    } else {
        printf("部分网络配置测试失败，请检查权限和系统配置。\n");
        return 1;
    }
}