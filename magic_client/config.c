/**
 * @file config.c
 * @brief MAGIC 客户端配置解析实现。
 * @details 实现了从 .conf 文件加载属性、设置默认值以及从 freeDiameter
 * 全局配置中提取身份信息的逻辑。
 */

#include "config.h"
#include "magic_dict_handles.h"
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 全局配置单例初始化。
 */
app_config_t g_cfg = {0};

/**
 * @brief 设置配置的系统默认值。
 * @details 在正式解析配置文件前调用，确保未定义的字段具有合理的初始值。
 */
static void config_set_defaults(void) {
  g_cfg.auth_app_id = MAGIC_APP_ID;
  g_cfg.priority = 3;
  g_cfg.max_bw = 5000000;
  g_cfg.requested_bw = 3000000;
  g_cfg.required_bw = 1000000;
  g_cfg.qos_level = 2;
  g_cfg.cost_tolerance = 1.5f;
  g_cfg.auto_reconnect = 1;
  g_cfg.keep_alive_interval = 30;
  strcpy(g_cfg.client_id, "UNKNOWN-EFB");
  strcpy(g_cfg.username, "ABS");
  strcpy(g_cfg.client_password, "1111");
  strcpy(g_cfg.server_password, "1111");
}

int magic_conf_parse(const char *config_file) {
  FILE *fp = NULL;
  char line[512];
  char key[128], value[256];

  config_set_defaults();

  // 自动获取 Origin-Host / Origin-Realm (必须在 fd_core_parseconf 之后调用)
  if (fd_g_config->cnf_diamid) {
    strncpy(g_cfg.origin_host, fd_g_config->cnf_diamid,
            sizeof(g_cfg.origin_host) - 1);
  }
  if (fd_g_config->cnf_diamrlm) {
    strncpy(g_cfg.origin_realm, fd_g_config->cnf_diamrlm,
            sizeof(g_cfg.origin_realm) - 1);
  }

  // 打开配置文件
  fp = fopen(config_file, "r");
  if (!fp) {
    LOG_E("Failed to open MAGIC config file: %s", config_file);
    return -1;
  }

  // 逐行解析
  while (fgets(line, sizeof(line), fp)) {
    // 跳过注释和空行
    if (line[0] == '#' || line[0] == '\n')
      continue;

    // 解析 key = value 格式（去除行尾注释）
    if (sscanf(line, "%127s = %255[^#\n]", key, value) == 2) {
      // 去除 value 尾部空白字符
      char *p = value + strlen(value) - 1;
      while (p >= value && (*p == ' ' || *p == '\t'))
        *p-- = '\0';
      if (strcmp(key, "CLIENT_ID") == 0) {
        strncpy(g_cfg.client_id, value, sizeof(g_cfg.client_id) - 1);
      } else if (strcmp(key, "TAIL_NUMBER") == 0) {
        strncpy(g_cfg.tail_number, value, sizeof(g_cfg.tail_number) - 1);
      } else if (strcmp(key, "DESTINATION_REALM") == 0) {
        strncpy(g_cfg.destination_realm, value,
                sizeof(g_cfg.destination_realm) - 1);
      } else if (strcmp(key, "CLIENT_TYPE") == 0) {
        g_cfg.client_type = atoi(value);
      } else if (strcmp(key, "PRIORITY") == 0) {
        g_cfg.priority = atoi(value);
      } else if (strcmp(key, "MAX_BW") == 0) {
        g_cfg.max_bw = strtoull(value, NULL, 10);
      } else if (strcmp(key, "REQUESTED_BW") == 0) {
        g_cfg.requested_bw = strtoull(value, NULL, 10);
      } else if (strcmp(key, "REQUIRED_BW") == 0) {
        g_cfg.required_bw = strtoull(value, NULL, 10);
      } else if (strcmp(key, "QOS_LEVEL") == 0) {
        g_cfg.qos_level = atoi(value);
      } else if (strcmp(key, "COST_TOLERANCE") == 0) {
        g_cfg.cost_tolerance = atof(value);
      } else if (strcmp(key, "USERNAME") == 0) {
        strncpy(g_cfg.username, value, sizeof(g_cfg.username) - 1);
      } else if (strcmp(key, "CLIENT_PASSWORD") == 0) {
        strncpy(g_cfg.client_password, value,
                sizeof(g_cfg.client_password) - 1);
      } else if (strcmp(key, "SERVER_PASSWORD") == 0) {
        strncpy(g_cfg.server_password, value,
                sizeof(g_cfg.server_password) - 1);
      } else if (strcmp(key, "PROFILE_NAME") == 0) {
        strncpy(g_cfg.profile_name, value, sizeof(g_cfg.profile_name) - 1);
      } else if (strcmp(key, "AIRCRAFT_TYPE") == 0) {
        strncpy(g_cfg.aircraft_type, value, sizeof(g_cfg.aircraft_type) - 1);
      } else if (strcmp(key, "REQUESTED_RETURN_BW") == 0) {
        g_cfg.requested_return_bw = strtoull(value, NULL, 10);
      } else if (strcmp(key, "REQUIRED_RETURN_BW") == 0) {
        g_cfg.required_return_bw = strtoull(value, NULL, 10);
      } else if (strcmp(key, "PRIORITY_CLASS") == 0) {
        g_cfg.priority_class = atoi(value);
      } else if (strcmp(key, "TIMEOUT") == 0) {
        g_cfg.timeout = atoi(value);
      } else if (strcmp(key, "KEEP_REQUEST") == 0) {
        g_cfg.keep_request = atoi(value);
      } else if (strcmp(key, "AUTO_DETECT") == 0) {
        g_cfg.auto_detect = atoi(value);
      } else if (strcmp(key, "ACCOUNTING_ENABLED") == 0) {
        g_cfg.accounting_enabled = atoi(value);
      } else if (strcmp(key, "AUTO_RECONNECT") == 0) {
        g_cfg.auto_reconnect = atoi(value);
      } else if (strcmp(key, "KEEP_ALIVE_INTERVAL") == 0) {
        g_cfg.keep_alive_interval = atoi(value);
      }
      /* TFT_GROUND.X 规则解析 */
      else if (strncmp(key, "TFT_GROUND.", 11) == 0) {
        int idx = atoi(key + 11) - 1; // TFT_GROUND.1 → index 0
        if (idx >= 0 && idx < 32) {
          strncpy(g_cfg.tft_ground_rules[idx], value,
                  sizeof(g_cfg.tft_ground_rules[idx]) - 1);
          if (idx >= g_cfg.tft_ground_count) {
            g_cfg.tft_ground_count = idx + 1;
          }
          LOG_D("解析 TFT_GROUND.%d: %s", idx + 1, value);
        }
      }
      /* TFT_AIR.X 规则解析 */
      else if (strncmp(key, "TFT_AIR.", 8) == 0) {
        int idx = atoi(key + 8) - 1; // TFT_AIR.1 → index 0
        if (idx >= 0 && idx < 32) {
          strncpy(g_cfg.tft_aircraft_rules[idx], value,
                  sizeof(g_cfg.tft_aircraft_rules[idx]) - 1);
          if (idx >= g_cfg.tft_aircraft_count) {
            g_cfg.tft_aircraft_count = idx + 1;
          }
          LOG_D("解析 TFT_AIR.%d: %s", idx + 1, value);
        }
      }
      /* NAPT.X 规则解析 */
      else if (strncmp(key, "NAPT.", 5) == 0) {
        int idx = atoi(key + 5) - 1; // NAPT.1 → index 0
        if (idx >= 0 && idx < 10) {
          strncpy(g_cfg.napt_rules[idx], value,
                  sizeof(g_cfg.napt_rules[idx]) - 1);
          if (idx >= g_cfg.napt_count) {
            g_cfg.napt_count = idx + 1;
          }
          LOG_D("解析 NAPT.%d: %s", idx + 1, value);
        }
      }
    }
  }

  fclose(fp);

  printf("\n[CONFIG] TFT规则解析结果: GROUND=%d, AIR=%d, NAPT=%d\n",
         g_cfg.tft_ground_count, g_cfg.tft_aircraft_count, g_cfg.napt_count);
  for (int i = 0; i < g_cfg.tft_ground_count && i < 5; i++) {
    printf("[CONFIG]   TFT_GROUND.%d: %s\n", i + 1, g_cfg.tft_ground_rules[i]);
  }

  LOG_I("=== MAGIC 配置加载完成 ===");
  LOG_I("飞机应用ID      : %s (%s)", g_cfg.client_id,
        g_cfg.tail_number[0] ? g_cfg.tail_number : "未设置");
  LOG_I("机型            : %s",
        g_cfg.aircraft_type[0] ? g_cfg.aircraft_type : "未设置");
  LOG_I("Origin      : %s @ %s", g_cfg.origin_host, g_cfg.origin_realm);
  LOG_I("目标Realm   : %s", g_cfg.destination_realm);
  LOG_I("认证用户名  : %s", g_cfg.username);
  LOG_I("Profile-Name: %s",
        g_cfg.profile_name[0] ? g_cfg.profile_name : "未设置");
  LOG_I("带宽需求(下): %llu / %llu (请求/最低)",
        (unsigned long long)g_cfg.requested_bw,
        (unsigned long long)g_cfg.required_bw);
  LOG_I("带宽需求(上): %llu / %llu (请求/最低)",
        (unsigned long long)g_cfg.requested_return_bw,
        (unsigned long long)g_cfg.required_return_bw);
  LOG_I("优先级/QoS  : %u / %u", g_cfg.priority_class, g_cfg.qos_level);
  LOG_I("TFT规则数   : GROUND=%d, AIR=%d, NAPT=%d", g_cfg.tft_ground_count,
        g_cfg.tft_aircraft_count, g_cfg.napt_count);
  for (int i = 0; i < g_cfg.tft_ground_count; i++) {
    LOG_I("  TFT_GROUND.%d: %s", i + 1, g_cfg.tft_ground_rules[i]);
  }
  for (int i = 0; i < g_cfg.tft_aircraft_count; i++) {
    LOG_I("  TFT_AIR.%d: %s", i + 1, g_cfg.tft_aircraft_rules[i]);
  }

  return 0;
}