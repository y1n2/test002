/**
 * @file magic_error_codes.h
 * @brief MAGIC 错误码定义桥接头文件。
 * @details 本文件作为 MAGIC
 * 业务模块与词典定义系统（`dict_magic_839`）之间的桥接，直接使用了
 * `dict_magic_codes.h` 中的定义。
 *          这种设计确保了协议层词典与业务逻辑层共享同一套错误码定义，保持高度一致性。
 *
 * @see extensions/dict_magic_839/dict_magic_codes.h
 */

#ifndef MAGIC_ERROR_CODES_H
#define MAGIC_ERROR_CODES_H

/*===========================================================================
 * 直接引用词典系统的完整错误码定义
 *
 * 包含内容：
 * - DIAMETER_* 常量：18个标准 Diameter Result-Code
 * - MAGIC_ERROR_* 常量：62个 MAGIC 业务错误码
 * - MAGIC_INFO_* 常量：MAGIC 信息码
 * - 辅助宏：IS_DIAMETER_SUCCESS, IS_MAGIC_ERROR, IS_MAGIC_INFO
 * - 字符串转换函数：magic_status_code_str()
 *===========================================================================*/
#include "../dict_magic_839/dict_magic_codes.h"

/*===========================================================================
 * 使用说明
 *
 * 1. Diameter Result-Code (AVP 268):
 *    使用 DIAMETER_* 常量
 *    示例: DIAMETER_SUCCESS, DIAMETER_AUTHENTICATION_REJECTED
 *
 * 2. MAGIC-Status-Code (AVP 10053):
 *    使用 MAGIC_ERROR_* 或 MAGIC_INFO_* 常量
 *    示例: MAGIC_ERROR_AUTHENTICATION_FAILED, MAGIC_INFO_SET_LINK_QOS
 *
 * 3. 字符串转换:
 *    const char* desc = magic_status_code_str(code);
 *
 * 4. 状态判断:
 *    if (IS_DIAMETER_SUCCESS(result_code)) { ... }
 *    if (IS_MAGIC_ERROR(magic_status)) { ... }
 *
 * 5. 完整示例:
 *    ```c
 *    // 设置协议层结果 (Result-Code AVP)
 *    uint32_t result_code = DIAMETER_SUCCESS;
 *
 *    // 设置业务层状态 (MAGIC-Status-Code AVP)
 *    uint32_t magic_status = MAGIC_ERROR_AUTHENTICATION_FAILED;
 *
 *    // 日志输出
 *    fd_log_error("MAGIC Status: %s (%u)",
 *                 magic_status_code_str(magic_status), magic_status);
 *
 *    // 状态判断
 *    if (IS_MAGIC_ERROR(magic_status)) {
 *        // 处理错误
 *    }
 *    ```
 *===========================================================================*/

#endif /* MAGIC_ERROR_CODES_H */
