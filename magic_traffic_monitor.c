/**
 * @file magic_traffic_monitor.c
 * @brief MAGIC 流量监控模块实现 - 基于 Linux Netlink 机制
 * @description 使用 libnetfilter_conntrack 读取内核连接追踪计数器
 *
 * 实现说明:
 * 1. 使用 conntrack mark 标记每个会话的连接
 * 2. 通过 Netlink 查询带有特定 mark 的 conntrack 条目
 * 3. 聚合同一 mark 下所有连接的字节/数据包计数器
 * 4. 同时支持 iptables 和 nftables 后端
 *
 * 依赖:
 * - libnetfilter_conntrack (需安装 libnetfilter_conntrack-dev)
 * - iptables 或 nftables
 *
 * @author MAGIC System Development Team
 * @version 2.1
 * @date 2025-12-02
 */

#include "magic_traffic_monitor.h"
#include <arpa/inet.h>
#include <errno.h>
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* libnetfilter_conntrack 头文件 */
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack_tcp.h>

/*===========================================================================
 * 内部数据结构
 *===========================================================================*/

/**
 * @brief Netlink 回调上下文。
 * @details 在遍历 conntrack 表时传递给回调函数，用于过滤和累加统计数据。
 */
typedef struct {
  uint32_t target_mark; ///< 查询的目标 conntrack mark。
  TrafficStats *stats;  ///< 输出统计结构指针。
  int match_count;      ///< 匹配的连接数。
} NetlinkCallbackCtx;

/*===========================================================================
 * 辅助函数
 *===========================================================================*/

/**
 * @brief DJB2 hash 算法。
 * @details 经典的字符串哈希算法，用于将 session_id 映射到 conntrack mark 范围。
 *
 * @param str 输入字符串。
 * @return uint32_t 哈希值。
 */
static uint32_t djb2_hash(const char *str) {
  uint32_t hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

/**
 * @brief 计算 session_id 的 hash 值作为 conntrack mark。
 * @details 使用 DJB2 算法，并将结果映射到 [TRAFFIC_MARK_BASE, TRAFFIC_MARK_MAX]
 * 范围。
 *
 * @param session_id Diameter 会话 ID。
 * @return uint32_t 计算出的 conntrack mark，无效输入返回 0。
 */
uint32_t traffic_session_id_to_mark(const char *session_id) {
  if (!session_id || !session_id[0]) {
    return 0;
  }

  uint32_t hash = djb2_hash(session_id);
  /* 映射到 [TRAFFIC_MARK_BASE, TRAFFIC_MARK_MAX] 范围 */
  uint32_t range = TRAFFIC_MARK_MAX - TRAFFIC_MARK_BASE + 1;
  return TRAFFIC_MARK_BASE + (hash % range);
}

/**
 * @brief 检测可用的防火墙后端类型。
 * @details 优先检测 nftables，其次 iptables。如果都不可用，默认使用 iptables。
 *
 * @return TrafficBackendType 检测到的后端类型。
 */
TrafficBackendType traffic_detect_backend(void) {
  int ret;

  /* 检查 nftables - 使用绝对路径 */
  ret = system("/usr/sbin/nft --version > /dev/null 2>&1 || "
               "/sbin/nft --version > /dev/null 2>&1 || "
               "nft --version > /dev/null 2>&1");
  if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
    fd_log_notice("[traffic] 检测到 nftables 后端");
    return TRAFFIC_BACKEND_NFTABLES;
  }

  /* 检查 iptables - 使用绝对路径 */
  ret = system("/usr/sbin/iptables --version > /dev/null 2>&1 || "
               "/sbin/iptables --version > /dev/null 2>&1 || "
               "iptables --version > /dev/null 2>&1");
  if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
    fd_log_notice("[traffic] 检测到 iptables 后端");
    return TRAFFIC_BACKEND_IPTABLES;
  }

  /* 文件检查作为备选 */
  if (access("/usr/sbin/iptables", X_OK) == 0 ||
      access("/sbin/iptables", X_OK) == 0) {
    fd_log_notice("[traffic] 检测到 iptables 后端 (文件检查)");
    return TRAFFIC_BACKEND_IPTABLES;
  }

  fd_log_notice("[traffic] 未检测到防火墙后端，使用默认 iptables");
  return TRAFFIC_BACKEND_IPTABLES;
}
/**
 * @brief 执行系统命令
 */
static int exec_cmd(const char *cmd) {
  if (!cmd)
    return -1;

  fd_log_debug("[traffic] 执行命令: %s", cmd);

  int ret = system(cmd);
  if (ret == -1) {
    fd_log_error("[traffic] system() 执行失败: %s", strerror(errno));
    return -1;
  }

  return WEXITSTATUS(ret);
}

/**
 * @brief 查找空闲的会话槽位
 */
static TrafficSession *find_free_slot(TrafficMonitorContext *ctx) {
  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    if (!ctx->sessions[i].in_use) {
      return &ctx->sessions[i];
    }
  }
  return NULL;
}

/*===========================================================================
 * Netlink 回调函数
 *===========================================================================*/

/**
 * @brief conntrack 条目回调函数
 * 当查询到匹配的 conntrack 条目时调用
 */
static int conntrack_callback(enum nf_conntrack_msg_type type,
                              struct nf_conntrack *ct, void *data) {
  (void)type;
  NetlinkCallbackCtx *cb_ctx = (NetlinkCallbackCtx *)data;

  if (!ct || !cb_ctx) {
    return NFCT_CB_CONTINUE;
  }

  /* 获取 conntrack mark */
  uint32_t mark = nfct_get_attr_u32(ct, ATTR_MARK);

  /* 检查是否匹配目标 mark */
  if (mark != cb_ctx->target_mark) {
    return NFCT_CB_CONTINUE;
  }

  /* 累加原方向 (客户端发送) 的字节/数据包 */
  uint64_t bytes_orig = nfct_get_attr_u64(ct, ATTR_ORIG_COUNTER_BYTES);
  uint64_t pkts_orig = nfct_get_attr_u64(ct, ATTR_ORIG_COUNTER_PACKETS);

  /* 累加反方向 (客户端接收) 的字节/数据包 */
  uint64_t bytes_reply = nfct_get_attr_u64(ct, ATTR_REPL_COUNTER_BYTES);
  uint64_t pkts_reply = nfct_get_attr_u64(ct, ATTR_REPL_COUNTER_PACKETS);

  cb_ctx->stats->bytes_in += bytes_orig;
  cb_ctx->stats->packets_in += pkts_orig;
  cb_ctx->stats->bytes_out += bytes_reply;
  cb_ctx->stats->packets_out += pkts_reply;
  cb_ctx->match_count++;

  fd_log_debug(
      "[traffic] conntrack match: mark=0x%x, orig=%lu/%lu, reply=%lu/%lu", mark,
      bytes_orig, pkts_orig, bytes_reply, pkts_reply);

  return NFCT_CB_CONTINUE;
}

/**
 * @brief 通过 Netlink 查询指定 mark 的流量统计
 */
static int query_conntrack_stats(TrafficMonitorContext *ctx, uint32_t mark,
                                 TrafficStats *stats) {
  if (!ctx || !stats)
    return -1;

  memset(stats, 0, sizeof(TrafficStats));

  /* 创建 conntrack 句柄 */
  struct nfct_handle *h = nfct_open(CONNTRACK, 0);
  if (!h) {
    fd_log_error("[traffic] nfct_open 失败: %s", strerror(errno));
    return -1;
  }

  /* 设置回调上下文 */
  NetlinkCallbackCtx cb_ctx = {
      .target_mark = mark, .stats = stats, .match_count = 0};

  /* 注册回调 */
  nfct_callback_register(h, NFCT_T_ALL, conntrack_callback, &cb_ctx);

  /* 查询所有 conntrack 条目 */
  int ret = nfct_query(h, NFCT_Q_DUMP, &(int){AF_INET});
  if (ret == -1) {
    fd_log_error("[traffic] nfct_query 失败: %s", strerror(errno));
    nfct_close(h);
    return -1;
  }

  /* 同时查询 IPv6 */
  nfct_query(h, NFCT_Q_DUMP, &(int){AF_INET6});

  nfct_close(h);

  stats->last_update = time(NULL);

  fd_log_debug("[traffic] 查询 mark=0x%x: %d 条匹配, in=%lu/%lu, out=%lu/%lu",
               mark, cb_ctx.match_count, stats->bytes_in, stats->packets_in,
               stats->bytes_out, stats->packets_out);

  return 0;
}

/*===========================================================================
 * 防火墙规则管理
 *===========================================================================*/

/**
 * @brief 添加 iptables 规则标记连接
 */
static int add_iptables_rule(const char *client_ip, uint32_t mark) {
  char cmd[512];

  /* 创建 MAGIC 专用链 (如果不存在) */
  exec_cmd("iptables -t mangle -N MAGIC_MARK 2>/dev/null");
  exec_cmd("iptables -t mangle -C PREROUTING -j MAGIC_MARK 2>/dev/null || "
           "iptables -t mangle -I PREROUTING 1 -j MAGIC_MARK");

  /* 添加标记规则 - 来自客户端的流量 */
  snprintf(cmd, sizeof(cmd),
           "iptables -t mangle -C MAGIC_MARK -s %s -j CONNMARK --set-mark 0x%x "
           "2>/dev/null || "
           "iptables -t mangle -A MAGIC_MARK -s %s -j CONNMARK --set-mark 0x%x",
           client_ip, mark, client_ip, mark);

  if (exec_cmd(cmd) != 0) {
    fd_log_error("[traffic] 添加 iptables 规则失败: %s mark=0x%x", client_ip,
                 mark);
    return -1;
  }

  /* 添加标记规则 - 发往客户端的流量 */
  snprintf(cmd, sizeof(cmd),
           "iptables -t mangle -C MAGIC_MARK -d %s -j CONNMARK --set-mark 0x%x "
           "2>/dev/null || "
           "iptables -t mangle -A MAGIC_MARK -d %s -j CONNMARK --set-mark 0x%x",
           client_ip, mark, client_ip, mark);
  exec_cmd(cmd);

  /* 恢复 connmark 到 nfmark (用于计数) */
  exec_cmd("iptables -t mangle -C MAGIC_MARK -j CONNMARK --restore-mark "
           "2>/dev/null || "
           "iptables -t mangle -A MAGIC_MARK -j CONNMARK --restore-mark");

  fd_log_notice("[traffic] ✓ 添加 iptables 规则: %s → mark=0x%x", client_ip,
                mark);

  return 0;
}

/**
 * @brief 删除 iptables 规则
 */
static int del_iptables_rule(const char *client_ip, uint32_t mark) {
  char cmd[512];

  /* 删除来自客户端的规则 */
  snprintf(cmd, sizeof(cmd),
           "iptables -t mangle -D MAGIC_MARK -s %s -j CONNMARK --set-mark 0x%x "
           "2>/dev/null",
           client_ip, mark);
  exec_cmd(cmd);

  /* 删除发往客户端的规则 */
  snprintf(cmd, sizeof(cmd),
           "iptables -t mangle -D MAGIC_MARK -d %s -j CONNMARK --set-mark 0x%x "
           "2>/dev/null",
           client_ip, mark);
  exec_cmd(cmd);

  fd_log_notice("[traffic] ✓ 删除 iptables 规则: %s mark=0x%x", client_ip,
                mark);

  return 0;
}

/**
 * @brief 添加 nftables 规则标记连接
 */
static int add_nftables_rule(const char *client_ip, uint32_t mark) {
  char cmd[512];

  /* 创建 MAGIC 表和链 (如果不存在) */
  exec_cmd("nft add table inet magic 2>/dev/null");
  exec_cmd("nft add chain inet magic prerouting '{ type filter hook prerouting "
           "priority -150; policy accept; }' 2>/dev/null");

  /* 添加标记规则 - 来自客户端 */
  snprintf(cmd, sizeof(cmd),
           "nft add rule inet magic prerouting ip saddr %s ct mark set 0x%x",
           client_ip, mark);

  if (exec_cmd(cmd) != 0) {
    fd_log_error("[traffic] 添加 nftables 规则失败: %s mark=0x%x", client_ip,
                 mark);
    return -1;
  }

  /* 添加标记规则 - 发往客户端 */
  snprintf(cmd, sizeof(cmd),
           "nft add rule inet magic prerouting ip daddr %s ct mark set 0x%x",
           client_ip, mark);
  exec_cmd(cmd);

  fd_log_notice("[traffic] ✓ 添加 nftables 规则: %s → mark=0x%x", client_ip,
                mark);

  return 0;
}

/**
 * @brief 删除 nftables 规则
 */
static int del_nftables_rule(const char *client_ip, uint32_t mark) {
  char cmd[512];

  /* nftables 需要先获取规则 handle，这里简化处理 */
  /* 删除匹配 IP 和 mark 的规则 */
  snprintf(cmd, sizeof(cmd),
           "nft -a list chain inet magic prerouting 2>/dev/null | "
           "grep -E '(saddr|daddr) %s.*ct mark set 0x%x' | "
           "awk '{print $NF}' | xargs -I{} nft delete rule inet magic "
           "prerouting handle {}",
           client_ip, mark);
  exec_cmd(cmd);

  fd_log_notice("[traffic] ✓ 删除 nftables 规则: %s mark=0x%x", client_ip,
                mark);

  return 0;
}

/*===========================================================================
 * 公共 API 实现
 *===========================================================================*/

int traffic_monitor_init(TrafficMonitorContext *ctx,
                         TrafficBackendType backend) {
  if (!ctx) {
    fd_log_error("[traffic] 上下文指针为空");
    return -1;
  }

  memset(ctx, 0, sizeof(TrafficMonitorContext));
  pthread_mutex_init(&ctx->mutex, NULL);

  /* 检测或设置后端 */
  if (backend == TRAFFIC_BACKEND_AUTO) {
    ctx->backend = traffic_detect_backend();
  } else {
    ctx->backend = backend;
  }

  ctx->next_mark = TRAFFIC_MARK_BASE;

  /* 测试 Netlink 连接 */
  struct nfct_handle *h = nfct_open(CONNTRACK, 0);
  if (h) {
    nfct_close(h);
    ctx->netlink_available = true;
    fd_log_notice("[traffic] Netlink conntrack 连接测试成功");

    /* 检查并自动启用 conntrack 计数 */
    FILE *fp = fopen("/proc/sys/net/netfilter/nf_conntrack_acct", "r");
    if (fp) {
      char val;
      if (fread(&val, 1, 1, fp) > 0 && val == '0') {
        fclose(fp);
        fd_log_notice(
            "[traffic] 检测到 nf_conntrack_acct 未启用，尝试自动启用...");

        /* 尝试自动启用 */
        fp = fopen("/proc/sys/net/netfilter/nf_conntrack_acct", "w");
        if (fp) {
          if (fwrite("1", 1, 1, fp) > 0) {
            fd_log_notice(
                "[traffic] ✓ 已自动启用 net.netfilter.nf_conntrack_acct");
          } else {
            fd_log_error("[traffic] ✗ 自动启用失败: %s", strerror(errno));
            fd_log_error("[traffic] 请手动运行: sysctl -w "
                         "net.netfilter.nf_conntrack_acct=1");
          }
          fclose(fp);
        } else {
          fd_log_error("[traffic] ✗ 无权限启用 nf_conntrack_acct");
          fd_log_error("[traffic] 请以 root 权限运行，或手动执行: sysctl -w "
                       "net.netfilter.nf_conntrack_acct=1");
        }
      } else {
        fclose(fp);
      }
    }
  } else {
    ctx->netlink_available = false;
    fd_log_error("[traffic] Netlink conntrack 连接失败: %s", strerror(errno));
    fd_log_error("[traffic] 请确保以 root 权限运行，并加载 nf_conntrack 模块");
  }

  ctx->is_initialized = true;

  fd_log_notice("[traffic] ════════════════════════════════════════");
  fd_log_notice("[traffic] MAGIC 流量监控模块初始化");
  fd_log_notice("[traffic]   后端: %s", ctx->backend == TRAFFIC_BACKEND_NFTABLES
                                            ? "nftables"
                                            : "iptables");
  fd_log_notice("[traffic]   Netlink: %s",
                ctx->netlink_available ? "可用" : "不可用");
  fd_log_notice("[traffic]   Mark 范围: 0x%x - 0x%x", TRAFFIC_MARK_BASE,
                TRAFFIC_MARK_MAX);
  fd_log_notice("[traffic] ════════════════════════════════════════");

  return 0;
}

uint32_t traffic_register_session(TrafficMonitorContext *ctx,
                                  const char *session_id, const char *client_id,
                                  const char *client_ip) {
  if (!ctx || !ctx->is_initialized) {
    fd_log_error("[traffic] 流量监控未初始化");
    return 0;
  }

  if (!session_id || !client_ip) {
    fd_log_error("[traffic] 参数无效");
    return 0;
  }

  pthread_mutex_lock(&ctx->mutex);

  /* 检查是否已注册 */
  TrafficSession *existing = traffic_find_session(ctx, session_id);
  if (existing) {
    fd_log_notice("[traffic] 会话已注册: %s mark=0x%x", session_id,
                  existing->conntrack_mark);
    pthread_mutex_unlock(&ctx->mutex);
    return existing->conntrack_mark;
  }

  /* 查找空闲槽位 */
  TrafficSession *slot = find_free_slot(ctx);
  if (!slot) {
    fd_log_error("[traffic] 会话槽位已满: %u", MAX_TRAFFIC_SESSIONS);
    pthread_mutex_unlock(&ctx->mutex);
    return 0;
  }

  /* 计算 conntrack mark (基于 session_id hash) */
  uint32_t mark = traffic_session_id_to_mark(session_id);

  /* 检查 mark 冲突 */
  TrafficSession *conflict = traffic_find_by_mark(ctx, mark);
  if (conflict) {
    /* 发生冲突，使用递增分配 */
    mark = ctx->next_mark++;
    if (ctx->next_mark > TRAFFIC_MARK_MAX) {
      ctx->next_mark = TRAFFIC_MARK_BASE;
    }
    fd_log_debug("[traffic] mark 冲突，使用递增分配: 0x%x", mark);
  }

  /* 添加防火墙规则 */
  int ret;
  if (ctx->backend == TRAFFIC_BACKEND_NFTABLES) {
    ret = add_nftables_rule(client_ip, mark);
  } else {
    ret = add_iptables_rule(client_ip, mark);
  }

  if (ret != 0) {
    fd_log_error("[traffic] 添加防火墙规则失败");
    pthread_mutex_unlock(&ctx->mutex);
    return 0;
  }

  /* 填充会话信息 */
  memset(slot, 0, sizeof(TrafficSession));
  slot->in_use = true;
  strncpy(slot->session_id, session_id, MAX_SESSION_ID_LEN - 1);
  if (client_id) {
    strncpy(slot->client_id, client_id, sizeof(slot->client_id) - 1);
  }
  strncpy(slot->client_ip, client_ip, sizeof(slot->client_ip) - 1);
  slot->conntrack_mark = mark;
  slot->stats.start_time = time(NULL);

  ctx->session_count++;

  pthread_mutex_unlock(&ctx->mutex);

  fd_log_notice("[traffic] ✓ 注册会话: %s client=%s ip=%s mark=0x%x",
                session_id, client_id ? client_id : "", client_ip, mark);

  return mark;
}

int traffic_unregister_session(TrafficMonitorContext *ctx,
                               const char *session_id) {
  if (!ctx || !ctx->is_initialized || !session_id) {
    return -1;
  }

  pthread_mutex_lock(&ctx->mutex);

  TrafficSession *sess = traffic_find_session(ctx, session_id);
  if (!sess) {
    fd_log_debug("[traffic] 未找到会话: %s", session_id);
    pthread_mutex_unlock(&ctx->mutex);
    return -1;
  }

  /* 删除防火墙规则 */
  if (ctx->backend == TRAFFIC_BACKEND_NFTABLES) {
    del_nftables_rule(sess->client_ip, sess->conntrack_mark);
  } else {
    del_iptables_rule(sess->client_ip, sess->conntrack_mark);
  }

  fd_log_notice("[traffic] ✓ 注销会话: %s mark=0x%x 流量: in=%lu out=%lu",
                session_id, sess->conntrack_mark, sess->stats.bytes_in,
                sess->stats.bytes_out);

  sess->in_use = false;
  ctx->session_count--;

  pthread_mutex_unlock(&ctx->mutex);

  return 0;
}

int traffic_get_session_stats(TrafficMonitorContext *ctx,
                              const char *session_id, TrafficStats *stats) {
  if (!ctx || !ctx->is_initialized || !session_id || !stats) {
    return -1;
  }

  memset(stats, 0, sizeof(TrafficStats));

  pthread_mutex_lock(&ctx->mutex);

  TrafficSession *sess = traffic_find_session(ctx, session_id);
  if (!sess) {
    fd_log_debug("[traffic] 未找到会话: %s", session_id);
    pthread_mutex_unlock(&ctx->mutex);
    return -1;
  }

  /* 检查缓存是否有效 */
  time_t now = time(NULL);
  if (now - sess->cache_time < STATS_CACHE_TTL_SEC) {
    /* 使用缓存 */
    memcpy(stats, &sess->cached_stats, sizeof(TrafficStats));
    pthread_mutex_unlock(&ctx->mutex);
    return 0;
  }

  /* 查询 Netlink */
  if (ctx->netlink_available) {
    if (query_conntrack_stats(ctx, sess->conntrack_mark, stats) == 0) {
      /* 更新缓存 */
      memcpy(&sess->cached_stats, stats, sizeof(TrafficStats));
      sess->cache_time = now;

      /* 累加到会话统计 */
      sess->stats.bytes_in = stats->bytes_in;
      sess->stats.bytes_out = stats->bytes_out;
      sess->stats.packets_in = stats->packets_in;
      sess->stats.packets_out = stats->packets_out;
      sess->stats.last_update = now;
    }
  } else {
    /* Netlink 不可用，返回已缓存的统计 */
    memcpy(stats, &sess->stats, sizeof(TrafficStats));
  }

  stats->start_time = sess->stats.start_time;

  pthread_mutex_unlock(&ctx->mutex);

  return 0;
}

int traffic_get_client_stats(TrafficMonitorContext *ctx, const char *client_id,
                             TrafficStats *total_stats) {
  if (!ctx || !ctx->is_initialized || !client_id || !total_stats) {
    return -1;
  }

  memset(total_stats, 0, sizeof(TrafficStats));
  int count = 0;

  pthread_mutex_lock(&ctx->mutex);

  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    TrafficSession *sess = &ctx->sessions[i];
    if (sess->in_use && strcmp(sess->client_id, client_id) == 0) {
      TrafficStats sess_stats;

      /* 查询每个会话的统计 */
      if (ctx->netlink_available) {
        query_conntrack_stats(ctx, sess->conntrack_mark, &sess_stats);
      } else {
        memcpy(&sess_stats, &sess->stats, sizeof(TrafficStats));
      }

      total_stats->bytes_in += sess_stats.bytes_in;
      total_stats->bytes_out += sess_stats.bytes_out;
      total_stats->packets_in += sess_stats.packets_in;
      total_stats->packets_out += sess_stats.packets_out;

      if (total_stats->start_time == 0 ||
          sess->stats.start_time < total_stats->start_time) {
        total_stats->start_time = sess->stats.start_time;
      }

      count++;
    }
  }

  total_stats->last_update = time(NULL);

  pthread_mutex_unlock(&ctx->mutex);

  fd_log_debug("[traffic] 客户端 %s: %d 个会话, in=%lu out=%lu", client_id,
               count, total_stats->bytes_in, total_stats->bytes_out);

  return count;
}

int traffic_get_all_stats(TrafficMonitorContext *ctx,
                          TrafficStats *total_stats) {
  if (!ctx || !ctx->is_initialized || !total_stats) {
    return -1;
  }

  memset(total_stats, 0, sizeof(TrafficStats));
  int count = 0;

  pthread_mutex_lock(&ctx->mutex);

  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    TrafficSession *sess = &ctx->sessions[i];
    if (sess->in_use) {
      TrafficStats sess_stats;

      if (ctx->netlink_available) {
        query_conntrack_stats(ctx, sess->conntrack_mark, &sess_stats);
      } else {
        memcpy(&sess_stats, &sess->stats, sizeof(TrafficStats));
      }

      total_stats->bytes_in += sess_stats.bytes_in;
      total_stats->bytes_out += sess_stats.bytes_out;
      total_stats->packets_in += sess_stats.packets_in;
      total_stats->packets_out += sess_stats.packets_out;

      if (total_stats->start_time == 0 ||
          sess->stats.start_time < total_stats->start_time) {
        total_stats->start_time = sess->stats.start_time;
      }

      count++;
    }
  }

  total_stats->last_update = time(NULL);

  pthread_mutex_unlock(&ctx->mutex);

  return count;
}

int traffic_refresh_stats(TrafficMonitorContext *ctx) {
  if (!ctx || !ctx->is_initialized) {
    return -1;
  }

  if (!ctx->netlink_available) {
    fd_log_debug("[traffic] Netlink 不可用，跳过刷新");
    return 0;
  }

  pthread_mutex_lock(&ctx->mutex);

  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    TrafficSession *sess = &ctx->sessions[i];
    if (sess->in_use) {
      TrafficStats stats;
      if (query_conntrack_stats(ctx, sess->conntrack_mark, &stats) == 0) {
        memcpy(&sess->stats, &stats, sizeof(TrafficStats));
        sess->stats.start_time = sess->stats.start_time; /* 保留开始时间 */
        memcpy(&sess->cached_stats, &stats, sizeof(TrafficStats));
        sess->cache_time = time(NULL);
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);

  fd_log_notice("[traffic] ✓ 刷新所有会话统计");

  return 0;
}

TrafficSession *traffic_find_session(TrafficMonitorContext *ctx,
                                     const char *session_id) {
  if (!ctx || !session_id)
    return NULL;

  /* 注意: 调用者应已持有锁 */
  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    if (ctx->sessions[i].in_use &&
        strcmp(ctx->sessions[i].session_id, session_id) == 0) {
      return &ctx->sessions[i];
    }
  }

  return NULL;
}

TrafficSession *traffic_find_by_mark(TrafficMonitorContext *ctx,
                                     uint32_t mark) {
  if (!ctx)
    return NULL;

  /* 注意: 调用者应已持有锁 */
  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    if (ctx->sessions[i].in_use && ctx->sessions[i].conntrack_mark == mark) {
      return &ctx->sessions[i];
    }
  }

  return NULL;
}

void traffic_monitor_print_status(TrafficMonitorContext *ctx) {
  if (!ctx)
    return;

  pthread_mutex_lock(&ctx->mutex);

  fd_log_notice("[traffic] ════════════════════════════════════════");
  fd_log_notice("[traffic] 流量监控状态");
  fd_log_notice("[traffic] ════════════════════════════════════════");
  fd_log_notice("[traffic] 后端: %s", ctx->backend == TRAFFIC_BACKEND_NFTABLES
                                          ? "nftables"
                                          : "iptables");
  fd_log_notice("[traffic] Netlink: %s",
                ctx->netlink_available ? "可用" : "不可用");
  fd_log_notice("[traffic] 活动会话: %u / %u", ctx->session_count,
                MAX_TRAFFIC_SESSIONS);

  fd_log_notice("[traffic] ─────────────────────────────────────");

  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    TrafficSession *sess = &ctx->sessions[i];
    if (sess->in_use) {
      fd_log_notice("[traffic]   [0x%03x] %s", sess->conntrack_mark,
                    sess->session_id);
      fd_log_notice("[traffic]           client=%s ip=%s",
                    sess->client_id[0] ? sess->client_id : "-",
                    sess->client_ip);
      fd_log_notice("[traffic]           in=%lu bytes / %lu pkts",
                    sess->stats.bytes_in, sess->stats.packets_in);
      fd_log_notice("[traffic]           out=%lu bytes / %lu pkts",
                    sess->stats.bytes_out, sess->stats.packets_out);
    }
  }

  fd_log_notice("[traffic] ════════════════════════════════════════");

  pthread_mutex_unlock(&ctx->mutex);
}

void traffic_monitor_cleanup(TrafficMonitorContext *ctx) {
  if (!ctx)
    return;

  pthread_mutex_lock(&ctx->mutex);

  fd_log_notice("[traffic] 正在清理流量监控模块...");

  /* 删除所有会话的防火墙规则 */
  for (uint32_t i = 0; i < MAX_TRAFFIC_SESSIONS; i++) {
    TrafficSession *sess = &ctx->sessions[i];
    if (sess->in_use) {
      if (ctx->backend == TRAFFIC_BACKEND_NFTABLES) {
        del_nftables_rule(sess->client_ip, sess->conntrack_mark);
      } else {
        del_iptables_rule(sess->client_ip, sess->conntrack_mark);
      }
      sess->in_use = false;
    }
  }
  ctx->session_count = 0;

  /* 清理防火墙链 */
  if (ctx->backend == TRAFFIC_BACKEND_NFTABLES) {
    exec_cmd("nft delete table inet magic 2>/dev/null");
  } else {
    exec_cmd("iptables -t mangle -F MAGIC_MARK 2>/dev/null");
    exec_cmd("iptables -t mangle -D PREROUTING -j MAGIC_MARK 2>/dev/null");
    exec_cmd("iptables -t mangle -X MAGIC_MARK 2>/dev/null");
  }

  ctx->is_initialized = false;

  pthread_mutex_unlock(&ctx->mutex);
  pthread_mutex_destroy(&ctx->mutex);

  fd_log_notice("[traffic] ✓ 流量监控模块已清理");
}
