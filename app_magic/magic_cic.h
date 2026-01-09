/**
 * @file magic_cic.h
 * @brief MAGIC CIC (Client Interface Component) 头文件。
 * @details 定义 CIC 模块的接口和数据结构。
 *
 * 本文件定义了 MAGIC 系统中的 Client Interface Component (CIC) 模块接口。
 * CIC 模块负责处理来自客户端的 Diameter 消息，包括:
 * - MCAR (Client Authentication Request) - 客户端认证请求
 * - MCCR (Communication Change Request) - 通信变更请求
 * - STR (Session Termination Request) - 会话终止请求
 *
 * CIC 模块是 MAGIC 系统的入口点，负责:
 * 1. 接收和解析 Diameter 消息
 * 2. 验证客户端身份和权限
 * 3. 调用策略引擎进行链路选择
 * 4. 通过 LMI 接口与 DLM 交互
 * 5. 生成和发送响应消息
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 Compliant
 */

#ifndef MAGIC_CIC_H /* 头文件保护宏开始 */
#define MAGIC_CIC_H /* 防止重复包含 */

/*===========================================================================
 * 前向声明 (Forward Declarations)
 *===========================================================================*/

/**
 * @brief MagicContext 前向声明。
 * @details MAGIC 系统的主上下文结构体。
 *
 * 由于 MagicContext 在其他头文件中定义，这里使用前向声明
 * 避免循环包含问题。
 */
typedef struct MagicContext MagicContext;

/*===========================================================================
 * CIC 模块 API 函数声明
 *===========================================================================*/

/**
 * @brief 初始化 CIC 模块。
 * @details 注册 Diameter 命令处理器，准备处理客户端消息。
 *
 * 初始化过程包括:
 * 1. 初始化 MAGIC Diameter 字典。
 * 2. 注册 MAGIC 应用支持。
 * 3. 注册 MCAR/MCCR/STR 消息处理器。
 * 4. 设置全局上下文指针。
 *
 * @param ctx 指向 MAGIC 系统上下文的指针。
 * @return 0 成功，-1 失败。
 */
int magic_cic_init(MagicContext *ctx);

/**
 * @brief 清理 CIC 模块。
 * @details 释放 CIC 模块的资源，取消注册的处理器。
 *
 * 清理过程包括:
 * 1. 清除全局上下文指针。
 * 2. 记录清理完成日志。
 *
 * @param ctx 指向 MAGIC 系统上下文的指针。
 */
void magic_cic_cleanup(MagicContext *ctx);

#endif /* MAGIC_CIC_H */ /* 头文件保护宏结束 */
