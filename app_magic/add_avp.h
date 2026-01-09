/**
 * @file add_avp.h
 * @brief Diameter AVP 加载辅助工具函数库。
 * @details 本文件提供了一组内联函数和宏，用于简化 AVP
 * 的创建、赋值及嵌套（Grouped AVP）操作。 它封装了常用的 freeDiameter AVP
 * 操作，并引入了“垃圾桶”机制解决 AVP 内存释放问题。
 */

#ifndef FD_AVP_HELPERS_H
#define FD_AVP_HELPERS_H

#include <freeDiameter/extension.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief 内部清理函数：释放未成功添加的 AVP
 *
 * 由于 freeDiameter 库没有直接提供
 * fd_avp_free()，该函数通过创建一个临时消息 并将 AVP
 * 添加到该消息中，然后释放消息来间接释放 AVP 内存。
 *
 * @param avp 需要释放的 AVP 指针
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
 * @brief 设置 AVP 的字符串值
 *
 * @param avp AVP 指针
 * @param str 要设置的字符串内容
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_str(struct avp *avp, const char *str) {
  union avp_value val;
  if (!str)
    str = ""; /* 防止 NULL 指针 */
  val.os.data = (uint8_t *)str;
  val.os.len = strlen(str);
  return fd_msg_avp_setvalue(avp, &val);
}

/**
 * @brief 设置 AVP 的字符串值（兼容性别名）
 *
 * @param avp AVP 指针
 * @param str 要设置的字符串内容
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_strvalue(struct avp *avp, const char *str) {
  return fd_avp_set_str(avp, str); /* 与上面完全相同，仅为兼容旧代码 */
}

/**
 * @brief 创建并添加一个字符串类型的 AVP 到消息或父 AVP 中
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param str 字符串值
 * @return int 成功返回 0，失败返回 -1
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
/* 2. Unsigned32 / Integer32                                          */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 32 位无符号整数值
 *
 * @param avp AVP 指针
 * @param val 32 位无符号整数值
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_uint32(struct avp *avp, uint32_t val) {
  union avp_value v = {.u32 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 创建并添加一个 32 位无符号整数类型的 AVP 到消息或父 AVP 中
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 32 位无符号整数值
 * @return int 成功返回 0，失败返回 -1
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
/* 3. Unsigned64 / Integer64                                          */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 64 位无符号整数值
 *
 * @param avp AVP 指针
 * @param val 64 位无符号整数值
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_uint64(struct avp *avp, uint64_t val) {
  union avp_value v = {.u64 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 创建并添加一个 64 位无符号整数类型的 AVP 到消息或父 AVP 中
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 64 位无符号整数值
 * @return int 成功返回 0，失败返回 -1
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
/* 4. Float32 / Float64                                               */
/* ================================================================== */

/**
 * @brief 设置 AVP 的 32 位浮点数值
 *
 * @param avp AVP 指针
 * @param val 浮点数值
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_float32(struct avp *avp, float val) {
  union avp_value v = {.f32 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/**
 * @brief 设置 AVP 的 64 位双精度浮点数值
 *
 * @param avp AVP 指针
 * @param val 双精度浮点数值
 * @return int 成功返回 0，失败返回错误码
 */
static inline int fd_avp_set_float64(struct avp *avp, double val) {
  union avp_value v = {.f64 = val};
  return fd_msg_avp_setvalue(avp, &v);
}

/* 兼容旧项目常用名字 */
#define fd_avp_set_float fd_avp_set_float32

/**
 * @brief 创建并添加一个 32 位浮点数类型的 AVP 到消息或父 AVP 中
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 浮点数值
 * @return int 成功返回 0，失败返回 -1
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
 * @brief 创建并添加一个厂商特定的字符串类型 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param str 字符串值
 * @param vendor_id 厂商 ID
 * @return int 成功返回 0，失败返回 -1
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

/**
 * @brief 创建并添加一个厂商特定的 32 位无符号整数类型 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 32 位无符号整数值
 * @param vendor_id 厂商 ID
 * @return int 成功返回 0，失败返回 -1
 */
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

/**
 * @brief 创建并添加一个厂商特定的 64 位无符号整数类型 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 64 位无符号整数值
 * @param vendor_id 厂商 ID
 * @return int 成功返回 0，失败返回 -1
 */
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

/**
 * @brief 创建并添加一个厂商特定的 32 位浮点数类型 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 浮点数值
 * @param vendor_id 厂商 ID
 * @return int 成功返回 0，失败返回 -1
 */
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

/**
 * @brief 创建并添加一个 32 位有符号整数类型的 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 32 位有符号整数值
 * @return int 成功返回 0，失败返回 -1
 */
static inline int fd_msg_avp_add_int32(msg_or_avp *parent,
                                       struct dict_object *model, int32_t val) {
  struct avp *avp = NULL;
  union avp_value value;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  memset(&value, 0, sizeof(value));
  value.i32 = val;
  CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &value), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  CHECK_FCT_DO(fd_msg_avp_add(parent, MSG_BRW_LAST_CHILD, avp), {
    __fd_avp_cleanup(avp);
    return -1;
  });
  return 0;
}

/**
 * @brief 创建并添加一个厂商特定的 32 位有符号整数类型 AVP
 *
 * @param parent 父消息或父 AVP 指针
 * @param model 字典中的 AVP 定义对象
 * @param val 32 位有符号整数值
 * @param vendor_id 厂商 ID
 * @return int 成功返回 0，失败返回 -1
 */
static inline int fd_msg_avp_add_int32_v(msg_or_avp *parent,
                                         struct dict_object *model, int32_t val,
                                         uint32_t vendor_id) {
  struct avp *avp = NULL;
  union avp_value value;
  CHECK_FCT_DO(fd_msg_avp_new(model, 0, &avp), return -1);
  memset(&value, 0, sizeof(value));
  value.i32 = val;
  CHECK_FCT_DO(fd_msg_avp_setvalue(avp, &value), {
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

#define ADD_AVP_I32(msg, model, val)                                           \
  CHECK_FCT_DO(fd_msg_avp_add_int32(msg, model, val), return -1)
#define ADD_AVP_I32_V(msg, model, val, vid)                                    \
  CHECK_FCT_DO(fd_msg_avp_add_int32_v(msg, model, val, vid), return -1)

/* ====================== add group ====================== */
#ifndef MAGIC_VENDOR_ID
#define MAGIC_VENDOR_ID 13712
#endif

/**
 * @brief 终极 Grouped AVP 构建宏。
 * @details 该宏允许以声明式风格构建嵌套的 Grouped AVP。它会自动处理 AVP
 * 的创建、子 AVP 的添加以及出错时的内存回滚。 利用了 GCC 的 Statement
 * Expression 特性（({ ... })）来执行变长参数中的子项代码。
 *
 * @param msg_or_parent 父消息 (struct msg*) 或父 AVP (struct avp*) 指针。
 * @param model         Grouped AVP 的字典对象。
 * @param ...           在 {} 块内部调用的子项添加逻辑（如 S_STR, S_U32 等）。
 *
 * @warning 如果子项添加失败，该宏会自动释放已分配的内存并返回 -1。
 */
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