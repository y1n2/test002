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
#define CMD_DEVICE_WATCHDOG 280  // 使用标准的Device-Watchdog命令代码
#define AVP_ORIGIN_HOST 264
#define AVP_ORIGIN_REALM 296
#define AVP_ORIGIN_STATE_ID 278  // 使用标准的Origin-State-Id AVP
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
    
    // 计算AVP总长度（包括头部）
    int total_len = sizeof(avp_header_t) + data_len;
    
    // 设置长度字段（24位，大端序）
    avp_hdr->length[0] = (total_len >> 16) & 0xFF;
    avp_hdr->length[1] = (total_len >> 8) & 0xFF;
    avp_hdr->length[2] = total_len & 0xFF;
    
    // 复制数据
    if (data && data_len > 0) {
        memcpy(buffer + offset + sizeof(avp_header_t), data, data_len);
    }
    
    // 计算填充长度（4字节对齐）
    int padding = (4 - (total_len % 4)) % 4;
    if (padding > 0) {
        memset(buffer + offset + total_len, 0, padding);
    }
    
    return total_len + padding;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char avp_buffer[BUFFER_SIZE];
    int avp_offset = 0;
    
    // 解析命令行参数
    const char* server_ip = "192.168.37.136";  // 服务端IP
    int server_port = 3868;  // 服务端端口
    
    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        server_port = atoi(argv[2]);
    }
    
    printf("🚀 开始数据传输测试\n");
    printf("目标服务器: %s:%d\n", server_ip, server_port);
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket创建失败");
        return 1;
    }
    
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    // 连接到服务器
    printf("📡 正在连接到服务器...\n");
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接失败");
        close(sock);
        return 1;
    }
    
    printf("✅ 已连接到服务器 (%s:%d)\n", server_ip, server_port);
    
    // 构建Capabilities Exchange消息的AVP数据
    
    // 1. Origin-Host AVP (必需)
    const char* origin_host = "client.example.com";
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_ORIGIN_HOST, 0x40, 
                         origin_host, strlen(origin_host));
    
    // 2. Origin-Realm AVP (必需)
    const char* origin_realm = "example.com";
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_ORIGIN_REALM, 0x40, 
                         origin_realm, strlen(origin_realm));
    
    // 3. Host-IP-Address AVP (必需) - AVP Code: 257
    uint32_t host_ip = inet_addr("192.168.37.136");  // 客户端IP地址
    avp_offset += add_avp(avp_buffer, avp_offset, 257, 0x40, 
                         &host_ip, sizeof(host_ip));
    
    // 4. Vendor-Id AVP (必需) - AVP Code: 266
    uint32_t vendor_id = htonl_custom(0);  // 0表示IETF
    avp_offset += add_avp(avp_buffer, avp_offset, 266, 0x40, 
                         &vendor_id, sizeof(vendor_id));
    
    // 5. Product-Name AVP (必需) - AVP Code: 269
    const char* product_name = "TestClient";
    avp_offset += add_avp(avp_buffer, avp_offset, 269, 0x00, 
                         product_name, strlen(product_name));
    
    // 6. Origin-State-Id AVP (可选)
    uint32_t state_id = htonl_custom(54321);
    avp_offset += add_avp(avp_buffer, avp_offset, AVP_ORIGIN_STATE_ID, 0x40, 
                         &state_id, sizeof(state_id));
    
    // 构建Capabilities Exchange消息头
    diameter_header_t cer_header;
    memset(&cer_header, 0, sizeof(cer_header));
    
    cer_header.version = DIAMETER_VERSION;
    cer_header.flags = 0x80;  // REQUEST标志
    
    // 设置命令代码（24位）- Capabilities Exchange Request (257)
    cer_header.command_code[0] = (257 >> 16) & 0xFF;
    cer_header.command_code[1] = (257 >> 8) & 0xFF;
    cer_header.command_code[2] = 257 & 0xFF;
    
    cer_header.application_id = htonl_custom(0);  // Base Protocol应用ID为0
    cer_header.hop_by_hop_id = htonl_custom(0x12345678);
    cer_header.end_to_end_id = htonl_custom(0x87654321);
    
    // 计算总长度
    int total_length = sizeof(diameter_header_t) + avp_offset;
    
    // 设置长度字段（24位）
    cer_header.length[0] = (total_length >> 16) & 0xFF;
    cer_header.length[1] = (total_length >> 8) & 0xFF;
    cer_header.length[2] = total_length & 0xFF;
    
    // 组装完整消息
    memcpy(buffer, &cer_header, sizeof(diameter_header_t));
    memcpy(buffer + sizeof(diameter_header_t), avp_buffer, avp_offset);
    
    printf("📤 发送Capabilities Exchange请求...\n");
    printf("消息长度: %d 字节\n", total_length);
    
    // 发送消息
    if (send(sock, buffer, total_length, 0) < 0) {
        perror("发送失败");
        close(sock);
        return 1;
    }
    
    printf("✅ 数据传输请求已发送\n");
    
    // 接收响应
    printf("⏳ 等待服务器响应...\n");
    int recv_len = recv(sock, buffer, BUFFER_SIZE, 0);
    if (recv_len < 0) {
        perror("接收响应失败");
        close(sock);
        return 1;
    }
    
    printf("📥 收到响应: %d 字节\n", recv_len);
    
    if (recv_len >= sizeof(diameter_header_t)) {
        diameter_header_t* resp_header = (diameter_header_t*)buffer;
        
        // 解析响应头
        uint32_t resp_cmd = (resp_header->command_code[0] << 16) | 
                           (resp_header->command_code[1] << 8) | 
                           resp_header->command_code[2];
        uint32_t resp_length = (resp_header->length[0] << 16) | 
                              (resp_header->length[1] << 8) | 
                              resp_header->length[2];
        
        printf("✅ 响应命令代码: %u (期望: 257)\n", resp_cmd);
        printf("✅ 响应标志: 0x%02X (应该没有REQUEST标志)\n", resp_header->flags);
        printf("✅ 响应长度: %u 字节\n", resp_length);
        
        // 查找Result-Code AVP
        char* avp_data = buffer + sizeof(diameter_header_t);
        int remaining = recv_len - sizeof(diameter_header_t);
        
        while (remaining >= sizeof(avp_header_t)) {
            avp_header_t* avp = (avp_header_t*)avp_data;
            uint32_t avp_code = ntohl_custom(avp->code);
            uint32_t avp_len = (avp->length[0] << 16) | (avp->length[1] << 8) | avp->length[2];
            
            if (avp_code == AVP_RESULT_CODE && avp_len >= sizeof(avp_header_t) + 4) {
                uint32_t result_code = ntohl_custom(*(uint32_t*)(avp_data + sizeof(avp_header_t)));
                printf("✅ Result-Code: %u (期望: 2001 DIAMETER_SUCCESS)\n", result_code);
                break;
            }
            
            // 移动到下一个AVP（考虑4字节对齐）
            int padded_len = (avp_len + 3) & ~3;
            avp_data += padded_len;
            remaining -= padded_len;
        }
        
        printf("🎉 数据传输测试完成！\n");
    } else {
        printf("❌ 响应长度不足\n");
    }
    
    close(sock);
    return 0;
}