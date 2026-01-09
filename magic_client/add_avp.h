/**
 * @file add_avp.h
 * @brief 自定义 AVP 构造辅助函数库。
 * @details 提供了一系列 inline 函数和宏，用于简化向 msg 或 parent avp 中添加
 *          子 AVP 的操作。自动处理 AVP 创建、值设置和内存清理（在失败时）。
 *          涵盖基本类型 (String, Integer, Float) 及其 Vendor-Specific 版本。
 */

#ifndef FD_AVP_HELPERS_H
#define FD_AVP_HELPERS_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <string.h>

/* ================================================================== */
/* 内部工具函数 */
/* ================================================================== */

/**
 * @brief 内部清理函数：安全释放游离的 AVP。
 * @details freeDiameter 不提供直接释放 avp 的 API，此函数通过创建一个临时
 *          msg 容器来间接销毁 avp，防止内存泄漏。
 *
 * @param avp 需要释放的 AVP 对象指针。
 */
static inline void __fd_avp_cleanup(struct avp *avp) {
  if (avp) {
    struct msg *trash = NULL;
    /* 创建一个临时空消息，把出错的 avp 塞进去，然后整个扔掉 */
    if (fd_msg_new(NULL, 0, &trash) == 0) {
      fd_msg_avp_add(trash, MSG_BRW_LAST_CHILD, avp);
      fd_msg_free(trash); /* 这会自动释放 avp 本身 */
    }
    /* 如果连 trash 都创建失败，说明内存已耗尽，进程很快会崩溃，直接忽略 */
  }
}

/* ================================================================== */
/* 1. 字符串类 AVP（UTF8String / OctetString）                         */
/* ================================================================== */

/**
 * @brief 设置 AVP 的字符串值。
 * @param avp 目标 AVP 对象。
 * @param str以此 null-terminated 字符串设置值。如果为 NULL 则设为空串。
 * @return int 成功返回 0，失败返回错误码。
 */
static inline int fd_avp_set_str(struct avp *avp, const char *str) {
  union avp_value val;
  if (!str)
    str = ""; /* 防止 NULL 指针 */
  val.os.data = (uint8_t *)str;
  val.os.len = strlen(str);
  return fd_msg_avp_setvalue(avp, &val);
}

static inline int fd_avp_set_strvalue(struct avp *avp, const char *str) {
  return fd_avp_set_str(avp, str); /* 与上面完全相同，仅为兼容旧代码 */
}

/**
 * @brief 创建并添加字符串类型的 AVP。
 *
 * @param parent 父节点（消息或分组 AVP）。
 * @param model AVP 字典模型。
 * @param str 字符串值。
 * @return int 成功返回 0，失败返回 -1。
 */
static inline int fd_msg_avp_add_str(msg_or_avp *parent,
                                     struct dict_object *model,
                                     const char *str) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp),
               return -1); /* 创建标准 AVP (flags=0) */
  CHECK_FCT_DO(fd_avp_set_str(avp, str), {
    __fd_avp_cleanup(avp);
    return -1;
  }); /* 设置值失败 → 释放 */
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  }); /* 添加失败 → 释放 */
  return 0;
}

/* ================================================================== */
/* 2. Unsigned32 / Integer32                                          */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 uint32 值。
 * @param avp 目标 AVP 对象。
 * @param val 32位整数值。
 * @return int 成功返回 0。
 */
static inline int fd_avp_set_uint32(struct avp *avp, uint32_t val) {
  union avp_value v = {.u32 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 创建并添加 uint32 类型的 AVP。
 * @param parent 父节点。
 * @param model AVP 字典模型。
 * @param val 值。
 * @return int 成功返回 0，失败返回 -1。
 */
static inline int fd_msg_avp_add_u32(msg_or_avp *parent,
                                     struct dict_object *model, uint32_t val) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_uint32(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* ================================================================== */
/* 3. Unsigned64 / Integer64                                          */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 uint64 值。
 */
static inline int fd_avp_set_uint64(struct avp *avp, uint64_t val) {
  union avp_value v = {.u64 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 创建并添加 uint64 类型的 AVP。
 */
static inline int fd_msg_avp_add_u64(msg_or_avp *parent,
                                     struct dict_object *model, uint64_t val) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_uint64(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* ================================================================== */
/* 4. Float32 / Float64                                               */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 float32 值。
 */
static inline int fd_avp_set_float32(struct avp *avp, float val) {
  union avp_value v = {.f32 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 设置 AVP 的 float64 值。
 */
static inline int fd_avp_set_float64(struct avp *avp, double val) {
  union avp_value v = {.f64 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/* 兼容旧项目常用名字 */
#define fd_avp_set_float fd_avp_set_float32

/**
 * @brief 创建并添加 float32 类型的 AVP。
 */
static inline int fd_msg_avp_add_float(msg_or_avp *parent,
                                       struct dict_object *model, float val) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_float32(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* ================================================================== */
/* 5. 【新增】厂商特定 AVP 辅助函数 (Vendor-Specific Helpers)           */
/* 注: 这里的 vendor_id 参数在 fd_msg_avp_new 中被忽略，但用于签名一致性 */
/* fd_msg_avp_add 的第一个参数可以是 struct msg* 或 struct avp*        */
/* ================================================================== */

/**
 * @brief 创建并添加 Vendor-Specific 的字符串 AVP。
 * @note flags=0 会让 freeDiameter 根据字典定义自动判断是否设置 'V' 标志。
 */
static inline int fd_msg_avp_add_str_v(msg_or_avp *parent,
                                       struct dict_object *model,
                                       const char *str, uint32_t vendor_id) {
  struct avp *avp = NULL;
  /* flags=0 让 freeDiameter 从字典自动获取正确的 flags（包括 V 位） */
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_str(avp, str), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* 厂商特定 Unsigned32 AVP */
static inline int fd_msg_avp_add_u32_v(msg_or_avp *parent,
                                       struct dict_object *model, uint32_t val,
                                       uint32_t vendor_id) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_uint32(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* 厂商特定 Unsigned64 AVP */
static inline int fd_msg_avp_add_u64_v(msg_or_avp *parent,
                                       struct dict_object *model, uint64_t val,
                                       uint32_t vendor_id) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_uint64(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* 厂商特定 Float32 AVP (为保持宏完整性添加) */
static inline int fd_msg_avp_add_float_v(msg_or_avp *parent,
                                         struct dict_object *model, float val,
                                         uint32_t vendor_id) {
  struct avp *avp = NULL;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  CHECK_FCT_DO(fd_avp_set_float32(avp, val), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/* ================================================================== */
/* 6. 超级好用的宏（一行代码添加一个 AVP，强烈推荐！）                  */
/* ================================================================== */
#define ADD_AVP_STR(msg, model, str)                                           \
  CHECK_FCT_DO(fd_msg_avp_add_str(msg, model, str), return -1)
#define ADD_AVP_U32(msg, model, val)                                           \
  CHECK_FCT_DO(fd_msg_avp_add_u32(msg, model, val), return -1)
#define ADD_AVP_U64(msg, model, val)                                           \
  CHECK_FCT_DO(fd_msg_avp_add_u64(msg, model, val), return -1)
#define ADD_AVP_FLOAT(msg, model, val)                                         \
  CHECK_FCT_DO(fd_msg_avp_add_float(msg, model, val), return -1)

/* 【新增宏】厂商特定 AVP 宏 */
#define ADD_AVP_STR_V(msg, model, str, vid)                                    \
  CHECK_FCT_DO(fd_msg_avp_add_str_v(msg, model, str, vid), return -1)
#define ADD_AVP_U32_V(msg, model, val, vid)                                    \
  CHECK_FCT_DO(fd_msg_avp_add_u32_v(msg, model, val, vid), return -1)
#define ADD_AVP_U64_V(msg, model, val, vid)                                    \
  CHECK_FCT_DO(fd_msg_avp_add_u64_v(msg, model, val, vid), return -1)
#define ADD_AVP_FLOAT_V(msg, model, val, vid)                                  \
  CHECK_FCT_DO(fd_msg_avp_add_float_v(msg, model, val, vid), return -1)

/* ====================== add group ====================== */
#ifndef MAGIC_VENDOR_ID
#define MAGIC_VENDOR_ID 13712
#endif

/* 终极 Grouped AVP 宏 —— 支持任意深度嵌套 + 子宏复用你最爱的 ADD_AVP_XXX_V */
#define ADD_GROUPED(msg_or_parent, model, ...)                                 \
  do {                                                                         \
    struct avp *__grp_avp = NULL;                                              \
    if (fd_msg_avp_new(model, 0, &__grp_avp) != 0) {                           \
      return -1;                                                               \
    }                                                                          \
    struct avp *parent_for_sub = __grp_avp;                                    \
    int __sub_ret = ({ __VA_ARGS__ 0; });                                      \
    if (__sub_ret != 0) {                                                      \
      __fd_avp_cleanup(__grp_avp);                                             \
      return -1;                                                               \
    }                                                                          \
    if (fd_msg_avp_add(msg_or_parent, MSG_BRW_LAST_CHILD, __grp_avp) != 0) {   \
      __fd_avp_cleanup(__grp_avp);                                             \
      return -1;                                                               \
    }                                                                          \
  } while (0)

/* 子 AVP 专用宏（在 {} 块内部使用） */
/* 注意：对于标准 AVP 使用不带 _V 后缀的版本，对于厂商 AVP 使用带 _V 的版本 */
#define S_STR(m, v) ADD_AVP_STR_V(parent_for_sub, m, v, MAGIC_VENDOR_ID)
#define S_U32(m, v) ADD_AVP_U32_V(parent_for_sub, m, v, MAGIC_VENDOR_ID)
#define S_U64(m, v) ADD_AVP_U64_V(parent_for_sub, m, v, MAGIC_VENDOR_ID)
#define S_FLOAT(m, v) ADD_AVP_FLOAT_V(parent_for_sub, m, v, MAGIC_VENDOR_ID)

/* 标准 AVP 专用宏（Vendor=0，不设置 V 标志） */
#define S_STD_STR(m, v) ADD_AVP_STR(parent_for_sub, m, v)
#define S_STD_U32(m, v) ADD_AVP_U32(parent_for_sub, m, v)
#define S_STD_U64(m, v) ADD_AVP_U64(parent_for_sub, m, v)

#endif /* FD_AVP_HELPERS_H */