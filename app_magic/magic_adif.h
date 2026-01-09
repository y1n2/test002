/**
 * @file magic_adif.h
 * @brief MAGIC Aircraft Data Interface Function (ADIF) 模块
 * @details 基于 ARINC 834-1 ADBP 协议的飞机数据接口。
 *          本模块实现 MAGIC 系统与飞机数据接口功能 (ADIF) 服务之间的通信。
 *          MAGIC 作为客户端订阅飞机状态数据，ADIF 服务作为服务端推送实时数据。
 *          通信协议: ADBP (Avionics Data Broadcast Protocol) over TCP/IP
 *          数据格式: XML
 *
 * @author MAGIC System Development Team
 * @version 1.0
 * @date 2024
 * @copyright ARINC 839-2014 / ARINC 834-1 Compliant
 */

#ifndef MAGIC_ADIF_H
#define MAGIC_ADIF_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

/*===========================================================================
 * 常量定义
 *===========================================================================*/

#define ADIF_MAX_TAIL_NUMBER_LEN    16      ///< 飞机尾号最大长度
#define ADIF_DEFAULT_REFRESH_MS     1000    ///< 默认刷新周期 (毫秒)
#define ADIF_DEFAULT_SERVER_PORT    4000    ///< ADIF 服务端默认端口
#define ADIF_DEFAULT_ASYNC_PORT     64001   ///< 异步数据接收端口
#define ADIF_MAX_XML_BUFFER         4096    ///< XML 缓冲区大小

/*===========================================================================
 * 飞行阶段枚举 (ARINC 834 Flight Phase)
 *===========================================================================*/

/**
 * @brief 飞行阶段枚举
 * @details 基于 ARINC 834 自动计算的飞行阶段状态机。
 */
typedef enum {
    FLIGHT_PHASE_UNKNOWN    = 0,    ///< 未知阶段
    FLIGHT_PHASE_GATE       = 1,    ///< 登机口 - WoW=地面, 地速<2kts
    FLIGHT_PHASE_TAXI       = 2,    ///< 滑行 - WoW=地面, 地速>=2kts
    FLIGHT_PHASE_TAKEOFF    = 3,    ///< 起飞 - 地速>=80kts
    FLIGHT_PHASE_CLIMB      = 4,    ///< 爬升 - 垂直速度>100ft/min
    FLIGHT_PHASE_CRUISE     = 5,    ///< 巡航 - 稳定飞行
    FLIGHT_PHASE_DESCENT    = 6,    ///< 下降 - 垂直速度<-100ft/min
    FLIGHT_PHASE_APPROACH   = 7,    ///< 进近
    FLIGHT_PHASE_LANDING    = 8     ///< 着陆
} AdifFlightPhase;

/**
 * @brief 将飞行阶段枚举转换为字符串
 * @param phase 飞行阶段枚举值
 * @return const char* 对应的字符串描述
 */
const char* adif_flight_phase_to_string(AdifFlightPhase phase);

/**
 * @brief 将字符串转换为飞行阶段枚举
 * @param str 飞行阶段字符串
 * @return AdifFlightPhase 对应的枚举值
 */
AdifFlightPhase adif_string_to_flight_phase(const char* str);

/*===========================================================================
 * 数据有效性枚举 (ARINC 834 Validity)
 *===========================================================================*/

/**
 * @brief 数据有效性枚举
 * @details validity 属性定义 (ARINC 834)。
 */
typedef enum {
    VALIDITY_NO_DATA        = 0,    ///< ND - 无数据
    VALIDITY_NORMAL         = 1,    ///< NO - 正常运行，数据有效
    VALIDITY_NO_COMPUTED    = 2     ///< NCD - 无法计算数据
} AdifValidity;

/*===========================================================================
 * 飞机状态数据结构 (Aircraft State Parameters)
 *===========================================================================*/

/**
 * @brief Weight on Wheels (WoW) 状态
 * @details 轮载信号 - 关键路由切换依据。
 */
typedef struct {
    bool            on_ground;      ///< true=地面, false=空中
    AdifValidity    validity;       ///< 数据有效性
    uint64_t        timestamp_ms;   ///< Unix Epoch 毫秒时间戳
} AdifWeightOnWheels;

/**
 * @brief 位置信息 (Spatial Location)
 */
typedef struct {
    double          latitude;       ///< 纬度 (度), 范围 ±90
    double          longitude;      ///< 经度 (度), 范围 ±180
    double          altitude_ft;    ///< 气压高度 (英尺), 范围 0-131072
    AdifValidity    lat_validity;   ///< 纬度有效性
    AdifValidity    lon_validity;   ///< 经度有效性
    AdifValidity    alt_validity;   ///< 高度有效性
    uint64_t        timestamp_ms;   ///< Unix Epoch 毫秒时间戳
} AdifPosition;

/**
 * @brief 飞行阶段信息
 */
typedef struct {
    AdifFlightPhase phase;          ///< 当前飞行阶段
    AdifValidity    validity;       ///< 数据有效性
    uint64_t        timestamp_ms;   ///< Unix Epoch 毫秒时间戳
} AdifFlightPhaseData;

/**
 * @brief 飞机识别信息
 */
typedef struct {
    char            tail_number[ADIF_MAX_TAIL_NUMBER_LEN]; ///< 飞机尾号/注册号
    AdifValidity    validity;       ///< 数据有效性
    uint64_t        timestamp_ms;   ///< Unix Epoch 毫秒时间戳
} AdifAircraftId;

/**
 * @brief 速度信息 (用于飞行阶段计算)
 */
typedef struct {
    double          ground_speed_kts;   ///< 地速 (节)
    double          vertical_speed_fpm; ///< 垂直速度 (英尺/分钟)
    AdifValidity    gs_validity;        ///< 地速有效性
    AdifValidity    vs_validity;        ///< 垂直速度有效性
    uint64_t        timestamp_ms;       ///< Unix Epoch 毫秒时间戳
} AdifSpeed;

/**
 * @brief 完整的飞机状态数据
 * @details 聚合所有 ARINC 834 通用参数。
 */
typedef struct {
    AdifWeightOnWheels  wow;            ///< Weight on Wheels 状态
    AdifPosition        position;       ///< 位置信息
    AdifFlightPhaseData flight_phase;   ///< 飞行阶段
    AdifAircraftId      aircraft_id;    ///< 飞机识别信息
    AdifSpeed           speed;          ///< 速度信息
    
    bool                data_valid;     ///< 整体数据有效性标志
    uint64_t            last_update_ms; ///< 最后更新时间
} AdifAircraftState;

/*===========================================================================
 * ADIF 客户端上下文 (MAGIC 侧)
 *===========================================================================*/

/**
 * @brief ADIF 客户端连接状态枚举
 */
typedef enum {
    ADIF_CLIENT_DISCONNECTED = 0,   ///< 未连接
    ADIF_CLIENT_CONNECTING   = 1,   ///< 连接中
    ADIF_CLIENT_SUBSCRIBED   = 2,   ///< 已订阅，正常接收数据
    ADIF_CLIENT_ERROR        = 3    ///< 连接错误
} AdifClientState;

/**
 * @brief ADIF 客户端配置结构体
 */
typedef struct {
    char        server_host[64];        ///< ADIF 服务器地址
    uint16_t    server_port;            ///< ADIF 服务器端口 (默认 4000)
    uint16_t    async_port;             ///< 异步数据接收端口 (默认 64001)
    uint32_t    refresh_period_ms;      ///< 刷新周期 (毫秒)
    bool        auto_reconnect;         ///< 自动重连标志
    uint32_t    reconnect_interval_ms;  ///< 重连间隔 (毫秒)
} AdifClientConfig;

/**
 * @brief 飞机状态变化回调函数类型
 * @param state 最新的飞机状态
 * @param user_data 用户自定义数据指针
 */
typedef void (*AdifStateCallback)(const AdifAircraftState* state, void* user_data);

/**
 * @brief ADIF 客户端上下文结构体
 * @details MAGIC 侧的 ADIF 客户端，用于接收飞机状态数据。
 */
typedef struct {
    AdifClientConfig    config;         ///< 客户端配置
    AdifClientState     state;          ///< 连接状态
    AdifAircraftState   aircraft_state; ///< 当前飞机状态
    
    int                 sync_sock;      ///< 同步控制连接套接字
    int                 async_sock;     ///< 异步数据接收套接字
    
    pthread_t           receiver_thread;///< 数据接收线程
    pthread_mutex_t     state_mutex;    ///< 状态数据互斥锁
    bool                running;        ///< 运行标志
    
    AdifStateCallback   callback;       ///< 状态变化回调函数
    void*               callback_data;  ///< 回调函数用户数据
    
    char                error_msg[128]; ///< 错误信息
} AdifClientContext;

/*===========================================================================
 * ADIF 客户端 API (MAGIC 侧)
 *===========================================================================*/

/**
 * @brief 初始化 ADIF 客户端
 * @details 设置默认配置并初始化互斥锁。
 * @param ctx 客户端上下文指针
 * @param config 配置参数，NULL 使用默认配置
 * @return int 成功返回 0，失败返回 -1
 */
int adif_client_init(AdifClientContext* ctx, const AdifClientConfig* config);

/**
 * @brief 连接到 ADIF 服务器并订阅数据
 * @details 建立 TCP 连接，发送订阅请求 XML，并启动接收线程。
 * @param ctx 客户端上下文指针
 * @return int 成功返回 0，失败返回 -1
 * @warning 该函数会启动后台线程，调用前确保 ctx 已初始化。
 */
int adif_client_connect(AdifClientContext* ctx);

/**
 * @brief 断开与 ADIF 服务器的连接
 * @details 停止接收线程并关闭套接字。
 * @param ctx 客户端上下文指针
 */
void adif_client_disconnect(AdifClientContext* ctx);

/**
 * @brief 获取当前飞机状态（线程安全）
 * @details 通过互斥锁保护，拷贝最新的飞机状态数据。
 * @param ctx 客户端上下文指针
 * @param state [out] 输出：当前飞机状态副本
 * @return int 成功返回 0，失败返回 -1
 */
int adif_client_get_state(AdifClientContext* ctx, AdifAircraftState* state);

/**
 * @brief 设置状态变化回调函数
 * @details 当接收到新的飞机状态数据时，将调用此函数。
 * @param ctx 客户端上下文指针
 * @param callback 回调函数指针
 * @param user_data 用户自定义数据
 */
void adif_client_set_callback(AdifClientContext* ctx, 
                              AdifStateCallback callback, 
                              void* user_data);

/**
 * @brief 检查客户端是否已连接
 * @param ctx 客户端上下文指针
 * @return bool true=已连接, false=未连接
 */
bool adif_client_is_connected(AdifClientContext* ctx);

/**
 * @brief 清理 ADIF 客户端资源
 * @details 停止所有线程，关闭连接并销毁互斥锁。
 * @param ctx 客户端上下文指针
 */
void adif_client_cleanup(AdifClientContext* ctx);

/*===========================================================================
 * ADIF 与策略引擎集成
 *===========================================================================*/

/**
 * @brief 将 ADIF 飞行阶段映射到策略引擎飞行阶段字符串
 * @param phase ADIF 飞行阶段枚举
 * @return const char* 策略引擎使用的飞行阶段字符串 (如 "GROUND_OPS", "CRUISE" 等)
 */
const char* adif_phase_to_policy_phase(AdifFlightPhase phase);

/**
 * @brief 检查当前飞行阶段是否应该切换路由
 * @param old_phase 旧飞行阶段
 * @param new_phase 新飞行阶段
 * @return bool true=需要重新评估路由, false=不需要
 */
bool adif_should_reevaluate_routing(AdifFlightPhase old_phase, AdifFlightPhase new_phase);

/*===========================================================================
 * XML 解析辅助函数
 *===========================================================================*/

/**
 * @brief 解析 ADIF 发布的 XML 数据
 * @details 解析符合 ARINC 834 标准的 XML 状态报告。
 * @param xml XML 字符串
 * @param state [out] 输出：解析后的飞机状态
 * @return int 成功返回 0，失败返回 -1
 */
int adif_parse_publish_xml(const char* xml, AdifAircraftState* state);

/**
 * @brief 生成订阅请求 XML
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param async_port 异步数据接收端口
 * @param refresh_period_ms 刷新周期
 * @return 生成的 XML 长度，-1=失败
 */
int adif_generate_subscribe_xml(char* buffer, size_t buffer_size,
                                uint16_t async_port, uint32_t refresh_period_ms);

#endif /* MAGIC_ADIF_H */
