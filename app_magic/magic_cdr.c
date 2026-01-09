/**
 * @file magic_cdr.c
 * @brief MAGIC CDR (Call Data Record) 管理模块实现
 * @description 实现 CDR 的创建、关闭、切分和 JSON 持久化存储
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2025-12-03
 */

#include "magic_cdr.h"

#include <dirent.h>
#include <errno.h>
#include <freeDiameter/extension.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*===========================================================================
 * 内部辅助函数声明
 *===========================================================================*/

static int ensure_directory_exists(const char *path);
static int cdr_to_json(const CDRRecord *cdr, char *buffer, size_t buf_size);
static int json_to_cdr(const char *json, CDRRecord *cdr);
static char *read_file_content(const char *filepath);

/*===========================================================================
 * UUID 生成
 *===========================================================================*/

void cdr_generate_uuid(char *out_uuid, size_t max_len) {
  if (!out_uuid || max_len < 37)
    return;

  /* 简单的基于时间和随机数的 UUID 生成 */
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  unsigned int seed = (unsigned int)(ts.tv_nsec ^ ts.tv_sec ^ getpid());

  snprintf(out_uuid, max_len, "%08x-%04x-%04x-%04x-%012lx",
           (unsigned int)(ts.tv_sec & 0xFFFFFFFF),
           (unsigned int)(rand_r(&seed) & 0xFFFF),
           (unsigned int)((rand_r(&seed) & 0x0FFF) | 0x4000), /* Version 4 */
           (unsigned int)((rand_r(&seed) & 0x3FFF) | 0x8000), /* Variant */
           (unsigned long)(ts.tv_nsec) ^ (unsigned long)rand_r(&seed));
}

/*===========================================================================
 * 状态名称转换
 *===========================================================================*/

const char *cdr_status_name(CDRStatus status) {
  switch (status) {
  case CDR_STATUS_ACTIVE:
    return "ACTIVE";
  case CDR_STATUS_FINISHED:
    return "FINISHED";
  case CDR_STATUS_ARCHIVED:
    return "ARCHIVED";
  case CDR_STATUS_ROLLOVER:
    return "ROLLOVER";
  default:
    return "UNKNOWN";
  }
}

/*===========================================================================
 * 溢出检测
 *===========================================================================*/

bool cdr_detect_overflow(uint64_t current, uint64_t previous) {
  /* 检测计数器回绕: 当前值显著小于上次值 */
  if (current < previous) {
    /* 排除小幅波动 (可能是并发问题) */
    if (previous - current > (UINT64_MAX / 2)) {
      return true; /* 真正的溢出 */
    }
  }

  /* 检测接近最大值 */
  if (current > TRAFFIC_OVERFLOW_THRESHOLD) {
    fd_log_notice("[CDR] Warning: Traffic counter approaching overflow: %lu",
                  (unsigned long)current);
  }

  return false;
}

/*===========================================================================
 * 目录管理
 *===========================================================================*/

static int ensure_directory_exists(const char *path) {
  struct stat st = {0};

  if (stat(path, &st) == -1) {
    /* 递归创建目录 */
    char tmp[MAX_CDR_PATH_LEN];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
      tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
      if (*p == '/') {
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
          fd_log_error("[CDR] Failed to create directory %s: %s", tmp,
                       strerror(errno));
          return -1;
        }
        *p = '/';
      }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
      fd_log_error("[CDR] Failed to create directory %s: %s", tmp,
                   strerror(errno));
      return -1;
    }
  }

  return 0;
}

/*===========================================================================
 * 初始化和清理
 *===========================================================================*/

int cdr_manager_init(CDRManager *mgr, const char *base_dir,
                     uint32_t retention_sec) {
  if (!mgr)
    return -1;

  memset(mgr, 0, sizeof(CDRManager));

  /* 设置存储目录 */
  if (base_dir && base_dir[0]) {
    snprintf(mgr->base_dir, sizeof(mgr->base_dir), "%s", base_dir);
  } else {
    snprintf(mgr->base_dir, sizeof(mgr->base_dir), "%s", CDR_BASE_DIR);
  }

  snprintf(mgr->active_dir, sizeof(mgr->active_dir), "%s/active",
           mgr->base_dir);
  snprintf(mgr->archive_dir, sizeof(mgr->archive_dir), "%s/archive",
           mgr->base_dir);

  /* 设置归档保留时间 */
  mgr->retention_sec =
      retention_sec > 0 ? retention_sec : CDR_ARCHIVE_RETENTION_SEC;

  /* 创建目录结构 */
  if (ensure_directory_exists(mgr->base_dir) != 0 ||
      ensure_directory_exists(mgr->active_dir) != 0 ||
      ensure_directory_exists(mgr->archive_dir) != 0) {
    fd_log_error("[CDR] Failed to create CDR directories");
    return -1;
  }

  /* 初始化管理器锁 */
  if (pthread_mutex_init(&mgr->manager_lock, NULL) != 0) {
    fd_log_error("[CDR] Failed to initialize manager mutex");
    return -1;
  }

  /* 生成初始 CDR ID (基于时间戳) */
  mgr->next_cdr_id = (uint32_t)(time(NULL) & 0xFFFFFFFF);

  /* 加载已有的活跃 CDR */
  int loaded = cdr_load_all_active(mgr);
  if (loaded > 0) {
    fd_log_notice("[CDR] Loaded %d active CDR records", loaded);
  }

  mgr->is_initialized = true;
  mgr->last_cleanup_time = time(NULL);

  fd_log_notice("[CDR] CDR Manager initialized:");
  fd_log_notice("[CDR]   Base dir: %s", mgr->base_dir);
  fd_log_notice("[CDR]   Retention: %u seconds (%u hours)", mgr->retention_sec,
                mgr->retention_sec / 3600);

  return 0;
}

void cdr_manager_cleanup(CDRManager *mgr) {
  if (!mgr || !mgr->is_initialized)
    return;

  pthread_mutex_lock(&mgr->manager_lock);

  /* 保存所有活跃 CDR */
  cdr_save_all_active(mgr);

  /* 销毁所有 CDR 锁 */
  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && cdr->lock_initialized) {
      pthread_mutex_destroy(&cdr->lock);
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  pthread_mutex_destroy(&mgr->manager_lock);

  mgr->is_initialized = false;

  fd_log_notice("[CDR] CDR Manager cleaned up. Stats: created=%lu, "
                "archived=%lu, deleted=%lu",
                (unsigned long)mgr->total_cdrs_created,
                (unsigned long)mgr->total_cdrs_archived,
                (unsigned long)mgr->total_cdrs_deleted);
}

/*===========================================================================
 * CDR 生命周期管理
 *===========================================================================*/

CDRRecord *cdr_create(CDRManager *mgr, const char *session_id,
                      const char *client_id, const char *dlm_name) {
  if (!mgr || !mgr->is_initialized || !session_id)
    return NULL;

  pthread_mutex_lock(&mgr->manager_lock);

  CDRRecord *cdr = NULL;

  /* 查找空闲槽位 */
  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    if (!mgr->records[i].in_use) {
      cdr = &mgr->records[i];
      break;
    }
  }

  if (!cdr) {
    fd_log_error("[CDR] No free CDR slot available");
    pthread_mutex_unlock(&mgr->manager_lock);
    return NULL;
  }

  /* 初始化 CDR 记录 */
  memset(cdr, 0, sizeof(CDRRecord));

  cdr->cdr_id = mgr->next_cdr_id++;
  cdr_generate_uuid(cdr->cdr_uuid, sizeof(cdr->cdr_uuid));

  strncpy(cdr->session_id, session_id, sizeof(cdr->session_id) - 1);
  if (client_id) {
    strncpy(cdr->client_id, client_id, sizeof(cdr->client_id) - 1);
  }
  if (dlm_name) {
    strncpy(cdr->dlm_name, dlm_name, sizeof(cdr->dlm_name) - 1);
  }

  cdr->status = CDR_STATUS_ACTIVE;
  cdr->start_time = time(NULL);
  cdr->in_use = true;

  /* 初始化 CDR 级别锁 */
  if (pthread_mutex_init(&cdr->lock, NULL) == 0) {
    cdr->lock_initialized = true;
  }

  mgr->record_count++;
  mgr->total_cdrs_created++;

  pthread_mutex_unlock(&mgr->manager_lock);

  /* 保存到文件 */
  cdr_save_to_file(mgr, cdr);

  fd_log_notice("[CDR] Created CDR: id=%u, uuid=%s, session=%s, client=%s",
                cdr->cdr_id, cdr->cdr_uuid, cdr->session_id, cdr->client_id);

  return cdr;
}

int cdr_close(CDRManager *mgr, CDRRecord *cdr, uint64_t final_bytes_in,
              uint64_t final_bytes_out) {
  if (!mgr || !cdr || !cdr->in_use)
    return -1;

  cdr_lock(cdr);

  /* 更新最终流量 */
  cdr->bytes_in = final_bytes_in;
  cdr->bytes_out = final_bytes_out;
  cdr->stop_time = time(NULL);
  cdr->status = CDR_STATUS_FINISHED;

  cdr_unlock(cdr);

  /* 保存并归档 */
  cdr_save_to_file(mgr, cdr);
  cdr_archive(mgr, cdr);

  fd_log_notice(
      "[CDR] Closed CDR: id=%u, bytes_in=%lu, bytes_out=%lu, duration=%ld sec",
      cdr->cdr_id, (unsigned long)final_bytes_in,
      (unsigned long)final_bytes_out, (long)(cdr->stop_time - cdr->start_time));

  return 0;
}

int cdr_rollover(CDRManager *mgr, const char *session_id,
                 uint64_t current_bytes_in, uint64_t current_bytes_out,
                 CDRRolloverResult *result) {
  if (!mgr || !session_id || !result)
    return -1;

  memset(result, 0, sizeof(CDRRolloverResult));

  pthread_mutex_lock(&mgr->manager_lock);

  /* 1. 查找当前活跃的 CDR */
  CDRRecord *old_cdr = NULL;
  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && cdr->status == CDR_STATUS_ACTIVE &&
        strcmp(cdr->session_id, session_id) == 0) {
      old_cdr = cdr;
      break;
    }
  }

  if (!old_cdr) {
    result->error_code = -1;
    snprintf(result->error_message, sizeof(result->error_message),
             "No active CDR found for session: %s", session_id);
    pthread_mutex_unlock(&mgr->manager_lock);
    return -1;
  }

  /* 锁定旧 CDR */
  cdr_lock(old_cdr);

  /* 2. 计算旧 CDR 的最终流量 (考虑溢出) */
  uint64_t actual_bytes_in = current_bytes_in;
  uint64_t actual_bytes_out = current_bytes_out;

  /* 检测溢出 */
  if (cdr_detect_overflow(current_bytes_in, old_cdr->last_bytes_in)) {
    old_cdr->overflow_count_in++;
    fd_log_notice("[CDR] Overflow detected for bytes_in, count=%u",
                  old_cdr->overflow_count_in);
    /* 溢出时，使用最大值计算本次增量 */
    actual_bytes_in = UINT64_MAX - old_cdr->last_bytes_in + current_bytes_in;
  }

  if (cdr_detect_overflow(current_bytes_out, old_cdr->last_bytes_out)) {
    old_cdr->overflow_count_out++;
    fd_log_notice("[CDR] Overflow detected for bytes_out, count=%u",
                  old_cdr->overflow_count_out);
    actual_bytes_out = UINT64_MAX - old_cdr->last_bytes_out + current_bytes_out;
  }

  /* 3. 关闭旧 CDR */
  old_cdr->bytes_in = actual_bytes_in;
  old_cdr->bytes_out = actual_bytes_out;
  old_cdr->stop_time = time(NULL);
  old_cdr->status = CDR_STATUS_ROLLOVER;

  result->old_cdr_id = old_cdr->cdr_id;
  strncpy(result->old_cdr_uuid, old_cdr->cdr_uuid,
          sizeof(result->old_cdr_uuid) - 1);

  /* 计算实际流量 (减去基准偏移) */
  cdr_get_actual_traffic(old_cdr, &result->final_bytes_in,
                         &result->final_bytes_out);

  cdr_unlock(old_cdr);

  /* 4. 创建新 CDR */
  CDRRecord *new_cdr = NULL;
  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    if (!mgr->records[i].in_use) {
      new_cdr = &mgr->records[i];
      break;
    }
  }

  if (!new_cdr) {
    result->error_code = -2;
    snprintf(result->error_message, sizeof(result->error_message),
             "No free CDR slot for new record");
    pthread_mutex_unlock(&mgr->manager_lock);
    /* 回滚: 恢复旧 CDR 状态 */
    cdr_lock(old_cdr);
    old_cdr->status = CDR_STATUS_ACTIVE;
    old_cdr->stop_time = 0;
    cdr_unlock(old_cdr);
    return -1;
  }

  /* 初始化新 CDR */
  memset(new_cdr, 0, sizeof(CDRRecord));

  new_cdr->cdr_id = mgr->next_cdr_id++;
  cdr_generate_uuid(new_cdr->cdr_uuid, sizeof(new_cdr->cdr_uuid));

  strncpy(new_cdr->session_id, old_cdr->session_id,
          sizeof(new_cdr->session_id) - 1);
  strncpy(new_cdr->client_id, old_cdr->client_id,
          sizeof(new_cdr->client_id) - 1);
  strncpy(new_cdr->dlm_name, old_cdr->dlm_name, sizeof(new_cdr->dlm_name) - 1);
  new_cdr->bearer_id = old_cdr->bearer_id;

  new_cdr->status = CDR_STATUS_ACTIVE;
  new_cdr->start_time = time(NULL); /* 应与旧 CDR 的 stop_time 毫秒级连续 */
  new_cdr->in_use = true;

  /* 关键: 记录基准偏移量 = 当前累计流量 */
  new_cdr->base_offset_in = current_bytes_in;
  new_cdr->base_offset_out = current_bytes_out;
  new_cdr->last_bytes_in = current_bytes_in;
  new_cdr->last_bytes_out = current_bytes_out;

  /* 初始化锁 */
  if (pthread_mutex_init(&new_cdr->lock, NULL) == 0) {
    new_cdr->lock_initialized = true;
  }

  mgr->record_count++;
  mgr->total_cdrs_created++;

  result->new_cdr_id = new_cdr->cdr_id;
  strncpy(result->new_cdr_uuid, new_cdr->cdr_uuid,
          sizeof(result->new_cdr_uuid) - 1);
  result->success = true;

  pthread_mutex_unlock(&mgr->manager_lock);

  /* 5. 保存文件 */
  cdr_save_to_file(mgr, old_cdr);
  cdr_save_to_file(mgr, new_cdr);

  /* 6. 归档旧 CDR */
  cdr_archive(mgr, old_cdr);

  fd_log_notice("[CDR] Rollover complete: old_id=%u -> new_id=%u, session=%s",
                result->old_cdr_id, result->new_cdr_id, session_id);
  fd_log_notice("[CDR]   Old CDR traffic: in=%lu, out=%lu",
                (unsigned long)result->final_bytes_in,
                (unsigned long)result->final_bytes_out);
  fd_log_notice("[CDR]   New CDR base_offset: in=%lu, out=%lu",
                (unsigned long)new_cdr->base_offset_in,
                (unsigned long)new_cdr->base_offset_out);

  return 0;
}

/*===========================================================================
 * 查找和查询
 *===========================================================================*/

CDRRecord *cdr_find_by_session(CDRManager *mgr, const char *session_id) {
  if (!mgr || !session_id)
    return NULL;

  pthread_mutex_lock(&mgr->manager_lock);

  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && cdr->status == CDR_STATUS_ACTIVE &&
        strcmp(cdr->session_id, session_id) == 0) {
      pthread_mutex_unlock(&mgr->manager_lock);
      return cdr;
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  return NULL;
}

CDRRecord *cdr_find_by_id(CDRManager *mgr, uint32_t cdr_id) {
  if (!mgr)
    return NULL;

  pthread_mutex_lock(&mgr->manager_lock);

  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && cdr->cdr_id == cdr_id) {
      pthread_mutex_unlock(&mgr->manager_lock);
      return cdr;
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  return NULL;
}

int cdr_find_by_client(CDRManager *mgr, const char *client_id,
                       CDRRecord **out_cdrs, int max_count) {
  if (!mgr || !client_id || !out_cdrs || max_count <= 0)
    return 0;

  int count = 0;

  pthread_mutex_lock(&mgr->manager_lock);

  for (int i = 0; i < MAX_CDR_RECORDS && count < max_count; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && strcmp(cdr->client_id, client_id) == 0) {
      out_cdrs[count++] = cdr;
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  return count;
}

/*===========================================================================
 * 流量更新
 *===========================================================================*/

int cdr_update_traffic(CDRRecord *cdr, uint64_t bytes_in, uint64_t bytes_out,
                       uint64_t packets_in, uint64_t packets_out) {
  if (!cdr || !cdr->in_use)
    return -1;

  cdr_lock(cdr);

  int overflow_detected = 0;

  /* 检测入站溢出 */
  if (cdr_detect_overflow(bytes_in, cdr->last_bytes_in)) {
    cdr->overflow_count_in++;
    overflow_detected = 1;
    fd_log_notice("[CDR %u] Overflow detected: bytes_in wrapped", cdr->cdr_id);
  }

  /* 检测出站溢出 */
  if (cdr_detect_overflow(bytes_out, cdr->last_bytes_out)) {
    cdr->overflow_count_out++;
    overflow_detected = 1;
    fd_log_notice("[CDR %u] Overflow detected: bytes_out wrapped", cdr->cdr_id);
  }

  /* 更新统计 */
  cdr->bytes_in = bytes_in;
  cdr->bytes_out = bytes_out;
  cdr->packets_in = packets_in;
  cdr->packets_out = packets_out;
  cdr->last_bytes_in = bytes_in;
  cdr->last_bytes_out = bytes_out;

  cdr_unlock(cdr);

  return overflow_detected;
}

void cdr_get_actual_traffic(const CDRRecord *cdr, uint64_t *out_bytes_in,
                            uint64_t *out_bytes_out) {
  if (!cdr) {
    if (out_bytes_in)
      *out_bytes_in = 0;
    if (out_bytes_out)
      *out_bytes_out = 0;
    return;
  }

  /* 实际流量 = 累计流量 - 基准偏移 (考虑溢出) */
  if (out_bytes_in) {
    if (cdr->bytes_in >= cdr->base_offset_in) {
      *out_bytes_in = cdr->bytes_in - cdr->base_offset_in;
    } else {
      /* 溢出情况: 累计值比基准小，说明发生了回绕 */
      *out_bytes_in = (UINT64_MAX - cdr->base_offset_in) + cdr->bytes_in + 1;
    }

    /* 加上溢出次数对应的增量 */
    if (cdr->overflow_count_in > 0) {
      *out_bytes_in += (uint64_t)cdr->overflow_count_in * UINT64_MAX;
    }
  }

  if (out_bytes_out) {
    if (cdr->bytes_out >= cdr->base_offset_out) {
      *out_bytes_out = cdr->bytes_out - cdr->base_offset_out;
    } else {
      *out_bytes_out = (UINT64_MAX - cdr->base_offset_out) + cdr->bytes_out + 1;
    }

    if (cdr->overflow_count_out > 0) {
      *out_bytes_out += (uint64_t)cdr->overflow_count_out * UINT64_MAX;
    }
  }
}

/*===========================================================================
 * JSON 序列化
 *===========================================================================*/

static int cdr_to_json(const CDRRecord *cdr, char *buffer, size_t buf_size) {
  if (!cdr || !buffer || buf_size < 512)
    return -1;

  int written = snprintf(
      buffer, buf_size,
      "{\n"
      "  \"cdr_id\": %u,\n"
      "  \"cdr_uuid\": \"%s\",\n"
      "  \"session_id\": \"%s\",\n"
      "  \"client_id\": \"%s\",\n"
      "  \"dlm_name\": \"%s\",\n"
      "  \"bearer_id\": %u,\n"
      "  \"status\": \"%s\",\n"
      "  \"status_code\": %d,\n"
      "  \"start_time\": %ld,\n"
      "  \"stop_time\": %ld,\n"
      "  \"archive_time\": %ld,\n"
      "  \"traffic\": {\n"
      "    \"bytes_in\": %lu,\n"
      "    \"bytes_out\": %lu,\n"
      "    \"packets_in\": %lu,\n"
      "    \"packets_out\": %lu,\n"
      "    \"base_offset_in\": %lu,\n"
      "    \"base_offset_out\": %lu\n"
      "  },\n"
      "  \"overflow\": {\n"
      "    \"count_in\": %u,\n"
      "    \"count_out\": %u,\n"
      "    \"last_bytes_in\": %lu,\n"
      "    \"last_bytes_out\": %lu\n"
      "  }\n"
      "}\n",
      cdr->cdr_id, cdr->cdr_uuid, cdr->session_id, cdr->client_id,
      cdr->dlm_name, cdr->bearer_id, cdr_status_name(cdr->status),
      (int)cdr->status, (long)cdr->start_time, (long)cdr->stop_time,
      (long)cdr->archive_time, (unsigned long)cdr->bytes_in,
      (unsigned long)cdr->bytes_out, (unsigned long)cdr->packets_in,
      (unsigned long)cdr->packets_out, (unsigned long)cdr->base_offset_in,
      (unsigned long)cdr->base_offset_out, cdr->overflow_count_in,
      cdr->overflow_count_out, (unsigned long)cdr->last_bytes_in,
      (unsigned long)cdr->last_bytes_out);

  return (written > 0 && (size_t)written < buf_size) ? 0 : -1;
}

/* 简单的 JSON 解析辅助函数 */
static char *json_find_string(const char *json, const char *key) {
  char search[128];
  snprintf(search, sizeof(search), "\"%s\":", key);

  char *pos = strstr(json, search);
  if (!pos)
    return NULL;

  pos += strlen(search);
  while (*pos == ' ' || *pos == '\t')
    pos++;

  if (*pos == '"') {
    pos++;
    static char value[256];
    char *end = strchr(pos, '"');
    if (end && (size_t)(end - pos) < sizeof(value)) {
      strncpy(value, pos, end - pos);
      value[end - pos] = '\0';
      return value;
    }
  }

  return NULL;
}

static long json_find_long(const char *json, const char *key) {
  char search[128];
  snprintf(search, sizeof(search), "\"%s\":", key);

  char *pos = strstr(json, search);
  if (!pos)
    return 0;

  pos += strlen(search);
  while (*pos == ' ' || *pos == '\t')
    pos++;

  return atol(pos);
}

static unsigned long json_find_ulong(const char *json, const char *key) {
  char search[128];
  snprintf(search, sizeof(search), "\"%s\":", key);

  char *pos = strstr(json, search);
  if (!pos)
    return 0;

  pos += strlen(search);
  while (*pos == ' ' || *pos == '\t')
    pos++;

  return strtoul(pos, NULL, 10);
}

static int json_to_cdr(const char *json, CDRRecord *cdr) {
  if (!json || !cdr)
    return -1;

  memset(cdr, 0, sizeof(CDRRecord));

  /* 解析基本字段 */
  cdr->cdr_id = (uint32_t)json_find_ulong(json, "cdr_id");

  char *str = json_find_string(json, "cdr_uuid");
  if (str)
    strncpy(cdr->cdr_uuid, str, sizeof(cdr->cdr_uuid) - 1);

  str = json_find_string(json, "session_id");
  if (str)
    strncpy(cdr->session_id, str, sizeof(cdr->session_id) - 1);

  str = json_find_string(json, "client_id");
  if (str)
    strncpy(cdr->client_id, str, sizeof(cdr->client_id) - 1);

  str = json_find_string(json, "dlm_name");
  if (str)
    strncpy(cdr->dlm_name, str, sizeof(cdr->dlm_name) - 1);

  cdr->bearer_id = (uint8_t)json_find_ulong(json, "bearer_id");
  cdr->status = (CDRStatus)json_find_long(json, "status_code");

  cdr->start_time = (time_t)json_find_long(json, "start_time");
  cdr->stop_time = (time_t)json_find_long(json, "stop_time");
  cdr->archive_time = (time_t)json_find_long(json, "archive_time");

  /* 流量统计 */
  cdr->bytes_in = json_find_ulong(json, "bytes_in");
  cdr->bytes_out = json_find_ulong(json, "bytes_out");
  cdr->packets_in = json_find_ulong(json, "packets_in");
  cdr->packets_out = json_find_ulong(json, "packets_out");
  cdr->base_offset_in = json_find_ulong(json, "base_offset_in");
  cdr->base_offset_out = json_find_ulong(json, "base_offset_out");

  /* 溢出信息 */
  cdr->overflow_count_in = (uint32_t)json_find_ulong(json, "count_in");
  cdr->overflow_count_out = (uint32_t)json_find_ulong(json, "count_out");
  cdr->last_bytes_in = json_find_ulong(json, "last_bytes_in");
  cdr->last_bytes_out = json_find_ulong(json, "last_bytes_out");

  cdr->in_use = (cdr->status == CDR_STATUS_ACTIVE);

  return 0;
}

/*===========================================================================
 * 文件存储
 *===========================================================================*/

static char *read_file_content(const char *filepath) {
  FILE *fp = fopen(filepath, "r");
  if (!fp)
    return NULL;

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (size <= 0 || size > 1024 * 1024) { /* 最大 1MB */
    fclose(fp);
    return NULL;
  }

  char *content = malloc(size + 1);
  if (!content) {
    fclose(fp);
    return NULL;
  }

  size_t read = fread(content, 1, size, fp);
  fclose(fp);

  if ((long)read != size) {
    free(content);
    return NULL;
  }

  content[size] = '\0';
  return content;
}

/**
 * @brief 将 CDR 保存到文件 (JSON 格式)。
 * @details 序列化 CDR 为 JSON 字符串并写入文件。根据 CDR 状态选择保存到 active
 * 或 archive 目录。
 *
 * @param mgr CDR 管理器指针。
 * @param cdr CDR 记录指针。
 * @return int 成功返回 0，失败返回 -1。
 */
int cdr_save_to_file(CDRManager *mgr, const CDRRecord *cdr) {
  if (!mgr || !cdr)
    return -1;

  char filepath[MAX_CDR_PATH_LEN];
  const char *dir =
      (cdr->status == CDR_STATUS_ACTIVE) ? mgr->active_dir : mgr->archive_dir;

  snprintf(filepath, sizeof(filepath), "%s/cdr_%u_%s.json", dir, cdr->cdr_id,
           cdr->cdr_uuid);

  char json[4096];
  if (cdr_to_json(cdr, json, sizeof(json)) != 0) {
    fd_log_error("[CDR] Failed to serialize CDR %u to JSON", cdr->cdr_id);
    return -1;
  }

  FILE *fp = fopen(filepath, "w");
  if (!fp) {
    fd_log_error("[CDR] Failed to open file for writing: %s", filepath);
    return -1;
  }

  fprintf(fp, "%s", json);
  fclose(fp);

  fd_log_debug("[CDR] Saved CDR %u to %s", cdr->cdr_id, filepath);
  return 0;
}

/**
 * @brief 从文件加载 CDR。
 * @details 读取 JSON 文件，解析并填充到新的 CDR 记录中。
 *          如果状态为 ACTIVE，则初始化锁并加入活跃列表。
 *          如果状态已完成，则忽略 (本函数主要用于恢复活跃会话)。
 *
 * @param mgr CDR 管理器指针。
 * @param filepath 文件路径。
 * @return CDRRecord* 加载成功的 CDR 指针，失败返回 NULL。
 */
CDRRecord *cdr_load_from_file(CDRManager *mgr, const char *filepath) {
  if (!mgr || !filepath)
    return NULL;

  char *content = read_file_content(filepath);
  if (!content) {
    fd_log_error("[CDR] Failed to read file: %s", filepath);
    return NULL;
  }

  pthread_mutex_lock(&mgr->manager_lock);

  /* 查找空闲槽位 */
  CDRRecord *cdr = NULL;
  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    if (!mgr->records[i].in_use) {
      cdr = &mgr->records[i];
      break;
    }
  }

  if (!cdr) {
    pthread_mutex_unlock(&mgr->manager_lock);
    free(content);
    return NULL;
  }

  if (json_to_cdr(content, cdr) != 0) {
    pthread_mutex_unlock(&mgr->manager_lock);
    free(content);
    return NULL;
  }

  free(content);

  /* 只加载活跃的 CDR */
  if (cdr->status == CDR_STATUS_ACTIVE) {
    cdr->in_use = true;

    /* 初始化锁 */
    if (pthread_mutex_init(&cdr->lock, NULL) == 0) {
      cdr->lock_initialized = true;
    }

    mgr->record_count++;

    /* 更新 next_cdr_id */
    if (cdr->cdr_id >= mgr->next_cdr_id) {
      mgr->next_cdr_id = cdr->cdr_id + 1;
    }
  } else {
    memset(cdr, 0, sizeof(CDRRecord));
    cdr = NULL;
  }

  pthread_mutex_unlock(&mgr->manager_lock);

  return cdr;
}

/**
 * @brief 加载所有活跃 CDR。
 * @details 遍历 active 目录，加载所有 .json 文件。用于系统重启后恢复状态。
 *
 * @param mgr CDR 管理器指针。
 * @return int 成功加载的 CDR 数量，失败返回 -1。
 */
int cdr_load_all_active(CDRManager *mgr) {
  if (!mgr)
    return -1;

  DIR *dir = opendir(mgr->active_dir);
  if (!dir) {
    fd_log_notice("[CDR] No active CDR directory to load from");
    return 0;
  }

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "cdr_", 4) != 0)
      continue;
    if (strstr(entry->d_name, ".json") == NULL)
      continue;

    char filepath[MAX_CDR_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", mgr->active_dir,
             entry->d_name);

    if (cdr_load_from_file(mgr, filepath) != NULL) {
      count++;
    }
  }

  closedir(dir);
  return count;
}

/**
 * @brief 保存所有活跃 CDR。
 * @details 遍历内存中所有 ACTIVE 状态的 CDR 并保存到磁盘。
 *          通常在系统关闭或 periodic maintenance 时调用。
 *
 * @param mgr CDR 管理器指针。
 * @return int 成功保存的 CDR 数量，失败返回 -1。
 */
int cdr_save_all_active(CDRManager *mgr) {
  if (!mgr)
    return -1;

  int count = 0;

  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && cdr->status == CDR_STATUS_ACTIVE) {
      if (cdr_save_to_file(mgr, cdr) == 0) {
        count++;
      }
    }
  }

  return count;
}

/*===========================================================================
 * 归档和清理
 *===========================================================================*/

/**
 * @brief 归档已完成的 CDR。
 * @details
 *          1. 验证 CDR 不再处于 ACTIVE 状态。
 *          2. 将其在 active 目录下的旧文件删除。
 *          3. 设置状态为 ARCHIVED 并更新归档时间。
 *          4. 保存到 archive 目录。
 *          5. 释放内存资源 (互斥锁)。
 *
 * @param mgr CDR 管理器指针。
 * @param cdr CDR 记录指针。
 * @return int 成功返回 0，失败返回 -1。
 */
int cdr_archive(CDRManager *mgr, CDRRecord *cdr) {
  if (!mgr || !cdr)
    return -1;

  if (cdr->status == CDR_STATUS_ACTIVE) {
    return -1; /* 不能归档活跃的 CDR */
  }

  cdr_lock(cdr);

  /* 从 active 目录删除旧文件 */
  char old_path[MAX_CDR_PATH_LEN];
  snprintf(old_path, sizeof(old_path), "%s/cdr_%u_%s.json", mgr->active_dir,
           cdr->cdr_id, cdr->cdr_uuid);
  unlink(old_path);

  /* 更新状态 */
  cdr->status = CDR_STATUS_ARCHIVED;
  cdr->archive_time = time(NULL);

  cdr_unlock(cdr);

  /* 保存到 archive 目录 */
  cdr_save_to_file(mgr, cdr);

  /* 释放内存槽位 */
  pthread_mutex_lock(&mgr->manager_lock);

  if (cdr->lock_initialized) {
    pthread_mutex_destroy(&cdr->lock);
  }

  memset(cdr, 0, sizeof(CDRRecord));
  mgr->record_count--;
  mgr->total_cdrs_archived++;

  pthread_mutex_unlock(&mgr->manager_lock);

  return 0;
}

/**
 * @brief 清理过期的归档 CDR。
 * @details 遍历 archive 目录，删除修改时间早于 retention_sec 的 .json 文件。
 *
 * @param mgr CDR 管理器指针。
 * @return int 删除的文件数量，失败返回 -1。
 */
int cdr_cleanup_expired(CDRManager *mgr) {
  if (!mgr)
    return -1;

  DIR *dir = opendir(mgr->archive_dir);
  if (!dir)
    return 0;

  time_t now = time(NULL);
  time_t cutoff = now - mgr->retention_sec;
  int deleted = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "cdr_", 4) != 0)
      continue;
    if (strstr(entry->d_name, ".json") == NULL)
      continue;

    char filepath[MAX_CDR_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", mgr->archive_dir,
             entry->d_name);

    struct stat st;
    if (stat(filepath, &st) == 0) {
      if (st.st_mtime < cutoff) {
        if (unlink(filepath) == 0) {
          deleted++;
          fd_log_debug("[CDR] Deleted expired archive: %s", entry->d_name);
        }
      }
    }
  }

  closedir(dir);

  if (deleted > 0) {
    mgr->total_cdrs_deleted += deleted;
    fd_log_notice("[CDR] Cleaned up %d expired CDR archives", deleted);
  }

  return deleted;
}

/**
 * @brief 执行周期性维护。
 * @details
 *          1. 检查是否达到清理间隔 (CDR_CLEANUP_INTERVAL_SEC)。
 *          2. 刷新所有活跃 CDR 到磁盘 (防崩溃丢失数据)。
 *          3. 清理过期的归档文件。
 *
 * @param mgr CDR 管理器指针。
 */
void cdr_periodic_maintenance(CDRManager *mgr) {
  if (!mgr || !mgr->is_initialized)
    return;

  time_t now = time(NULL);

  /* 检查是否需要执行清理 */
  if (now - mgr->last_cleanup_time < CDR_CLEANUP_INTERVAL_SEC) {
    return;
  }

  mgr->last_cleanup_time = now;

  fd_log_debug("[CDR] Running periodic maintenance...");

  /* 保存所有活跃 CDR */
  cdr_save_all_active(mgr);

  /* 清理过期归档 */
  cdr_cleanup_expired(mgr);
}

/*===========================================================================
 * 锁操作
 *===========================================================================*/

/**
 * @brief 锁定 CDR (获取互斥锁)。
 * @param cdr CDR 记录指针。
 * @return int 成功返回 0。
 */
int cdr_lock(CDRRecord *cdr) {
  if (!cdr || !cdr->lock_initialized)
    return -1;
  return pthread_mutex_lock(&cdr->lock);
}

/**
 * @brief 解锁 CDR (释放互斥锁)。
 * @param cdr CDR 记录指针。
 * @return int 成功返回 0。
 */
int cdr_unlock(CDRRecord *cdr) {
  if (!cdr || !cdr->lock_initialized)
    return -1;
  return pthread_mutex_unlock(&cdr->lock);
}

/**
 * @brief 锁定会话的所有 CDR。
 * @details 某些操作 (如 rollover) 可能涉及同一会话的多个 CDR，需批量加锁。
 *
 * @param mgr CDR 管理器指针。
 * @param session_id 会话 ID。
 * @return int 成功返回 0。
 */
int cdr_lock_session(CDRManager *mgr, const char *session_id) {
  if (!mgr || !session_id)
    return -1;

  pthread_mutex_lock(&mgr->manager_lock);

  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && strcmp(cdr->session_id, session_id) == 0) {
      cdr_lock(cdr);
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  return 0;
}

/**
 * @brief 解锁会话的所有 CDR。
 * @param mgr CDR 管理器指针。
 * @param session_id 会话 ID。
 * @return int 成功返回 0。
 */
int cdr_unlock_session(CDRManager *mgr, const char *session_id) {
  if (!mgr || !session_id)
    return -1;

  pthread_mutex_lock(&mgr->manager_lock);

  for (int i = 0; i < MAX_CDR_RECORDS; i++) {
    CDRRecord *cdr = &mgr->records[i];
    if (cdr->in_use && strcmp(cdr->session_id, session_id) == 0) {
      cdr_unlock(cdr);
    }
  }

  pthread_mutex_unlock(&mgr->manager_lock);
  return 0;
}

/*===========================================================================
 * 调试和打印
 *===========================================================================*/

/**
 * @brief 打印 CDR 信息 (调试用)。
 * @param cdr CDR 记录指针。
 */
void cdr_print_info(const CDRRecord *cdr) {
  if (!cdr)
    return;

  uint64_t actual_in, actual_out;
  cdr_get_actual_traffic(cdr, &actual_in, &actual_out);

  fd_log_notice("[CDR] CDR Info:");
  fd_log_notice("  ID: %u (%s)", cdr->cdr_id, cdr->cdr_uuid);
  fd_log_notice("  Session: %s", cdr->session_id);
  fd_log_notice("  Client: %s", cdr->client_id);
  fd_log_notice("  Status: %s", cdr_status_name(cdr->status));
  fd_log_notice("  DLM: %s, Bearer: %u", cdr->dlm_name, cdr->bearer_id);
  fd_log_notice("  Start: %ld, Stop: %ld", (long)cdr->start_time,
                (long)cdr->stop_time);
  fd_log_notice("  Traffic (actual): in=%lu, out=%lu", (unsigned long)actual_in,
                (unsigned long)actual_out);
  fd_log_notice("  Base offset: in=%lu, out=%lu",
                (unsigned long)cdr->base_offset_in,
                (unsigned long)cdr->base_offset_out);
  fd_log_notice("  Overflow count: in=%u, out=%u", cdr->overflow_count_in,
                cdr->overflow_count_out);
}

/**
 * @brief 打印 CDR 管理器状态 (调试用)。
 * @param mgr CDR 管理器指针。
 */
void cdr_manager_print_status(const CDRManager *mgr) {
  if (!mgr)
    return;

  fd_log_notice("[CDR] CDR Manager Status:");
  fd_log_notice("  Initialized: %s", mgr->is_initialized ? "yes" : "no");
  fd_log_notice("  Base dir: %s", mgr->base_dir);
  fd_log_notice("  Active records: %u / %d", mgr->record_count,
                MAX_CDR_RECORDS);
  fd_log_notice("  Next CDR ID: %u", mgr->next_cdr_id);
  fd_log_notice("  Retention: %u seconds (%u hours)", mgr->retention_sec,
                mgr->retention_sec / 3600);
  fd_log_notice("  Stats: created=%lu, archived=%lu, deleted=%lu",
                (unsigned long)mgr->total_cdrs_created,
                (unsigned long)mgr->total_cdrs_archived,
                (unsigned long)mgr->total_cdrs_deleted);
}
