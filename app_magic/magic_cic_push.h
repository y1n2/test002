/**
 * @file magic_cic_push.h
 * @brief MAGIC CIC 主动推送功能 - MNTR/MSCR 服务端推送
 * @details 根据 ARINC 839 设计方案 4.1.3.3 和 4.1.3.4：
 *          - MNTR: 通知客户端会话参数变更（如链路中断、带宽调整）
 *          - MSCR: 向已订阅的客户端广播链路状态变化
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MAGIC_CIC_PUSH_H
#define MAGIC_CIC_PUSH_H

#include "app_magic.h"
#include "magic_session.h"

/*===========================================================================
 * MNTR 配置常量
 *===========================================================================*/

#define MNTR_ACK_TIMEOUT_SEC 5  /* MNTA 超时时间 (秒) */
#define MNTR_MIN_INTERVAL_SEC 1 /* 同一会话 MNTR 最小发送间隔 (秒) */
#define MNTR_BW_CHANGE_THRESHOLD                                               \
  10 /* 带宽变化阈值 (百分比), 小于此值不发送                                \
      */

/* MAGIC Status Codes (根据 ARINC 839 Attachment 1, §1.3.2)
 *
 * 重要: 以下状态码必须与 dict_magic_codes.h 中的定义一致！
 * 如需使用完整的状态码列表，请直接包含 dict_magic_codes.h
 */
#define MAGIC_STATUS_SUCCESS 0

/* 错误码 - 用于 MNTR 通知 */
#define MAGIC_STATUS_NO_FREE_BANDWIDTH 1016 /* 系统无剩余带宽 (规范1016) */
#define MAGIC_STATUS_SESSION_TIMEOUT 1024   /* 会话超时 (规范1024) */
#define MAGIC_STATUS_MAGIC_SHUTDOWN 1025    /* MAGIC 关闭 (规范1025) */

/* 系统错误码 (2000-2010) */
#define MAGIC_STATUS_LINK_ERROR 2007        /* 通用链路错误 (规范2007) */
#define MAGIC_STATUS_CLOSE_LINK_FAILED 2008 /* 关闭链路失败 (规范2008) */
#define MAGIC_STATUS_MAGIC_FAILURE 2009     /* MAGIC 内部故障 (规范2009) */
#define MAGIC_STATUS_FORCED_REROUTING 2010 /* 强制重路由/链路切换 (规范2010) \
                                            */

/* 错误码 3000+ */
#define MAGIC_STATUS_UNKNOWN_ISSUE 3000
#define MAGIC_STATUS_AVIONICSDATA_MISSING 3001

/*===========================================================================
 * MNTR 通知原因 - 基于 MAGIC-Status-Code (符合 ARINC 839 规范)
 *
 * 注意: ARINC 839 规范中 MNTR 不使用单独的 Notification-Type AVP，
 *       而是通过 MAGIC-Status-Code 和 Communication-Report-Parameters
 *       来传递通知信息。
 *
 * ARINC 839 规范定义的 MAGIC-Status-Code (用于 MNTR):
 *   - 1016 (NO_FREE_BANDWIDTH)   : 带宽不足/被抢占
 *   - 1024 (SESSION_TIMEOUT)     : 会话超时
 *   - 1025 (MAGIC_SHUTDOWN)      : 服务关闭
 *   - 2007 (LINK_ERROR)          : 链路错误/丢失
 *   - 2010 (FORCED_REROUTING)    : 链路切换/强制重路由
 *
 * 注: 链路恢复时，通过 Communication-Report-Parameters 中的
 *     Granted-Bandwidth > 0 来表示，Status-Code 可使用 0 (SUCCESS)
 *===========================================================================*/

/**
 * @brief 状态变更类型枚举
 */
typedef enum {
  STATUS_CHANGE_DLM_UP = 1,       ///< DLM 链路上线
  STATUS_CHANGE_DLM_DOWN = 2,     ///< DLM 链路下线
  STATUS_CHANGE_DLM_DEGRADED = 3, ///< DLM 链路降级
  STATUS_CHANGE_CLIENT_JOIN = 4,  ///< 客户端加入
  STATUS_CHANGE_CLIENT_LEAVE = 5  ///< 客户端离开
} StatusChangeType;

/*===========================================================================
 * 通知参数结构 (符合 ARINC 839 规范)
 *
 * MNTR 消息结构:
 *   <Session-Id>
 *   {Origin-Host}
 *   {Origin-Realm}
 *   {Destination-Realm}
 *   {Communication-Report-Parameters}
 *   [MAGIC-Status-Code]
 *   [Error-Message]
 *===========================================================================*/

/**
 * @brief MNTR 通知参数结构体
 * @details 包含 MNTR 消息的所有参数，符合 ARINC 839 规范。
 */
typedef struct {
  uint32_t magic_status_code; ///< MAGIC-Status-Code (必填)
  const char *error_message;  ///< Error-Message (可选)

  /* Communication-Report-Parameters 中的变更参数 */
  uint32_t new_granted_bw;     ///< 新的授予带宽 (bps)
  uint32_t new_granted_ret_bw; ///< 新的返回带宽 (bps)
  const char *new_link_id;     ///< 新的链路 ID
  uint32_t new_bearer_id;      ///< 新的 Bearer ID

  /* v2.1 新增: 链路切换时的网关 IP */
  const char *new_gateway_ip; ///< 新的网关 IP (切换通知时填充)

  /* v2.1 新增: 强制发送标志 (绕过风暴抑制) */
  bool force_send; ///< true=强制发送, 忽略阈值和间隔检查
} MNTRParams;

/**
 * @brief MSCR 状态变更参数结构体
 * @details 包含链路状态广播的所有参数。
 */
typedef struct {
  StatusChangeType type;      ///< 状态变更类型
  uint32_t magic_status_code; ///< MAGIC 状态码
  const char *error_message;  ///< 错误描述信息

  /* 状态信息 */
  const char *dlm_name;      ///< 变化的 DLM 名称
  int dlm_available;         ///< DLM 是否可用
  float max_bandwidth;       ///< 最大带宽
  float allocated_bandwidth; ///< 已分配带宽
} MSCRParams;

/*===========================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 向指定会话发送 MNTR 通知
 * @details 构造并发送 MNTR 消息，用于通知客户端链路状态、带宽变更等。
 *          包含风暴抑制和确认机制。
 *
 * @param ctx MAGIC 上下文
 * @param session 目标会话
 * @param params 通知参数 (状态码、带宽、错误信息等)
 * @return int 0 成功, -1 失败
 */
int magic_cic_send_mntr(MagicContext *ctx, ClientSession *session,
                        const MNTRParams *params);

/**
 * @brief 向所有已订阅状态的会话广播 MSCR
 * @details 遍历所有会话，查找到已订阅状态更新的会话，并向其发送 MSCR 消息。
 *          根据订阅级别 (MAGIC/DLM/Link) 过滤发送内容。
 *
 * @param ctx MAGIC 上下文
 * @param params 状态变更参数
 * @return int 发送成功的会话数量
 */
int magic_cic_broadcast_mscr(MagicContext *ctx, const MSCRParams *params);

/**
 * @brief 链路状态变化时调用 - 自动向相关会话发送通知
 * @details 当底层 DLM 链路状态发生变化 (UP/DOWN) 时调用。
 *          1. 向该链路上的所有活动会话发送 MNTR 通知。
 *          2. 向所有订阅了 DLM 状态的客户端广播 MSCR。
 *
 * @param ctx MAGIC 上下文
 * @param link_id 变化的链路 ID
 * @param is_up true=链路恢复, false=链路中断
 * @return int 0 成功, -1 失败
 */
int magic_cic_on_link_status_change(MagicContext *ctx, const char *link_id,
                                    bool is_up);

/**
 * @brief 带宽变更时调用 - 向受影响的会话发送 MNTR
 * @details 当会话的可用带宽发生显著变化时调用。
 *          内部会自动进行风暴抑制检查 (阈值和时间窗)。
 *
 * @param ctx MAGIC 上下文
 * @param session 受影响的会话
 * @param new_bw 新的带宽 (kbps)
 * @param reason 变更原因 (可选)
 * @return int 0 成功, -1 失败
 */
int magic_cic_on_bandwidth_change(MagicContext *ctx, ClientSession *session,
                                  uint32_t new_bw, const char *reason);

/**
 * @brief 链路切换时调用 - 向会话发送 MNTR (含新网关 IP)
 * @details 当会话从一条链路切换到另一条链路时调用。
 *          发送 MNTR 通知客户端新的链路 ID 和可能的网关 IP 变更。
 *          此通知通常强制发送 (Force Send)。
 *
 * @param ctx MAGIC 上下文
 * @param session 受影响的会话
 * @param new_link_id 新链路 ID
 * @param new_gateway_ip 新网关 IP 地址 (可选)
 * @return int 0 成功, -1 失败
 */
int magic_cic_on_handover(MagicContext *ctx, ClientSession *session,
                          const char *new_link_id, const char *new_gateway_ip);

/**
 * @brief 检查并清理 MNTR 超时的会话
 * @details 应由定时器周期性调用，检查 mntr_pending_ack 超时的会话并强制清理。
 * @param ctx MAGIC 上下文
 */
void magic_cic_check_mntr_timeouts(MagicContext *ctx);

/**
 * @brief 检查是否应该发送 MNTR (风暴抑制)
 * @details 根据时间窗 (MNTR_MIN_INTERVAL_SEC) 和带宽变化及其阈值
 * (MNTR_BW_CHANGE_THRESHOLD) 判断是否应该抑制本次通知。
 *
 * @param session 目标会话
 * @param new_bw_kbps 新带宽值 (kbps)
 * @param force 是否强制发送 (如链路通断、切换等重大事件)
 * @return bool true=应该发送, false=应该抑制
 */
bool magic_cic_should_send_mntr(ClientSession *session, uint32_t new_bw_kbps,
                                bool force);

/**
 * @brief 发送初始状态推送 MSCR (用于 MCAR 订阅后)
 * @details 当客户端刚完成注册并订阅了状态更新时，立即发送一次全量状态 MSCR。
 *          确保客户端获得当前的系统状态快照。
 *
 * @param ctx MAGIC 上下文
 * @param session 目标会话
 * @return int 0=成功, -1=失败
 */
int magic_cic_send_initial_mscr(MagicContext *ctx, ClientSession *session);

#endif /* MAGIC_CIC_PUSH_H */
