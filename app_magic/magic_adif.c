/**
 * @file magic_adif.c
 * @brief MAGIC Aircraft Data Interface Function (ADIF) 客户端实现
 * @description 基于 ARINC 834-1 ADBP 协议的飞机数据接口客户端
 *
 * 本模块实现 MAGIC 系统作为客户端订阅飞机状态数据的功能。
 * 使用 TCP/IP 连接到 ADIF 服务器，通过 XML 消息进行通信。
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 */

#include "magic_adif.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*===========================================================================
 * 内部宏定义
 *===========================================================================*/

#define LOG_PREFIX "[ADIF] "
#define LOG_INFO(fmt, ...)                                                     \
  fprintf(stdout, LOG_PREFIX "INFO: " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                    \
  fprintf(stderr, LOG_PREFIX "ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)                                                    \
  fprintf(stdout, LOG_PREFIX "DEBUG: " fmt "\n", ##__VA_ARGS__)

/*===========================================================================
 * 飞行阶段字符串映射
 *===========================================================================*/

static const char *flight_phase_strings[] = {
    "UNKNOWN",  /* FLIGHT_PHASE_UNKNOWN */
    "GATE",     /* FLIGHT_PHASE_GATE */
    "TAXI",     /* FLIGHT_PHASE_TAXI */
    "TAKE OFF", /* FLIGHT_PHASE_TAKEOFF */
    "CLIMB",    /* FLIGHT_PHASE_CLIMB */
    "CRUISE",   /* FLIGHT_PHASE_CRUISE */
    "DESCENT",  /* FLIGHT_PHASE_DESCENT */
    "APPROACH", /* FLIGHT_PHASE_APPROACH */
    "LANDING"   /* FLIGHT_PHASE_LANDING */
};

/**
 * @brief 将飞行阶段枚举转换为字符串。
 *
 * @param phase 飞行阶段枚举值。
 * @return const char* 对应的字符串描述 (如 "CRUISE", "LANDING" 等)。
 */
const char *adif_flight_phase_to_string(AdifFlightPhase phase) {
  if (phase >= 0 && phase <= FLIGHT_PHASE_LANDING) {
    return flight_phase_strings[phase];
  }
  return "UNKNOWN";
}

/**
 * @brief 将字符串转换为飞行阶段枚举。
 *
 * @param str 飞行阶段字符串 (不区分大小写)。
 * @return AdifFlightPhase 对应的枚举值，如果未知则返回 FLIGHT_PHASE_UNKNOWN。
 */
AdifFlightPhase adif_string_to_flight_phase(const char *str) {
  if (!str)
    return FLIGHT_PHASE_UNKNOWN;

  if (strcasecmp(str, "GATE") == 0)
    return FLIGHT_PHASE_GATE;
  if (strcasecmp(str, "TAXI") == 0)
    return FLIGHT_PHASE_TAXI;
  if (strcasecmp(str, "TAKE OFF") == 0)
    return FLIGHT_PHASE_TAKEOFF;
  if (strcasecmp(str, "TAKEOFF") == 0)
    return FLIGHT_PHASE_TAKEOFF;
  if (strcasecmp(str, "CLIMB") == 0)
    return FLIGHT_PHASE_CLIMB;
  if (strcasecmp(str, "CRUISE") == 0)
    return FLIGHT_PHASE_CRUISE;
  if (strcasecmp(str, "DESCENT") == 0)
    return FLIGHT_PHASE_DESCENT;
  if (strcasecmp(str, "APPROACH") == 0)
    return FLIGHT_PHASE_APPROACH;
  if (strcasecmp(str, "LANDING") == 0)
    return FLIGHT_PHASE_LANDING;

  return FLIGHT_PHASE_UNKNOWN;
}

/*===========================================================================
 * 策略引擎集成
 *===========================================================================*/

/**
 * @brief 将 ADIF 飞行阶段映射到策略引擎飞行阶段字符串。
 * @details 策略引擎可能使用不同的字符串集合，此函数负责转换。
 *
 * @param phase ADIF 飞行阶段枚举。
 * @return const char* 策略引擎使用的飞行阶段字符串。
 */
const char *adif_phase_to_policy_phase(AdifFlightPhase phase) {
  switch (phase) {
  case FLIGHT_PHASE_GATE:
    return "GATE";

  case FLIGHT_PHASE_TAXI:
    return "TAXI";

  case FLIGHT_PHASE_TAKEOFF:
    return "TAKEOFF";

  case FLIGHT_PHASE_CLIMB:
    return "CLIMB";

  case FLIGHT_PHASE_CRUISE:
    return "CRUISE";

  case FLIGHT_PHASE_DESCENT:
    return "DESCENT";

  case FLIGHT_PHASE_APPROACH:
    return "APPROACH";

  case FLIGHT_PHASE_LANDING:
    return "LANDING";

  default:
    return "GATE";
  }
}

/**
 * @brief 检查当前飞行阶段变化是否应该触发路由重新评估。
 * @details 某些飞行阶段变化（如从 CRUISE 到 DESCENT）可能需要切换链路（如从
 * SATCOM 切回 WiFi）。 将飞行阶段分为几组，组内变化通常不触发重评估。
 *
 * @param old_phase 旧飞行阶段。
 * @param new_phase 新飞行阶段。
 * @return true 需要重新评估路由, false 不需要。
 */
bool adif_should_reevaluate_routing(AdifFlightPhase old_phase,
                                    AdifFlightPhase new_phase) {
  if (old_phase == new_phase)
    return false;

  /* 飞行阶段组定义 - 组内切换不需要重新评估 */
  /* 组1: 地面操作 (GATE, TAXI) */
  /* 组2: 起飞 (TAKEOFF) */
  /* 组3: 空中上升 (CLIMB) */
  /* 组4: 巡航 (CRUISE) */
  /* 组5: 下降进近 (DESCENT, APPROACH, LANDING) */

  int old_group = 0, new_group = 0;

  switch (old_phase) {
  case FLIGHT_PHASE_GATE:
  case FLIGHT_PHASE_TAXI:
    old_group = 1;
    break;
  case FLIGHT_PHASE_TAKEOFF:
    old_group = 2;
    break;
  case FLIGHT_PHASE_CLIMB:
    old_group = 3;
    break;
  case FLIGHT_PHASE_CRUISE:
    old_group = 4;
    break;
  case FLIGHT_PHASE_DESCENT:
  case FLIGHT_PHASE_APPROACH:
  case FLIGHT_PHASE_LANDING:
    old_group = 5;
    break;
  default:
    old_group = 0;
  }

  switch (new_phase) {
  case FLIGHT_PHASE_GATE:
  case FLIGHT_PHASE_TAXI:
    new_group = 1;
    break;
  case FLIGHT_PHASE_TAKEOFF:
    new_group = 2;
    break;
  case FLIGHT_PHASE_CLIMB:
    new_group = 3;
    break;
  case FLIGHT_PHASE_CRUISE:
    new_group = 4;
    break;
  case FLIGHT_PHASE_DESCENT:
  case FLIGHT_PHASE_APPROACH:
  case FLIGHT_PHASE_LANDING:
    new_group = 5;
    break;
  default:
    new_group = 0;
  }

  return (old_group != new_group);
}

/*===========================================================================
 * XML 生成与解析
 *===========================================================================*/

/**
 * @brief 生成订阅请求 XML。
 * @details 构建符合 ARINC 834 协议的 subscribeAvionicParameters XML 消息。
 *
 * @param buffer 输出缓冲区。
 * @param buffer_size 缓冲区大小。
 * @param async_port 本地用于接收异步推送数据的端口。
 * @param refresh_period_ms 数据刷新周期 (毫秒)。
 * @return int 生成的 XML 长度，如果缓冲区不足则返回 -1。
 */
int adif_generate_subscribe_xml(char *buffer, size_t buffer_size,
                                uint16_t async_port,
                                uint32_t refresh_period_ms) {
  if (!buffer || buffer_size < 512) {
    return -1;
  }

  int len = snprintf(
      buffer, buffer_size,
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<subscribeAvionicParameters method=\"subscribeAvionicParameters\">\n"
      "    <publishport value=\"%u\"/>\n"
      "    <refreshperiod value=\"%u\"/>\n"
      "    <parameters>\n"
      "        <parameter name=\"WeightOnWheels\"/>\n"
      "        <parameter name=\"Latitude\"/>\n"
      "        <parameter name=\"Longitude\"/>\n"
      "        <parameter name=\"BaroCorrectedAltitude\"/>\n"
      "        <parameter name=\"FlightPhase\"/>\n"
      "        <parameter name=\"AircraftTailNumber\"/>\n"
      "        <parameter name=\"GroundSpeed\"/>\n"
      "        <parameter name=\"VerticalSpeed\"/>\n"
      "    </parameters>\n"
      "</subscribeAvionicParameters>\n",
      async_port, refresh_period_ms);

  if (len >= (int)buffer_size) {
    return -1;
  }

  return len;
}

/**
 * @brief XML 属性提取辅助函数。
 * @details 在 XML 字符串中查找指定标签的特定属性值。
 *          注意：这是一个简单的字符串匹配实现，不依赖外部 XML 库。
 *
 * @param xml XML 输入字符串。
 * @param tag 标签名称 (如 "WeightOnWheels")。
 * @param attr 属性名称 (如 "value", "validity")。
 * @param value [out] 输出属性值的缓冲区。
 * @param value_size 缓冲区大小。
 * @return int 成功返回 0，失败返回 -1。
 */
static int extract_xml_attr(const char *xml, const char *tag, const char *attr,
                            char *value, size_t value_size) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "name=\"%s\"", tag);

  const char *pos = strstr(xml, pattern);
  if (!pos)
    return -1;

  snprintf(pattern, sizeof(pattern), "%s=\"", attr);
  pos = strstr(pos, pattern);
  if (!pos)
    return -1;

  pos += strlen(pattern);
  const char *end = strchr(pos, '"');
  if (!end)
    return -1;

  size_t len = end - pos;
  if (len >= value_size)
    len = value_size - 1;

  strncpy(value, pos, len);
  value[len] = '\0';

  return 0;
}

/**
 * @brief 解析 ADIF 发布的 XML 数据。
 * @details 解析从 ADIF Server 接收到的 XML 状态报告，更新 AdifAircraftState
 * 结构体。
 *
 * @param xml 接收到的 XML 字符串。
 * @param state [out] 用于存储解析结果的飞机状态结构体指针。
 * @return int 成功返回 0，失败返回 -1。
 */
int adif_parse_publish_xml(const char *xml, AdifAircraftState *state) {
  if (!xml || !state)
    return -1;

  char value_str[64];
  char validity_str[8];
  char time_str[32];

  /* 解析 WeightOnWheels */
  if (extract_xml_attr(xml, "WeightOnWheels", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->wow.on_ground = (atoi(value_str) == 0);

    if (extract_xml_attr(xml, "WeightOnWheels", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->wow.validity = (AdifValidity)atoi(validity_str);
    }
    if (extract_xml_attr(xml, "WeightOnWheels", "time", time_str,
                         sizeof(time_str)) == 0) {
      state->wow.timestamp_ms = strtoull(time_str, NULL, 10);
    }
  }

  /* 解析 Latitude */
  if (extract_xml_attr(xml, "Latitude", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->position.latitude = atof(value_str);
    if (extract_xml_attr(xml, "Latitude", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->position.lat_validity = (AdifValidity)atoi(validity_str);
    }
  }

  /* 解析 Longitude */
  if (extract_xml_attr(xml, "Longitude", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->position.longitude = atof(value_str);
    if (extract_xml_attr(xml, "Longitude", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->position.lon_validity = (AdifValidity)atoi(validity_str);
    }
  }

  /* 解析 BaroCorrectedAltitude */
  if (extract_xml_attr(xml, "BaroCorrectedAltitude", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->position.altitude_ft = atof(value_str);
    if (extract_xml_attr(xml, "BaroCorrectedAltitude", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->position.alt_validity = (AdifValidity)atoi(validity_str);
    }
    if (extract_xml_attr(xml, "BaroCorrectedAltitude", "time", time_str,
                         sizeof(time_str)) == 0) {
      state->position.timestamp_ms = strtoull(time_str, NULL, 10);
    }
  }

  /* 解析 FlightPhase */
  if (extract_xml_attr(xml, "FlightPhase", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->flight_phase.phase = adif_string_to_flight_phase(value_str);
    if (extract_xml_attr(xml, "FlightPhase", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->flight_phase.validity = (AdifValidity)atoi(validity_str);
    }
    if (extract_xml_attr(xml, "FlightPhase", "time", time_str,
                         sizeof(time_str)) == 0) {
      state->flight_phase.timestamp_ms = strtoull(time_str, NULL, 10);
    }
  }

  /* 解析 AircraftTailNumber */
  if (extract_xml_attr(xml, "AircraftTailNumber", "value", value_str,
                       sizeof(value_str)) == 0) {
    strncpy(state->aircraft_id.tail_number, value_str,
            ADIF_MAX_TAIL_NUMBER_LEN - 1);
    state->aircraft_id.tail_number[ADIF_MAX_TAIL_NUMBER_LEN - 1] = '\0';
    if (extract_xml_attr(xml, "AircraftTailNumber", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->aircraft_id.validity = (AdifValidity)atoi(validity_str);
    }
  }

  /* 解析 GroundSpeed */
  if (extract_xml_attr(xml, "GroundSpeed", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->speed.ground_speed_kts = atof(value_str);
    if (extract_xml_attr(xml, "GroundSpeed", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->speed.gs_validity = (AdifValidity)atoi(validity_str);
    }
  }

  /* 解析 VerticalSpeed */
  if (extract_xml_attr(xml, "VerticalSpeed", "value", value_str,
                       sizeof(value_str)) == 0) {
    state->speed.vertical_speed_fpm = atof(value_str);
    if (extract_xml_attr(xml, "VerticalSpeed", "validity", validity_str,
                         sizeof(validity_str)) == 0) {
      state->speed.vs_validity = (AdifValidity)atoi(validity_str);
    }
    if (extract_xml_attr(xml, "VerticalSpeed", "time", time_str,
                         sizeof(time_str)) == 0) {
      state->speed.timestamp_ms = strtoull(time_str, NULL, 10);
    }
  }

  /* 更新整体状态 */
  state->data_valid = (state->wow.validity == VALIDITY_NORMAL) &&
                      (state->flight_phase.validity == VALIDITY_NORMAL);
  state->last_update_ms = (uint64_t)time(NULL) * 1000;

  return 0;
}

/*===========================================================================
 * 网络通信
 *===========================================================================*/

/**
 * @brief 创建 TCP 套接字。
 * @details 创建一个流式套接字并设置 SO_REUSEADDR 选项。
 *
 * @return int 套接字文件描述符，失败返回 -1。
 */
static int create_tcp_socket(void) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    LOG_ERROR("Failed to create socket: %s", strerror(errno));
    return -1;
  }

  /* 设置 socket 选项 */
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  return sock;
}

/**
 * @brief 连接到 ADIF 服务器。
 * @details 解析主机名并建立 TCP 连接。
 *
 * @param host 服务器主机名或 IP 地址。
 * @param port 服务器端口号。
 * @return int 连接套接字描述符，失败返回 -1。
 */
static int connect_to_server(const char *host, uint16_t port) {
  int sock = create_tcp_socket();
  if (sock < 0)
    return -1;

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  /* 解析主机名 */
  struct hostent *he = gethostbyname(host);
  if (he) {
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  } else {
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
      LOG_ERROR("Invalid server address: %s", host);
      close(sock);
      return -1;
    }
  }

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    LOG_ERROR("Failed to connect to %s:%u: %s", host, port, strerror(errno));
    close(sock);
    return -1;
  }

  LOG_INFO("Connected to ADIF server %s:%u", host, port);
  return sock;
}

/**
 * @brief 创建异步监听套接字。
 * @details 用于接收 ADIF 服务器推送的数据。绑定端口并开始监听。
 *
 * @param port 监听端口号。
 * @return int 监听套接字描述符，失败返回 -1。
 */
static int create_async_listener(uint16_t port) {
  int sock = create_tcp_socket();
  if (sock < 0)
    return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_ERROR("Failed to bind async port %u: %s", port, strerror(errno));
    close(sock);
    return -1;
  }

  if (listen(sock, 1) < 0) {
    LOG_ERROR("Failed to listen on async port %u: %s", port, strerror(errno));
    close(sock);
    return -1;
  }

  LOG_INFO("Async listener ready on port %u", port);
  return sock;
}

/*===========================================================================
 * 接收线程
 *===========================================================================*/

/**
 * @brief ADIF 数据接收线程函数。
 * @details 处理异步连接的接受和数据接收循环。
 *          - 接受来自 ADIF 服务器的连接
 *          - 循环接收 XML 数据
 *          - 解析并更新本地状态
 *          - 只在状态发生实质变化时触发回调
 *
 * @param arg ADIF 客户端上下文 (AdifClientContext*)。
 * @return void* 线程返回值 (通常为 NULL)。
 */
static void *adif_receiver_thread(void *arg) {
  AdifClientContext *ctx = (AdifClientContext *)arg;
  char buffer[ADIF_MAX_XML_BUFFER];
  int async_client = -1;

  LOG_INFO("ADIF receiver thread started");

  /* 等待 ADIF 服务器连接到异步端口 */
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  /* 设置非阻塞以支持优雅退出 */
  fd_set read_fds;
  struct timeval tv;

  while (ctx->running && async_client < 0) {
    FD_ZERO(&read_fds);
    FD_SET(ctx->async_sock, &read_fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(ctx->async_sock + 1, &read_fds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(ctx->async_sock, &read_fds)) {
      async_client =
          accept(ctx->async_sock, (struct sockaddr *)&client_addr, &addr_len);
      if (async_client >= 0) {
        LOG_INFO("ADIF server connected to async port");
      }
    }
  }

  if (async_client < 0) {
    LOG_ERROR("Failed to accept async connection");
    return NULL;
  }

  /* 主接收循环 */
  while (ctx->running) {
    FD_ZERO(&read_fds);
    FD_SET(async_client, &read_fds);
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ret = select(async_client + 1, &read_fds, NULL, NULL, &tv);
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      LOG_ERROR("Select error: %s", strerror(errno));
      break;
    }

    if (ret == 0) {
      /* 超时，检查连接状态 */
      continue;
    }

    if (FD_ISSET(async_client, &read_fds)) {
      ssize_t n = recv(async_client, buffer, sizeof(buffer) - 1, 0);
      if (n <= 0) {
        if (n == 0) {
          LOG_INFO("ADIF server disconnected");
        } else {
          LOG_ERROR("Recv error: %s", strerror(errno));
        }
        break;
      }

      buffer[n] = '\0';
      LOG_DEBUG("Received ADIF data (%zd bytes)", n);

      /* 解析 XML 并更新状态 */
      AdifAircraftState new_state;
      memset(&new_state, 0, sizeof(new_state));

      if (adif_parse_publish_xml(buffer, &new_state) == 0) {
        AdifFlightPhase old_phase;
        bool state_changed = false;

        pthread_mutex_lock(&ctx->state_mutex);
        old_phase = ctx->aircraft_state.flight_phase.phase;

        /* 检查状态是否真正变化（飞行阶段或 WoW 状态） */
        if (old_phase != new_state.flight_phase.phase ||
            ctx->aircraft_state.wow.on_ground != new_state.wow.on_ground) {
          state_changed = true;
        }

        memcpy(&ctx->aircraft_state, &new_state, sizeof(AdifAircraftState));
        pthread_mutex_unlock(&ctx->state_mutex);

        /* 只在状态真正变化时触发回调和日志 */
        if (state_changed) {
          /* 检查是否需要通知策略引擎 */
          if (adif_should_reevaluate_routing(old_phase,
                                             new_state.flight_phase.phase)) {
            LOG_INFO(
                "Flight phase changed: %s -> %s, triggering route reevaluation",
                adif_flight_phase_to_string(old_phase),
                adif_flight_phase_to_string(new_state.flight_phase.phase));
          }

          /* 调用回调函数 - 仅在状态变化时 */
          if (ctx->callback) {
            ctx->callback(&new_state, ctx->callback_data);
          }
        }
      }
    }
  }

  if (async_client >= 0) {
    close(async_client);
  }

  LOG_INFO("ADIF receiver thread exiting");
  return NULL;
}

/*===========================================================================
 * 公共 API 实现
 *===========================================================================*/

/**
 * @brief 初始化 ADIF 客户端。
 * @details 设置默认配置，初始化状态数据和互斥锁。
 *
 * @param ctx 客户端上下文指针。
 * @param config 配置参数，如果为 NULL 则使用默认值。
 * @return int 成功返回 0，失败返回 -1。
 */
int adif_client_init(AdifClientContext *ctx, const AdifClientConfig *config) {
  if (!ctx)
    return -1;

  memset(ctx, 0, sizeof(AdifClientContext));

  /* 设置默认配置 */
  if (config) {
    memcpy(&ctx->config, config, sizeof(AdifClientConfig));
  } else {
    strcpy(ctx->config.server_host, "127.0.0.1");
    ctx->config.server_port = ADIF_DEFAULT_SERVER_PORT;
    ctx->config.async_port = ADIF_DEFAULT_ASYNC_PORT;
    ctx->config.refresh_period_ms = ADIF_DEFAULT_REFRESH_MS;
    ctx->config.auto_reconnect = true;
    ctx->config.reconnect_interval_ms = 5000;
  }

  ctx->sync_sock = -1;
  ctx->async_sock = -1;
  ctx->state = ADIF_CLIENT_DISCONNECTED;

  if (pthread_mutex_init(&ctx->state_mutex, NULL) != 0) {
    LOG_ERROR("Failed to initialize mutex");
    return -1;
  }

  /* 初始化默认飞机状态 */
  ctx->aircraft_state.wow.on_ground = true;
  ctx->aircraft_state.wow.validity = VALIDITY_NORMAL;
  ctx->aircraft_state.flight_phase.phase = FLIGHT_PHASE_GATE;
  ctx->aircraft_state.flight_phase.validity = VALIDITY_NORMAL;

  LOG_INFO(
      "ADIF client initialized (server=%s:%u, async_port=%u, refresh=%ums)",
      ctx->config.server_host, ctx->config.server_port, ctx->config.async_port,
      ctx->config.refresh_period_ms);

  return 0;
}

/**
 * @brief 连接到 ADIF 服务器并订阅数据。
 * @details
 *          1. 创建异步监听端口等待推送。
 *          2. 连接到服务器同步端口。
 *          3. 发送 subscribeAvionicParameters 请求。
 *          4. 验证订阅响应。
 *          5. 启动后台接收线程。
 *
 * @param ctx 客户端上下文指针。
 * @return int 成功返回 0，失败返回 -1。
 */
int adif_client_connect(AdifClientContext *ctx) {
  if (!ctx)
    return -1;

  ctx->state = ADIF_CLIENT_CONNECTING;

  /* 创建异步监听套接字 */
  ctx->async_sock = create_async_listener(ctx->config.async_port);
  if (ctx->async_sock < 0) {
    ctx->state = ADIF_CLIENT_ERROR;
    snprintf(ctx->error_msg, sizeof(ctx->error_msg),
             "Failed to create async listener on port %u",
             ctx->config.async_port);
    return -1;
  }

  /* 连接到 ADIF 服务器 */
  ctx->sync_sock =
      connect_to_server(ctx->config.server_host, ctx->config.server_port);
  if (ctx->sync_sock < 0) {
    close(ctx->async_sock);
    ctx->async_sock = -1;
    ctx->state = ADIF_CLIENT_ERROR;
    snprintf(ctx->error_msg, sizeof(ctx->error_msg),
             "Failed to connect to ADIF server %s:%u", ctx->config.server_host,
             ctx->config.server_port);
    return -1;
  }

  /* 发送订阅请求 */
  char subscribe_xml[ADIF_MAX_XML_BUFFER];
  int xml_len = adif_generate_subscribe_xml(
      subscribe_xml, sizeof(subscribe_xml), ctx->config.async_port,
      ctx->config.refresh_period_ms);
  if (xml_len < 0) {
    LOG_ERROR("Failed to generate subscribe XML");
    adif_client_disconnect(ctx);
    return -1;
  }

  if (send(ctx->sync_sock, subscribe_xml, xml_len, 0) != xml_len) {
    LOG_ERROR("Failed to send subscribe request: %s", strerror(errno));
    adif_client_disconnect(ctx);
    return -1;
  }

  LOG_INFO("Subscription request sent");

  /* 等待订阅响应 */
  char response[ADIF_MAX_XML_BUFFER];
  ssize_t n = recv(ctx->sync_sock, response, sizeof(response) - 1, 0);
  if (n <= 0) {
    LOG_ERROR("Failed to receive subscription response");
    adif_client_disconnect(ctx);
    return -1;
  }
  response[n] = '\0';

  /* 检查响应中的 errorcode */
  if (strstr(response, "errorcode=\"0\"") == NULL) {
    LOG_ERROR("Subscription rejected by server: %s", response);
    adif_client_disconnect(ctx);
    return -1;
  }

  LOG_INFO("Subscription confirmed by ADIF server");

  /* 启动接收线程 */
  ctx->running = true;
  if (pthread_create(&ctx->receiver_thread, NULL, adif_receiver_thread, ctx) !=
      0) {
    LOG_ERROR("Failed to create receiver thread");
    ctx->running = false;
    adif_client_disconnect(ctx);
    return -1;
  }

  ctx->state = ADIF_CLIENT_SUBSCRIBED;
  return 0;
}

/**
 * @brief 断开与 ADIF 服务器的连接。
 * @details 停止接收线程，关闭同步和异步套接字，重置状态。
 *
 * @param ctx 客户端上下文指针。
 */
void adif_client_disconnect(AdifClientContext *ctx) {
  if (!ctx)
    return;

  ctx->running = false;

  /* 等待接收线程退出 */
  if (ctx->state == ADIF_CLIENT_SUBSCRIBED) {
    pthread_join(ctx->receiver_thread, NULL);
  }

  if (ctx->sync_sock >= 0) {
    close(ctx->sync_sock);
    ctx->sync_sock = -1;
  }

  if (ctx->async_sock >= 0) {
    close(ctx->async_sock);
    ctx->async_sock = -1;
  }

  ctx->state = ADIF_CLIENT_DISCONNECTED;
  LOG_INFO("ADIF client disconnected");
}

/**
 * @brief 获取当前飞机状态（线程安全）。
 * @details 使用互斥锁保护，复制最新的状态数据到用户提供的缓冲区。
 *
 * @param ctx 客户端上下文指针。
 * @param state [out] 输出状态数据的缓冲区。
 * @return int 成功返回 0，失败返回 -1。
 */
int adif_client_get_state(AdifClientContext *ctx, AdifAircraftState *state) {
  if (!ctx || !state)
    return -1;

  pthread_mutex_lock(&ctx->state_mutex);
  memcpy(state, &ctx->aircraft_state, sizeof(AdifAircraftState));
  pthread_mutex_unlock(&ctx->state_mutex);

  return 0;
}

/**
 * @brief 设置状态变化回调函数。
 * @details 当飞机状态（如飞行阶段、WoW）发生变化时调用此回调。
 *
 * @param ctx 客户端上下文指针。
 * @param callback 回调函数指针。
 * @param user_data 用户数据指针，将传递给回调函数。
 */
void adif_client_set_callback(AdifClientContext *ctx,
                              AdifStateCallback callback, void *user_data) {
  if (!ctx)
    return;

  ctx->callback = callback;
  ctx->callback_data = user_data;
}

/**
 * @brief 检查客户端是否已连接并已订阅。
 *
 * @param ctx 客户端上下文指针。
 * @return bool true 已连接且已订阅，false 未连接。
 */
bool adif_client_is_connected(AdifClientContext *ctx) {
  if (!ctx)
    return false;
  return (ctx->state == ADIF_CLIENT_SUBSCRIBED);
}

/**
 * @brief 清理 ADIF 客户端资源。
 * @details 断开连接并销毁互斥锁。
 *
 * @param ctx 客户端上下文指针。
 */
void adif_client_cleanup(AdifClientContext *ctx) {
  if (!ctx)
    return;

  adif_client_disconnect(ctx);
  pthread_mutex_destroy(&ctx->state_mutex);

  LOG_INFO("ADIF client cleaned up");
}
