/**
 * @file config.h
 * @brief ARINC 839-2014 MAGIC 协议客户端全局配置定义。
 * @details 定义了客户端所需的所有静态配置参数，包括 Diameter 身份、认证凭据、
 *          带宽策略以及流量控制规则（TFT/NAPT）。
 *
 * @author MAGIC System Development Team
 * @date 2025-12-16
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief MAGIC 客户端全局配置结构体。
 * @details 存储从配置文件加载的所有静态参数，供全局访问。
 */
typedef struct {

  /* ==================== Diameter 基础身份（freeDiameter 自动填充）
   * ==================== */
  char origin_host[256];  ///< 本地 Diameter ID (Origin-Host)，例如
                          ///< "efb-a320.b-8888.airchina.com"
  char origin_realm[256]; ///< 本地 Realm (Origin-Realm)，例如 "airchina.com"
  char destination_realm[256]; ///< 目标 Realm，例如 "ground.airchina.com"
  char destination_host[256];  ///< 可选的目标 Host (Destination-Host)

  /* ==================== 固定协议参数 ==================== */
  uint32_t vendor_id;   ///< Vendor-Id，固定为 13712 (AEEC)
  uint32_t auth_app_id; ///< Auth-Application-Id，固定为 1094202169 (MAGIC)
  uint32_t cmd_code;    ///< Command-Code，固定为 839

  /* ==================== 客户端基本身份信息 ==================== */
  char client_id[64];     ///< 设备唯一ID (User-Name)，必填，如 "EFB-A320-001"
  char tail_number[32];   ///< 飞机注册号，如 "B-8888"
  char aircraft_type[32]; ///< 机型，如 "A320"
  uint32_t client_type;   ///< 客户端类型编码

  /* ==================== 认证相关字段 ==================== */
  char username[64];        ///< 认证用户名，用于身份校验
  char client_password[64]; ///< 客户端密码 (Client-Credentials)
  char server_password[64]; ///< 服务端确认密码 (Server-Password)

  /* ==================== 带宽与优先级策略 ==================== */
  uint64_t max_bw;      ///< 客户端理论最大可申请带宽 (bit/s)
  uint32_t priority;    ///< 旧版优先级字段 (兼容性预留)
  float cost_tolerance; ///< 费用容忍度（如 1.5 表示接受最高 50% 溢价）

  /* ==================== Communication-Request-Parameters (20001) 真实协议字段
   * ==================== */
  char profile_name[64]; ///< 业务配置文件名称，如 "IP_DATA"

  uint64_t requested_bw;        ///< 向服务器请求的下行带宽
  uint64_t requested_return_bw; ///< 向服务器请求的上行带宽
  uint64_t required_bw;         ///< 必须保证的最低下行带宽
  uint64_t required_return_bw;  ///< 必须保证的最低上行带宽

  uint32_t priority_type;  ///< 协议标准优先级类型 (1: Normal, 3: Emergency)
  uint32_t priority_class; ///< 协议标准优先级等级 (1-8)

  uint32_t qos_level;      ///< QoS 服务等级 (0-3)
  bool accounting_enabled; ///< 是否启用计费功能

  char dlm_name[64]; ///< 显式指定的优先数据链路模块 (DLM) 名称

  uint32_t flight_phase; ///< 当前飞行阶段代码
  uint32_t altitude;     ///< 当前飞行高度 (单位：米)

  uint32_t timeout;  ///< 会话超时时间 (秒)
  bool keep_request; ///< 链路中断后是否尝试保持会话
  bool auto_detect;  ///< 是否启用链路自动探测

  /* ==================== 流量控制规则 ==================== */
  char tft_ground_rules[32][256];   ///< 地面到飞机的流量过滤器 (TFT) 规则集
  int tft_ground_count;             ///< 有效地面规则数量
  char tft_aircraft_rules[32][256]; ///< 飞机到地面的流量过滤器 (TFT) 规则集
  int tft_aircraft_count;           ///< 有效飞机规则数量
  char napt_rules[32][256];         ///< 网络地址与端口转换 (NAPT) 规则集
  int napt_count;                   ///< 有效 NAPT 规则数量

  /* ==================== 运行时控制 ==================== */
  bool auto_reconnect;          ///< 是否在连接断开时自动重连
  uint32_t keep_alive_interval; ///< Keep-Alive 心跳间隔 (秒)
  uint32_t reconnect_delay;     ///< 自动重连前的延迟 (秒)

  int log_level;      ///< 客户端运行日志级别
  bool dump_messages; ///< 是否打印 Diameter 原始消息内容

  /* ==================== 预留扩展区 ==================== */
  char reserved[512]; ///< 结构体对齐与未来扩展预留

} app_config_t;

/**
 * @brief 全局配置实例。
 * @note 此变量在 config.c 中定义，受单线程解析保护。
 */
extern app_config_t g_cfg;

/**
 * @brief 解析客户端配置文件。
 * @details 读取指定的配置文件，解析 key = value 格式，并填充全局配置结构体
 * g_cfg。 如果某项缺失，将使用系统默认值。
 *
 * @param config_file 配置文件路径。
 * @return int 成功返回 0，失败返回 -1。
 *
 * @warning 配置文件必须具有读取权限。如果解析严重失败，可能会返回错误。
 */
int magic_conf_parse(const char *config_file);

#endif /* _CONFIG_H_ */