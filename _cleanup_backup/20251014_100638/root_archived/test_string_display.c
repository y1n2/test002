#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server_addr;
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("创建socket失败");
        return 1;
    }
    
    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8001);  // 以太网链路端口
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 连接到链路模拟器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接失败");
        close(sock);
        return 1;
    }
    
    printf("✅ 成功连接到以太网链路模拟器\n");
    
    // 等待一下确保连接建立
    sleep(1);
    
    // 发送包含各种字符的测试消息
    char test_message[] = "Hello World! 这是一个测试消息 with special chars: @#$%^&*()_+{}|:<>?[]\\;'\",./ and some binary data: \x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10";
    
    printf("📤 发送测试消息: \"%s\"\n", test_message);
    printf("📏 消息长度: %zu 字节\n", strlen(test_message));
    
    ssize_t sent = send(sock, test_message, strlen(test_message), 0);
    if (sent < 0) {
        perror("发送消息失败");
    } else {
        printf("✅ 成功发送 %zd 字节\n", sent);
    }
    
    // 等待链路模拟器处理消息
    printf("⏳ 等待链路模拟器处理消息...\n");
    sleep(3);
    
    // 接收响应
    char response[1024];
    ssize_t received = recv(sock, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        printf("📥 收到响应: \"%s\"\n", response);
    } else if (received == 0) {
        printf("🔌 服务器关闭了连接\n");
    } else {
        perror("接收响应失败");
    }
    
    printf("⏳ 保持连接2秒钟...\n");
    sleep(2);
    
    close(sock);
    printf("🔌 连接已关闭\n");
    
    return 0;
}