# MAGIC LMI/DLM 架构设计文档

## 概述

本设计实现了 ARINC 839-2014 MAGIC 规范中的 **Link Management Interface (LMI)** 和 **Data Link Module (DLM)** 架构。

### 核心概念

- **LMI (Link Management Interface)**: 定义了中央管理模块(CM)和数据链路模块(DLM)之间的标准接口，实现介质无关的链路管理
- **DLM (Data Link Module)**: 具体的硬件驱动实现，将 LMI 的通用指令翻译为特定硬件的私有协议

## 架构层次

```
┌─────────────────────────────────────────────────────────────┐
│          Client Interface Controller (CIC)                  │
│         (Diameter MAGIC Commands: MCAR/MCCR/MNTR...)        │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────────┐
│         Central Management (CM) - Policy Engine             │
│  • Link Selection (Policy Based Routing)                    │
│  • Resource Allocation                                       │
│  • Handover Decision                                         │
└────────────────────┬────────────────────────────────────────┘
                     │
         ┌───────────┼───────────┐  ◄─── LMI (标准接口)
         │           │           │
    ┌────▼────┐ ┌───▼────┐ ┌───▼────┐
    │ SATCOM  │ │CELLULAR│ │  WIFI  │  ◄─── DLM (驱动实现)
    │   DLM   │ │  DLM   │ │  DLM   │
    └────┬────┘ └────┬───┘ └────┬───┘
         │           │           │
    ┌────▼────┐ ┌───▼────┐ ┌───▼────┐
    │  eth1   │ │  eth2  │ │  eth3  │  ◄─── 物理网卡
    │ (卫星)  │ │ (4G/5G)│ │ (WiFi) │
    └─────────┘ └────────┘ └────────┘
```

## 文件结构

### LMI 核心文件

```
magic_server/lmi/
├── magic_lmi.h              # LMI 接口定义（头文件）
│   ├── 基础类型定义（link_id, bandwidth, latency）
│   ├── 枚举类型（link_type, link_state, event_type）
│   ├── 数据结构（capability, policy, request/response）
│   ├── 回调函数类型（event_callback, log_callback）
│   └── lmi_operations_t 操作接口（10个函数指针）
│
├── magic_lmi_utils.c        # LMI 辅助函数实现
│   ├── 枚举转字符串函数
│   ├── 会话 ID 生成器
│   └── 时间戳获取函数
│
└── lmi_test_example.c       # 完整测试示例程序
    ├── 回调函数实现
    ├── CM 模拟函数
    └── 主测试流程
```

### DLM 实现文件

```
DLM_SATCOM/
└── dlm_satcom.c             # 卫星链路 DLM
    ├── AT 指令接口（模拟）
    ├── PDP Context 管理
    ├── 信号质量监控
    └── 导出: dlm_satcom_ops

DLM_CELLULAR/
└── dlm_cellular.c           # 蜂窝网络 DLM
    ├── LTE/5G 连接管理
    ├── 基站切换检测
    └── 导出: dlm_cellular_ops

DLM_WIFI/
└── dlm_wifi.c               # WiFi DLM
    ├── WiFi 网络扫描
    ├── 停机位检测
    └── 导出: dlm_wifi_ops
```

### 配置文件

```
magic_server/config/
└── Datalink_Profile.xml     # 链路配置
    ├── LINK_SATCOM (eth1)   - Inmarsat Global Xpress
    ├── LINK_CELLULAR (eth2) - 4G/5G ATG Network
    └── LINK_WIFI (eth3)     - Airport Gatelink
```

## LMI 接口详解

### lmi_operations_t 函数表

每个 DLM 必须实现以下 10 个标准接口：

| 函数名 | ARINC 839 原语 | 功能描述 |
|--------|----------------|----------|
| `init()` | - | 初始化 DLM，设置回调函数 |
| `register_link()` | Link_Register | 向 CM 注册链路信息 |
| `discover_capability()` | Link_Capability_Discover | 查询链路能力参数 |
| `request_resource()` | Link_Resource | 资源分配/修改/释放（核心接口） |
| `get_state()` | Link_Get_Parameters | 获取当前链路状态 |
| `get_statistics()` | - | 查询流量统计信息 |
| `suspend_link()` | Link_Action (SUSPEND) | 暂停链路（保留资源） |
| `resume_link()` | Link_Action (RESUME) | 恢复链路 |
| `health_check()` | - | 健康检查（周期性调用） |
| `shutdown()` | - | 清理并关闭 DLM |

### 关键数据流

#### 1. 链路注册流程

```
DLM.init()
    ↓
填充 lmi_link_info_t 结构
    ↓
DLM.register_link(&link_info)
    ↓
CM 保存链路到全局注册表
```

#### 2. 资源分配流程

```
Client 发起 MCCR (Communication Change Request)
    ↓
CM 根据 Policy 选择最佳链路
    ↓
构造 lmi_resource_request_t
    ↓
DLM.request_resource(ALLOCATE, request, response)
    ↓
DLM 内部:
  - 检查状态
  - 激活硬件连接（AT 指令 / API 调用）
  - 状态切换: AVAILABLE → ACTIVATING → ACTIVE
    ↓
返回 lmi_resource_response_t（分配的 IP/带宽）
    ↓
DLM 通过 event_callback 发送 LINK_UP 事件
    ↓
CM 配置路由表，将流量导向该链路
```

#### 3. 异步事件处理

```
DLM 监控线程定期检测硬件状态
    ↓
发现异常（信号弱 / 离开覆盖区）
    ↓
构造 lmi_link_event_t
    ↓
调用 event_callback() 通知 CM
    ↓
CM 评估策略：
  - 是否需要切换链路？
  - 新链路是否可用？
    ↓
执行 Handover:
  1. 在新链路上分配资源
  2. 更新路由表
  3. 释放旧链路资源
```

## 三个 DLM 特性对比

| 特性 | SATCOM DLM | CELLULAR DLM | WIFI DLM |
|------|------------|--------------|----------|
| **物理接口** | eth1 | eth2 | eth3 |
| **硬件类型** | Inmarsat 卫星终端 | 4G/5G ATG 模块 | WiFi AP |
| **最大带宽** | 2 Mbps | 20 Mbps | 100 Mbps |
| **典型时延** | 600 ms | 50 ms | 5 ms |
| **覆盖范围** | 全球 | 陆地航线 | 仅停机位 |
| **成本指数** | 90 (最高) | 20 (中等) | 1 (最低) |
| **安全等级** | HIGH | MEDIUM | LOW |
| **优先级** | 8 | 5 | 10 (成本优先) |
| **控制接口** | AT 指令 (串口) | ModemManager / AT | wpa_supplicant / nmcli |
| **激活时间** | 3-5 秒 | 2 秒 | 1 秒 |
| **特殊监控** | 信号强度 (dBm) | RSRP/SINR, 小区切换 | 停机位检测 |

## 链路选择策略示例

CM 根据以下规则选择链路（可配置）：

### 策略 1: 成本优先（默认）

```
IF (在停机位) THEN
    选择 WIFI (cost=1)
ELSE IF (在陆地上空) THEN
    选择 CELLULAR (cost=20)
ELSE (在海洋上空)
    选择 SATCOM (cost=90)
END IF
```

### 策略 2: 时延优先（实时应用）

```
IF (带宽需求 < 5 Mbps AND 时延要求 < 100ms) THEN
    优先: WIFI > CELLULAR > SATCOM
END IF
```

### 策略 3: 安全优先（敏感数据）

```
IF (client.security_level == HIGH) THEN
    必须使用 SATCOM (IPsec + 端到端加密)
END IF
```

## 编译和测试

### 编译命令

```bash
# 编译 LMI 工具库
gcc -c magic_lmi_utils.c -o magic_lmi_utils.o

# 编译 DLM 驱动
gcc -c dlm_satcom.c -pthread -o dlm_satcom.o
gcc -c dlm_cellular.c -pthread -o dlm_cellular.o
gcc -c dlm_wifi.c -pthread -o dlm_wifi.o

# 编译测试程序
gcc lmi_test_example.c \
    magic_lmi_utils.o \
    dlm_satcom.o \
    dlm_cellular.o \
    dlm_wifi.o \
    -pthread -o lmi_test

# 运行测试
./lmi_test
```

### 预期输出

```
========================================
  MAGIC LMI/DLM Test Example
========================================

=== Step 1: Initialize DLMs ===
[SATCOM] SATCOM DLM initialized with config: (null)
[CELLULAR] CELLULAR DLM initialized
[WIFI] WIFI DLM initialized
[CM] Registered link: LINK_SATCOM (Inmarsat Global Xpress) - Type: SATELLITE
      Capability: Tx=2048 kbps, Latency=600 ms
      Policy: Cost=90, Security=2, Priority=8
[CM] Registered link: LINK_CELLULAR (4G/5G AGT Network) - Type: CELLULAR
      ...
[CM] Total 3 links registered

[CM] Policy decision: Select LINK_WIFI (Priority=10, Cost=1)

=== Step 2: Allocate Resource on LINK_WIFI ===
[WIFI] Resource ALLOCATE: session=1
[WIFI] Connecting to airport WiFi: Airport_Gate_WiFi
[WIFI] WiFi connected successfully

[EVENT] Link: LINK_WIFI, Type: LINK_UP, State: AVAILABLE -> ACTIVE
[EVENT] Message: Airport WiFi connected
[CM] Link LINK_WIFI is now ACTIVE, updating routing table...
[CM] Resource allocated successfully!
     Session ID: 1
     Granted Tx: 2048 kbps, Rx: 2048 kbps
     Local IP: 172.16.50.100
     ...
```

## 与 MAGIC 系统其他模块的集成

### 1. 与 CIC (Client Interface Controller) 集成

```c
/* 在 cmd_mccr() 处理函数中 */
int cmd_mccr(int argc, char *argv[]) {
    // 1. 解析客户端请求
    parse_mccr_request();
    
    // 2. 调用 CM 选择链路
    registered_link_t *best_link = cm_select_best_link();
    
    // 3. 通过 LMI 分配资源
    lmi_resource_request_t req = { ... };
    lmi_resource_response_t resp;
    best_link->ops->request_resource(link_id, &req, &resp);
    
    // 4. 配置网络模块
    network_module_add_route(resp.local_ip, best_link->interface);
    
    // 5. 返回 MCCA 应答给客户端
    send_mcca_response(resp);
}
```

### 2. 与 Network Management 集成

```c
/* LMI 事件回调中触发网络配置 */
void on_link_event(const lmi_link_event_t *event, void *user_data) {
    if (event->event_type == LMI_EVENT_LINK_UP) {
        // 配置路由表
        system("ip route add default via 10.20.30.1 dev eth1");
        
        // 配置 QoS
        system("tc qdisc add dev eth1 root tbf rate 2mbit ...");
        
        // 配置 NAT
        system("iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE");
    }
}
```

### 3. 与 Accounting 集成

```c
/* 资源分配成功后记录 CDR */
if (response.result_code == LMI_SUCCESS) {
    cdr_record_t cdr = {
        .session_id = response.session_id,
        .client_id = request.client_id,
        .link_id = link_id,
        .start_time = time(NULL),
        .allocated_tx = response.granted_tx_rate,
        .allocated_rx = response.granted_rx_rate,
    };
    
    accounting_create_cdr(&cdr);
}
```

## 扩展性设计

### 添加新的 DLM (例如: 5G NTN 卫星)

1. 创建 `dlm_5g_ntn.c`
2. 实现 `lmi_operations_t` 的 10 个接口
3. 在 `Datalink_Profile.xml` 添加配置
4. 在 CM 初始化时注册：
   ```c
   extern lmi_operations_t dlm_5g_ntn_ops;
   dlm_5g_ntn_ops.init(...);
   dlm_5g_ntn_ops.register_link(&link_info);
   ```

### 自定义 Policy Engine

修改 `cm_select_best_link()` 函数，支持复杂策略：

```c
registered_link_t* cm_select_best_link_advanced(
    const client_profile_t *client,
    const central_policy_t *policy) 
{
    // 1. 过滤: 只考虑满足最低带宽要求的链路
    // 2. 评分: cost_score = cost_index * weight_cost
    //          latency_score = typical_latency * weight_latency
    // 3. 排序: 选择综合评分最低的链路
    // 4. 备份: 如果首选链路失败，自动切换到次优链路
}
```

## 故障处理

### DLM 崩溃

- CM 通过 `health_check()` 定期检测
- 如果连续 3 次失败，标记链路为 ERROR 状态
- 自动切换到其他可用链路

### 硬件故障

- DLM 发送 `LMI_EVENT_ERROR` 事件
- CM 记录日志，触发告警
- 尝试重启 DLM 或禁用该链路

### 链路质量下降

- DLM 发送 `LMI_EVENT_CAPABILITY_CHANGE` 事件
- CM 根据策略决定是否降低客户端的分配带宽
- 如果低于最低要求，触发 Handover

## 性能优化建议

1. **并发处理**: 使用线程池处理多个客户端的资源请求
2. **缓存优化**: 缓存 `discover_capability()` 结果，减少查询次数
3. **事件聚合**: 批量处理低优先级事件，减少回调次数
4. **预激活**: 提前激活备用链路，加速 Handover

## 安全考虑

1. **访问控制**: CM 验证 DLM 的调用权限（通过 Unix socket + credentials）
2. **数据加密**: 敏感配置（如密码）存储时加密
3. **审计日志**: 所有资源操作记录到 syslog
4. **沙箱隔离**: DLM 以低权限用户运行，限制系统调用

## 参考文档

- ARINC 839-2014 Section 4.2: Link Management Interface
- ARINC 839-2014 Attachment 2: Common Link Interface Specification
- IEEE 802.21: Media Independent Handover Services
- RFC 6733: Diameter Base Protocol
- Linux Network Administration: `iproute2`, `tc`, `iptables`

## 联系方式

如有问题，请参考 `magic_server/README.md` 或联系开发团队。
