# MAGIC DLM/CM 系统

**版本**: 1.0  
**状态**: ✅ 验证通过  
**日期**: 2024-11-24

---

## 🎯 项目简介

MAGIC DLM/CM 系统是一个基于 Unix Domain Socket 的**数据链路管理（Data Link Manager, DLM）**与**连接管理核心（Connection Management Core, CM Core）**通信框架。

该系统用于航空通信场景中的多链路管理，支持：
- 🛰️ 卫星链路（SATCOM）
- 📱 蜂窝链路（CELLULAR）
- 📡 WiFi 链路

### 关键特性

- **进程隔离**: 每个 DLM 作为独立进程运行，故障隔离
- **实时监控**: 3秒检测周期，快速响应链路状态变化
- **动态注册**: DLM 启动时自动向 CM Core 注册
- **事件驱动**: 链路 UP/DOWN 事件实时通知
- **心跳保活**: 10秒心跳检测，自动识别 DLM 异常

---

## 📂 项目结构

```
freeDiameter/
├── magic_server/
│   ├── ipc_protocol.h         # IPC 通信协议定义
│   ├── dlm_common.h            # DLM 通用头文件
│   ├── dlm_common.c            # DLM 通用实现
│   └── cm_core_simple.c        # CM Core 服务器
├── DLM_SATCOM/
│   └── dlm_satcom_main.c       # SATCOM DLM 主程序
├── DLM_CELLULAR/
│   └── dlm_cellular_main.c     # CELLULAR DLM 主程序
├── DLM_WIFI/
│   └── dlm_wifi_main.c         # WIFI DLM 主程序
├── docs/
│   ├── DLM使用说明             # 用户使用手册
│   ├── DLM_CM_SYSTEM_DESIGN.md # 详细设计文档
│   └── DLM_VERIFICATION_REPORT.md # 验证报告
├── Makefile.magic              # 编译配置
├── setup_virtual_interfaces.sh # 虚拟网卡设置脚本
└── magic_control.sh            # 系统控制脚本 ⭐
```

---

## 🚀 快速开始

### 1. 环境准备

**系统要求**:
- Linux (内核 >= 3.10)
- GCC >= 4.8
- Root 权限（用于创建虚拟网卡）

**创建虚拟网卡** (开发环境):
```bash
sudo ./setup_virtual_interfaces.sh
```

这将创建三个虚拟网卡：
- `eth1`: 192.168.1.10/24 (SATCOM)
- `eth2`: 192.168.2.10/24 (CELLULAR)
- `eth3`: 192.168.3.10/24 (WIFI)

### 2. 编译

```bash
make -f Makefile.magic clean
make -f Makefile.magic all
```

生成的可执行文件：
- `cm_core` - CM Core 服务器
- `dlm_satcom` - SATCOM DLM
- `dlm_cellular` - CELLULAR DLM
- `dlm_wifi` - WIFI DLM

### 3. 启动系统

**方式 1: 使用控制脚本（推荐）**

```bash
# 启动所有进程
./magic_control.sh start

# 查看系统状态
./magic_control.sh status

# 查看日志
./magic_control.sh logs cm_core

# 停止所有进程
./magic_control.sh stop
```

**方式 2: 手动启动**

```bash
# 终端 1: CM Core
./cm_core > logs/cm_core.log 2>&1 &

# 终端 2-4: DLM
./dlm_satcom > logs/dlm_satcom.log 2>&1 &
./dlm_cellular > logs/dlm_cellular.log 2>&1 &
./dlm_wifi > logs/dlm_wifi.log 2>&1 &
```

### 4. 测试链路状态变化

```bash
# 使 SATCOM 链路下线
sudo ip link set eth1 down

# 观察日志（应该看到 LINK_DOWN 事件）
tail -f logs/cm_core.log

# 恢复 SATCOM 链路
sudo ip link set eth1 up

# 观察日志（应该看到 LINK_UP 事件）
```

---

## 📊 系统架构

```
┌─────────────────────────────────────────────────┐
│                  CM Core                        │
│  (Unix Socket Server: /tmp/magic_core.sock)    │
│                                                 │
│  - 接受 DLM 注册                                │
│  - 跟踪活动链路                                 │
│  - 处理链路事件                                 │
│  - 心跳检测                                     │
└────────┬───────────┬────────────┬───────────────┘
         │           │            │
    Unix Socket IPC (二进制协议)
         │           │            │
┌────────┴────┐ ┌───┴──────┐ ┌───┴─────────┐
│ DLM_SATCOM  │ │ DLM_     │ │ DLM_WIFI    │
│             │ │ CELLULAR │ │             │
│ 监控: eth1  │ │          │ │ 监控: eth3  │
│ 2Mbps/600ms │ │ 监控:    │ │100Mbps/5ms  │
│ cost=90     │ │ eth2     │ │ cost=1      │
│             │ │ 20Mbps/  │ │             │
└─────────────┘ │ 50ms     │ └─────────────┘
                │ cost=20  │
                │          │
                └──────────┘
```

### IPC 消息类型

| 消息类型          | 方向       | 说明                     |
|-------------------|------------|--------------------------|
| MSG_TYPE_REGISTER | DLM → CM   | DLM 注册请求             |
| MSG_TYPE_REGISTER_ACK | CM → DLM | 注册确认，分配 Link ID |
| MSG_TYPE_LINK_EVENT | DLM → CM | 链路状态变化（UP/DOWN）|
| MSG_TYPE_HEARTBEAT | DLM → CM  | 心跳保活（10秒）         |
| MSG_TYPE_SHUTDOWN | DLM → CM   | 关闭通知                 |

---

## 🔍 验证结果

### ✅ 功能验证

| 功能                 | 状态 | 说明                          |
|----------------------|------|-------------------------------|
| DLM 注册             | ✅   | 三个 DLM 成功注册，分配 ID    |
| 链路 UP 检测         | ✅   | 正确检测并上报 IP、带宽       |
| 链路 DOWN 检测       | ✅   | 实时检测网卡下线事件          |
| 链路恢复             | ✅   | DOWN → UP 正确上报            |
| 心跳机制             | ✅   | 10秒心跳正常                  |
| 进程清理             | ✅   | 优雅关闭，无资源泄漏          |

### 📈 性能指标

| 指标               | 目标值   | 实测值    |
|--------------------|----------|-----------|
| 注册延迟           | < 100ms  | < 50ms    |
| 事件检测周期       | 3s       | 3s        |
| CPU 占用 (per DLM) | < 1%     | ~0.1%     |
| 内存占用 (per DLM) | < 5MB    | ~2MB      |

**详细验证报告**: 见 `docs/DLM_VERIFICATION_REPORT.md`

---

## 📝 使用示例

### 示例 1: 查看系统状态

```bash
$ ./magic_control.sh status

=========================================
  MAGIC DLM/CM System Status
=========================================

Processes:
  ./cm_core       PID: 21012  CPU: 0.0% MEM: 0.0%
  ./dlm_satcom    PID: 21028  CPU: 0.0% MEM: 0.0%
  ./dlm_cellular  PID: 21040  CPU: 0.0% MEM: 0.0%
  ./dlm_wifi      PID: 21045  CPU: 0.0% MEM: 0.0%

Network Interfaces:
  eth1: UP
  eth2: UP
  eth3: UP

Socket:
srwxr-xr-x 1 root root 0 Nov 24 13:50 /tmp/magic_core.sock
=========================================
```

### 示例 2: 观察链路切换

```bash
# 终端 1: 监控 CM Core 日志
$ ./magic_control.sh logs cm_core

# 终端 2: 触发链路切换
$ sudo ip link set eth1 down

# 终端 1 输出:
[CM CORE] Link Event from DLM_SATCOM: DOWN ✗

$ sudo ip link set eth1 up

# 终端 1 输出:
[CM CORE] Link Event from DLM_SATCOM: UP ✓
    IP:        192.168.1.10
    Bandwidth: 2048 kbps
    Latency:   600 ms
```

---

## 🛠️ 配置说明

### 链路配置参数

每个 DLM 的配置位于对应的主程序中（例如 `DLM_SATCOM/dlm_satcom_main.c`）：

```c
MsgRegister config = {
    .dlm_id = "DLM_SATCOM",           // 唯一标识
    .link_profile_id = "LINK_SATCOM", // 链路类型
    .iface_name = "eth1",             // 监控的网卡
    .cost_index = 90,                 // 成本指数 (1-100)
    .max_bw_kbps = 2048,              // 最大带宽 (kbps)
    .typical_latency_ms = 600,        // 典型延迟 (ms)
    .priority = 5,                    // 优先级 (1-10)
    .coverage = 1                     // 覆盖范围
};
```

### 生产环境配置

如需监控真实网卡（例如 `eth0`），修改配置：

```c
.iface_name = "eth0",  // 改为实际网卡名称
```

重新编译后即可使用。

---

## 📖 文档

| 文档                                  | 说明                     |
|---------------------------------------|--------------------------|
| `docs/DLM使用说明`                    | 详细使用指南             |
| `docs/DLM_CM_SYSTEM_DESIGN.md`       | 系统设计文档（1300行）   |
| `docs/DLM_VERIFICATION_REPORT.md`    | 功能验证报告             |

---

## 🔧 故障排查

### 问题 1: DLM 无法连接 CM Core

**症状**: `Failed to connect: No such file or directory`

**解决方案**:
```bash
# 确保 CM Core 先启动
./magic_control.sh start

# 或检查 socket 文件
ls -l /tmp/magic_core.sock
```

### 问题 2: 看不到日志输出

**原因**: 输出缓冲问题

**解决方案**: 代码中已添加 `setbuf(stdout, NULL)` 禁用缓冲，确保使用最新编译的版本

### 问题 3: 虚拟网卡不存在

**症状**: `Cannot find device eth1`

**解决方案**:
```bash
# 创建虚拟网卡
sudo ./setup_virtual_interfaces.sh

# 验证
ip link show eth1 eth2 eth3
```

---

## 🚦 下一步计划

### Phase 2: 策略引擎
- 链路选择算法（成本优先/延迟优先/带宽优先）
- 链路质量评分机制
- 多链路负载均衡

### Phase 3: QoS 控制
- 使用 `tc` 动态调整带宽
- 使用 `tc-netem` 模拟延迟和丢包
- 流量整形和优先级队列

### Phase 4: 路由集成
- 根据链路状态更新路由表
- 主备链路自动切换
- ECMP 多路径路由

### Phase 5: Diameter 集成
- 与 MAGIC Diameter Server 集成
- CDR 记录包含链路信息
- 基于链路状态的 Diameter 路由决策

---

## 📄 许可证

[待补充]

---

## 👥 贡献者

- 系统架构设计
- DLM 通用框架实现
- CM Core 服务器实现
- 文档编写

---

## 📞 支持

如有问题，请查看：
1. `docs/DLM使用说明` - 详细使用指南
2. `docs/DLM_CM_SYSTEM_DESIGN.md` - 系统设计文档
3. `docs/DLM_VERIFICATION_REPORT.md` - 验证报告

---

**最后更新**: 2024-11-24  
**系统状态**: ✅ 生产就绪
