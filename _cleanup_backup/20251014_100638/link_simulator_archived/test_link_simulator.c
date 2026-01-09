#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

int main() {
    int sock;
    struct sockaddr_in server_addr;
    
    printf("🚀 测试链路模拟器连接\n");
    
    // 创建套接字
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    // 设置服务器地址 (WiFi 链路端口 8002)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8002);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    printf("📡 连接到链路模拟器 (127.0.0.1:8002)...\n");
    
    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    printf("✅ 已连接到链路模拟器\n");
    
    // 构造 Device-Watchdog 请求
    diameter_header_t header;
    memset(&header, 0, sizeof(header));
    header.version = 1;
    
    // 设置消息长度 (20字节头 + 12字节AVP)
    uint32_t msg_len = 32;
    header.length[0] = (msg_len >> 16) & 0xFF;
    header.length[1] = (msg_len >> 8) & 0xFF;
    header.length[2] = msg_len & 0xFF;
    
    header.flags = 0x80; // Request flag
    
    // Device-Watchdog 命令代码 (280)
    header.command_code[0] = 0x00;
    header.command_code[1] = 0x01;
    header.command_code[2] = 0x18;
    
    header.application_id = 0; // Base protocol
    header.hop_by_hop_id = htonl(12345);
    header.end_to_end_id = htonl(54321);
    
    // Origin-State-Id AVP (AVP Code: 278)
    uint8_t avp_data[12];
    uint32_t avp_code = htonl(278);
    uint32_t avp_flags_length = htonl(0x40000008); // M flag, length 8
    uint32_t state_id = htonl(54321);
    
    memcpy(avp_data, &avp_code, 4);
    memcpy(avp_data + 4, &avp_flags_length, 4);
    memcpy(avp_data + 8, &state_id, 4);
    
    printf("📤 发送 Device-Watchdog 请求...\n");
    
    // 发送消息头
    if (send(sock, &header, sizeof(header), 0) < 0) {
        perror("send header");
        close(sock);
        return 1;
    }
    
    // 发送 AVP 数据
    if (send(sock, avp_data, sizeof(avp_data), 0) < 0) {
        perror("send avp");
        close(sock);
        return 1;
    }
    
    printf("✅ 消息已发送 (%d 字节)\n", (int)(sizeof(header) + sizeof(avp_data)));
    
    // 接收响应
    uint8_t response[1024];
    printf("⏳ 等待响应...\n");
    
    int bytes_received = recv(sock, response, sizeof(response), 0);
    if (bytes_received < 0) {
        perror("recv");
        close(sock);
        return 1;
    }
    
    printf("📥 收到响应: %d 字节\n", bytes_received);
    
    if (bytes_received >= 20) {
        diameter_header_t *resp_header = (diameter_header_t*)response;
        uint32_t resp_len = (resp_header->length[0] << 16) | 
                           (resp_header->length[1] << 8) | 
                           resp_header->length[2];
        uint32_t cmd_code = (resp_header->command_code[0] << 16) |
                           (resp_header->command_code[1] << 8) |
                           resp_header->command_code[2];
        
        printf("✅ 响应解析成功:\n");
        printf("   版本: %d\n", resp_header->version);
        printf("   长度: %d\n", resp_len);
        printf("   命令代码: %d\n", cmd_code);
        printf("   标志: 0x%02X\n", resp_header->flags);
    } else {
        printf("❌ 响应长度不足\n");
    }
    
    close(sock);
    printf("🔌 连接已关闭\n");
    
    return 0;
}