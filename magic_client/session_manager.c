/**
 * @file session_manager.c
 * @brief MAGIC 客户端会话与 DLM 状态管理器实现。
 * @details 提供了高并发场景下多个 Diameter 会话的维护逻辑，
 *          以及一个受线程互斥保护的 DLM 状态缓存池。
 */

#include "session_manager.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 外部全局变量 */
extern app_config_t g_cfg;

/* 会话管理器锁 */
static pthread_mutex_t g_session_mutex = PTHREAD_MUTEX_INITIALIZER;

/*===========================================================================
 * 初始化
 *===========================================================================*/

void session_manager_init(SessionManager *mgr) {
  if (!mgr)
    return;

  pthread_mutex_lock(&g_session_mutex);

  memset(mgr->sessions, 0, sizeof(mgr->sessions));
  mgr->num_active = 0;
  mgr->current_session_id[0] = '\0';

  pthread_mutex_unlock(&g_session_mutex);

  printf("[Session Manager] Initialized\n");
}

/*===========================================================================
 * Session-Id 生成
 *===========================================================================*/

int session_manager_generate_id(SessionManager *mgr, char *session_id,
                                size_t size) {
  if (!mgr || !session_id || size == 0)
    return -1;

  /* 格式: <Origin-Host>;<timestamp>;<random> */
  snprintf(session_id, size, "%s;%ld;%08x", g_cfg.origin_host, (long)time(NULL),
           (unsigned int)rand());

  return 0;
}

/*===========================================================================
 * 会话创建
 *===========================================================================*/

ClientSessionRecord *session_manager_create(SessionManager *mgr,
                                            const char *session_id) {
  if (!mgr || !session_id)
    return NULL;

  pthread_mutex_lock(&g_session_mutex);

  /* 查找空闲槽位 */
  int free_slot = -1;
  for (int i = 0; i < MAX_CLIENT_SESSIONS; i++) {
    if (!mgr->sessions[i].in_use) {
      free_slot = i;
      break;
    }
  }

  if (free_slot == -1) {
    pthread_mutex_unlock(&g_session_mutex);
    fprintf(stderr, "[Session Manager] ERROR: No available session slots\n");
    return NULL;
  }

  ClientSessionRecord *session = &mgr->sessions[free_slot];
  memset(session, 0, sizeof(ClientSessionRecord));

  session->in_use = true;
  strncpy(session->session_id, session_id, sizeof(session->session_id) - 1);
  session->state = CLIENT_SESSION_AUTHENTICATING;
  session->created_at = time(NULL);
  session->last_activity = time(NULL);

  mgr->num_active++;

  /* 设置为当前会话 */
  strncpy(mgr->current_session_id, session_id,
          sizeof(mgr->current_session_id) - 1);

  pthread_mutex_unlock(&g_session_mutex);

  printf("[Session Manager] ✓ Session created: %s\n", session_id);
  return session;
}

/*===========================================================================
 * 会话查找
 *===========================================================================*/

ClientSessionRecord *session_manager_find(SessionManager *mgr,
                                          const char *session_id) {
  if (!mgr || !session_id)
    return NULL;

  pthread_mutex_lock(&g_session_mutex);

  for (int i = 0; i < MAX_CLIENT_SESSIONS; i++) {
    if (mgr->sessions[i].in_use &&
        strcmp(mgr->sessions[i].session_id, session_id) == 0) {
      pthread_mutex_unlock(&g_session_mutex);
      return &mgr->sessions[i];
    }
  }

  pthread_mutex_unlock(&g_session_mutex);
  return NULL;
}

/*===========================================================================
 * 状态更新
 *===========================================================================*/

int session_manager_update_state(SessionManager *mgr, const char *session_id,
                                 ClientSessionState new_state) {
  ClientSessionRecord *session = session_manager_find(mgr, session_id);
  if (!session)
    return -1;

  pthread_mutex_lock(&g_session_mutex);
  session->state = new_state;
  session->last_activity = time(NULL);
  pthread_mutex_unlock(&g_session_mutex);

  return 0;
}

int session_manager_authenticated(SessionManager *mgr, const char *session_id) {
  return session_manager_update_state(mgr, session_id,
                                      CLIENT_SESSION_AUTHENTICATED);
}

int session_manager_link_established(SessionManager *mgr,
                                     const char *session_id,
                                     uint32_t granted_bw_kbps,
                                     uint32_t granted_ret_bw_kbps,
                                     uint8_t bearer_id, const char *link_id) {
  ClientSessionRecord *session = session_manager_find(mgr, session_id);
  if (!session)
    return -1;

  pthread_mutex_lock(&g_session_mutex);

  session->state = CLIENT_SESSION_ACTIVE;
  session->granted_bw_kbps = granted_bw_kbps;
  session->granted_ret_bw_kbps = granted_ret_bw_kbps;
  session->bearer_id = bearer_id;
  if (link_id) {
    strncpy(session->assigned_link, link_id,
            sizeof(session->assigned_link) - 1);
  }
  session->last_activity = time(NULL);

  pthread_mutex_unlock(&g_session_mutex);

  printf(
      "[Session Manager] ✓ Link established: %s (Bearer %u, %s, %u/%u kbps)\n",
      session_id, bearer_id, link_id ? link_id : "unknown", granted_bw_kbps,
      granted_ret_bw_kbps);

  return 0;
}

/*===========================================================================
 * 会话删除
 *===========================================================================*/

int session_manager_delete(SessionManager *mgr, const char *session_id) {
  if (!mgr || !session_id)
    return -1;

  pthread_mutex_lock(&g_session_mutex);

  for (int i = 0; i < MAX_CLIENT_SESSIONS; i++) {
    if (mgr->sessions[i].in_use &&
        strcmp(mgr->sessions[i].session_id, session_id) == 0) {

      printf("[Session Manager] Session deleted: %s\n", session_id);

      memset(&mgr->sessions[i], 0, sizeof(ClientSessionRecord));
      mgr->num_active--;

      /* 如果删除的是当前会话,清空当前会话 ID */
      if (strcmp(mgr->current_session_id, session_id) == 0) {
        mgr->current_session_id[0] = '\0';
      }

      pthread_mutex_unlock(&g_session_mutex);
      return 0;
    }
  }

  pthread_mutex_unlock(&g_session_mutex);
  return -1;
}

/*===========================================================================
 * 会话列表
 *===========================================================================*/

void session_manager_list_active(SessionManager *mgr) {
  if (!mgr)
    return;

  pthread_mutex_lock(&g_session_mutex);

  printf("\n========================================\n");
  printf("  Active Sessions (%d/%d)\n", mgr->num_active, MAX_CLIENT_SESSIONS);
  printf("========================================\n");

  if (mgr->num_active == 0) {
    printf("  (No active sessions)\n");
  } else {
    for (int i = 0; i < MAX_CLIENT_SESSIONS; i++) {
      if (mgr->sessions[i].in_use) {
        ClientSessionRecord *s = &mgr->sessions[i];
        const char *state_str = "UNKNOWN";
        switch (s->state) {
        case CLIENT_SESSION_AUTHENTICATING:
          state_str = "AUTHENTICATING";
          break;
        case CLIENT_SESSION_AUTHENTICATED:
          state_str = "AUTHENTICATED";
          break;
        case CLIENT_SESSION_ESTABLISHING:
          state_str = "ESTABLISHING";
          break;
        case CLIENT_SESSION_ACTIVE:
          state_str = "ACTIVE";
          break;
        case CLIENT_SESSION_MODIFYING:
          state_str = "MODIFYING";
          break;
        case CLIENT_SESSION_TERMINATING:
          state_str = "TERMINATING";
          break;
        default:
          break;
        }

        printf("\n[%d] Session-Id: %s\n", i + 1, s->session_id);
        printf("    State: %s\n", state_str);
        if (s->state == CLIENT_SESSION_ACTIVE) {
          printf("    Link: %s (Bearer %u)\n",
                 s->assigned_link[0] ? s->assigned_link : "unknown",
                 s->bearer_id);
          printf("    Bandwidth: %u/%u kbps\n", s->granted_bw_kbps,
                 s->granted_ret_bw_kbps);
        }
        printf("    Created: %ld seconds ago\n",
               (long)(time(NULL) - s->created_at));
      }
    }
  }

  if (mgr->current_session_id[0]) {
    printf("\n  Current Session: %s\n", mgr->current_session_id);
  }

  printf("========================================\n\n");

  pthread_mutex_unlock(&g_session_mutex);
}

/*===========================================================================
 * 当前会话管理
 *===========================================================================*/

int session_manager_set_current(SessionManager *mgr, const char *session_id) {
  if (!mgr || !session_id)
    return -1;

  ClientSessionRecord *session = session_manager_find(mgr, session_id);
  if (!session)
    return -1;

  pthread_mutex_lock(&g_session_mutex);
  strncpy(mgr->current_session_id, session_id,
          sizeof(mgr->current_session_id) - 1);
  pthread_mutex_unlock(&g_session_mutex);

  printf("[Session Manager] Current session set to: %s\n", session_id);
  return 0;
}

const char *session_manager_get_current(SessionManager *mgr) {
  if (!mgr)
    return NULL;

  pthread_mutex_lock(&g_session_mutex);
  const char *sid = mgr->current_session_id[0] ? mgr->current_session_id : NULL;
  pthread_mutex_unlock(&g_session_mutex);

  return sid;
}

int session_manager_count_active(SessionManager *mgr) {
  if (!mgr)
    return 0;

  pthread_mutex_lock(&g_session_mutex);
  int count = mgr->num_active;
  pthread_mutex_unlock(&g_session_mutex);

  return count;
}

/*===========================================================================
 * 清理
 *===========================================================================*/

void session_manager_cleanup(SessionManager *mgr) {
  if (!mgr)
    return;

  pthread_mutex_lock(&g_session_mutex);

  memset(mgr->sessions, 0, sizeof(mgr->sessions));
  mgr->num_active = 0;
  mgr->current_session_id[0] = '\0';

  pthread_mutex_unlock(&g_session_mutex);

  printf("[Session Manager] Cleaned up\n");
}

/*===========================================================================
 * DLM 状态管理实现 (v2.1: MSCR 解析支持)
 *===========================================================================*/

/* 全局 DLM 状态管理器 */
DLMStatusManager g_dlm_status_mgr;

/* DLM 状态管理锁 */
static pthread_mutex_t g_dlm_mutex = PTHREAD_MUTEX_INITIALIZER;

void dlm_status_init(void) {
  pthread_mutex_lock(&g_dlm_mutex);
  memset(&g_dlm_status_mgr, 0, sizeof(DLMStatusManager));
  pthread_mutex_unlock(&g_dlm_mutex);

  printf("[DLM Status] Initialized\n");
}

DLMStatusRecord *dlm_status_find_or_create(const char *dlm_name) {
  if (!dlm_name || dlm_name[0] == '\0')
    return NULL;

  pthread_mutex_lock(&g_dlm_mutex);

  /* 查找现有记录 */
  for (uint32_t i = 0; i < g_dlm_status_mgr.count; i++) {
    if (g_dlm_status_mgr.records[i].in_use &&
        strcmp(g_dlm_status_mgr.records[i].dlm_name, dlm_name) == 0) {
      pthread_mutex_unlock(&g_dlm_mutex);
      return &g_dlm_status_mgr.records[i];
    }
  }

  /* 查找空闲槽位 */
  for (uint32_t i = 0; i < MAX_DLM_COUNT; i++) {
    if (!g_dlm_status_mgr.records[i].in_use) {
      DLMStatusRecord *rec = &g_dlm_status_mgr.records[i];
      memset(rec, 0, sizeof(DLMStatusRecord));
      rec->in_use = true;
      strncpy(rec->dlm_name, dlm_name, sizeof(rec->dlm_name) - 1);
      rec->dlm_available = 2; /* 2 = 未知 */
      rec->last_update = time(NULL);
      g_dlm_status_mgr.count++;

      pthread_mutex_unlock(&g_dlm_mutex);
      printf("[DLM Status] New DLM registered: %s\n", dlm_name);
      return rec;
    }
  }

  pthread_mutex_unlock(&g_dlm_mutex);
  fprintf(stderr, "[DLM Status] WARNING: No free slots for DLM '%s'\n",
          dlm_name);
  return NULL;
}

bool dlm_status_update_available(const char *dlm_name, uint32_t new_available) {
  DLMStatusRecord *rec = dlm_status_find_or_create(dlm_name);
  if (!rec)
    return false;

  pthread_mutex_lock(&g_dlm_mutex);

  uint32_t old_available = rec->dlm_available;
  bool changed = (old_available != new_available &&
                  old_available != 2); /* 初始状态不算变化 */

  rec->dlm_available = new_available;
  rec->last_update = time(NULL);

  pthread_mutex_unlock(&g_dlm_mutex);

  if (changed) {
    const char *old_str = (old_available == 0)   ? "UNAVAILABLE"
                          : (old_available == 1) ? "AVAILABLE"
                                                 : "UNKNOWN";
    const char *new_str = (new_available == 0)   ? "UNAVAILABLE"
                          : (new_available == 1) ? "AVAILABLE"
                                                 : "UNKNOWN";
    printf("[DLM Status] *** STATUS CHANGE: %s: %s -> %s ***\n", dlm_name,
           old_str, new_str);
  }

  return changed;
}

void dlm_status_print_all(void) {
  pthread_mutex_lock(&g_dlm_mutex);

  printf("\n╔══════════════════════════════════════════════════════════════════"
         "╗\n");
  printf(
      "║               DLM Status Table (MSCR Data)                       ║\n");
  printf(
      "╠══════════════════════════════════════════════════════════════════╣\n");

  if (g_dlm_status_mgr.count == 0) {
    printf("║  (No DLM status received yet - waiting for MSCR)                 "
           "║\n");
  } else {
    printf(
        "║  Registered Clients: %-4u    Last MSCR: %ld sec ago           ║\n",
        g_dlm_status_mgr.registered_clients,
        (g_dlm_status_mgr.last_mscr_time > 0)
            ? (long)(time(NULL) - g_dlm_status_mgr.last_mscr_time)
            : -1);
    printf("╠══════════════════════════════════════════════════════════════════"
           "╣\n");

    for (uint32_t i = 0; i < MAX_DLM_COUNT; i++) {
      DLMStatusRecord *r = &g_dlm_status_mgr.records[i];
      if (!r->in_use)
        continue;

      const char *avail_str = (r->dlm_available == 0)   ? "AVAILABLE"
                              : (r->dlm_available == 1) ? "UNAVAILABLE"
                                                        : "UNKNOWN";

      printf("║ DLM: %-20s  Status: %-12s               ║\n", r->dlm_name,
             avail_str);
      printf("║   Max Links: %-3u  Alloc Links: %-3u                           "
             "   ║\n",
             r->dlm_max_links, r->dlm_alloc_links);
      printf(
          "║   Max BW: %8.1f kbps  Alloc BW: %8.1f kbps                  ║\n",
          r->dlm_max_bw_kbps, r->dlm_alloc_bw_kbps);

      if (r->link_count > 0) {
        printf("║   Links (%u):                                                "
               "     ║\n",
               r->link_count);
        for (uint32_t j = 0; j < r->link_count && j < MAX_LINKS_PER_DLM; j++) {
          LinkStatusRecord *lnk = &r->links[j];
          const char *conn_str = (lnk->link_conn_status == 0)   ? "DISCONNECTED"
                                 : (lnk->link_conn_status == 1) ? "CONNECTED"
                                 : (lnk->link_conn_status == 2) ? "FORCED_OFF"
                                                                : "?";
          printf("║     [%u] %-15s %-12s                          ║\n",
                 lnk->link_number,
                 lnk->link_name[0] ? lnk->link_name : "unnamed", conn_str);
        }
      }
      printf("║   Updated: %ld sec ago                                         "
             " ║\n",
             (long)(time(NULL) - r->last_update));
      printf("╠────────────────────────────────────────────────────────────────"
             "──╣\n");
    }
  }

  printf("╚══════════════════════════════════════════════════════════════════╝"
         "\n\n");

  pthread_mutex_unlock(&g_dlm_mutex);
}
