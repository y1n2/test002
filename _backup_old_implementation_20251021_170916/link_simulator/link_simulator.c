#include "link_simulator.h"

// 全局变量
Link links[MAX_LINKS];
int running = 1;

// 链路配置（基于magic_server.conf）
LinkConfig link_configs[MAX_LINKS] = {
    {LINK_ETHERNET, "以太网", 10001, 100, 1, 99, 100},
    {LINK_WIFI, "WiFi", 10002, 54, 5, 90, 80},
    {LINK_CELLULAR, "蜂窝", 10003, 10, 50, 80, 70},
    {LINK_SATELLITE, "卫星", 10004, 2, 500, 70, 60}
};

// 初始化链路
void init_links() {
    for (int i = 0; i < MAX_LINKS; i++) {
        links[i].id = i + 1;
        links[i].type = link_configs[i].type;
        links[i].socket = -1;
        links[i].server_socket = -1;
        links[i].connected = 0;
        links[i].config = link_configs[i];
        links[i].last_activity = time(NULL);
        
        printf("链路模拟器: 初始化链路 %d (%s) - 端口:%d, 带宽:%dMbps, 延迟:%dms, 可靠性:%d%%, 信号强度:%d%%\n",
               links[i].id, links[i].config.name, links[i].config.port,
               links[i].config.bandwidth, links[i].config.latency,
               links[i].config.reliability, links[i].config.signal_strength);
    }
}

// 创建监听套接字
int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // 创建套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("链路模拟器: 套接字创建失败");
        return -1;
    }
    
    // 设置套接字选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("链路模拟器: 设置套接字选项失败");
        close(server_fd);
        return -1;
    }
    
    // 绑定地址和端口
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("链路模拟器: 绑定失败");
        close(server_fd);
        return -1;
    }
    
    // 开始监听
    if (listen(server_fd, 10) < 0) {
        perror("链路模拟器: 监听失败");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

// 模拟链路特性
void simulate_link_characteristics(int link_id, char* buffer, int size) {
    if (link_id < 0 || link_id >= MAX_LINKS) return;
    
    LinkConfig* config = &links[link_id].config;
    
    // 模拟延迟
    if (config->latency > 0) {
        usleep(config->latency * 1000); // 转换为微秒
    }
    
    // 模拟可靠性（随机丢包）
    if (config->reliability < 100) {
        int random_val = rand() % 100;
        if (random_val >= config->reliability) {
            printf("链路模拟器: 链路 %d (%s) 模拟丢包\n", 
                   links[link_id].id, config->name);
            return; // 模拟丢包
        }
    }
    
    // 模拟带宽限制（简单的延迟模拟）
    if (config->bandwidth > 0 && size > 0) {
        // 计算传输时间（微秒）
        int transmission_time = (size * 8 * 1000) / (config->bandwidth * 1000); // 微秒
        if (transmission_time > 0) {
            usleep(transmission_time);
        }
    }
}

// 处理新连接
void handle_new_connection(int server_fd, int link_id) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int new_socket;
    
    if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("链路模拟器: 接受连接失败");
        }
        return;
    }
    
    if (links[link_id].connected) {
        printf("链路模拟器: 链路 %d (%s) 已有连接，拒绝新连接\n", 
               links[link_id].id, links[link_id].config.name);
        close(new_socket);
        return;
    }
    
    // 分配链路
    links[link_id].socket = new_socket;
    links[link_id].addr = client_addr;
    links[link_id].connected = 1;
    links[link_id].last_activity = time(NULL);
    
    printf("链路模拟器: 客户端 %s:%d 连接到链路 %d (%s)\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), 
           links[link_id].id, links[link_id].config.name);
}

// 处理链路数据
void handle_link_data(int link_id) {
    if (link_id < 0 || link_id >= MAX_LINKS || !links[link_id].connected) {
        return;
    }
    
    fd_set read_fds;
    struct timeval tv;
    
    FD_ZERO(&read_fds);
    FD_SET(links[link_id].socket, &read_fds);
    
    // 设置超时
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms
    
    // 检查是否有数据可读
    int activity = select(links[link_id].socket + 1, &read_fds, NULL, NULL, &tv);
    
    if (activity < 0 && errno != EINTR) {
        perror("链路模拟器: select 错误");
        return;
    }
    
    if (activity > 0 && FD_ISSET(links[link_id].socket, &read_fds)) {
        char buffer[BUFFER_SIZE];
        int valread = read(links[link_id].socket, buffer, BUFFER_SIZE);
        
        if (valread <= 0) {
            // 连接关闭或错误
            printf("链路模拟器: 链路 %d (%s) 断开连接\n", 
                   links[link_id].id, links[link_id].config.name);
            close(links[link_id].socket);
            links[link_id].socket = -1;
            links[link_id].connected = 0;
        } else {
            // 更新活动时间
            links[link_id].last_activity = time(NULL);
            
            // 处理接收到的数据
            printf("链路模拟器: 从链路 %d (%s) 接收到 %d 字节数据\n", 
                   links[link_id].id, links[link_id].config.name, valread);
            
            // 模拟链路特性
            simulate_link_characteristics(link_id, buffer, valread);
            
            // 构造响应数据
            char response[BUFFER_SIZE];
            memcpy(response, buffer, valread);
            
            // 如果是查询消息，修改第一个字节表示这是响应
            if (valread > 0 && (buffer[0] & 0x0F) == 0x01) {
                response[0] = (response[0] & 0xF0) | 0x02; // 设置为响应类型
            }
            
            // 模拟发送时的链路特性
            simulate_link_characteristics(link_id, response, valread);
            
            // 发送响应
            int sent = write(links[link_id].socket, response, valread);
            if (sent > 0) {
                printf("链路模拟器: 向链路 %d (%s) 发送了 %d 字节响应\n", 
                       links[link_id].id, links[link_id].config.name, sent);
            }
        }
    }
}

// 链路线程函数
void* link_thread(void* arg) {
    int link_id = *(int*)arg;
    free(arg); // 释放分配的内存
    
    // 创建该链路的服务器套接字
    int server_fd = create_server_socket(links[link_id].config.port);
    if (server_fd < 0) {
        printf("链路模拟器: 无法为链路 %d (%s) 创建服务器套接字\n", 
               links[link_id].id, links[link_id].config.name);
        return NULL;
    }
    
    links[link_id].server_socket = server_fd;
    printf("链路模拟器: 链路 %d (%s) 在端口 %d 上启动\n", 
           links[link_id].id, links[link_id].config.name, links[link_id].config.port);
    
    // 主循环
    while (running) {
        fd_set read_fds;
        struct timeval tv;
        
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        // 设置超时
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // 500ms
        
        // 检查新连接
        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            perror("链路模拟器: select 错误");
            break;
        }
        
        // 处理新连接
        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            handle_new_connection(server_fd, link_id);
        }
        
        // 处理链路数据
        handle_link_data(link_id);
    }
    
    // 清理
    if (links[link_id].connected) {
        close(links[link_id].socket);
        links[link_id].connected = 0;
    }
    close(server_fd);
    
    return NULL;
}

// 信号处理函数
void signal_handler(int sig) {
    printf("链路模拟器: 接收到信号 %d，准备退出\n", sig);
    running = 0;
}

// 清理资源
void cleanup() {
    for (int i = 0; i < MAX_LINKS; i++) {
        if (links[i].connected) {
            close(links[i].socket);
            links[i].socket = -1;
            links[i].connected = 0;
        }
        if (links[i].server_socket >= 0) {
            close(links[i].server_socket);
            links[i].server_socket = -1;
        }
    }
    printf("链路模拟器: 已清理所有资源\n");
}

// 显示链路状态
void show_link_status() {
    printf("链路模拟器: 当前链路状态:\n");
    for (int i = 0; i < MAX_LINKS; i++) {
        time_t now = time(NULL);
        printf("  链路 %d (%s): %s, 端口:%d, 最后活动:%lds前\n", 
               links[i].id, links[i].config.name,
               links[i].connected ? "已连接" : "未连接",
               links[i].config.port,
               now - links[i].last_activity);
    }
}

void print_usage(char *program_name) {
    printf("用法: %s [选项]\n", program_name);
    printf("选项:\n");
    printf("  -h         显示此帮助信息\n");
    printf("  -v         显示详细日志\n");
    printf("\n链路配置:\n");
    for (int i = 0; i < MAX_LINKS; i++) {
        printf("  链路 %d (%s): 端口 %d, 带宽 %dMbps, 延迟 %dms\n",
               i+1, link_configs[i].name, link_configs[i].port,
               link_configs[i].bandwidth, link_configs[i].latency);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    int opt;
    
    // 解析命令行参数
    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "未知选项: %c\n", opt);
                print_usage(argv[0]);
                break;
        }
    }
    
    printf("链路模拟器: 启动 (详细模式: %s)\n", verbose ? "开启" : "关闭");
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化随机数种子
    srand(time(NULL));
    
    // 初始化链路
    init_links();
    
    // 为每个链路创建线程
    for (int i = 0; i < MAX_LINKS; i++) {
        // 为每个线程分配独立的link_id内存
        int* link_id = malloc(sizeof(int));
        *link_id = i;
        if (pthread_create(&links[i].thread, NULL, link_thread, link_id) != 0) {
            perror("链路模拟器: 创建线程失败");
            free(link_id);
            cleanup();
            exit(EXIT_FAILURE);
        }
    }
    
    printf("链路模拟器: 所有链路已启动，等待连接...\n");
    
    // 主循环 - 定期显示状态
    while (running) {
        sleep(5);
        if (verbose) {
            show_link_status();
        }
    }
    
    // 等待所有线程结束
    for (int i = 0; i < MAX_LINKS; i++) {
        pthread_join(links[i].thread, NULL);
    }
    
    // 清理资源
    cleanup();
    
    return 0;
}