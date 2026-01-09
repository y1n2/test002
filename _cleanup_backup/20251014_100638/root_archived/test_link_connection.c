#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int test_connection(const char *host, int port, const char *link_name) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        printf("❌ 创建%s链路socket失败: %s\n", link_name, strerror(errno));
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        printf("❌ 无效的IP地址: %s\n", host);
        close(sock_fd);
        return -1;
    }
    
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("❌ 连接%s链路模拟器失败 %s:%d: %s\n", link_name, host, port, strerror(errno));
        close(sock_fd);
        return -1;
    }
    
    printf("✅ 成功连接到%s链路模拟器: %s:%d\n", link_name, host, port);
    close(sock_fd);
    return 0;
}

int main() {
    const char *host = "127.0.0.1";
    
    printf("测试链路模拟器连接...\n");
    
    test_connection(host, 8001, "以太网");
    test_connection(host, 8002, "WiFi");
    test_connection(host, 8003, "蜂窝");
    test_connection(host, 8004, "卫星");
    
    return 0;
}