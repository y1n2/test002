#ifndef LINK_SIMULATOR_H
#define LINK_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>

#define MAX_LINKS 4
#define DEFAULT_PORT 30000
#define BUFFER_SIZE 4096

// 链路类型枚举
typedef enum {
    LINK_ETHERNET = 0,
    LINK_WIFI = 1,
    LINK_CELLULAR = 2,
    LINK_SATELLITE = 3
} LinkType;

// 链路配置结构
typedef struct {
    LinkType type;
    const char* name;
    int port;
    int bandwidth;      // Mbps
    int latency;        // ms
    int reliability;    // 0-100
    int signal_strength; // 0-100
} LinkConfig;

// 链路状态结构
typedef struct {
    int id;
    LinkType type;
    int socket;
    int server_socket;
    struct sockaddr_in addr;
    int connected;
    LinkConfig config;
    pthread_t thread;
    time_t last_activity;
} Link;

// 函数声明
void init_links();
void* link_thread(void* arg);
int create_server_socket(int port);
void handle_new_connection(int server_fd, int link_id);
void handle_link_data(int link_id);
void simulate_link_characteristics(int link_id, char* buffer, int size);
void signal_handler(int sig);
void cleanup();
void show_link_status();
void print_usage(char *program_name);

#endif /* LINK_SIMULATOR_H */