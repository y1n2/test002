#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// 链路类型定义
typedef enum {
    LINK_ETHERNET = 1,
    LINK_WIFI = 2,
    LINK_CELLULAR = 3,
    LINK_SATELLITE = 4
} link_type_t;

int test_link_communication(const char *host, int port, link_type_t link_type) {
    printf("测试连接到 %s:%d (链路类型: %d)\n", host, port, link_type);
    
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        printf("创建socket失败: %s\n", strerror(errno));
        return -1;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        printf("无效的IP地址: %s\n", host);
        close(sock_fd);
        return -1;
    }
    
    // 连接到服务器
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("连接失败: %s\n", strerror(errno));
        close(sock_fd);
        return -1;
    }
    
    printf("✅ 成功连接到链路模拟器\n");
    
    // 创建测试消息 (模拟 cm_send_to_link_socket 的格式)
    const char *test_content = "TEST_MADR_MESSAGE_FROM_DIAMETER_SERVER";
    size_t content_size = strlen(test_content);
    
    // 消息格式：[链路类型:1字节][消息长度:4字节][消息内容]
    size_t total_size = 1 + 4 + content_size;
    char *buffer = malloc(total_size);
    if (!buffer) {
        printf("分配缓冲区失败\n");
        close(sock_fd);
        return -1;
    }
    
    // 填充消息
    buffer[0] = (char)link_type;  // 链路类型
    uint32_t msg_len = htonl(content_size);  // 消息长度（网络字节序）
    memcpy(buffer + 1, &msg_len, 4);
    memcpy(buffer + 5, test_content, content_size);  // 消息内容
    
    // 发送消息
    ssize_t sent = send(sock_fd, buffer, total_size, 0);
    if (sent != (ssize_t)total_size) {
        printf("发送失败: sent=%zd, expected=%zu, error=%s\n", 
               sent, total_size, strerror(errno));
        free(buffer);
        close(sock_fd);
        return -1;
    }
    
    printf("📤 发送测试消息: link_type=%d, size=%zu\n", link_type, content_size);
    printf("📤 消息内容: %s\n", test_content);
    
    // 等待一下，让链路模拟器处理消息
    sleep(1);
    
    free(buffer);
    close(sock_fd);
    printf("✅ 测试完成\n\n");
    return 0;
}

int main() {
    printf("=== 测试链路模拟器通信 ===\n\n");
    
    // 测试所有链路类型
    struct {
        int port;
        link_type_t type;
        const char* name;
    } links[] = {
        {8001, LINK_ETHERNET, "以太网"},
        {8002, LINK_WIFI, "WiFi"},
        {8003, LINK_CELLULAR, "蜂窝"},
        {8004, LINK_SATELLITE, "卫星"}
    };
    
    int num_links = sizeof(links) / sizeof(links[0]);
    int success_count = 0;
    
    for (int i = 0; i < num_links; i++) {
        printf("测试 %s 链路...\n", links[i].name);
        if (test_link_communication("127.0.0.1", links[i].port, links[i].type) == 0) {
            success_count++;
        }
        sleep(1); // 短暂延迟
    }
    
    printf("=== 测试结果 ===\n");
    printf("成功: %d/%d 链路\n", success_count, num_links);
    
    return (success_count == num_links) ? 0 : 1;
}