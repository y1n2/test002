#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define ETHERNET_PORT 8001
#define WIFI_PORT 8002
#define CELLULAR_PORT 8003
#define SATELLITE_PORT 8004

// 测试消息结构
typedef struct {
    char type[32];
    char data[256];
    int length;
} test_message_t;

int connect_to_link(int port, const char* link_name) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("创建socket失败 (%s): %s\n", link_name, strerror(errno));
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("连接到%s链路模拟器失败 (端口 %d): %s\n", link_name, port, strerror(errno));
        close(sock);
        return -1;
    }
    
    printf("✅ 成功连接到%s链路模拟器 (端口 %d)\n", link_name, port);
    return sock;
}

int send_test_message(int sock, const char* link_name, const char* message) {
    test_message_t test_msg;
    strncpy(test_msg.type, "TEST_FORWARD", sizeof(test_msg.type) - 1);
    strncpy(test_msg.data, message, sizeof(test_msg.data) - 1);
    test_msg.length = strlen(message);
    
    int sent = send(sock, &test_msg, sizeof(test_msg), 0);
    if (sent < 0) {
        printf("❌ 发送消息到%s链路失败: %s\n", link_name, strerror(errno));
        return -1;
    }
    
    printf("📤 发送测试消息到%s链路: \"%s\" (%d bytes)\n", link_name, message, sent);
    
    // 等待响应
    char response[512];
    int received = recv(sock, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        printf("📥 从%s链路接收到响应: \"%s\" (%d bytes)\n", link_name, response, received);
    } else if (received == 0) {
        printf("🔌 %s链路连接已关闭\n", link_name);
    } else {
        printf("❌ 从%s链路接收响应失败: %s\n", link_name, strerror(errno));
    }
    
    return received;
}

int main() {
    printf("=== 测试消息转发到链路模拟器 ===\n\n");
    
    // 测试连接到所有链路
    struct {
        int port;
        const char* name;
        int sock;
    } links[] = {
        {ETHERNET_PORT, "以太网", -1},
        {WIFI_PORT, "WiFi", -1},
        {CELLULAR_PORT, "蜂窝", -1},
        {SATELLITE_PORT, "卫星", -1}
    };
    
    int num_links = sizeof(links) / sizeof(links[0]);
    int connected_count = 0;
    
    // 连接到所有链路
    for (int i = 0; i < num_links; i++) {
        links[i].sock = connect_to_link(links[i].port, links[i].name);
        if (links[i].sock >= 0) {
            connected_count++;
        }
    }
    
    printf("\n连接结果: %d/%d 链路连接成功\n\n", connected_count, num_links);
    
    if (connected_count == 0) {
        printf("❌ 没有链路连接成功，退出测试\n");
        return 1;
    }
    
    // 发送测试消息到所有连接的链路
    for (int i = 0; i < num_links; i++) {
        if (links[i].sock >= 0) {
            char message[128];
            snprintf(message, sizeof(message), "Test message to %s link at %ld", 
                    links[i].name, time(NULL));
            
            send_test_message(links[i].sock, links[i].name, message);
            printf("\n");
            
            // 短暂延迟
            usleep(500000); // 0.5秒
        }
    }
    
    // 关闭所有连接
    for (int i = 0; i < num_links; i++) {
        if (links[i].sock >= 0) {
            close(links[i].sock);
            printf("🔌 关闭%s链路连接\n", links[i].name);
        }
    }
    
    printf("\n=== 消息转发测试完成 ===\n");
    return 0;
}