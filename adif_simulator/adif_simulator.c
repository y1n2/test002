/**
 * @file adif_simulator.c
 * @brief ADIF (Aircraft Data Interface Function) 模拟器服务
 * @description 基于 ARINC 834-1 ADBP 协议的飞机数据接口模拟器
 *
 * 本程序模拟 ADIF 服务器，为 MAGIC 系统提供飞机状态数据。
 * 提供 CLI 界面，允许用户手动输入参数模拟飞机飞行状态。
 *
 * 用法: ./adif_simulator [端口号]
 *
 * CLI 命令:
 *   wow <0|1>                    - 设置 Weight on Wheels (0=空中, 1=地面)
 *   phase <GATE|TAXI|...>        - 设置飞行阶段
 *   pos <lat> <lon> <alt>        - 设置位置 (纬度, 经度, 高度ft)
 *   speed <gs> <vs>              - 设置地速(kts)和垂直速度(ft/min)
 *   tail <尾号>                  - 设置飞机尾号
 *   auto <scenario>              - 自动执行飞行场景
 *   status                       - 显示当前状态
 *   help                         - 显示帮助
 *   quit                         - 退出
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

#define ADIF_DEFAULT_PORT       4000
#define ADIF_MAX_CLIENTS        10
#define ADIF_XML_BUFFER_SIZE    4096
#define ADIF_REFRESH_PERIOD_MS  1000

/*===========================================================================
 * 飞行阶段枚举
 *===========================================================================*/

typedef enum {
    PHASE_UNKNOWN   = 0,
    PHASE_GATE      = 1,
    PHASE_TAXI      = 2,
    PHASE_TAKEOFF   = 3,
    PHASE_CLIMB     = 4,
    PHASE_CRUISE    = 5,
    PHASE_DESCENT   = 6,
    PHASE_APPROACH  = 7,
    PHASE_LANDING   = 8
} FlightPhase;

static const char* phase_names[] = {
    "UNKNOWN", "GATE", "TAXI", "TAKE OFF", "CLIMB", 
    "CRUISE", "DESCENT", "APPROACH", "LANDING"
};

/*===========================================================================
 * 飞机状态结构体
 *===========================================================================*/

typedef struct {
    /* Weight on Wheels */
    bool        wow_on_ground;      /* true=地面, false=空中 */
    
    /* 位置信息 */
    double      latitude;           /* 纬度 (度) */
    double      longitude;          /* 经度 (度) */
    double      altitude_ft;        /* 气压高度 (英尺) */
    
    /* 速度信息 */
    double      ground_speed_kts;   /* 地速 (节) */
    double      vertical_speed_fpm; /* 垂直速度 (英尺/分钟) */
    
    /* 飞行阶段 */
    FlightPhase phase;
    
    /* 飞机识别 */
    char        tail_number[16];
    
    /* 数据有效性 */
    int         validity;           /* 1=有效 */
    
    /* 互斥锁 */
    pthread_mutex_t mutex;
} AircraftState;

/*===========================================================================
 * 订阅客户端结构体
 *===========================================================================*/

typedef struct {
    int         sync_sock;          /* 同步控制连接 */
    int         async_sock;         /* 异步数据推送连接 */
    uint16_t    async_port;         /* 客户端异步端口 */
    uint32_t    refresh_period_ms;  /* 刷新周期 */
    bool        subscribed;         /* 是否已订阅 */
    struct sockaddr_in client_addr; /* 客户端地址 */
} SubscribedClient;

/*===========================================================================
 * 全局变量
 *===========================================================================*/

static volatile bool g_running = true;
static AircraftState g_state;
static SubscribedClient g_clients[ADIF_MAX_CLIENTS];
static int g_num_clients = 0;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_server_sock = -1;
static uint16_t g_server_port = ADIF_DEFAULT_PORT;

/*===========================================================================
 * 信号处理
 *===========================================================================*/

static void signal_handler(int signum)
{
    (void)signum;
    printf("\n[ADIF] Shutting down...\n");
    g_running = false;
}

/*===========================================================================
 * 辅助函数
 *===========================================================================*/

static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static const char* phase_to_string(FlightPhase phase)
{
    if (phase >= 0 && phase <= PHASE_LANDING) {
        return phase_names[phase];
    }
    return "UNKNOWN";
}

static FlightPhase string_to_phase(const char* str)
{
    if (!str) return PHASE_UNKNOWN;
    
    for (int i = 0; i <= PHASE_LANDING; i++) {
        if (strcasecmp(str, phase_names[i]) == 0) {
            return (FlightPhase)i;
        }
    }
    
    /* 处理 "TAKE OFF" 的变体 */
    if (strcasecmp(str, "TAKEOFF") == 0) return PHASE_TAKEOFF;
    
    return PHASE_UNKNOWN;
}

/*===========================================================================
 * XML 生成
 *===========================================================================*/

static int generate_publish_xml(char* buffer, size_t buffer_size, AircraftState* state)
{
    uint64_t timestamp = get_timestamp_ms();
    
    pthread_mutex_lock(&state->mutex);
    
    int len = snprintf(buffer, buffer_size,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<method name=\"publishAvionicParameters\">\n"
        "    <parameters>\n"
        "        <parameter name=\"WeightOnWheels\" value=\"%d\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"Latitude\" value=\"%.6f\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"Longitude\" value=\"%.6f\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"BaroCorrectedAltitude\" value=\"%.0f\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"FlightPhase\" value=\"%s\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"AircraftTailNumber\" value=\"%s\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"GroundSpeed\" value=\"%.1f\" validity=\"%d\" time=\"%llu\"/>\n"
        "        <parameter name=\"VerticalSpeed\" value=\"%.0f\" validity=\"%d\" time=\"%llu\"/>\n"
        "    </parameters>\n"
        "</method>\n",
        state->wow_on_ground ? 0 : 1, state->validity, (unsigned long long)timestamp,
        state->latitude, state->validity, (unsigned long long)timestamp,
        state->longitude, state->validity, (unsigned long long)timestamp,
        state->altitude_ft, state->validity, (unsigned long long)timestamp,
        phase_to_string(state->phase), state->validity, (unsigned long long)timestamp,
        state->tail_number, state->validity, (unsigned long long)timestamp,
        state->ground_speed_kts, state->validity, (unsigned long long)timestamp,
        state->vertical_speed_fpm, state->validity, (unsigned long long)timestamp
    );
    
    pthread_mutex_unlock(&state->mutex);
    
    return len;
}

static int generate_subscribe_response(char* buffer, size_t buffer_size, int errorcode)
{
    uint64_t timestamp = get_timestamp_ms();
    
    return snprintf(buffer, buffer_size,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<response method=\"subscribeAvionicParametersResponse\" errorcode=\"%d\">\n"
        "    <parameters>\n"
        "        <parameter name=\"WeightOnWheels\" validity=\"1\" type=\"6\" value=\"0\" time=\"%llu\"/>\n"
        "    </parameters>\n"
        "</response>\n",
        errorcode, (unsigned long long)timestamp
    );
}

/*===========================================================================
 * 客户端管理
 *===========================================================================*/

static int parse_subscribe_request(const char* xml, uint16_t* async_port, uint32_t* refresh_period)
{
    /* 解析 publishport */
    const char* pos = strstr(xml, "publishport");
    if (pos) {
        pos = strstr(pos, "value=\"");
        if (pos) {
            pos += 7;
            *async_port = (uint16_t)atoi(pos);
        }
    }
    
    /* 解析 refreshperiod */
    pos = strstr(xml, "refreshperiod");
    if (pos) {
        pos = strstr(pos, "value=\"");
        if (pos) {
            pos += 7;
            *refresh_period = (uint32_t)atoi(pos);
        }
    }
    
    return 0;
}

static int add_client(int sync_sock, struct sockaddr_in* addr, uint16_t async_port, uint32_t refresh_period)
{
    pthread_mutex_lock(&g_clients_mutex);
    
    if (g_num_clients >= ADIF_MAX_CLIENTS) {
        pthread_mutex_unlock(&g_clients_mutex);
        return -1;
    }
    
    SubscribedClient* client = &g_clients[g_num_clients];
    client->sync_sock = sync_sock;
    client->async_port = async_port;
    client->refresh_period_ms = refresh_period > 0 ? refresh_period : ADIF_REFRESH_PERIOD_MS;
    client->subscribed = true;
    memcpy(&client->client_addr, addr, sizeof(struct sockaddr_in));
    
    /* 连接到客户端的异步端口 */
    client->async_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->async_sock >= 0) {
        struct sockaddr_in async_addr;
        memset(&async_addr, 0, sizeof(async_addr));
        async_addr.sin_family = AF_INET;
        async_addr.sin_addr = addr->sin_addr;
        async_addr.sin_port = htons(async_port);
        
        if (connect(client->async_sock, (struct sockaddr*)&async_addr, sizeof(async_addr)) < 0) {
            printf("[ADIF] Warning: Failed to connect to client async port %u\n", async_port);
            close(client->async_sock);
            client->async_sock = -1;
        } else {
            printf("[ADIF] Connected to client async port %u\n", async_port);
        }
    }
    
    g_num_clients++;
    pthread_mutex_unlock(&g_clients_mutex);
    
    return 0;
}

/*===========================================================================
 * 数据推送线程
 *===========================================================================*/

static void* publish_thread(void* arg)
{
    (void)arg;
    char buffer[ADIF_XML_BUFFER_SIZE];
    
    printf("[ADIF] Publish thread started\n");
    
    while (g_running) {
        usleep(ADIF_REFRESH_PERIOD_MS * 1000);
        
        if (!g_running) break;
        
        int len = generate_publish_xml(buffer, sizeof(buffer), &g_state);
        if (len <= 0) continue;
        
        pthread_mutex_lock(&g_clients_mutex);
        
        for (int i = 0; i < g_num_clients; i++) {
            if (g_clients[i].subscribed && g_clients[i].async_sock >= 0) {
                ssize_t sent = send(g_clients[i].async_sock, buffer, len, MSG_NOSIGNAL);
                if (sent <= 0) {
                    printf("[ADIF] Client %d disconnected\n", i);
                    close(g_clients[i].async_sock);
                    g_clients[i].async_sock = -1;
                    g_clients[i].subscribed = false;
                }
            }
        }
        
        pthread_mutex_unlock(&g_clients_mutex);
    }
    
    printf("[ADIF] Publish thread exiting\n");
    return NULL;
}

/*===========================================================================
 * 服务器线程
 *===========================================================================*/

static void* server_thread(void* arg)
{
    (void)arg;
    char buffer[ADIF_XML_BUFFER_SIZE];
    
    printf("[ADIF] Server thread started on port %u\n", g_server_port);
    
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(g_server_sock, &read_fds);
        
        struct timeval tv = {1, 0};
        int ret = select(g_server_sock + 1, &read_fds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (ret == 0) continue;
        
        if (FD_ISSET(g_server_sock, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_sock = accept(g_server_sock, (struct sockaddr*)&client_addr, &addr_len);
            if (client_sock < 0) {
                continue;
            }
            
            printf("[ADIF] New connection from %s:%u\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            /* 接收订阅请求 */
            ssize_t n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                
                if (strstr(buffer, "subscribeAvionicParameters")) {
                    uint16_t async_port = 64001;
                    uint32_t refresh_period = ADIF_REFRESH_PERIOD_MS;
                    
                    parse_subscribe_request(buffer, &async_port, &refresh_period);
                    
                    printf("[ADIF] Subscription request: async_port=%u, refresh=%ums\n",
                           async_port, refresh_period);
                    
                    /* 发送响应 */
                    char response[1024];
                    int resp_len = generate_subscribe_response(response, sizeof(response), 0);
                    send(client_sock, response, resp_len, 0);
                    
                    /* 添加客户端 */
                    add_client(client_sock, &client_addr, async_port, refresh_period);
                    
                    printf("[ADIF] Client subscribed successfully\n");
                }
            }
        }
    }
    
    printf("[ADIF] Server thread exiting\n");
    return NULL;
}

/*===========================================================================
 * CLI 命令处理
 *===========================================================================*/

static void print_status(void)
{
    pthread_mutex_lock(&g_state.mutex);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         ADIF Simulator - Aircraft State                    ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Tail Number:     %-40s ║\n", g_state.tail_number);
    printf("║  Weight on Wheels: %-39s ║\n", g_state.wow_on_ground ? "ON GROUND" : "IN AIR");
    printf("║  Flight Phase:    %-40s ║\n", phase_to_string(g_state.phase));
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Position:                                                 ║\n");
    printf("║    Latitude:      %+10.6f °                            ║\n", g_state.latitude);
    printf("║    Longitude:     %+11.6f °                           ║\n", g_state.longitude);
    printf("║    Altitude:      %8.0f ft                             ║\n", g_state.altitude_ft);
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Speed:                                                    ║\n");
    printf("║    Ground Speed:  %8.1f kts                            ║\n", g_state.ground_speed_kts);
    printf("║    Vertical Speed: %+8.0f ft/min                        ║\n", g_state.vertical_speed_fpm);
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Validity:        %d (1=Normal)                            ║\n", g_state.validity);
    printf("║  Subscribed Clients: %d                                     ║\n", g_num_clients);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    pthread_mutex_unlock(&g_state.mutex);
}

static void print_help(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║             ADIF Simulator - Command Reference             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  wow <0|1>              Set Weight on Wheels               ║\n");
    printf("║                         0=In Air, 1=On Ground              ║\n");
    printf("║  phase <name>           Set flight phase                   ║\n");
    printf("║                         GATE|TAXI|TAKEOFF|CLIMB|CRUISE|    ║\n");
    printf("║                         DESCENT|APPROACH|LANDING           ║\n");
    printf("║  pos <lat> <lon> <alt>  Set position                       ║\n");
    printf("║                         lat/lon in degrees, alt in feet    ║\n");
    printf("║  speed <gs> <vs>        Set ground speed (kts) and         ║\n");
    printf("║                         vertical speed (ft/min)            ║\n");
    printf("║  tail <number>          Set aircraft tail number           ║\n");
    printf("║  auto <scenario>        Run automatic flight scenario      ║\n");
    printf("║                         takeoff|cruise|landing|full        ║\n");
    printf("║  status                 Show current aircraft state        ║\n");
    printf("║  help                   Show this help message             ║\n");
    printf("║  quit                   Exit the simulator                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void cmd_wow(const char* arg)
{
    if (!arg) {
        printf("Usage: wow <0|1>\n");
        return;
    }
    
    int value = atoi(arg);
    pthread_mutex_lock(&g_state.mutex);
    g_state.wow_on_ground = (value != 0);
    pthread_mutex_unlock(&g_state.mutex);
    
    printf("[ADIF] Weight on Wheels: %s\n", g_state.wow_on_ground ? "ON GROUND" : "IN AIR");
}

static void cmd_phase(const char* arg)
{
    if (!arg) {
        printf("Usage: phase <GATE|TAXI|TAKEOFF|CLIMB|CRUISE|DESCENT|APPROACH|LANDING>\n");
        return;
    }
    
    FlightPhase phase = string_to_phase(arg);
    if (phase == PHASE_UNKNOWN) {
        printf("Unknown phase: %s\n", arg);
        return;
    }
    
    pthread_mutex_lock(&g_state.mutex);
    g_state.phase = phase;
    pthread_mutex_unlock(&g_state.mutex);
    
    printf("[ADIF] Flight Phase: %s\n", phase_to_string(phase));
}

static void cmd_pos(char* args)
{
    if (!args) {
        printf("Usage: pos <lat> <lon> <alt>\n");
        return;
    }
    
    double lat, lon, alt;
    if (sscanf(args, "%lf %lf %lf", &lat, &lon, &alt) != 3) {
        printf("Invalid position format. Usage: pos <lat> <lon> <alt>\n");
        return;
    }
    
    pthread_mutex_lock(&g_state.mutex);
    g_state.latitude = lat;
    g_state.longitude = lon;
    g_state.altitude_ft = alt;
    pthread_mutex_unlock(&g_state.mutex);
    
    printf("[ADIF] Position: %.6f, %.6f @ %.0f ft\n", lat, lon, alt);
}

static void cmd_speed(char* args)
{
    if (!args) {
        printf("Usage: speed <ground_speed_kts> <vertical_speed_fpm>\n");
        return;
    }
    
    double gs, vs;
    if (sscanf(args, "%lf %lf", &gs, &vs) != 2) {
        printf("Invalid speed format. Usage: speed <gs> <vs>\n");
        return;
    }
    
    pthread_mutex_lock(&g_state.mutex);
    g_state.ground_speed_kts = gs;
    g_state.vertical_speed_fpm = vs;
    pthread_mutex_unlock(&g_state.mutex);
    
    printf("[ADIF] Speed: GS=%.1f kts, VS=%.0f ft/min\n", gs, vs);
}

static void cmd_tail(const char* arg)
{
    if (!arg) {
        printf("Usage: tail <tail_number>\n");
        return;
    }
    
    pthread_mutex_lock(&g_state.mutex);
    strncpy(g_state.tail_number, arg, sizeof(g_state.tail_number) - 1);
    g_state.tail_number[sizeof(g_state.tail_number) - 1] = '\0';
    pthread_mutex_unlock(&g_state.mutex);
    
    printf("[ADIF] Tail Number: %s\n", g_state.tail_number);
}

static void auto_scenario_step(const char* desc, bool wow, FlightPhase phase,
                               double lat, double lon, double alt,
                               double gs, double vs, int delay_sec)
{
    printf("[AUTO] %s\n", desc);
    
    pthread_mutex_lock(&g_state.mutex);
    g_state.wow_on_ground = wow;
    g_state.phase = phase;
    g_state.latitude = lat;
    g_state.longitude = lon;
    g_state.altitude_ft = alt;
    g_state.ground_speed_kts = gs;
    g_state.vertical_speed_fpm = vs;
    pthread_mutex_unlock(&g_state.mutex);
    
    print_status();
    
    for (int i = delay_sec; i > 0 && g_running; i--) {
        printf("[AUTO] Next step in %d seconds...\r", i);
        fflush(stdout);
        sleep(1);
    }
    printf("                                    \r");
}

static void cmd_auto(const char* scenario)
{
    if (!scenario) {
        printf("Usage: auto <takeoff|cruise|landing|full>\n");
        return;
    }
    
    printf("\n[AUTO] Starting scenario: %s\n", scenario);
    printf("[AUTO] Press Ctrl+C to cancel\n\n");
    
    if (strcasecmp(scenario, "takeoff") == 0) {
        /* 起飞场景 */
        auto_scenario_step("Gate - Preparing for departure",
            true, PHASE_GATE, 33.9425, -118.4081, 120, 0, 0, 3);
        auto_scenario_step("Taxi - Moving to runway",
            true, PHASE_TAXI, 33.9430, -118.4070, 120, 15, 0, 3);
        auto_scenario_step("Takeoff roll",
            true, PHASE_TAKEOFF, 33.9440, -118.4050, 120, 120, 0, 2);
        auto_scenario_step("Liftoff! Climbing...",
            false, PHASE_CLIMB, 33.9450, -118.4020, 500, 180, 2500, 3);
        auto_scenario_step("Continuing climb",
            false, PHASE_CLIMB, 33.9500, -118.3900, 5000, 280, 2000, 3);
        printf("[AUTO] Takeoff scenario complete!\n");
        
    } else if (strcasecmp(scenario, "cruise") == 0) {
        /* 巡航场景 */
        auto_scenario_step("Cruise flight at FL350",
            false, PHASE_CRUISE, 35.0000, -115.0000, 35000, 450, 0, 5);
        auto_scenario_step("Cruise continuing",
            false, PHASE_CRUISE, 36.0000, -112.0000, 35000, 450, 0, 5);
        printf("[AUTO] Cruise scenario complete!\n");
        
    } else if (strcasecmp(scenario, "landing") == 0) {
        /* 降落场景 */
        auto_scenario_step("Beginning descent",
            false, PHASE_DESCENT, 34.5000, -118.2000, 25000, 350, -2000, 3);
        auto_scenario_step("Approach",
            false, PHASE_APPROACH, 33.9600, -118.3800, 3000, 160, -800, 3);
        auto_scenario_step("Final approach",
            false, PHASE_APPROACH, 33.9450, -118.4030, 500, 140, -500, 2);
        auto_scenario_step("Touchdown!",
            true, PHASE_LANDING, 33.9440, -118.4050, 120, 100, 0, 2);
        auto_scenario_step("Taxi to gate",
            true, PHASE_TAXI, 33.9430, -118.4070, 120, 15, 0, 2);
        auto_scenario_step("At gate",
            true, PHASE_GATE, 33.9425, -118.4081, 120, 0, 0, 1);
        printf("[AUTO] Landing scenario complete!\n");
        
    } else if (strcasecmp(scenario, "full") == 0) {
        /* 完整飞行场景 */
        printf("[AUTO] Running full flight scenario (gate to gate)\n\n");
        
        /* 起飞 */
        auto_scenario_step("Gate - Preparing for departure",
            true, PHASE_GATE, 33.9425, -118.4081, 120, 0, 0, 3);
        auto_scenario_step("Taxi to runway",
            true, PHASE_TAXI, 33.9440, -118.4050, 120, 15, 0, 3);
        auto_scenario_step("Takeoff roll",
            true, PHASE_TAKEOFF, 33.9450, -118.4020, 120, 140, 0, 2);
        auto_scenario_step("Liftoff!",
            false, PHASE_CLIMB, 33.9500, -118.3900, 2000, 200, 3000, 3);
        
        /* 爬升 */
        auto_scenario_step("Climbing to cruise altitude",
            false, PHASE_CLIMB, 34.0000, -118.0000, 15000, 350, 2000, 3);
        auto_scenario_step("Reaching cruise altitude",
            false, PHASE_CLIMB, 34.5000, -117.0000, 32000, 420, 1000, 3);
        
        /* 巡航 */
        auto_scenario_step("Cruise at FL350",
            false, PHASE_CRUISE, 35.0000, -115.0000, 35000, 450, 0, 5);
        auto_scenario_step("Cruise continuing",
            false, PHASE_CRUISE, 35.5000, -113.0000, 35000, 450, 0, 5);
        
        /* 下降 */
        auto_scenario_step("Top of descent",
            false, PHASE_DESCENT, 36.0000, -112.0000, 35000, 400, -1500, 3);
        auto_scenario_step("Descending",
            false, PHASE_DESCENT, 36.5000, -111.0000, 20000, 320, -2000, 3);
        
        /* 进近 */
        auto_scenario_step("Approach",
            false, PHASE_APPROACH, 36.8000, -110.5000, 8000, 220, -1000, 3);
        auto_scenario_step("Final approach",
            false, PHASE_APPROACH, 37.0000, -110.2000, 2000, 140, -600, 3);
        
        /* 着陆 */
        auto_scenario_step("Touchdown",
            true, PHASE_LANDING, 37.0500, -110.1000, 100, 100, 0, 2);
        auto_scenario_step("Taxi to gate",
            true, PHASE_TAXI, 37.0510, -110.0950, 100, 15, 0, 2);
        auto_scenario_step("At gate",
            true, PHASE_GATE, 37.0520, -110.0900, 100, 0, 0, 1);
        
        printf("\n[AUTO] Full flight scenario complete!\n");
        
    } else {
        printf("Unknown scenario: %s\n", scenario);
        printf("Available: takeoff, cruise, landing, full\n");
    }
}

static void process_command(char* line)
{
    if (!line || strlen(line) == 0) return;
    
    /* 添加到历史记录 */
    add_history(line);
    
    /* 解析命令 */
    char* cmd = strtok(line, " \t");
    if (!cmd) return;
    
    char* args = strtok(NULL, "");
    
    if (strcasecmp(cmd, "quit") == 0 || strcasecmp(cmd, "exit") == 0) {
        g_running = false;
    } else if (strcasecmp(cmd, "help") == 0) {
        print_help();
    } else if (strcasecmp(cmd, "status") == 0) {
        print_status();
    } else if (strcasecmp(cmd, "wow") == 0) {
        cmd_wow(args);
    } else if (strcasecmp(cmd, "phase") == 0) {
        cmd_phase(args);
    } else if (strcasecmp(cmd, "pos") == 0) {
        cmd_pos(args);
    } else if (strcasecmp(cmd, "speed") == 0) {
        cmd_speed(args);
    } else if (strcasecmp(cmd, "tail") == 0) {
        cmd_tail(args);
    } else if (strcasecmp(cmd, "auto") == 0) {
        cmd_auto(args);
    } else {
        printf("Unknown command: %s (type 'help' for commands)\n", cmd);
    }
}

/*===========================================================================
 * 初始化
 *===========================================================================*/

static int init_server(uint16_t port)
{
    g_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_sock < 0) {
        perror("socket");
        return -1;
    }
    
    int optval = 1;
    setsockopt(g_server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(g_server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_sock);
        return -1;
    }
    
    if (listen(g_server_sock, 5) < 0) {
        perror("listen");
        close(g_server_sock);
        return -1;
    }
    
    g_server_port = port;
    return 0;
}

static void init_default_state(void)
{
    memset(&g_state, 0, sizeof(g_state));
    pthread_mutex_init(&g_state.mutex, NULL);
    
    /* 默认状态: 在登机口 */
    g_state.wow_on_ground = true;
    g_state.phase = PHASE_GATE;
    g_state.latitude = 33.9425;     /* LAX */
    g_state.longitude = -118.4081;
    g_state.altitude_ft = 120;
    g_state.ground_speed_kts = 0;
    g_state.vertical_speed_fpm = 0;
    strcpy(g_state.tail_number, "N12345");
    g_state.validity = 1;
}

/*===========================================================================
 * 主函数
 *===========================================================================*/

int main(int argc, char* argv[])
{
    uint16_t port = ADIF_DEFAULT_PORT;
    
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     ADIF Simulator - Aircraft Data Interface Function      ║\n");
    printf("║     Based on ARINC 834-1 ADBP Protocol                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* 设置信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* 初始化默认飞机状态 */
    init_default_state();
    
    /* 初始化服务器 */
    if (init_server(port) < 0) {
        fprintf(stderr, "Failed to initialize server on port %u\n", port);
        return 1;
    }
    
    printf("[ADIF] Server listening on port %u\n", port);
    printf("[ADIF] Type 'help' for available commands\n\n");
    
    /* 启动服务器线程 */
    pthread_t server_tid;
    pthread_create(&server_tid, NULL, server_thread, NULL);
    
    /* 启动数据推送线程 */
    pthread_t publish_tid;
    pthread_create(&publish_tid, NULL, publish_thread, NULL);
    
    /* 显示初始状态 */
    print_status();
    
    /* CLI 主循环 */
    while (g_running) {
        char* line = readline("adif> ");
        if (!line) {
            /* EOF (Ctrl+D) */
            printf("\n");
            break;
        }
        
        process_command(line);
        free(line);
    }
    
    /* 清理 */
    g_running = false;
    
    pthread_join(server_tid, NULL);
    pthread_join(publish_tid, NULL);
    
    if (g_server_sock >= 0) {
        close(g_server_sock);
    }
    
    pthread_mutex_destroy(&g_state.mutex);
    
    printf("[ADIF] Simulator stopped\n");
    return 0;
}
