# MAGIC Server 代码结构说明

## 📁 文件组织

```
magic_server/
├── cm_core_simple.c          # CM Core 服务器简化版（DLM 连接管理）
├── magic_core_main.c          # MAGIC Core 主程序（配置加载、IPC管理）
├── policy_engine.c            # 策略引擎（路径选择、飞行阶段管理）
├── policy_engine.h            # 策略引擎头文件
├── xml_config_parser.c        # XML 配置文件解析器
├── xml_config_parser.h        # XML 解析器头文件
├── dlm_common.c               # DLM 公共函数库
├── dlm_common.h               # DLM 公共头文件
├── ipc_protocol.h             # IPC 协议定义（消息类型、结构体）
├── config/                    # 配置文件目录
│   ├── magic_system.xml       # 系统配置
│   ├── datalink_profiles.xml  # 链路配置
│   ├── policy_rules.xml       # 策略规则
│   └── client_profiles.xml    # 客户端配置
├── cm_core/                   # CM Core 完整版（未使用）
└── lmi/                       # LMI 模块（未使用）
```

## 🔧 核心组件功能说明

### 1. cm_core_simple.c - CM Core 简化服务器

**职责**：
- 管理 DLM (Data Link Manager) 模块的注册和连接
- 维护链路状态（UP/DOWN、带宽、延迟等）
- 处理心跳消息，监控链路健康状态
- 提供 Unix Domain Socket IPC 接口

**关键数据结构**：
```c
ActiveDlmClient {
    int fd;                    // Socket 连接
    char dlm_id[32];          // DLM 标识符
    bool is_link_up;          // 链路状态
    uint32_t max_bw_kbps;     // 最大带宽
    uint32_t latency_ms;      // 延迟
    // ...
}
```

**主要流程**：
1. 创建 Unix Socket 服务器 (`/tmp/magic_core.sock`)
2. 等待 DLM 连接 (`accept()`)
3. 为每个 DLM 创建独立线程处理消息
4. 处理注册、链路事件、心跳等消息
5. 定期打印活跃链路状态

**关键函数**：
- `handle_registration()` - 处理 DLM 注册请求
- `handle_link_event()` - 处理链路状态变化
- `handle_heartbeat()` - 处理心跳消息
- `handle_client()` - 客户端消息循环

---

### 2. magic_core_main.c - MAGIC Core 主程序

**职责**：
- 加载 XML 配置文件（系统配置、链路配置、策略规则等）
- 启动 CM Core 服务器（与 DLM 通信）
- 管理 DLM 连接生命周期
- 提供配置查询接口

**配置文件加载**：
```c
magic_config_load_all(&g_config)
  ├── 加载 magic_system.xml      // 系统参数
  ├── 加载 datalink_profiles.xml  // 链路配置
  ├── 加载 policy_rules.xml       // 策略规则
  └── 加载 client_profiles.xml    // 客户端配置
```

**DLM 管理**：
- 维护活跃 DLM 列表 (`ActiveDlmClient g_dlm_clients[10]`)
- 为每个 DLM 分配唯一 ID
- 关联 DLM 与链路配置
- 标记链路活动状态

**消息处理**：
- `MSG_TYPE_REGISTER` → `handle_dlm_registration()`
- `MSG_TYPE_LINK_EVENT` → `handle_link_event()`
- `MSG_TYPE_HEARTBEAT` → `handle_heartbeat()`

---

### 3. policy_engine.c - 策略引擎

**职责**：
- 管理飞行阶段（停机、滑行、巡航、降落等）
- 维护链路状态（带宽、延迟、成本、在线状态）
- 执行路径选择算法（基于策略规则和链路状态）
- 提供流量类别映射（客户端 → 流量类型）

**核心数据结构**：
```c
PolicyEngineContext {
    MagicConfig* config;               // 配置引用
    FlightPhase current_phase;         // 当前飞行阶段
    PolicyRuleSet* active_ruleset;     // 当前激活的规则集
    LinkState link_states[MAX_LINKS];  // 所有链路状态
    PolicyEngineStats stats;           // 统计信息
}
```

**飞行阶段枚举**：
```c
FLIGHT_PHASE_PARKED      // 停机
FLIGHT_PHASE_TAXI        // 滑行
FLIGHT_PHASE_TAKEOFF     // 起飞
FLIGHT_PHASE_CRUISE      // 巡航
FLIGHT_PHASE_OCEANIC     // 洋区飞行
FLIGHT_PHASE_LANDING     // 降落
```

**流量类别**：
```c
TRAFFIC_CLASS_FLIGHT_CRITICAL        // 飞行关键数据
TRAFFIC_CLASS_COCKPIT_DATA           // 驾驶舱数据
TRAFFIC_CLASS_CABIN_OPERATIONS       // 客舱运营
TRAFFIC_CLASS_PASSENGER_ENTERTAINMENT // 旅客娱乐
TRAFFIC_CLASS_BULK_DATA              // 批量数据
TRAFFIC_CLASS_ACARS_COMMS            // ACARS 通信
```

**路径选择算法** (`policy_engine_select_path()`):
1. 获取当前飞行阶段的策略规则集
2. 查找匹配的流量类别规则
3. 评估所有路径偏好：
   - 检查链路是否在线
   - 检查是否被禁止 (ACTION_PROHIBIT)
   - 计算路径评分：
     ```
     评分 = 基础分 (10000)
          + 优先级排名分 (0-20000)
          + 带宽分 (可用带宽/1000)
          + 延迟分 (1000 - RTT ms)
          + 成本分 ((100 - cost_index) * 50)
          + 负载分 ((100 - load_percent) * 20)
          + 可靠性分 ((1 - loss_rate) * 1000)
     ```
4. 选择评分最高的可用路径
5. 返回路径选择决策

**关键函数**：
- `policy_engine_init()` - 初始化策略引擎
- `policy_engine_set_flight_phase()` - 切换飞行阶段
- `policy_engine_update_link_state()` - 更新链路状态
- `policy_engine_select_path()` - 选择最优路径
- `policy_engine_map_client_to_traffic_class()` - 客户端→流量类别映射

---

### 4. xml_config_parser.c - XML 配置解析器

**职责**：
- 解析 XML 配置文件（基于 libxml2）
- 构建内存中的配置数据结构
- 提供配置查询接口

**解析的配置类型**：

1. **系统配置** (`magic_system.xml`)
   ```xml
   <MagicSystem version="1.0">
     <SystemParameters>
       <MaxConcurrentSessions>100</MaxConcurrentSessions>
       <SessionTimeout>3600</SessionTimeout>
     </SystemParameters>
   </MagicSystem>
   ```

2. **链路配置** (`datalink_profiles.xml`)
   ```xml
   <DatalinkProfiles>
     <Datalink id="LINK_Satcom" name="卫星通信">
       <DLM_DriverID>Satcom_DLM</DLM_DriverID>
       <Capabilities>
         <MaxTxRate unit="kbps">432</MaxTxRate>
         <TypicalLatency unit="ms">600</TypicalLatency>
         <CostIndex>80</CostIndex>
       </Capabilities>
     </Datalink>
   </DatalinkProfiles>
   ```

3. **策略规则** (`policy_rules.xml`)
   ```xml
   <PolicyRuleSets>
     <RuleSet phase="CRUISE">
       <Rule>
         <TrafficClass>FLIGHT_CRITICAL</TrafficClass>
         <PathPreferences>
           <Path link_id="LINK_Satcom" ranking="1" action="prefer"/>
           <Path link_id="LINK_LTE" ranking="2" action="prefer"/>
         </PathPreferences>
       </Rule>
     </RuleSet>
   </PolicyRuleSets>
   ```

4. **客户端配置** (`client_profiles.xml`)
   ```xml
   <ClientProfiles>
     <Client id="CLIENT001">
       <Username>pilot001</Username>
       <Password>pass123</Password>
       <SystemRole>FLIGHT_CRITICAL</SystemRole>
       <DefaultProfile>IP_DATA</DefaultProfile>
       <MaxBandwidth>5000</MaxBandwidth>
     </Client>
   </ClientProfiles>
   ```

**关键函数**：
- `magic_config_load_all()` - 加载所有配置文件
- `magic_config_find_datalink()` - 查找链路配置
- `magic_config_find_ruleset()` - 查找策略规则集
- `magic_config_find_client()` - 查找客户端配置
- `magic_config_print_summary()` - 打印配置摘要

---

## 🔗 IPC 协议说明 (ipc_protocol.h)

### Socket 路径
```c
#define MAGIC_CORE_SOCKET_PATH "/tmp/magic_core.sock"
```

### 消息类型
```c
#define MSG_TYPE_REGISTER      0x01  // DLM 注册请求
#define MSG_TYPE_REGISTER_ACK  0x02  // 注册确认
#define MSG_TYPE_LINK_EVENT    0x03  // 链路事件（UP/DOWN）
#define MSG_TYPE_HEARTBEAT     0x04  // 心跳
#define MSG_TYPE_SHUTDOWN      0x05  // 关闭通知
```

### 消息结构

**消息头**：
```c
typedef struct {
    uint8_t  type;       // 消息类型
    uint8_t  version;    // 协议版本
    uint16_t length;     // 消息体长度
    uint32_t sequence;   // 序列号
} IpcHeader;
```

**注册消息**：
```c
typedef struct {
    char     dlm_id[32];              // DLM 标识符
    char     link_profile_id[64];     // 链路配置 ID
    char     iface_name[32];          // 网络接口名
    uint32_t max_bw_kbps;             // 最大带宽
    uint32_t typical_latency_ms;      // 典型延迟
    uint32_t cost_index;              // 成本指数
    uint8_t  priority;                // 优先级
    uint8_t  coverage;                // 覆盖范围
} MsgRegister;
```

**注册确认**：
```c
typedef struct {
    uint8_t  result;        // 0=成功, 非0=错误码
    uint32_t assigned_id;   // 分配的唯一 ID
    char     message[128];  // 描述信息
} MsgRegisterAck;
```

**链路事件**：
```c
typedef struct {
    bool     is_link_up;          // true=UP, false=DOWN
    uint32_t ip_address;          // IP 地址（网络字节序）
    uint32_t current_bw_kbps;     // 当前带宽
    uint32_t current_latency_ms;  // 当前延迟
} MsgLinkEvent;
```

**心跳消息**：
```c
typedef struct {
    uint64_t tx_bytes;     // 发送字节数
    uint64_t rx_bytes;     // 接收字节数
    uint32_t timestamp;    // 时间戳
} MsgHeartbeat;
```

---

## 📊 数据流示意图

```
┌─────────────────┐
│   DLM 模块      │ (LTE_DLM, Satcom_DLM, WiFi_DLM)
│  (独立进程)      │
└────────┬────────┘
         │ Unix Socket (/tmp/magic_core.sock)
         │ MSG_TYPE_REGISTER
         │ MSG_TYPE_LINK_EVENT
         │ MSG_TYPE_HEARTBEAT
         ↓
┌─────────────────┐
│  CM Core Server │ (cm_core_simple.c 或 magic_core_main.c)
│                 │ - 接收 DLM 连接
│  g_clients[]    │ - 管理链路状态
│                 │ - 转发状态到策略引擎
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  Policy Engine  │ (policy_engine.c)
│                 │ - 飞行阶段管理
│  link_states[]  │ - 路径选择算法
│  active_ruleset │ - 流量分类
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│ CIC Module      │ (app_magic_cic.fdx)
│ (freeDiameter)  │ - MCAR/MCCR 消息处理
│                 │ - 调用策略引擎
└─────────────────┘
```

---

## 🚀 启动流程

### CM Core 简化版 (cm_core_simple.c)
```
1. 初始化信号处理器 (SIGINT, SIGTERM)
2. 创建 Unix Domain Socket
3. 绑定到 /tmp/magic_core.sock
4. listen() 监听连接
5. 启动状态打印线程 (每30秒)
6. 进入 accept() 循环：
   - 接受 DLM 连接
   - 为每个 DLM 创建独立线程
   - 线程处理注册、事件、心跳消息
7. 接收 Ctrl+C 信号后优雅退出
```

### MAGIC Core 主程序 (magic_core_main.c)
```
1. 初始化信号处理器
2. 加载 XML 配置文件：
   - magic_system.xml
   - datalink_profiles.xml
   - policy_rules.xml
   - client_profiles.xml
3. 打印配置摘要
4. 启动 CM Core 服务器 (与 DLM 通信)
5. 进入 accept() 循环（与简化版类似）
6. 关联 DLM 与链路配置
7. 标记链路活动状态
```

---

## 🔍 调试技巧

### 1. 查看 DLM 注册信息
```
[CM CORE] ✓ DLM Registered:
    DLM ID:          Satcom_DLM
    Link Profile:    LINK_Satcom
    Interface:       eth0
    Assigned ID:     1000
    Max Bandwidth:   432 kbps
    Latency:         600 ms
```

### 2. 监控链路状态变化
```
[CM CORE] Link Event from Satcom_DLM: UP ✓
    IP:        192.168.1.100
    Bandwidth: 432 kbps
    Latency:   600 ms
```

### 3. 查看活跃链路列表
```
========================================
 Active Links: 2
========================================
 [1000] Satcom_DLM (eth0) - UP
 [1001] LTE_DLM (wlan0) - DOWN
========================================
```

### 4. 策略引擎路径选择日志
```
[POLICY] ========================================
[POLICY]  Path Selection Decision
[POLICY] ========================================
[POLICY]   Traffic Class: FLIGHT_CRITICAL
[POLICY]   Evaluated Paths: 3
[POLICY]     [1] LINK_Satcom (rank 1): Available (score: 12500) ← SELECTED ✓
[POLICY]     [2] LINK_LTE (rank 2): UNAVAILABLE ✗
[POLICY]     [3] LINK_WiFi (rank 3): PROHIBIT ✗
```

---

## 📝 编译命令

```bash
# 编译 CM Core 简化版
gcc -o cm_core_simple cm_core_simple.c dlm_common.c -lpthread

# 编译 MAGIC Core 主程序
gcc -o magic_core_main magic_core_main.c xml_config_parser.c dlm_common.c \
    -lpthread -lxml2 -I/usr/include/libxml2

# 编译策略引擎测试
gcc -o test_policy_engine test_policy_engine.c policy_engine.c \
    xml_config_parser.c -lxml2 -I/usr/include/libxml2
```

---

## 🧪 测试方法

### 1. 启动 CM Core
```bash
cd /home/zhuwuhui/freeDiameter/magic_server
./cm_core_simple
# 或
./magic_core_main
```

### 2. 启动 DLM 模块
```bash
cd /home/zhuwuhui/freeDiameter/link_simulator
./satcom_dlm
./lte_dlm
```

### 3. 观察日志
- CM Core 会显示 DLM 注册信息
- DLM 会定期发送心跳和链路事件
- 每30秒打印活跃链路列表

---

## 📚 相关文档

- `PC2地面服务器详细设计说明.md` - 服务器架构设计
- `network_connectivity_test_guide.md` - 网络测试指南
- `TESTING_GUIDE.md` - 集成测试指南
- `cross_platform_distributed_deployment_guide.md` - 部署指南

---

**最后更新**: 2025-11-25
**维护人员**: MAGIC 开发团队
