#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>

// Diameter 消息头结构
typedef struct {
    uint8_t version;
    uint8_t length[3];
    uint8_t flags;
    uint8_t command_code[3];
    uint32_t application_id;
    uint32_t hop_by_hop_id;
    uint32_t end_to_end_id;
} __attribute__((packed)) diameter_header_t;

// Diameter AVP 头结构
typedef struct {
    uint32_t code;
    uint8_t flags;
    uint8_t length[3];
} __attribute__((packed)) diameter_avp_header_t;

// 字节序转换函数
uint32_t htonl_custom(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | 
           (((hostlong >> 8) & 0xFF) << 16) | 
           (((hostlong >> 16) & 0xFF) << 8) | 
           ((hostlong >> 24) & 0xFF);
}

uint16_t htons_custom(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

// 设置24位长度字段
void set_24bit_length(uint8_t *field, uint32_t length) {
    field[0] = (length >> 16) & 0xFF;
    field[1] = (length >> 8) & 0xFF;
    field[2] = length & 0xFF;
}

// 获取24位长度字段
uint32_t get_24bit_length(const uint8_t *field) {
    return (field[0] << 16) | (field[1] << 8) | field[2];
}

// 添加AVP到消息中
int add_avp(uint8_t *message, int *offset, uint32_t code, uint8_t flags, const void *data, int data_len) {
    diameter_avp_header_t *avp = (diameter_avp_header_t *)(message + *offset);
    
    avp->code = htonl_custom(code);
    avp->flags = flags;
    
    int avp_length = sizeof(diameter_avp_header_t) + data_len;
    set_24bit_length(avp->length, avp_length);
    
    if (data && data_len > 0) {
        memcpy(message + *offset + sizeof(diameter_avp_header_t), data, data_len);
    }
    
    // AVP需要4字节对齐
    int padded_length = (avp_length + 3) & ~3;
    if (padded_length > avp_length) {
        memset(message + *offset + avp_length, 0, padded_length - avp_length);
    }
    
    *offset += padded_length;
    return padded_length;
}

int test_connection(const char *host, int port, const char *test_name) {
    printf("\n🔄 测试 %s\n", test_name);
    printf("目标: %s:%d\n", host, port);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("❌ 创建socket失败\n");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons_custom(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        printf("❌ 无效的IP地址\n");
        close(sock);
        return -1;
    }
    
    printf("📡 正在连接...\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("❌ 连接失败\n");
        close(sock);
        return -1;
    }
    
    printf("✅ 连接成功\n");
    
    // 构建MADR消息
    uint8_t message[1024];
    memset(message, 0, sizeof(message));
    
    diameter_header_t *header = (diameter_header_t *)message;
    header->version = 1;
    header->flags = 0x80; // REQUEST标志
    set_24bit_length(header->command_code, 100005); // MADR命令代码
    header->application_id = 0; // Base Protocol
    header->hop_by_hop_id = htonl_custom(0x12345678);
    header->end_to_end_id = htonl_custom(0x87654321);
    
    int offset = sizeof(diameter_header_t);
    
    // 添加Origin-Host AVP
    const char *origin_host = "test-client.example.com";
    add_avp(message, &offset, 264, 0x40, origin_host, strlen(origin_host));
    
    // 添加Origin-Realm AVP
    const char *origin_realm = "example.com";
    add_avp(message, &offset, 296, 0x40, origin_realm, strlen(origin_realm));
    
    // 添加CDR-Id AVP (自定义AVP代码)
    uint32_t cdr_id = htonl_custom(54321);
    add_avp(message, &offset, 1001, 0x40, &cdr_id, sizeof(cdr_id));
    
    // 设置消息总长度
    set_24bit_length(header->length, offset);
    
    printf("📤 发送数据传输请求 (CDR-Id: 54321)...\n");
    printf("消息长度: %d 字节\n", offset);
    
    if (send(sock, message, offset, 0) < 0) {
        printf("❌ 发送失败\n");
        close(sock);
        return -1;
    }
    
    printf("✅ 请求已发送\n");
    printf("⏳ 等待响应...\n");
    
    // 接收响应
    uint8_t response[1024];
    int response_len = recv(sock, response, sizeof(response), 0);
    
    printf("📥 收到响应: %d 字节\n", response_len);
    
    if (response_len >= sizeof(diameter_header_t)) {
        diameter_header_t *resp_header = (diameter_header_t *)response;
        uint32_t resp_length = get_24bit_length(resp_header->length);
        uint32_t resp_command = get_24bit_length(resp_header->command_code);
        
        printf("✅ 响应命令代码: %u\n", resp_command);
        printf("✅ 响应长度: %u 字节\n", resp_length);
        printf("✅ 响应标志: 0x%02X\n", resp_header->flags);
        
        // 查找Result-Code AVP
        int avp_offset = sizeof(diameter_header_t);
        while (avp_offset < response_len - sizeof(diameter_avp_header_t)) {
            diameter_avp_header_t *avp = (diameter_avp_header_t *)(response + avp_offset);
            uint32_t avp_code = htonl_custom(avp->code);
            uint32_t avp_length = get_24bit_length(avp->length);
            
            if (avp_code == 268) { // Result-Code AVP
                if (avp_length >= sizeof(diameter_avp_header_t) + 4) {
                    uint32_t result_code = *(uint32_t *)(response + avp_offset + sizeof(diameter_avp_header_t));
                    result_code = htonl_custom(result_code);
                    printf("✅ Result-Code: %u\n", result_code);
                    
                    if (result_code == 2001) {
                        printf("🎉 %s 测试成功！\n", test_name);
                        close(sock);
                        return 0;
                    }
                }
                break;
            }
            
            int padded_length = (avp_length + 3) & ~3;
            avp_offset += padded_length;
        }
    } else {
        printf("❌ 响应长度不足\n");
    }
    
    close(sock);
    return -1;
}

int main(int argc, char *argv[]) {
    printf("🚀 端到端数据传输测试\n");
    printf("======================\n");
    
    // 测试1: 直接连接到链路模拟器 (验证链路模拟器工作正常)
    printf("\n📋 测试计划:\n");
    printf("1. 直接连接到链路模拟器 (端口 8001) - 验证链路模拟器工作\n");
    printf("2. 连接到服务端 (端口 3868) - 测试服务端路由转发\n");
    
    // 测试链路模拟器
    if (test_connection("127.0.0.1", 8001, "链路模拟器直连测试") == 0) {
        printf("\n✅ 链路模拟器工作正常\n");
    } else {
        printf("\n❌ 链路模拟器测试失败\n");
        return 1;
    }
    
    // 测试服务端路由转发
    if (test_connection("192.168.37.136", 3868, "服务端路由转发测试") == 0) {
        printf("\n✅ 服务端路由转发工作正常\n");
    } else {
        printf("\n❌ 服务端路由转发测试失败\n");
        return 1;
    }
    
    printf("\n🎉 所有测试完成！端到端数据传输功能正常\n");
    return 0;
}