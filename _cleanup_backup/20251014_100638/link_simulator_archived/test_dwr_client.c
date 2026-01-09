#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CMD_DEVICE_WATCHDOG 280
#define DIAMETER_VERSION 1
#define DIAMETER_FLAG_REQUEST 0x80

typedef struct {
    uint8_t version;
    uint8_t length[3];      // 24位长度字段
    uint8_t flags;
    uint8_t command_code[3]; // 24位命令代码
    uint32_t application_id;
    uint32_t hop_by_hop_id;
    uint32_t end_to_end_id;
} __attribute__((packed)) diameter_header_t;

uint32_t htonl_custom(uint32_t hostlong) {
    return htonl(hostlong);
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    diameter_header_t dwr_header;
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        return 1;
    }
    
    // 设置服务器地址 (连接到以太网链路端口 8001)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8001);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        close(sock);
        return 1;
    }
    
    printf("✅ 已连接到链路模拟器 (端口 8001)\n");
    
    // 构建 Device Watchdog Request
    memset(&dwr_header, 0, sizeof(dwr_header));
    dwr_header.version = DIAMETER_VERSION;
    
    // 设置24位长度字段 (20字节 - 只有头部)
    dwr_header.length[0] = 0;
    dwr_header.length[1] = 0;
    dwr_header.length[2] = 20;
    
    dwr_header.flags = DIAMETER_FLAG_REQUEST;
    
    // 设置24位命令代码 (网络字节序)
    dwr_header.command_code[0] = (CMD_DEVICE_WATCHDOG >> 16) & 0xFF;
    dwr_header.command_code[1] = (CMD_DEVICE_WATCHDOG >> 8) & 0xFF;
    dwr_header.command_code[2] = CMD_DEVICE_WATCHDOG & 0xFF;
    
    dwr_header.application_id = 0; // Base protocol
    dwr_header.hop_by_hop_id = htonl_custom(0x12345678);
    dwr_header.end_to_end_id = htonl_custom(0x87654321);
    
    // 发送 DWR
    printf("📤 发送 Device Watchdog Request...\n");
    if (send(sock, &dwr_header, sizeof(dwr_header), 0) < 0) {
        perror("send failed");
        close(sock);
        return 1;
    }
    
    // 接收响应
    char response[1024];
    int bytes_received = recv(sock, response, sizeof(response), 0);
    if (bytes_received > 0) {
        printf("📥 收到响应: %d 字节\n", bytes_received);
        
        if (bytes_received >= sizeof(diameter_header_t)) {
            diameter_header_t* resp_header = (diameter_header_t*)response;
            uint32_t resp_cmd = (resp_header->command_code[0] << 16) | 
                               (resp_header->command_code[1] << 8) | 
                               resp_header->command_code[2];
            
            printf("✅ 响应命令代码: %u (期望: %u)\n", resp_cmd, CMD_DEVICE_WATCHDOG);
            printf("✅ 响应标志: 0x%02x (应该没有REQUEST标志)\n", resp_header->flags);
        }
    } else {
        printf("❌ 未收到响应\n");
    }
    
    close(sock);
    return 0;
}