/**
 * @file log.h
 * @brief 轻量级自定义日志系统定义。
 * @details 提供独立于 freeDiameter
 * 的日志记录宏和控制函数。支持日期格式化、级别过滤及彩色输出支持。
 */

#ifndef MAGIC_CLIENT_LOG_H
#define MAGIC_CLIENT_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

/**
 * @brief 日志级别定义。
 */
enum {
  LOG_ERROR = 0, ///< 严重错误，导致系统部分功能失效。
  LOG_WARN = 1,  ///< 警告信息，系统仍可继续运行。
  LOG_INFO = 2,  ///< 普通运行信息。
  LOG_DEBUG = 3  ///< 详细调试信息，生产环境下通常关闭。
};

/**
 * @brief 获取当前全局日志级别。
 * @return int 当前日志级别。
 */
static inline int magic_log_level(void) {
  extern int __magic_client_log_level;
  return __magic_client_log_level;
}

/**
 * @brief 设置全局日志级别。
 * @param lvl 新的日志级别。
 */
static inline void magic_set_log_level(int lvl) {
  extern int __magic_client_log_level;
  __magic_client_log_level = lvl;
}

/**
 * @brief 内部日志处理函数（支持可变参数列表）。
 * @details 负责格式化时间戳、级别标签并将内容输出到 stderr。
 *
 * @param level 消息级别。
 * @param tag 级别标签 (如 "E", "I")。
 * @param fmt 格式化字符串。
 * @param ap 可变参数列表。
 */
static inline void magic_vlog(int level, const char *tag, const char *fmt,
                              va_list ap) {
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);

  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

  fprintf(stderr, "%s [%s] ", ts, tag);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
}

/**
 * @brief 通用日志记录接口。
 * @details 只有当消息级别小于或等于当前设置的日志级别时才会触发记录。
 *
 * @param level 消息级别。
 * @param tag 标签字符串。
 * @param fmt 格式化字符串。
 */
static inline void magic_log(int level, const char *tag, const char *fmt, ...) {
  if (level > magic_log_level())
    return;
  va_list ap;
  va_start(ap, fmt);
  magic_vlog(level, tag, fmt, ap);
  va_end(ap);
}

/**
 * @name 便捷日志宏
 * @{
 */
#define LOGE(fmt, ...)                                                         \
  magic_log(LOG_ERROR, "E", fmt, ##__VA_ARGS__) ///< 错误日志。
#define LOGW(fmt, ...)                                                         \
  magic_log(LOG_WARN, "W", fmt, ##__VA_ARGS__) ///< 警告日志。
#define LOGI(fmt, ...)                                                         \
  magic_log(LOG_INFO, "I", fmt, ##__VA_ARGS__) ///< 信息日志。
#define LOGD(fmt, ...)                                                         \
  magic_log(LOG_DEBUG, "D", fmt, ##__VA_ARGS__) ///< 调试日志。
/** @} */

/* 兼容旧代码风格：为避免重构大量调用点，保留旧宏别名 */
#ifndef LOG_W
#define LOG_W(fmt, ...) LOGW(fmt, ##__VA_ARGS__) ///< 警告日志别名。
#endif
#ifndef LOG_E
#define LOG_E(fmt, ...) LOGE(fmt, ##__VA_ARGS__) ///< 错误日志别名。
#endif
#ifndef LOG_I
#define LOG_I(fmt, ...) LOGI(fmt, ##__VA_ARGS__) ///< 信息日志别名。
#endif
#ifndef LOG_D
#define LOG_D(fmt, ...) LOGD(fmt, ##__VA_ARGS__) ///< 调试日志别名。
#endif

/**
 * @brief 默认全局日志级别变量。
 * @note 使用弱符号定义，确保可以在链接时被重写。
 */
int __magic_client_log_level __attribute__((weak)) = LOG_INFO;

#endif /* MAGIC_CLIENT_LOG_H */