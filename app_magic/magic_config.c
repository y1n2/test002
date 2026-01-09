/**
 * @file magic_config.c
 * @brief MAGIC 配置管理器实现文件
 * @description 实现 XML 配置文件的加载、解析和管理功能
 *
 * 本文件实现了 MAGIC 系统配置管理器的核心功能:
 * 1. XML 文件解析 - 使用 libxml2 库解析配置文件
 * 2. 配置数据加载 - 从 3 个 XML 文件加载配置信息
 * 3. 数据结构转换 - 将字符串转换为枚举和数值类型
 * 4. 查找和查询 - 提供配置项的快速查找功能
 * 5. 配置摘要输出 - 打印配置信息摘要
 *
 * 配置加载顺序:
 * 1. magic_config_load_datalinks() - 加载数据链路配置
 * 2. magic_config_load_policy() - 加载策略配置
 * 3. magic_config_load_clients() - 加载客户端配置
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

/*===========================================================================
 * 头文件包含
 *===========================================================================*/
#include "magic_config.h" /* 包含配置管理器头文件，定义所有数据结构和函数声明 */
#include <freeDiameter/extension.h> /* 包含 freeDiameter 扩展框架，提供日志和扩展功能 */
#include <libxml/parser.h>          /* 包含 libxml2 XML 解析器头文件 */
#include <libxml/tree.h>            /* 包含 libxml2 XML 树结构头文件 */
#include <stdio.h>  /* 包含标准输入输出函数，如 printf, sprintf */
#include <stdlib.h> /* 包含标准库函数，如 atoi, malloc */
#include <string.h> /* 包含字符串操作函数，如 strcmp, strncpy, memset */

/*===========================================================================
 * XML 解析辅助函数
 *
 * 本节定义用于 XML 解析的辅助函数
 * 这些函数简化了从 XML 节点提取数据的操作
 *===========================================================================*/

/**
 * @brief 获取 XML 节点的文本内容
 * @param node XML 节点指针
 * @return 节点文本内容字符串，失败返回 NULL
 *
 * 该函数提取 XML 元素的文本内容
 * 例如: <Name>John</Name> 返回 "John"
 */
static char *get_node_content(xmlNode *node) {
  /* 检查节点指针是否有效 */
  if (node && node->children && node->children->content) {
    /* 返回节点的文本内容 */
    return (char *)node->children->content;
  }
  /* 节点无效或无内容，返回 NULL */
  return NULL;
}

/**
 * @brief 查找 XML 节点的子节点
 * @param parent 父节点指针
 * @param name 要查找的子节点名称
 * @return 找到的子节点指针，未找到返回 NULL
 *
 * 该函数在父节点的子节点中查找指定名称的元素节点
 */
static xmlNode *find_child_node(xmlNode *parent, const char *name) {
  /* 检查父节点是否有效 */
  if (!parent)
    return NULL;

  /* 遍历父节点的所有子节点 */
  for (xmlNode *node = parent->children; node; node = node->next) {
    /* 检查是否为元素节点且名称匹配 */
    if (node->type == XML_ELEMENT_NODE &&
        strcmp((char *)node->name, name) == 0) {
      /* 找到匹配的子节点，返回节点指针 */
      return node;
    }
  }
  /* 未找到匹配的子节点，返回 NULL */
  return NULL;
}

/**
 * @brief 获取 XML 子节点的内容
 * @param parent 父节点指针
 * @param child_name 子节点名称
 * @return 子节点文本内容，未找到返回 NULL
 *
 * 该函数结合 find_child_node 和 get_node_content 的功能
 */
static char *get_child_content(xmlNode *parent, const char *child_name) {
  /* 查找指定的子节点 */
  xmlNode *child = find_child_node(parent, child_name);
  /* 获取子节点的内容并返回 */
  return get_node_content(child);
}

/**
 * @brief 获取 XML 子节点的 uint32 值
 * @param parent 父节点指针
 * @param child_name 子节点名称
 * @param default_val 默认值
 * @return 解析的 uint32 值或默认值
 *
 * 该函数将 XML 子节点的文本内容转换为无符号 32 位整数
 */
static uint32_t get_child_uint32(xmlNode *parent, const char *child_name,
                                 uint32_t default_val) {
  /* 获取子节点的内容 */
  char *content = get_child_content(parent, child_name);
  /* 如果内容存在，使用 atoi 转换为整数，否则返回默认值 */
  return content ? (uint32_t)atoi(content) : default_val;
}

/**
 * @brief 获取 XML 节点的属性值
 * @param node XML 节点指针
 * @param attr_name 属性名称
 * @return 属性值字符串
 *
 * 该函数提取 XML 元素的属性值
 * 例如: <Link id="SATCOM"> 返回 "SATCOM"
 */
static char *get_attribute(xmlNode *node, const char *attr_name) {
  /* 使用 libxml2 函数获取属性值 */
  return (char *)xmlGetProp(node, (const xmlChar *)attr_name);
}

/*===========================================================================
 * 枚举转换函数
 *
 * 本节定义将字符串转换为枚举值的函数
 * 用于将 XML 文件中的字符串配置转换为内部枚举类型
 *===========================================================================*/

/**
 * @brief 将字符串转换为链路类型枚举 (已废弃，保留用于兼容)
 * @param str 输入字符串
 * @return 对应的 DLMType 枚举值
 *
 * 注意: v2.0 使用 DLMType，此函数仅用于兼容旧代码
 */
static DLMType parse_link_type(const char *str) {
  if (!str)
    return DLM_TYPE_UNKNOWN;
  if (strcmp(str, "SATELLITE") == 0)
    return DLM_TYPE_SATELLITE;
  if (strcmp(str, "CELLULAR") == 0)
    return DLM_TYPE_CELLULAR;
  if (strcmp(str, "GATELINK") == 0 || strcmp(str, "HYBRID") == 0)
    return DLM_TYPE_HYBRID;
  return DLM_TYPE_UNKNOWN;
}

/**
 * @brief 将字符串转换为 DLM 类型枚举 (v2.0 新增)
 * @param str 输入字符串或整数值
 * @return 对应的 DLMType 枚举值，未知返回 DLM_TYPE_UNKNOWN
 */
static DLMType parse_dlm_type(const char *str) {
  if (!str)
    return DLM_TYPE_UNKNOWN;
  int val = atoi(str);
  switch (val) {
  case 1:
    return DLM_TYPE_SATELLITE;
  case 2:
    return DLM_TYPE_CELLULAR;
  case 3:
    return DLM_TYPE_HYBRID;
  default:
    return DLM_TYPE_UNKNOWN;
  }
}

/**
 * @brief 将字符串转换为负载均衡算法枚举 (v2.0 新增)
 * @param str 输入字符串
 * @return 对应的 LoadBalanceAlgorithm 枚举值
 */
static LoadBalanceAlgorithm parse_load_balance_algorithm(const char *str) {
  if (!str)
    return LB_ALGO_UNKNOWN;
  if (strcmp(str, "RoundRobin") == 0)
    return LB_ALGO_ROUND_ROBIN;
  if (strcmp(str, "LeastLoaded") == 0)
    return LB_ALGO_LEAST_LOADED;
  if (strcmp(str, "Priority") == 0)
    return LB_ALGO_PRIORITY;
  int val = atoi(str);
  switch (val) {
  case 1:
    return LB_ALGO_ROUND_ROBIN;
  case 2:
    return LB_ALGO_LEAST_LOADED;
  case 3:
    return LB_ALGO_PRIORITY;
  default:
    return LB_ALGO_UNKNOWN;
  }
}

/**
 * @brief 获取 XML 子节点的 float 值
 * @param parent 父节点指针
 * @param child_name 子节点名称
 * @param default_val 默认值
 * @return 解析的 float 值或默认值
 */
static float get_child_float(xmlNode *parent, const char *child_name,
                             float default_val) {
  char *content = get_child_content(parent, child_name);
  return content ? (float)atof(content) : default_val;
}

/**
 * @brief 获取 XML 子节点的 double 值
 * @param parent 父节点指针
 * @param child_name 子节点名称
 * @param default_val 默认值
 * @return 解析的 double 值或默认值
 */
static double get_child_double(xmlNode *parent, const char *child_name,
                               double default_val) {
  char *content = get_child_content(parent, child_name);
  return content ? atof(content) : default_val;
}

/**
 * @brief 将字符串转换为覆盖范围枚举 (已废弃，v2.0 不再使用)
 */
static Coverage parse_coverage(const char *str) {
  if (!str)
    return COVERAGE_UNKNOWN;
  if (strcmp(str, "GLOBAL") == 0)
    return COVERAGE_GLOBAL;
  if (strcmp(str, "TERRESTRIAL") == 0)
    return COVERAGE_TERRESTRIAL;
  if (strcmp(str, "GATE_ONLY") == 0)
    return COVERAGE_GATE_ONLY;
  return COVERAGE_UNKNOWN;
}

/**
 * @brief 将字符串转换为安全等级枚举 (已废弃，v2.0 不再使用)
 */
static int parse_security_level(const char *str) {
  if (!str)
    return 0;
  if (strcmp(str, "HIGH") == 0)
    return 3;
  if (strcmp(str, "MEDIUM") == 0)
    return 2;
  if (strcmp(str, "LOW") == 0)
    return 1;
  return 0;
}

/**
/**
 * @brief 将字符串转换为策略动作枚举
 * @param str 输入字符串
 * @return 对应的 PolicyAction 枚举值，未知返回 ACTION_DEFAULT
 *
 * 支持的字符串值:
 * - "PERMIT" -> ACTION_PERMIT
 * - "PROHIBIT" -> ACTION_PROHIBIT
 */
static PolicyAction parse_policy_action(const char *str) {
  /* 检查输入字符串是否为空 */
  if (!str)
    return ACTION_DEFAULT;
  /* 检查是否为允许动作 */
  if (strcmp(str, "PERMIT") == 0)
    return ACTION_PERMIT;
  /* 检查是否为禁止动作 */
  if (strcmp(str, "PROHIBIT") == 0)
    return ACTION_PROHIBIT;
  /* 未知字符串，返回默认值 */
  return ACTION_DEFAULT;
}

/**
 * @brief 将字符串转换为飞行阶段枚举 (Client_Profile.xml v2.0)
 * @param str 输入字符串 (如 "CRUISE", "GATE")
 * @return 对应的 CfgFlightPhase 枚举值，未知返回 CFG_FLIGHT_PHASE_UNKNOWN
 */
CfgFlightPhase magic_config_parse_flight_phase(const char *str) {
  if (!str)
    return CFG_FLIGHT_PHASE_UNKNOWN;
  if (strcmp(str, "GATE") == 0)
    return CFG_FLIGHT_PHASE_GATE;
  if (strcmp(str, "TAXI") == 0)
    return CFG_FLIGHT_PHASE_TAXI;
  if (strcmp(str, "TAKE_OFF") == 0 || strcmp(str, "TAKEOFF") == 0)
    return CFG_FLIGHT_PHASE_TAKE_OFF;
  if (strcmp(str, "CLIMB") == 0)
    return CFG_FLIGHT_PHASE_CLIMB;
  if (strcmp(str, "CRUISE") == 0)
    return CFG_FLIGHT_PHASE_CRUISE;
  if (strcmp(str, "DESCENT") == 0)
    return CFG_FLIGHT_PHASE_DESCENT;
  if (strcmp(str, "APPROACH") == 0)
    return CFG_FLIGHT_PHASE_APPROACH;
  if (strcmp(str, "LANDING") == 0)
    return CFG_FLIGHT_PHASE_LANDING;
  if (strcmp(str, "MAINTENANCE") == 0)
    return CFG_FLIGHT_PHASE_MAINTENANCE;
  return CFG_FLIGHT_PHASE_UNKNOWN;
}

/**
 * @brief 将字符串转换为优先级类型枚举 (Client_Profile.xml v2.0)
 * @param str 输入字符串 (如 "1" 或 "2")
 * @return 对应的 PriorityType 枚举值
 */
static PriorityType parse_priority_type(const char *str) {
  if (!str)
    return PRIORITY_TYPE_UNKNOWN;
  int val = atoi(str);
  if (val == 1)
    return PRIORITY_TYPE_BLOCKING;
  if (val == 2)
    return PRIORITY_TYPE_PREEMPTION;
  return PRIORITY_TYPE_UNKNOWN;
}

/**
 * @brief 获取布尔值子节点内容
 * @param parent 父节点
 * @param child_name 子节点名称
 * @param default_val 默认值
 * @return 布尔值
 */
static bool get_child_bool(xmlNode *parent, const char *child_name,
                           bool default_val) {
  char *content = get_child_content(parent, child_name);
  if (!content)
    return default_val;
  if (strcmp(content, "true") == 0 || strcmp(content, "1") == 0)
    return true;
  if (strcmp(content, "false") == 0 || strcmp(content, "0") == 0)
    return false;
  return default_val;
}

/*===========================================================================
 * 初始化函数
 *
 * 本节定义配置管理器的初始化和清理函数
 *===========================================================================*/

/**
 * @brief 初始化配置管理器
 * @param config 指向配置管理器结构体的指针
 *
 * 该函数将配置结构体清零并设置为初始状态
 * 所有计数器设为 0，标志设为 false
 */
void magic_config_init(MagicConfig *config) {
  /* 使用 memset 将整个结构体清零 */
  memset(config, 0, sizeof(MagicConfig));
  /* 设置加载标志为 false，表示尚未加载配置 */
  config->is_loaded = false;
}

/*===========================================================================
 * 加载 Datalink_Profile.xml
 *
 * 本节实现数据链路配置文件的加载和解析
 * 从 XML 文件中读取所有数据链路的配置信息
 *===========================================================================*/

/**
 * @brief 加载数据链路配置文件 (Datalink_Profile.xml v2.0)。
 * @details 负责解析数据链路配置文件，提取所有 DLM (Data Link Manager)
 * 的物理特性、带宽、QoS 支持及覆盖范围。 v2.0 格式要求每个 DLM 节点下包含
 * Links, Coverage 和 LoadBalance 子节点。
 *
 * @param[in,out] config    指向全局配置句柄的指针。
 * @param[in]     base_path 配置文件所在的基准目录路径。
 *
 * @return int 成功返回 0；如果文件缺失、损坏或 XML根节点不匹配则返回 -1。
 *
 * @warning 该函数会清空原有的 DLM 配置。如果配置的 DLM 数量超过
 * MAX_LINKS，后续节点将被忽略。
 */
int magic_config_load_datalinks(MagicConfig *config, const char *base_path) {
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/Datalink_Profile.xml", base_path);

  fd_log_debug("[app_magic] Loading Datalink Profile v2.0: %s", filepath);

  xmlDoc *doc = xmlReadFile(filepath, NULL, 0);
  if (!doc) {
    fd_log_error("[app_magic] Failed to parse %s", filepath);
    return -1;
  }

  xmlNode *root = xmlDocGetRootElement(doc);
  if (!root || strcmp((char *)root->name, "DLMConfigs") != 0) {
    fd_log_error(
        "[app_magic] Invalid root element in %s (expected <DLMConfigs>)",
        filepath);
    xmlFreeDoc(doc);
    return -1;
  }

  config->num_dlm_configs = 0;

  /* 遍历所有 DLMConfig 元素 */
  for (xmlNode *dlm_node = root->children; dlm_node;
       dlm_node = dlm_node->next) {
    if (dlm_node->type != XML_ELEMENT_NODE ||
        strcmp((char *)dlm_node->name, "DLMConfig") != 0) {
      continue;
    }

    if (config->num_dlm_configs >= MAX_LINKS) {
      fd_log_error("[app_magic] Too many DLMs, max %d", MAX_LINKS);
      break;
    }

    DLMConfig *dlm = &config->dlm_configs[config->num_dlm_configs];
    memset(dlm, 0, sizeof(DLMConfig));

    /* 解析基本信息 */
    char *dlm_name = get_child_content(dlm_node, "DLMName");
    if (dlm_name) {
      strncpy(dlm->dlm_name, dlm_name, MAX_ID_LEN - 1);
    }

    char *desc = get_child_content(dlm_node, "Description");
    if (desc) {
      strncpy(dlm->description, desc, MAX_NAME_LEN - 1);
    }

    dlm->enabled = get_child_bool(dlm_node, "Enabled", true);

    char *dlm_type_str = get_child_content(dlm_node, "DLMType");
    dlm->dlm_type = parse_dlm_type(dlm_type_str);

    fd_log_debug("[app_magic]   Loading DLM: %s (Type=%d, Enabled=%s)",
                 dlm->dlm_name, dlm->dlm_type, dlm->enabled ? "yes" : "no");

    /* 解析 Links 节点 (仅取第一条 Link) */
    xmlNode *links_node = find_child_node(dlm_node, "Links");
    if (links_node) {
      xmlNode *link_node = find_child_node(links_node, "Link");
      if (link_node) {
        /* 链路名称和编号 */
        char *link_name = get_child_content(link_node, "LinkName");
        if (link_name) {
          strncpy(dlm->link_name, link_name, MAX_NAME_LEN - 1);
        }
        dlm->link_number = get_child_uint32(link_node, "LinkNumber", 1);

        /* 带宽配置 (kbps) */
        dlm->max_forward_bw_kbps =
            get_child_float(link_node, "MaxForwardBW", 0.0);
        dlm->max_return_bw_kbps =
            get_child_float(link_node, "MaxReturnBW", 0.0);
        dlm->oversubscription_ratio =
            get_child_float(link_node, "OversubscriptionRatio", 1.0);

        /* 解析 SupportedQoS */
        xmlNode *qos_node = find_child_node(link_node, "SupportedQoS");
        if (qos_node) {
          dlm->num_supported_qos = 0;
          for (xmlNode *level_node = qos_node->children; level_node;
               level_node = level_node->next) {
            if (level_node->type == XML_ELEMENT_NODE &&
                strcmp((char *)level_node->name, "Level") == 0) {
              char *level_str = get_node_content(level_node);
              if (level_str &&
                  dlm->num_supported_qos < MAX_QOS_LEVELS_PER_DLM) {
                dlm->supported_qos[dlm->num_supported_qos++] =
                    (uint8_t)atoi(level_str);
              }
            }
          }
        }

        /* 物理特性 */
        dlm->latency_ms = get_child_uint32(link_node, "Latency", 0);
        dlm->jitter_ms = get_child_uint32(link_node, "Jitter", 0);
        dlm->packet_loss_rate =
            get_child_float(link_node, "PacketLossRate", 0.0);

        /* MIH 接口信息 - 使用 MIHFID 生成 Socket 路径 */
        char *mihf_id = get_child_content(link_node, "MIHFID");
        if (mihf_id) {
          strncpy(dlm->mihf_id, mihf_id, MAX_ID_LEN - 1);
          /* 生成 Unix Domain Socket 路径: /tmp/{MIHFID}.sock */
          snprintf(dlm->dlm_socket_path, MAX_DLM_SOCKET_PATH_LEN,
                   "/tmp/%s.sock", mihf_id);
        } else {
          /* 如果没有 MIHFID，使用 DLMName 生成 */
          snprintf(dlm->dlm_socket_path, MAX_DLM_SOCKET_PATH_LEN,
                   "/tmp/%s.sock", dlm->dlm_name);
        }

        fd_log_debug("[app_magic]     Link: %s, BW: %.0f/%.0f kbps, Latency: "
                     "%u ms, Socket: %s",
                     dlm->link_name, dlm->max_forward_bw_kbps,
                     dlm->max_return_bw_kbps, dlm->latency_ms,
                     dlm->dlm_socket_path);
      }
    }

    /* 解析 LoadBalance 配置 */
    xmlNode *lb_node = find_child_node(dlm_node, "LoadBalance");
    if (lb_node) {
      char *algo = get_child_content(lb_node, "Algorithm");
      dlm->load_balance.algorithm = parse_load_balance_algorithm(algo);
      dlm->load_balance.enable_failover =
          get_child_bool(lb_node, "EnableFailover", false);
      dlm->load_balance.health_check_interval_sec =
          get_child_uint32(lb_node, "HealthCheckInterval", 60);
    }

    /* 解析 Coverage 配置 */
    xmlNode *cov_node = find_child_node(dlm_node, "Coverage");
    if (cov_node) {
      dlm->coverage.enabled = true;
      dlm->coverage.min_latitude =
          get_child_double(cov_node, "MinLatitude", -90.0);
      dlm->coverage.max_latitude =
          get_child_double(cov_node, "MaxLatitude", 90.0);
      dlm->coverage.min_longitude =
          get_child_double(cov_node, "MinLongitude", -180.0);
      dlm->coverage.max_longitude =
          get_child_double(cov_node, "MaxLongitude", 180.0);
      dlm->coverage.min_altitude_ft =
          get_child_uint32(cov_node, "MinAltitude", 0);
      dlm->coverage.max_altitude_ft =
          get_child_uint32(cov_node, "MaxAltitude", 60000);

      fd_log_debug("[app_magic]     Coverage: Lat[%.1f,%.1f] Lon[%.1f,%.1f] "
                   "Alt[%u,%u]ft",
                   dlm->coverage.min_latitude, dlm->coverage.max_latitude,
                   dlm->coverage.min_longitude, dlm->coverage.max_longitude,
                   dlm->coverage.min_altitude_ft,
                   dlm->coverage.max_altitude_ft);
    } else {
      dlm->coverage.enabled = false;
    }

    dlm->is_active = true;
    config->num_dlm_configs++;
  }

  xmlFreeDoc(doc);
  fd_log_notice("[app_magic] Loaded %u DLM configs (v2.0 format)",
                config->num_dlm_configs);
  return 0;
}

/*===========================================================================
 * 加载 Central_Policy_Profile.xml
 *
 * 本节实现中央策略配置文件的加载和解析
 * 从 XML 文件中读取所有策略规则集的配置信息
 *===========================================================================*/

/**
 * @brief 加载中央策略配置文件 (Central_Policy_Profile.xml)。
 * @details 解析策略规则，包括流量分类映射、路径偏好、延迟要求以及 WoW (Weight
 * on Wheels) 地面/空中感知策略。
 *
 * @param[in,out] config    指向全局配置句柄的指针。
 * @param[in]     base_path 配置文件所在的基准目录路径。
 *
 * @return int 成功返回 0；失败返回 -1。
 *
 * @note 策略加载后将用于后续的 magic_policy_decision 判定。
 */
int magic_config_load_policy(MagicConfig *config, const char *base_path) {
  /* 构建完整的中央策略配置文件路径 */
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/Central_Policy_Profile.xml",
           base_path);

  /* 记录策略加载开始的调试信息 */
  fd_log_debug("[app_magic] Loading Policy Profile: %s", filepath);

  /* 使用 libxml2 解析策略 XML 文件 */
  xmlDoc *doc = xmlReadFile(filepath, NULL, 0);
  /* 检查文件是否解析成功 */
  if (!doc) {
    /* 解析失败，记录错误并返回 */
    fd_log_error("[app_magic] Failed to parse %s", filepath);
    return -1;
  }

  /* 获取 XML 文档的根元素 */
  xmlNode *root = xmlDocGetRootElement(doc);
  /* 检查根元素是否为 CentralPolicyProfile */
  if (!root || strcmp((char *)root->name, "CentralPolicyProfile") != 0) {
    /* 根元素不匹配，记录错误，释放文档，返回失败 */
    fd_log_error("[app_magic] Invalid root element in %s", filepath);
    xmlFreeDoc(doc);
    return -1;
  }

  /* 获取中央策略配置结构体的指针 */
  CentralPolicyProfile *policy = &config->policy;
  /* 清空结构体，确保初始状态 */
  memset(policy, 0, sizeof(CentralPolicyProfile));

  /* 查找并解析可用链路节点 */
  xmlNode *links_node = find_child_node(root, "AvailableLinks");
  if (links_node) {
    /* 初始化可用链路计数器 */
    policy->num_links = 0;
    /* 遍历所有链路子节点 */
    for (xmlNode *link = links_node->children; link; link = link->next) {
      /* 检查是否为元素节点且为 Link 标签，且未超过最大链路数量 */
      if (link->type == XML_ELEMENT_NODE &&
          strcmp((char *)link->name, "Link") == 0 &&
          policy->num_links < MAX_LINKS) {

        /* 解析链路 ID 属性 */
        char *id = get_attribute(link, "id");
        if (id) {
          /* 复制链路 ID 到可用链路数组 */
          strncpy(policy->available_links[policy->num_links], id,
                  MAX_ID_LEN - 1);
          /* 增加可用链路计数 */
          policy->num_links++;
          /* 释放 libxml2 分配的内存 */
          xmlFree(id);
        }
      }
    }
  }

  /* 初始化规则集计数器 */
  policy->num_rulesets = 0;
  /* 遍历根节点的所有子节点，查找 PolicyRuleSet 元素 */
  for (xmlNode *ruleset_node = root->children; ruleset_node;
       ruleset_node = ruleset_node->next) {

    /* 检查是否为元素节点且为 PolicyRuleSet 标签 */
    if (ruleset_node->type != XML_ELEMENT_NODE ||
        strcmp((char *)ruleset_node->name, "PolicyRuleSet") != 0) {
      /* 跳过非 PolicyRuleSet 元素 */
      continue;
    }

    /* 检查是否超过最大规则集数量限制 */
    if (policy->num_rulesets >= MAX_POLICY_RULESETS) {
      /* 超过限制，记录错误并停止加载 */
      fd_log_error("[app_magic] Too many rulesets, max %d",
                   MAX_POLICY_RULESETS);
      break;
    }

    /* 获取当前规则集结构体的指针 */
    PolicyRuleSet *ruleset = &policy->rulesets[policy->num_rulesets];
    /* 清空结构体，确保初始状态 */
    memset(ruleset, 0, sizeof(PolicyRuleSet));

    /* 解析规则集 ID 属性 */
    char *id = get_attribute(ruleset_node, "id");
    if (id) {
      /* 复制 ID 字符串，限制长度 */
      strncpy(ruleset->ruleset_id, id, MAX_ID_LEN - 1);
      /* 释放 libxml2 分配的内存 */
      xmlFree(id);
    }

    /* 解析飞行阶段属性 */
    char *phases = get_attribute(ruleset_node, "flight_phases");
    if (phases) {
      /* 复制飞行阶段字符串，限制长度 */
      strncpy(ruleset->flight_phases, phases, MAX_NAME_LEN - 1);
      /* 释放 libxml2 分配的内存 */
      xmlFree(phases);
    }

    /* 初始化规则计数器 */
    ruleset->num_rules = 0;
    /* 遍历规则集的所有子节点，查找 PolicyRule 元素 */
    for (xmlNode *rule_node = ruleset_node->children; rule_node;
         rule_node = rule_node->next) {

      /* 检查是否为元素节点且为 PolicyRule 标签 */
      if (rule_node->type != XML_ELEMENT_NODE ||
          strcmp((char *)rule_node->name, "PolicyRule") != 0) {
        /* 跳过非 PolicyRule 元素 */
        continue;
      }

      /* 检查是否超过每个规则集的最大规则数量限制 */
      if (ruleset->num_rules >= MAX_RULES_PER_RULESET) {
        /* 超过限制，停止加载当前规则集的规则 */
        break;
      }

      /* 获取当前规则结构体的指针 */
      PolicyRule *rule = &ruleset->rules[ruleset->num_rules];
      /* 清空结构体，确保初始状态 */
      memset(rule, 0, sizeof(PolicyRule));

      /* 解析流量类别属性 */
      char *traffic_class = get_attribute(rule_node, "traffic_class");
      if (traffic_class) {
        /* 复制流量类别字符串，限制长度 */
        strncpy(rule->traffic_class, traffic_class, MAX_ID_LEN - 1);
        /* 释放 libxml2 分配的内存 */
        xmlFree(traffic_class);
      }

      /* 初始化路径偏好计数器 */
      rule->num_preferences = 0;
      /* 遍历规则的所有子节点，查找 PathPreference 元素 */
      for (xmlNode *pref_node = rule_node->children; pref_node;
           pref_node = pref_node->next) {

        /* 检查是否为元素节点且为 PathPreference 标签 */
        if (pref_node->type != XML_ELEMENT_NODE ||
            strcmp((char *)pref_node->name, "PathPreference") != 0) {
          /* 跳过非 PathPreference 元素 */
          continue;
        }

        /* 检查是否超过最大路径偏好数量限制 */
        if (rule->num_preferences >= MAX_PATH_PREFERENCES) {
          /* 超过限制，停止加载当前规则的偏好 */
          break;
        }

        /* 获取当前路径偏好结构体的指针 */
        PathPreference *pref = &rule->preferences[rule->num_preferences];
        /* 清空结构体，确保初始状态 */
        memset(pref, 0, sizeof(PathPreference));

        /* 解析排名属性并转换为整数 */
        char *ranking_str = get_attribute(pref_node, "ranking");
        if (ranking_str) {
          /* 将字符串转换为无符号整数 */
          pref->ranking = (uint32_t)atoi(ranking_str);
          /* 释放 libxml2 分配的内存 */
          xmlFree(ranking_str);
        }

        /* 解析链路 ID 属性 */
        char *link_id = get_attribute(pref_node, "link_id");
        if (link_id) {
          /* 复制链路 ID 字符串，限制长度 */
          strncpy(pref->link_id, link_id, MAX_ID_LEN - 1);
          /* 释放 libxml2 分配的内存 */
          xmlFree(link_id);
        }

        /* 解析动作属性字符串并转换为枚举 */
        char *action_str = get_attribute(pref_node, "action");
        pref->action = parse_policy_action(action_str);
        /* 如果动作字符串存在，释放内存 */
        if (action_str)
          xmlFree(action_str);

        /* 解析安全要求属性 */
        char *sec_req = get_attribute(pref_node, "security_required");
        if (sec_req) {
          /* 复制安全要求字符串，限制长度 */
          strncpy(pref->security_required, sec_req, 31);
          /* 释放 libxml2 分配的内存 */
          xmlFree(sec_req);
        }

        /* v2.0 新增: 解析 max_latency_ms 属性 */
        char *max_lat_str = get_attribute(pref_node, "max_latency_ms");
        if (max_lat_str) {
          pref->has_max_latency = true;
          pref->max_latency_ms = (uint32_t)atoi(max_lat_str);
          xmlFree(max_lat_str);
          fd_log_debug("[app_magic]     PathPreference %s: max_latency_ms=%u",
                       pref->link_id, pref->max_latency_ms);
        }

        /* v2.2 新增: 解析 on_ground_only 属性 (WoW 感知) */
        char *on_ground_only_str = get_attribute(pref_node, "on_ground_only");
        if (on_ground_only_str) {
          pref->on_ground_only = (strcmp(on_ground_only_str, "true") == 0 ||
                                  strcmp(on_ground_only_str, "1") == 0);
          xmlFree(on_ground_only_str);
          if (pref->on_ground_only) {
            fd_log_debug(
                "[app_magic]     PathPreference %s: on_ground_only=true",
                pref->link_id);
          }
        }

        /* v2.2 新增: 解析 airborne_only 属性 (WoW 感知) */
        char *airborne_only_str = get_attribute(pref_node, "airborne_only");
        if (airborne_only_str) {
          pref->airborne_only = (strcmp(airborne_only_str, "true") == 0 ||
                                 strcmp(airborne_only_str, "1") == 0);
          xmlFree(airborne_only_str);
          if (pref->airborne_only) {
            fd_log_debug(
                "[app_magic]     PathPreference %s: airborne_only=true",
                pref->link_id);
          }
        }

        /* 增加路径偏好计数 */
        rule->num_preferences++;
      }

      /* 增加规则计数 */
      ruleset->num_rules++;
    }

    /* 增加规则集计数 */
    policy->num_rulesets++;
    /* 记录已加载规则集的调试信息 */
    fd_log_debug("[app_magic]   Loaded ruleset: %s (%u rules)",
                 ruleset->ruleset_id, ruleset->num_rules);
  }

  /* ========================================================================
   * v2.0 新增: 解析 TrafficClassDefinitions 节点
   * ======================================================================== */
  xmlNode *tc_defs_node = find_child_node(root, "TrafficClassDefinitions");
  if (tc_defs_node) {
    policy->num_traffic_class_defs = 0;

    for (xmlNode *tc_node = tc_defs_node->children; tc_node;
         tc_node = tc_node->next) {
      if (tc_node->type != XML_ELEMENT_NODE ||
          strcmp((char *)tc_node->name, "TrafficClass") != 0) {
        continue;
      }

      if (policy->num_traffic_class_defs >= MAX_TRAFFIC_CLASS_DEFS) {
        fd_log_error("[app_magic] Too many TrafficClassDefinitions, max %d",
                     MAX_TRAFFIC_CLASS_DEFS);
        break;
      }

      TrafficClassDefinition *def =
          &policy->traffic_class_defs[policy->num_traffic_class_defs];
      memset(def, 0, sizeof(TrafficClassDefinition));

      /* 解析 id 属性 */
      char *tc_id = get_attribute(tc_node, "id");
      if (tc_id) {
        strncpy(def->traffic_class_id, tc_id, MAX_ID_LEN - 1);
        xmlFree(tc_id);
      }

      /* 解析子元素 */
      for (xmlNode *child = tc_node->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE)
          continue;

        if (strcmp((char *)child->name, "MatchPriorityClass") == 0) {
          char *content = get_node_content(child);
          if (content) {
            def->has_priority_class_match = true;
            def->match_priority_class = (uint8_t)atoi(content);
          }
        } else if (strcmp((char *)child->name, "MatchQoSLevel") == 0) {
          char *content = get_node_content(child);
          if (content) {
            def->has_qos_level_match = true;
            def->match_qos_level = (uint8_t)atoi(content);
          }
        } else if (strcmp((char *)child->name, "MatchProfileNamePattern") ==
                   0) {
          char *content = get_node_content(child);
          if (content && def->num_patterns < MAX_MATCH_PATTERNS) {
            strncpy(def->match_patterns[def->num_patterns], content,
                    MAX_NAME_LEN - 1);
            def->num_patterns++;
          }
        } else if (strcmp((char *)child->name, "Default") == 0) {
          char *content = get_node_content(child);
          if (content && strcmp(content, "true") == 0) {
            def->is_default = true;
          }
        }
      }

      policy->num_traffic_class_defs++;
      fd_log_debug("[app_magic]   TrafficClass: %s (prio=%s, qos=%s, "
                   "patterns=%u, default=%s)",
                   def->traffic_class_id,
                   def->has_priority_class_match ? "yes" : "no",
                   def->has_qos_level_match ? "yes" : "no", def->num_patterns,
                   def->is_default ? "yes" : "no");
    }
    fd_log_notice("[app_magic] Loaded %u TrafficClassDefinitions",
                  policy->num_traffic_class_defs);
  }

  /* ========================================================================
   * v2.0 新增: 解析 SwitchingPolicy 节点
   * ======================================================================== */
  xmlNode *sw_node = find_child_node(root, "SwitchingPolicy");
  if (sw_node) {
    policy->switching_policy.min_dwell_time_sec =
        get_child_uint32(sw_node, "MinDwellTime", 30);
    policy->switching_policy.hysteresis_percentage =
        get_child_uint32(sw_node, "HysteresisPercentage", 10);

    fd_log_notice(
        "[app_magic] SwitchingPolicy: MinDwellTime=%u sec, Hysteresis=%u%%",
        policy->switching_policy.min_dwell_time_sec,
        policy->switching_policy.hysteresis_percentage);
  } else {
    /* 使用默认值 */
    policy->switching_policy.min_dwell_time_sec = 30;
    policy->switching_policy.hysteresis_percentage = 10;
    fd_log_debug("[app_magic] Using default SwitchingPolicy (30 sec, 10%%)");
  }

  /* 释放 XML 文档内存 */
  xmlFreeDoc(doc);
  /* 记录加载成功的通知信息 */
  fd_log_notice("[app_magic] Loaded %u policy rulesets", policy->num_rulesets);
  /* 返回成功 */
  return 0;
}

/*===========================================================================
 * 加载 Client_Profile.xml v2.0
 *
 * 本节实现客户端配置文件的加载和解析
 * 支持新版 7 大分区结构:
 * 1. Auth - 认证信息
 * 2. Bandwidth - 带宽配置 (XML 存储 bps, 解析转换为 kbps)
 * 3. QoS - 服务质量配置
 * 4. LinkPolicy - 链路策略配置
 * 5. Session - 会话策略配置
 * 6. Traffic - 流量安全配置
 * 7. Location - 位置约束配置
 *===========================================================================*/

/**
 * @brief 解析 Auth 分区
 */
static void parse_client_auth(xmlNode *auth_node, ClientAuthConfig *auth) {
  if (!auth_node)
    return;

  char *username = get_child_content(auth_node, "Username");
  if (username)
    strncpy(auth->username, username, MAX_ID_LEN - 1);

  char *client_pwd = get_child_content(auth_node, "ClientPassword");
  if (client_pwd)
    strncpy(auth->client_password, client_pwd, MAX_ID_LEN - 1);

  char *server_pwd = get_child_content(auth_node, "ServerPassword");
  if (server_pwd)
    strncpy(auth->server_password, server_pwd, MAX_ID_LEN - 1);

  char *src_ip = get_child_content(auth_node, "SourceIP");
  if (src_ip)
    strncpy(auth->source_ip, src_ip, MAX_IP_STR_LEN - 1);
}

/**
 * @brief 解析 Bandwidth 分区 (bps -> kbps 转换)
 */
static void parse_client_bandwidth(xmlNode *bw_node, BandwidthConfig *bw) {
  if (!bw_node)
    return;

  /* XML 中存储的是 bps，需要除以 1000 转换为 kbps */
  bw->max_forward_kbps = get_child_uint32(bw_node, "MaxForward", 0) / 1000;
  bw->max_return_kbps = get_child_uint32(bw_node, "MaxReturn", 0) / 1000;
  bw->guaranteed_forward_kbps =
      get_child_uint32(bw_node, "GuaranteedForward", 0) / 1000;
  bw->guaranteed_return_kbps =
      get_child_uint32(bw_node, "GuaranteedReturn", 0) / 1000;
  bw->default_request_kbps =
      get_child_uint32(bw_node, "DefaultRequest", 0) / 1000;

  fd_log_debug("[app_magic]     Bandwidth: max_fwd=%u kbps, max_ret=%u kbps, "
               "guar_fwd=%u kbps",
               bw->max_forward_kbps, bw->max_return_kbps,
               bw->guaranteed_forward_kbps);
}

/**
 * @brief 解析 QoS 分区
 */
static void parse_client_qos(xmlNode *qos_node, QoSConfig *qos) {
  if (!qos_node)
    return;

  /* 优先级类型: 1=Blocking, 2=Preemption */
  char *prio_type = get_child_content(qos_node, "PriorityType");
  qos->priority_type = parse_priority_type(prio_type);

  /* 优先级类: 1-9 */
  qos->priority_class = (uint8_t)get_child_uint32(qos_node, "PriorityClass", 5);

  /* 默认 QoS 级别 */
  qos->default_level = (uint8_t)get_child_uint32(qos_node, "DefaultLevel", 0);

  /* 解析允许的 QoS 级别列表 */
  xmlNode *levels_node = find_child_node(qos_node, "AllowedLevels");
  if (levels_node) {
    qos->num_allowed_levels = 0;
    for (xmlNode *level = levels_node->children; level; level = level->next) {
      if (level->type == XML_ELEMENT_NODE &&
          strcmp((char *)level->name, "Level") == 0 &&
          qos->num_allowed_levels < MAX_ALLOWED_QOS_LEVELS) {
        char *content = get_node_content(level);
        if (content) {
          qos->allowed_levels[qos->num_allowed_levels++] =
              (uint8_t)atoi(content);
        }
      }
    }
  }

  fd_log_debug("[app_magic]     QoS: type=%d, class=%u, default=%u, levels=%u",
               qos->priority_type, qos->priority_class, qos->default_level,
               qos->num_allowed_levels);
}

/**
 * @brief 解析 LinkPolicy 分区
 */
static void parse_client_link_policy(xmlNode *lp_node, LinkPolicyConfig *lp) {
  if (!lp_node)
    return;

  /* 解析允许的 DLM 列表 */
  xmlNode *dlms_node = find_child_node(lp_node, "AllowedDLMs");
  if (dlms_node) {
    lp->num_allowed_dlms = 0;
    for (xmlNode *dlm = dlms_node->children; dlm; dlm = dlm->next) {
      if (dlm->type == XML_ELEMENT_NODE &&
          strcmp((char *)dlm->name, "DLM") == 0 &&
          lp->num_allowed_dlms < MAX_ALLOWED_DLMS) {
        char *content = get_node_content(dlm);
        if (content) {
          strncpy(lp->allowed_dlms[lp->num_allowed_dlms++], content,
                  MAX_ID_LEN - 1);
        }
      }
    }
  }

  /* 首选 DLM */
  char *preferred = get_child_content(lp_node, "PreferredDLM");
  if (preferred)
    strncpy(lp->preferred_dlm, preferred, MAX_ID_LEN - 1);

  /* 是否允许多链路 */
  lp->allow_multi_link = get_child_bool(lp_node, "AllowMultiLink", false);

  /* 最大并发链路数 */
  lp->max_concurrent_links = get_child_uint32(lp_node, "MaxConcurrentLinks", 1);

  fd_log_debug("[app_magic]     LinkPolicy: allowed_dlms=%u, preferred=%s, "
               "multi=%s, max=%u",
               lp->num_allowed_dlms, lp->preferred_dlm,
               lp->allow_multi_link ? "yes" : "no", lp->max_concurrent_links);
}

/**
 * @brief 解析 Session 分区
 */
static void parse_client_session(xmlNode *sess_node,
                                 SessionPolicyConfig *sess) {
  if (!sess_node) {
    /* v2.1: 无配置节点时设置默认值 */
    sess->max_concurrent_sessions = 1;
    sess->session_timeout_sec = 3600;
    sess->auto_reconnect = true;
    sess->reconnect_delay_sec = 5;
    sess->num_allowed_phases = 0;
    sess->allow_detailed_status = true;
    sess->allow_registered_clients = false;
    sess->msxr_rate_limit_sec = 5;
    sess->allow_cdr_control = true; /* v2.2: 默认允许 CDR 控制 */
    return;
  }

  sess->max_concurrent_sessions =
      get_child_uint32(sess_node, "MaxConcurrentSessions", 1);
  sess->session_timeout_sec =
      get_child_uint32(sess_node, "SessionTimeout", 3600);
  sess->auto_reconnect = get_child_bool(sess_node, "AutoReconnect", true);
  sess->reconnect_delay_sec = get_child_uint32(sess_node, "ReconnectDelay", 5);

  /* 解析允许的飞行阶段列表 */
  xmlNode *phases_node = find_child_node(sess_node, "AllowedPhases");
  if (phases_node) {
    sess->num_allowed_phases = 0;
    for (xmlNode *phase = phases_node->children; phase; phase = phase->next) {
      if (phase->type == XML_ELEMENT_NODE &&
          strcmp((char *)phase->name, "Phase") == 0 &&
          sess->num_allowed_phases < MAX_ALLOWED_PHASES) {
        char *content = get_node_content(phase);
        if (content) {
          sess->allowed_phases[sess->num_allowed_phases++] =
              magic_config_parse_flight_phase(content);
        }
      }
    }
  }

  /* v2.1: MSXR 权限控制 - 带默认值 */
  sess->allow_detailed_status =
      get_child_bool(sess_node, "AllowDetailedStatus", true);
  sess->allow_registered_clients =
      get_child_bool(sess_node, "AllowRegisteredClients", false);
  sess->msxr_rate_limit_sec =
      get_child_uint32(sess_node, "MsxrRateLimitSec", 5);

  /* v2.2: MACR CDR 控制权限 - 默认为 true，允许触发不断网切账单 */
  sess->allow_cdr_control = get_child_bool(sess_node, "AllowCDRControl", true);

  fd_log_debug("[app_magic]     Session: max=%u, timeout=%u sec, phases=%u, "
               "msxr_limit=%u, cdr_ctrl=%d",
               sess->max_concurrent_sessions, sess->session_timeout_sec,
               sess->num_allowed_phases, sess->msxr_rate_limit_sec,
               sess->allow_cdr_control);
}

/**
 * @brief 解析 Traffic 分区
 */
static void parse_client_traffic(xmlNode *traffic_node,
                                 TrafficSecurityConfig *traffic) {
  if (!traffic_node)
    return;

  traffic->encryption_required =
      get_child_bool(traffic_node, "EncryptionRequired", false);

  /* 解析允许的协议列表 */
  xmlNode *protos_node = find_child_node(traffic_node, "AllowedProtocols");
  if (protos_node) {
    traffic->num_allowed_protocols = 0;
    for (xmlNode *proto = protos_node->children; proto; proto = proto->next) {
      if (proto->type == XML_ELEMENT_NODE &&
          strcmp((char *)proto->name, "Protocol") == 0 &&
          traffic->num_allowed_protocols < 5) {
        char *content = get_node_content(proto);
        if (content) {
          strncpy(traffic->allowed_protocols[traffic->num_allowed_protocols++],
                  content, MAX_PROTOCOL_LEN - 1);
        }
      }
    }
  }

  /* 解析 TFT 白名单 - 允许的流量规则 */
  xmlNode *tfts_node = find_child_node(traffic_node, "TFTs");
  traffic->num_allowed_tfts = 0;
  if (tfts_node) {
    for (xmlNode *tft = tfts_node->children; tft; tft = tft->next) {
      if (tft->type == XML_ELEMENT_NODE &&
          strcmp((char *)tft->name, "TFT") == 0 &&
          traffic->num_allowed_tfts < 255) {
        char *content = get_node_content(tft);
        if (content) {
          // 去除首尾空白
          char *start = content;
          while (*start && isspace(*start))
            start++;
          char *end = start + strlen(start) - 1;
          while (end > start && isspace(*end))
            *end-- = '\0';

          if (*start) {
            strncpy(traffic->allowed_tfts[traffic->num_allowed_tfts++], start,
                    511);
            fd_log_debug("[app_magic]       允许的 TFT[%u]: %s",
                         traffic->num_allowed_tfts - 1, start);
          }
        }
      }
    }
  }

  traffic->max_packet_size =
      get_child_uint32(traffic_node, "MaxPacketSize", 1500);

  fd_log_debug("[app_magic]     Traffic: encryption=%s, protocols=%u, "
               "allowed_tfts=%u, max_pkt=%u",
               traffic->encryption_required ? "yes" : "no",
               traffic->num_allowed_protocols, traffic->num_allowed_tfts,
               traffic->max_packet_size);
}

/**
 * @brief 解析 Location 分区
 */
static void parse_client_location(xmlNode *loc_node,
                                  LocationConstraintConfig *loc) {
  if (!loc_node)
    return;

  loc->geo_restriction_enabled =
      get_child_bool(loc_node, "GeoRestrictionEnabled", false);

  /* 解析允许的区域列表 */
  xmlNode *regions_node = find_child_node(loc_node, "AllowedRegions");
  if (regions_node) {
    loc->num_allowed_regions = 0;
    for (xmlNode *region = regions_node->children; region;
         region = region->next) {
      if (region->type == XML_ELEMENT_NODE &&
          strcmp((char *)region->name, "Region") == 0 &&
          loc->num_allowed_regions < 5) {
        char *content = get_node_content(region);
        if (content) {
          strncpy(loc->allowed_regions[loc->num_allowed_regions++], content,
                  MAX_NAME_LEN - 1);
        }
      }
    }
  }

  loc->require_coverage = get_child_bool(loc_node, "RequireCoverage", false);

  char *min_cov = get_child_content(loc_node, "MinCoverageType");
  loc->min_coverage_type = parse_coverage(min_cov);

  fd_log_debug("[app_magic]     Location: geo=%s, regions=%u, coverage=%s",
               loc->geo_restriction_enabled ? "yes" : "no",
               loc->num_allowed_regions, loc->require_coverage ? "yes" : "no");
}

/**
 * @brief 加载客户端配置文件 (Client_Profile.xml v2.0)。
 * @details 负责解析客户端配置文件，提取 7 大分区（Auth, Bandwidth, QoS,
 * LinkPolicy, Session, Traffic, Location）的复杂配置。 支持多 Profile
 * 查询，并将带宽单位统一转换为 kbps。
 *
 * @param[in,out] config    指向全局配置句柄的指针。
 * @param[in]     base_path 配置文件所在的基准目录路径。
 *
 * @return int 成功返回 0；如果文件解析失败或根节点错误则返回 -1。
 *
 * @warning 该函数会递归解析多个 XML 子节点，对于格式不正确的 ClientProfile
 * 会跳过并记录错误，但不会中断整体加载。
 */
int magic_config_load_clients(MagicConfig *config, const char *base_path) {
  /* 构建完整的客户端配置文件路径 */
  char filepath[512];
  snprintf(filepath, sizeof(filepath), "%s/Client_Profile.xml", base_path);

  /* 记录客户端加载开始的调试信息 */
  fd_log_debug("[app_magic] Loading Client Profile v2.0: %s", filepath);

  /* 使用 libxml2 解析客户端 XML 文件 */
  xmlDoc *doc = xmlReadFile(filepath, NULL, 0);
  if (!doc) {
    fd_log_error("[app_magic] Failed to parse %s", filepath);
    return -1;
  }

  /* 获取 XML 文档的根元素 */
  xmlNode *root = xmlDocGetRootElement(doc);
  if (!root || strcmp((char *)root->name, "ClientProfiles") != 0) {
    fd_log_error("[app_magic] Invalid root element in %s", filepath);
    xmlFreeDoc(doc);
    return -1;
  }

  /* 初始化客户端计数器 */
  config->num_clients = 0;

  /* 遍历根节点的所有子节点，查找 ClientProfile 元素 */
  for (xmlNode *client_node = root->children; client_node;
       client_node = client_node->next) {

    /* 检查是否为 ClientProfile 标签 */
    if (client_node->type != XML_ELEMENT_NODE ||
        strcmp((char *)client_node->name, "ClientProfile") != 0) {
      continue;
    }

    /* 检查是否超过最大客户端数量限制 */
    if (config->num_clients >= MAX_CLIENTS) {
      fd_log_error("[app_magic] Too many clients, max %d", MAX_CLIENTS);
      break;
    }

    /* 获取当前客户端配置结构体的指针 */
    ClientProfile *client = &config->clients[config->num_clients];
    memset(client, 0, sizeof(ClientProfile));

    /* ====== 解析基础信息 ====== */

    /* ProfileName - 主查找键 */
    char *profile_name = get_child_content(client_node, "ProfileName");
    if (profile_name) {
      strncpy(client->profile_name, profile_name, MAX_NAME_LEN - 1);
    } else {
      fd_log_error("[app_magic] ClientProfile missing ProfileName, skipping");
      continue;
    }

    /* ClientID */
    char *client_id = get_child_content(client_node, "ClientID");
    if (client_id) {
      strncpy(client->client_id, client_id, MAX_ID_LEN - 1);
    }

    /* Description */
    char *desc = get_child_content(client_node, "Description");
    if (desc) {
      strncpy(client->description, desc, MAX_NAME_LEN - 1);
    }

    /* Enabled */
    client->enabled = get_child_bool(client_node, "Enabled", true);

    fd_log_debug("[app_magic]   Loading ClientProfile: %s (ID=%s, enabled=%s)",
                 client->profile_name, client->client_id,
                 client->enabled ? "yes" : "no");

    /* ====== 解析 7 大分区 ====== */

    /* 1. Auth 分区 */
    xmlNode *auth_node = find_child_node(client_node, "Auth");
    parse_client_auth(auth_node, &client->auth);

    /* 2. Bandwidth 分区 */
    xmlNode *bw_node = find_child_node(client_node, "Bandwidth");
    parse_client_bandwidth(bw_node, &client->bandwidth);

    /* 3. QoS 分区 */
    xmlNode *qos_node = find_child_node(client_node, "QoS");
    parse_client_qos(qos_node, &client->qos);

    /* 4. LinkPolicy 分区 */
    xmlNode *lp_node = find_child_node(client_node, "LinkPolicy");
    parse_client_link_policy(lp_node, &client->link_policy);

    /* 5. Session 分区 */
    xmlNode *sess_node = find_child_node(client_node, "Session");
    parse_client_session(sess_node, &client->session);

    /* 6. Traffic 分区 */
    xmlNode *traffic_node = find_child_node(client_node, "Traffic");
    parse_client_traffic(traffic_node, &client->traffic);

    /* 7. Location 分区 */
    xmlNode *loc_node = find_child_node(client_node, "Location");
    parse_client_location(loc_node, &client->location);

    /* 设置初始状态 */
    client->is_online = false;
    config->num_clients++;
  }

  /* 释放 XML 文档内存 */
  xmlFreeDoc(doc);
  fd_log_notice("[app_magic] Loaded %u client profiles (v2.0 format)",
                config->num_clients);
  return 0;
}

/*===========================================================================
 * 查找函数
 *
 * 本节提供配置查找功能
 * 用于根据 ID 快速定位配置项
 *===========================================================================*/

/**
 * @brief 根据 DLM 名称查找对应的 DLM 配置 (v2.0 推荐 API)。
 * @details 遍历 `config->dlm_configs` 数组，通过 `dlm_name` 字符串（如
 * "LINK_SATCOM"）进行精确匹配。
 *
 * @param[in] config   指向全局配置句柄的指针。
 * @param[in] dlm_name 要查找的 DLM 名称字符串。
 *
 * @return DLMConfig* 找到返回指向该配置的指针；未找到返回 NULL。
 *
 * @note 该查找是区分大小写的。
 */
DLMConfig *magic_config_find_dlm(MagicConfig *config, const char *dlm_name) {
  if (!config || !dlm_name)
    return NULL;

  for (uint32_t i = 0; i < config->num_dlm_configs; i++) {
    if (strcmp(config->dlm_configs[i].dlm_name, dlm_name) == 0) {
      return &config->dlm_configs[i];
    }
  }
  return NULL;
}

/**
 * @brief 根据链路 ID 查找数据链路配置 (兼容旧 API)
 * @param config 指向配置管理器结构体的指针
 * @param link_id 要查找的链路 ID
 * @return 找到的链路配置指针，NULL=未找到
 *
 * 注意: 该函数内部调用 magic_config_find_dlm，用于保持兼容性
 */
DatalinkProfile *magic_config_find_datalink(MagicConfig *config,
                                            const char *link_id) {
  return (DatalinkProfile *)magic_config_find_dlm(config, link_id);
}

/**
 * @brief 检查飞机位置是否在 DLM 覆盖范围内 (v2.0 新增)
 * @param dlm DLM 配置指针
 * @param latitude 当前纬度 (-90 to 90)
 * @param longitude 当前经度 (-180 to 180)
 * @param altitude_ft 当前高度 (英尺)
 * @return true=在覆盖范围内, false=不在范围内
 */
bool magic_config_check_dlm_coverage(const DLMConfig *dlm, double latitude,
                                     double longitude, double altitude_ft) {
  if (!dlm || !dlm->coverage.enabled) {
    /* 覆盖检查未启用，默认允许 */
    return true;
  }

  /* 检查纬度范围 */
  if (latitude < dlm->coverage.min_latitude ||
      latitude > dlm->coverage.max_latitude) {
    return false;
  }

  /* 检查经度范围 */
  if (longitude < dlm->coverage.min_longitude ||
      longitude > dlm->coverage.max_longitude) {
    return false;
  }

  /* 检查高度范围 */
  if (altitude_ft < dlm->coverage.min_altitude_ft ||
      altitude_ft > dlm->coverage.max_altitude_ft) {
    return false;
  }

  return true;
}

/**
 * @brief 检查 DLM 是否支持指定的 QoS 级别 (v2.0 新增)
 * @param dlm DLM 配置指针
 * @param qos_level 要检查的 QoS 级别
 * @return true=支持, false=不支持
 */
bool magic_config_dlm_supports_qos(const DLMConfig *dlm, uint8_t qos_level) {
  if (!dlm)
    return false;

  for (uint32_t i = 0; i < dlm->num_supported_qos; i++) {
    if (dlm->supported_qos[i] == qos_level) {
      return true;
    }
  }
  return false;
}

/**
 * @brief 根据客户端 ID 查找客户端配置
 * @param config 指向配置管理器结构体的指针
 * @param client_id 要查找的客户端 ID
 * @return 找到的客户端配置指针，NULL=未找到
 *
 * 该函数遍历所有客户端配置，查找匹配的客户端 ID
 */
ClientProfile *magic_config_find_client(MagicConfig *config,
                                        const char *client_id) {
  if (!config || !client_id)
    return NULL;

  /* 遍历所有客户端配置 */
  for (uint32_t i = 0; i < config->num_clients; i++) {
    /* 比较客户端 ID 是否匹配 */
    if (strcmp(config->clients[i].client_id, client_id) == 0) {
      /* 找到匹配的客户端，返回指针 */
      return &config->clients[i];
    }
  }
  /* 未找到匹配的客户端，返回 NULL */
  return NULL;
}

/**
 * @brief 根据 ProfileName 查找客户端配置 (主查找键)。
 * @details 该函数是 Diameter 认证阶段的核心查询接口，根据 AVP 中携带的
 * ProfileName 查找匹配且处于 `enabled=true` 状态的配置。
 *
 * @param[in] config       指向全局配置句柄的指针。
 * @param[in] profile_name 待查找的配置文件名称。
 *
 * @return ClientProfile* 找到且启用的配置指针；未找到或配置已禁用则返回 NULL。
 */
ClientProfile *magic_config_find_client_by_profile(MagicConfig *config,
                                                   const char *profile_name) {
  if (!config || !profile_name)
    return NULL;

  /* 遍历所有客户端配置 */
  for (uint32_t i = 0; i < config->num_clients; i++) {
    /* 比较 ProfileName 是否匹配 */
    if (strcmp(config->clients[i].profile_name, profile_name) == 0) {
      /* 检查配置是否启用 */
      if (config->clients[i].enabled) {
        return &config->clients[i];
      } else {
        fd_log_debug("[app_magic] Profile '%s' found but disabled",
                     profile_name);
        return NULL;
      }
    }
  }
  /* 未找到匹配的客户端 */
  return NULL;
}

/**
 * @brief 验证客户端是否允许使用指定的 DLM
 * @param client 客户端配置指针
 * @param dlm_id 要验证的 DLM ID (如 "LINK_SATCOM")
 * @return true=允许使用, false=不允许
 *
 * 检查 LinkPolicy.allowed_dlms 列表
 */
bool magic_config_is_dlm_allowed(const ClientProfile *client,
                                 const char *dlm_id) {
  if (!client || !dlm_id)
    return false;

  /* 如果没有配置允许的 DLM 列表，默认允许所有 */
  if (client->link_policy.num_allowed_dlms == 0) {
    return true;
  }

  /* 遍历允许的 DLM 列表 */
  for (uint32_t i = 0; i < client->link_policy.num_allowed_dlms; i++) {
    if (strcmp(client->link_policy.allowed_dlms[i], dlm_id) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 验证客户端是否允许使用指定的 QoS 级别
 * @param client 客户端配置指针
 * @param qos_level 要验证的 QoS 级别
 * @return true=允许使用, false=不允许
 *
 * 检查 QoS.allowed_levels 列表
 */
bool magic_config_is_qos_level_allowed(const ClientProfile *client,
                                       uint8_t qos_level) {
  if (!client)
    return false;

  /* 如果没有配置允许的 QoS 级别列表，默认允许所有 */
  if (client->qos.num_allowed_levels == 0) {
    return true;
  }

  /* 遍历允许的 QoS 级别列表 */
  for (uint32_t i = 0; i < client->qos.num_allowed_levels; i++) {
    if (client->qos.allowed_levels[i] == qos_level) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 验证客户端是否允许在指定的飞行阶段使用
 * @param client 客户端配置指针
 * @param phase 要验证的飞行阶段
 * @return true=允许使用, false=不允许
 *
 * 检查 Session.allowed_phases 列表
 */
bool magic_config_is_flight_phase_allowed(const ClientProfile *client,
                                          CfgFlightPhase phase) {
  if (!client)
    return false;

  /* 如果没有配置允许的飞行阶段列表，默认允许所有 */
  if (client->session.num_allowed_phases == 0) {
    return true;
  }

  /* 遍历允许的飞行阶段列表 */
  for (uint32_t i = 0; i < client->session.num_allowed_phases; i++) {
    if (client->session.allowed_phases[i] == phase) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 根据飞行阶段查找策略规则集
 * @param config 指向配置管理器结构体的指针
 * @param flight_phase 要查找的飞行阶段
 * @return 找到的规则集指针，NULL=未找到
 *
 * 该函数遍历所有策略规则集，查找包含指定飞行阶段的规则集
 */
PolicyRuleSet *magic_config_find_ruleset(MagicConfig *config,
                                         const char *flight_phase) {
  /* 遍历所有策略规则集 */
  for (uint32_t i = 0; i < config->policy.num_rulesets; i++) {
    /* 获取当前规则集指针 */
    PolicyRuleSet *ruleset = &config->policy.rulesets[i];
    /* 检查飞行阶段字符串是否包含指定的飞行阶段 */
    if (strstr(ruleset->flight_phases, flight_phase) != NULL) {
      /* 找到匹配的规则集，返回指针 */
      return ruleset;
    }
  }
  /* 未找到匹配的规则集，返回 NULL */
  return NULL;
}

/*===========================================================================
 * 打印配置摘要
 *
 * 本节提供配置信息打印功能
 * 用于调试和监控配置加载状态
 *===========================================================================*/

/**
 * @brief 打印配置摘要信息
 * @param config 指向配置管理器结构体的指针
 *
 * 该函数打印当前加载的所有配置信息的摘要
 * 包括 DLM 配置、策略规则集和客户端的数量及基本信息
 */
void magic_config_print_summary(const MagicConfig *config) {
  /* 打印摘要开始分隔符 */
  fd_log_notice("========================================");
  /* 打印标题 */
  fd_log_notice("  MAGIC Configuration Summary (v2.0)");
  /* 打印摘要结束分隔符 */
  fd_log_notice("========================================");

  /* 打印 DLM 配置数量 */
  fd_log_notice("DLM Configs: %u", config->num_dlm_configs);
  /* 遍历并打印每个 DLM 的详细信息 */
  for (uint32_t i = 0; i < config->num_dlm_configs; i++) {
    /* 获取当前 DLM 配置的指针 */
    const DLMConfig *dlm = &config->dlm_configs[i];
    /* 打印 DLM 序号、名称、类型、带宽和延迟 */
    fd_log_notice("  [%u] %s (Type=%d) - BW: %.0f/%.0f kbps, Latency: %u ms",
                  i + 1, dlm->dlm_name, dlm->dlm_type, dlm->max_forward_bw_kbps,
                  dlm->max_return_bw_kbps, dlm->latency_ms);
    fd_log_notice("      Socket: %s, Coverage: %s", dlm->dlm_socket_path,
                  dlm->coverage.enabled ? "enabled" : "disabled");
  }

  /* 打印 ADIF 降级模式状态 */
  if (config->adif_degraded_mode) {
    fd_log_notice("ADIF Status: DEGRADED MODE (only QoS 0-1 allowed)");
  }

  /* 打印策略规则集数量 */
  fd_log_notice("Policy Rulesets: %u", config->policy.num_rulesets);
  /* 打印客户端数量 */
  fd_log_notice("Clients: %u", config->num_clients);

  /* 打印摘要结束分隔符 */
  fd_log_notice("========================================");
}

/*===========================================================================
 * 清理
 *
 * 本节提供配置清理功能
 * 用于释放配置资源和重置状态
 *===========================================================================*/

/**
 * @brief 清理配置管理器
 * @param config 指向配置管理器结构体的指针
 *
 * 该函数将配置管理器重置为初始状态
 * 清除所有已加载的配置信息
 */
void magic_config_cleanup(MagicConfig *config) {
  /* 检查配置指针是否有效 */
  if (config) {
    /* 将整个配置结构体清零，释放所有资源 */
    memset(config, 0, sizeof(MagicConfig));
  }
}
