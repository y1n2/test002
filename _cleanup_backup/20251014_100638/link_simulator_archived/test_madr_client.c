#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define DIAMETER_VERSION 1
#define CMD_MADR 100005
#define AVP_ORIGIN_HOST 264
#define AVP_ORIGIN_REALM 296
#define AVP_CDR_ID 100046
#define AVP_RESULT_CODE 268

// Diameter消息头结构
typedef struct {
    uint8_t version;
    uint8_t length[3];      // 24位长度字段
    uint8_t flags;
    uint8_t command_code[3]; // 24位命令代码
    uint32_t application_id;
    uint32_t hop_by_hop_id;
    uint32_t end_to_end_id;
} __attribute__((packed)) diameter_header_t;

// AVP头结构
typedef struct {
    uint32_t code;
    uint8_t flags;
    uint8_t length[3];  // 24位长度字段
} __attribute__((packed)) avp_header_t;

// 字节序转换函数
uint32_t htonl_custom(uint32_t hostlong) {
    return htonl(hostlong);
}

uint32_t ntohl_custom(uint32_t netlong) {
    return ntohl(netlong);
}

// 添加AVP到缓冲区
int add_avp(char* buffer, int offset, uint32_t code, uint8_t flags, const void* data, int data_len) {
    avp_header_t* avp_hdr = (avp_header_t*)(buffer + offset);
    
    // 设置AVP头
    avp_hdr->code = htonl_custom(code);
    avp_hdr->flags = flags;
    
    // 计算总长度（头部8字节 + 数据长度）
    int total_len = 8 + data_len;
    avp_hdr->length[0] = (total_len >> 16) & 0xFF;
    avp_hdr->length[1] = (total_len >> 8) & 0xFF;
    avp_hdr->length[2] = total_len & 0xFF;
    
    // 复制数据
    if (data && data_len > 0) {
        memcpy(buffer + offset + 8, data, data_len);
    }
    
    // 返回填充后的长度（4字节对齐）
    return (total_len + 3) & ~3;
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char avp_buffer[BUFFER_SIZE];
    int avp_offset = 0;
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket创建失败");
        return 1;
    }
    
    // 设置服务器地址（连接到以太网链路端口8001）
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8001);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 连接到链路模拟器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接失败");
        close(sock);
        return 1;
    }
    
    printf("✅ 已连接到链路模拟器 (端口 8001)\n");
    
    // 构建MADR消息的AVP数据
    
    // 1. Origin-Host AVP
    const char* origin_host = "test-client.example.com";
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_ORIGIN_HOST, 0x40, 
                         origin_host, strlen(origin_host));
    
    // 2. Origin-Realm AVP
    const char* origin_realm = "example.com";
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_ORIGIN_REALM, 0x40, 
                         origin_realm, strlen(origin_realm));
    
    // 3. CDR-Id AVP (测试用的CDR ID)
    uint32_t cdr_id = htonl_custom(12345);
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_CDR_ID, 0x40, 
                         &cdr_id, sizeof(cdr_id));
    
    // 构建MADR消息头
    diameter_header_t madr_header;
    memset(&madr_header, 0, sizeof(madr_header));
    
    madr_header.version = DIAMETER_VERSION;
    madr_header.flags = 0x80;  // REQUEST标志
    
    // 设置24位命令代码
    madr_header.command_code[0] = (CMD_MADR >> 16) & 0xFF;
    madr_header.command_code[1] = (CMD_MADR >> 8) & 0xFF;
    madr_header.command_code[2] = CMD_MADR & 0xFF;
    
    madr_header.application_id = 0;  // 基础Diameter应用
    madr_header.hop_by_hop_id = htonl_custom(0x12345678);
    madr_header.end_to_end_id = htonl_custom(0x87654321);
    
    // 设置24位长度字段（头部20字节 + AVP数据长度）
    int total_length = 20 + avp_offset;
    madr_header.length[0] = (total_length >> 16) & 0xFF;
    madr_header.length[1] = (total_length >> 8) & 0xFF;
    madr_header.length[2] = total_length & 0xFF;
    
    // 发送MADR请求
    printf("📤 发送 MADR Request (CDR-Id: 12345)...\n");
    
    // 先发送消息头
    if (send(sock, &madr_header, sizeof(madr_header), 0) < 0) {
        perror("发送消息头失败");
        close(sock);
        return 1;
    }
    
    // 再发送AVP数据
    if (avp_offset > 0) {
        if (send(sock, avp_buffer, avp_offset, 0) < 0) {
            perror("发送AVP数据失败");
            close(sock);
            return 1;
        }
    }
    
    // 接收响应
    int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("接收响应失败");
        close(sock);
        return 1;
    }
    
    if (bytes_received >= 20) {
        diameter_header_t* resp_header = (diameter_header_t*)buffer;
        
        // 解析响应长度
        uint32_t resp_length = (resp_header->length[0] << 16) | 
                              (resp_header->length[1] << 8) | 
                              resp_header->length[2];
        
        // 解析命令代码
        uint32_t resp_cmd = (resp_header->command_code[0] << 16) | 
                           (resp_header->command_code[1] << 8) | 
                           resp_header->command_code[2];
        
        printf("📥 收到响应: %d 字节\n", bytes_received);
        printf("✅ 响应命令代码: %u (期望: %u)\n", resp_cmd, CMD_MADR);
        printf("✅ 响应标志: 0x%02X (应该没有REQUEST标志)\n", resp_header->flags);
        printf("✅ 响应长度: %u 字节\n", resp_length);
        
        // 解析AVP数据寻找Result-Code
        if (bytes_received > 20) {
            char* avp_data = buffer + 20;
            int avp_data_len = bytes_received - 20;
            char* avp_ptr = avp_data;
            int remaining = avp_data_len;
            
            while (remaining >= 8) {
                avp_header_t* avp_hdr = (avp_header_t*)avp_ptr;
                uint32_t avp_code = ntohl_custom(avp_hdr->code);
                uint32_t avp_len = (avp_hdr->length[0] << 16) | 
                                  (avp_hdr->length[1] << 8) | 
                                  avp_hdr->length[2];
                
                if (avp_code == AVP_RESULT_CODE && avp_len >= 12) {
                    uint32_t result_code = ntohl_custom(*(uint32_t*)(avp_ptr + 8));
                    printf("✅ Result-Code: %u (期望: 2001 DIAMETER_SUCCESS)\n", result_code);
                }
                
                int padded_len = (avp_len + 3) & ~3;
                avp_ptr += padded_len;
                remaining -= padded_len;
            }
        }
        
        if (resp_cmd == CMD_MADR && (resp_header->flags & 0x80) == 0) {
            printf("🎉 MADR 测试成功！\n");
        } else {
            printf("❌ MADR 测试失败\n");
        }
    } else {
        printf("❌ 收到的响应太短: %d 字节\n", bytes_received);
    }
    
    close(sock);
    return 0;
}