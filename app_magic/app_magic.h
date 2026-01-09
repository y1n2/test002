/**
 * @file app_magic.h
 * @brief MAGIC (Multi-link Aggregation Gateway for Internet Connectivity)
 * 扩展主头文件
 * @details 定义了 MAGIC
 * 扩展的核心上下文结构体，集成了配置、策略、LMI、会话、数据平面、ADIF
 * 和流量监控等所有子模块。
 */

#ifndef APP_MAGIC_H
#define APP_MAGIC_H

#include <freeDiameter/extension.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================
 * 从 magic_server 迁移的头文件
 *===========================================================================*/

#include "magic_adif.h"
#include "magic_cdr.h"
#include "magic_cic.h"
#include "magic_config.h"
#include "magic_dataplane.h"
#include "magic_lmi.h"
#include "magic_policy.h"
#include "magic_session.h"
#include "magic_traffic_monitor.h"

/*===========================================================================
 * MAGIC 全局上下文
 *===========================================================================*/

/**
 * @brief MAGIC 扩展全局上下文结构体
 * @details 该结构体作为单例存在，持有 MAGIC 扩展运行所需的所有子模块上下文。
 */
struct MagicContext {
  bool running;               ///< 运行标志位，为 false 时触发优雅退出。
  MagicConfig config;         ///< 静态配置数据（从 XML 加载）。
  PolicyContext policy_ctx;   ///< 策略决策引擎上下文。
  MagicLmiContext lmi_ctx;    ///< LMI (Link Management Interface) 接口上下文。
  SessionManager session_mgr; ///< Diameter 会话管理器。
  DataplaneContext dataplane_ctx; ///< 数据平面路由与 NAPT 上下文。
  AdifClientContext adif_ctx; ///< ADIF 客户端上下文 (用于获取飞机实时状态)。
  TrafficMonitorContext
      traffic_ctx;    ///< 流量监控上下文 (基于 nftables/iptables)。
  CDRManager cdr_mgr; ///< CDR (Call Detail Record) 管理器，负责计费数据持久化。
};
typedef struct MagicContext MagicContext;

/**
 * @brief 全局上下文单例引用。
 * @note
 * 该变量在应用生命周期内全程有效，由主线程初始化，各模块通过该句柄访问全局状态。
 *       由于包含多个子模块的互斥锁（如 session_mgr
 * 中的锁），访问时需注意各子模块内部的线程安全实现。
 */
extern MagicContext g_magic_ctx;

#endif /* APP_MAGIC_H */
