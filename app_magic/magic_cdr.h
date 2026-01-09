/**
 * @file magic_cdr.h
 * @brief MAGIC CDR (Call Data Record) 管理模块
 * @details 实现 CDR 的创建、关闭、切分和持久化存储。
 *          支持 MACR/MACA 命令的"不断网切账单"功能。
 *
 * 核心功能:
 * 1. CDR 生命周期管理 (创建/关闭/切分)
 * 2. JSON 格式文件存储
 * 3. CDR 级别互斥锁 (并发保护)
 * 4. 自动归档和清理 (默认保留1天)
 * 5. 流量计数器溢出检测
 *
 * 设计原则:
 * - Snapshot (快照) -> Archive (归档旧的) -> Create (创建新的)
 * - 宁可账单太长，不能丢账单
 * - 流量计算: 新CDR流量 = 当前累计 - base_offset
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2025-12-03
 * @copyright MAGIC System
 */

#ifndef MAGIC_CDR_H
#define MAGIC_CDR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

#define MAX_CDR_RECORDS         1024    ///< 最大活跃 CDR 数量
#define MAX_CDR_ID_LEN          64      ///< CDR ID 最大长度
#define MAX_CDR_SESSION_ID_LEN  128     ///< 会话 ID 最大长度
#define MAX_CDR_CLIENT_ID_LEN   64      ///< 客户端 ID 最大长度
#define MAX_CDR_PATH_LEN        256     ///< 文件路径最大长度

/* CDR 存储目录 */
#define CDR_BASE_DIR            "/var/lib/magic/cdr"         ///< CDR 基础存储目录
#define CDR_ACTIVE_DIR          "/var/lib/magic/cdr/active"  ///< 活跃 CDR 存储目录
#define CDR_ARCHIVE_DIR         "/var/lib/magic/cdr/archive" ///< 归档 CDR 存储目录

/* 归档策略 */
#define CDR_ARCHIVE_RETENTION_SEC   (24 * 3600)  ///< 默认保留1天 (86400秒)
#define CDR_CLEANUP_INTERVAL_SEC    3600         ///< 清理检查间隔 (1小时)

/* 流量溢出阈值 (接近 uint64_t 最大值的 90%) */
#define TRAFFIC_OVERFLOW_THRESHOLD  (UINT64_MAX / 10 * 9) ///< 流量溢出检测阈值

/*===========================================================================
 * CDR 状态枚举
 *===========================================================================*/

/**
 * @brief CDR 状态枚举
 */
typedef enum {
    CDR_STATUS_UNKNOWN    = 0,  ///< 未知状态
    CDR_STATUS_ACTIVE     = 1,  ///< 活跃中 - 正在记录流量
    CDR_STATUS_FINISHED   = 2,  ///< 已完成 - 正常关闭
    CDR_STATUS_ARCHIVED   = 3,  ///< 已归档 - 移动到归档目录
    CDR_STATUS_ROLLOVER   = 4   ///< 切分 - 被 MACR 切分关闭
} CDRStatus;

/*===========================================================================
 * CDR 记录结构体
 *===========================================================================*/

/**
 * @brief CDR 记录结构体
 * @details 存储单个计费数据记录的完整信息，包括标识、状态、时间和流量统计。
 */
typedef struct {
    /* 标识信息 */
    uint32_t        cdr_id;                         ///< CDR 唯一标识符 (数字ID)
    char            cdr_uuid[MAX_CDR_ID_LEN];       ///< CDR UUID 字符串
    char            session_id[MAX_CDR_SESSION_ID_LEN]; ///< 关联的会话 ID
    char            client_id[MAX_CDR_CLIENT_ID_LEN];   ///< 客户端 ID (Origin-Host)
    
    /* 状态信息 */
    CDRStatus       status;                         ///< CDR 状态
    bool            in_use;                         ///< 是否使用中
    
    /* 时间信息 */
    time_t          start_time;                     ///< 开始时间
    time_t          stop_time;                      ///< 结束时间 (0=进行中)
    time_t          archive_time;                   ///< 归档时间
    
    /* 流量统计 */
    uint64_t        bytes_in;                       ///< 入站字节数 (累计)
    uint64_t        bytes_out;                      ///< 出站字节数 (累计)
    uint64_t        packets_in;                     ///< 入站数据包数
    uint64_t        packets_out;                    ///< 出站数据包数
    
    /* 基准偏移量 (用于 CDR 切分时计算增量) */
    uint64_t        base_offset_in;                 ///< 入站基准偏移
    uint64_t        base_offset_out;                ///< 出站基准偏移
    
    /* 溢出标记 (处理 uint64 计数器回绕) */
    uint32_t        overflow_count_in;              ///< 入站溢出次数
    uint32_t        overflow_count_out;             ///< 出站溢出次数
    uint64_t        last_bytes_in;                  ///< 上次入站字节数 (检测溢出)
    uint64_t        last_bytes_out;                 ///< 上次出站字节数 (检测溢出)
    
    /* 链路信息 */
    char            dlm_name[64];                   ///< 使用的 DLM 名称
    uint8_t         bearer_id;                      ///< Bearer ID
    
    /* 并发控制 */
    pthread_mutex_t lock;                           ///< CDR 级别互斥锁
    bool            lock_initialized;               ///< 锁是否已初始化
} CDRRecord;

/*===========================================================================
 * CDR 管理器结构体
 *===========================================================================*/

/**
 * @brief CDR 管理器上下文结构体
 * @details 管理系统中所有活跃和归档的 CDR 记录。
 */
typedef struct {
    CDRRecord       records[MAX_CDR_RECORDS];       ///< CDR 记录数组
    uint32_t        record_count;                   ///< 活跃记录数量
    uint32_t        next_cdr_id;                    ///< 下一个可用 CDR ID
    
    pthread_mutex_t manager_lock;                   ///< 管理器级别锁
    bool            is_initialized;                 ///< 是否已初始化
    
    /* 存储路径 */
    char            base_dir[MAX_CDR_PATH_LEN];     ///< 基础目录
    char            active_dir[MAX_CDR_PATH_LEN];   ///< 活跃 CDR 目录
    char            archive_dir[MAX_CDR_PATH_LEN];  ///< 归档目录
    
    /* 归档策略 */
    uint32_t        retention_sec;                  ///< 归档保留时间 (秒)
    time_t          last_cleanup_time;              ///< 上次清理时间
    
    /* 统计信息 */
    uint64_t        total_cdrs_created;             ///< 累计创建的 CDR 数
    uint64_t        total_cdrs_archived;            ///< 累计归档的 CDR 数
    uint64_t        total_cdrs_deleted;             ///< 累计删除的 CDR 数
} CDRManager;

/*===========================================================================
 * CDR 切分结果结构体 (用于 MACA 响应)
 *===========================================================================*/

/**
 * @brief CDR 切分操作结果结构体
 * @details 包含切分操作的成功状态、新旧 CDR 的标识以及最终统计信息。
 */
typedef struct {
    bool            success;                        ///< 操作是否成功
    uint32_t        old_cdr_id;                     ///< 被关闭的旧 CDR ID
    char            old_cdr_uuid[MAX_CDR_ID_LEN];   ///< 旧 CDR UUID
    uint32_t        new_cdr_id;                     ///< 新创建的 CDR ID
    char            new_cdr_uuid[MAX_CDR_ID_LEN];   ///< 新 CDR UUID
    
    /* 旧 CDR 最终统计 */
    uint64_t        final_bytes_in;                 ///< 旧 CDR 最终入站流量
    uint64_t        final_bytes_out;                ///< 旧 CDR 最终出站流量
    
    /* 错误信息 */
    int             error_code;                     ///< 错误码 (0=成功)
    char            error_message[128];             ///< 错误描述
} CDRRolloverResult;

/*===========================================================================
 * API 函数声明 - 初始化和清理
 *===========================================================================*/

/**
 * @brief 初始化 CDR 管理器
 * @details 创建必要的目录结构，加载活跃 CDR 记录。
 * @param mgr CDR 管理器指针
 * @param base_dir 基础存储目录 (NULL=使用默认目录)
 * @param retention_sec 归档保留时间 (0=使用默认值)
 * @return int 0=成功, -1=失败
 * @note 如果目录不存在，函数将尝试递归创建。
 */
int cdr_manager_init(CDRManager *mgr, const char *base_dir, uint32_t retention_sec);

/**
 * @brief 清理 CDR 管理器
 * @details 保存所有活跃 CDR，释放资源。
 * @param mgr CDR 管理器指针
 * @warning 在系统退出前必须调用此函数以防止数据丢失。
 */
void cdr_manager_cleanup(CDRManager *mgr);

/*===========================================================================
 * API 函数声明 - CDR 生命周期管理
 *===========================================================================*/

/**
 * @brief 为会话创建新的 CDR
 * @details 在管理器中分配一个新的 CDR 记录并初始化。
 * @param mgr CDR 管理器指针
 * @param session_id 会话 ID
 * @param client_id 客户端 ID
 * @param dlm_name DLM 名称 (可为 NULL)
 * @return CDRRecord* 新 CDR 指针, NULL=失败
 */
CDRRecord* cdr_create(CDRManager *mgr, 
                      const char *session_id, 
                      const char *client_id,
                      const char *dlm_name);

/**
 * @brief 关闭 CDR (正常终止)
 * @details 停止流量记录，更新最终统计并标记为已完成。
 * @param mgr CDR 管理器指针
 * @param cdr CDR 记录指针
 * @param final_bytes_in 最终入站流量
 * @param final_bytes_out 最终出站流量
 * @return int 0=成功, -1=失败
 */
int cdr_close(CDRManager *mgr, 
              CDRRecord *cdr, 
              uint64_t final_bytes_in, 
              uint64_t final_bytes_out);

/**
 * @brief CDR 切分 (MACR 核心操作)
 * @details 原子性地关闭当前 CDR 并创建新 CDR，用于不断网计费切换。
 * @param mgr CDR 管理器指针
 * @param session_id 目标会话 ID
 * @param current_bytes_in 当前累计入站流量
 * @param current_bytes_out 当前累计出站流量
 * @param result 输出: 切分结果
 * @return int 0=成功, -1=失败
 */
int cdr_rollover(CDRManager *mgr,
                 const char *session_id,
                 uint64_t current_bytes_in,
                 uint64_t current_bytes_out,
                 CDRRolloverResult *result);

/*===========================================================================
 * API 函数声明 - 查找和查询
 *===========================================================================*/

/**
 * @brief 根据会话 ID 查找活跃 CDR
 * @param mgr CDR 管理器指针
 * @param session_id 会话 ID
 * @return CDRRecord* CDR 指针, NULL=未找到
 */
CDRRecord* cdr_find_by_session(CDRManager *mgr, const char *session_id);

/**
 * @brief 根据 CDR ID 查找记录
 * @param mgr CDR 管理器指针
 * @param cdr_id CDR ID
 * @return CDRRecord* CDR 指针, NULL=未找到
 */
CDRRecord* cdr_find_by_id(CDRManager *mgr, uint32_t cdr_id);

/**
 * @brief 根据客户端 ID 获取所有 CDR
 * @param mgr CDR 管理器指针
 * @param client_id 客户端 ID
 * @param out_cdrs 输出: CDR 指针数组
 * @param max_count 数组最大容量
 * @return int 找到的 CDR 数量
 */
int cdr_find_by_client(CDRManager *mgr, 
                       const char *client_id,
                       CDRRecord **out_cdrs, 
                       int max_count);

/*===========================================================================
 * API 函数声明 - 流量更新
 *===========================================================================*/

/**
 * @brief 更新 CDR 流量统计 (带溢出检测)
 * @details 更新字节和包计数，并检测 uint64 计数器是否发生回绕。
 * @param cdr CDR 记录指针
 * @param bytes_in 当前入站字节数
 * @param bytes_out 当前出站字节数
 * @param packets_in 当前入站数据包数
 * @param packets_out 当前出站数据包数
 * @return int 0=正常, 1=检测到溢出, -1=错误
 */
int cdr_update_traffic(CDRRecord *cdr,
                       uint64_t bytes_in,
                       uint64_t bytes_out,
                       uint64_t packets_in,
                       uint64_t packets_out);

/**
 * @brief 计算 CDR 的实际流量 (减去基准偏移)
 * @details 用于获取自 CDR 创建或切分以来的增量流量。
 * @param cdr CDR 记录指针
 * @param out_bytes_in 输出: 实际入站流量
 * @param out_bytes_out 输出: 实际出站流量
 */
void cdr_get_actual_traffic(const CDRRecord *cdr,
                            uint64_t *out_bytes_in,
                            uint64_t *out_bytes_out);

/*===========================================================================
 * API 函数声明 - 持久化存储
 *===========================================================================*/

/**
 * @brief 保存 CDR 到文件 (JSON 格式)
 * @param mgr CDR 管理器指针
 * @param cdr CDR 记录指针
 * @return int 0=成功, -1=失败
 */
int cdr_save_to_file(CDRManager *mgr, const CDRRecord *cdr);

/**
 * @brief 从文件加载 CDR
 * @param mgr CDR 管理器指针
 * @param filepath 文件路径
 * @return CDRRecord* CDR 指针, NULL=失败
 */
CDRRecord* cdr_load_from_file(CDRManager *mgr, const char *filepath);

/**
 * @brief 加载所有活跃 CDR
 * @details 系统启动时从活跃目录恢复状态。
 * @param mgr CDR 管理器指针
 * @return int 加载的 CDR 数量, -1=失败
 */
int cdr_load_all_active(CDRManager *mgr);

/**
 * @brief 保存所有活跃 CDR
 * @details 周期性或关机前保存所有内存中的 CDR 记录。
 * @param mgr CDR 管理器指针
 * @return int 保存的 CDR 数量, -1=失败
 */
int cdr_save_all_active(CDRManager *mgr);

/*===========================================================================
 * API 函数声明 - 归档和清理
 *===========================================================================*/

/**
 * @brief 归档已完成的 CDR
 * @details 将已关闭的 CDR 移动到归档目录。
 * @param mgr CDR 管理器指针
 * @param cdr CDR 记录指针
 * @return int 0=成功, -1=失败
 */
int cdr_archive(CDRManager *mgr, CDRRecord *cdr);

/**
 * @brief 清理过期的归档 CDR
 * @details 删除超过保留时间的归档文件。
 * @param mgr CDR 管理器指针
 * @return int 删除的文件数量, -1=失败
 */
int cdr_cleanup_expired(CDRManager *mgr);

/**
 * @brief 执行周期性维护
 * @details 自动归档已完成 CDR，清理过期文件。
 * @param mgr CDR 管理器指针
 */
void cdr_periodic_maintenance(CDRManager *mgr);

/*===========================================================================
 * API 函数声明 - 锁操作
 *===========================================================================*/

/**
 * @brief 锁定 CDR (获取互斥锁)
 * @param cdr CDR 记录指针
 * @return int 0=成功, -1=失败
 */
int cdr_lock(CDRRecord *cdr);

/**
 * @brief 解锁 CDR (释放互斥锁)
 * @param cdr CDR 记录指针
 * @return int 0=成功, -1=失败
 */
int cdr_unlock(CDRRecord *cdr);

/**
 * @brief 锁定会话的所有 CDR
 * @param mgr CDR 管理器指针
 * @param session_id 会话 ID
 * @return int 0=成功, -1=失败
 */
int cdr_lock_session(CDRManager *mgr, const char *session_id);

/**
 * @brief 解锁会话的所有 CDR
 * @param mgr CDR 管理器指针
 * @param session_id 会话 ID
 * @return int 0=成功, -1=失败
 */
int cdr_unlock_session(CDRManager *mgr, const char *session_id);

/*===========================================================================
 * API 函数声明 - 辅助函数
 *===========================================================================*/

/**
 * @brief 生成 CDR UUID
 * @param out_uuid 输出缓冲区
 * @param max_len 缓冲区大小
 */
void cdr_generate_uuid(char *out_uuid, size_t max_len);

/**
 * @brief 获取 CDR 状态名称
 * @param status CDR 状态
 * @return const char* 状态名称字符串
 */
const char* cdr_status_name(CDRStatus status);

/**
 * @brief 检测流量计数器溢出
 * @param current 当前值
 * @param previous 上次值
 * @return bool true=检测到溢出, false=正常
 */
bool cdr_detect_overflow(uint64_t current, uint64_t previous);

/**
 * @brief 打印 CDR 信息 (调试用)
 * @param cdr CDR 记录指针
 */
void cdr_print_info(const CDRRecord *cdr);

/**
 * @brief 打印 CDR 管理器状态
 * @param mgr CDR 管理器指针
 */
void cdr_manager_print_status(const CDRManager *mgr);

#endif /* MAGIC_CDR_H */
