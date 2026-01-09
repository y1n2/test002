# MAGIC DLM/CM 独立进程架构设计文档

## 1. 架构概述

本设计采用**独立进程 + Unix Domain Socket IPC** 架构，实现 MAGIC 系统的链路管理功能。

### 1.1 核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│              (MAGIC Client, Diameter Server)                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                 CM Core Server (服务器)                      │
│  - 监听 /tmp/magic_cm.sock                                  │
│  - 接收 DLM 注册                                            │
│  - 维护 Active_Links_List                                   │
│  - 链路选择与资源分配                                        │
└──────────┬──────────────┬──────────────┬───────────────────┘
           │              │              │
           │ Unix Socket  │              │
           ▼              ▼              ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ DLM SATCOM   │  │ DLM CELLULAR │  │  DLM WIFI    │
│  (客户端)    │  │  (客户端)    │  │  (客户端)    │
│              │  │              │  │              │
│ Monitor:     │  │ Monitor:     │  │ Monitor:     │
│   eth1       │  │   eth2       │  │   eth3       │
└──────┬───────┘  └──────┬───────┘  └──────┬───────┘
       │                 │                 │
       ▼                 ▼                 ▼
  ┌─────────┐      ┌─────────┐      ┌─────────┐
  │  eth1   │      │  eth2   │      │  eth3   │
  │ SATCOM  │      │CELLULAR │      │  WIFI   │
  └─────────┘      └─────────┘      └─────────┘
```

### 1.2 进程职责

| 进程名称 | 角色 | 监控网卡 | 主要职责 |
|---------|------|---------|---------|
| **cm_core_server** | 服务器 | - | 监听 Socket，接收 DLM 注册，维护链路列表，链路选择策略 |
| **dlm_satcom_daemon** | 客户端 | eth1 | 监控卫星链路状态，发送心跳，报告状态变化 |
| **dlm_cellular_daemon** | 客户端 | eth2 | 监控蜂窝网络状态，发送心跳，报告状态变化 |
| **dlm_wifi_daemon** | 客户端 | eth3 | 监控 WiFi 状态，发送心跳，报告状态变化 |

## 2. IPC 通信协议

### 2.1 Socket 配置

- **路径**: `/tmp/magic_cm.sock`
- **类型**: `AF_UNIX` + `SOCK_STREAM`
- **模式**: CM Core 为服务器，所有 DLM 为客户端

### 2.2 消息类型

#### 注册与生命周期
- `MSG_REGISTER_REQUEST` (0x0001): DLM → CM 注册链路
- `MSG_REGISTER_RESPONSE` (0x0002): CM → DLM 确认注册
- `MSG_HEARTBEAT` (0x0003): DLM → CM 心跳
- `MSG_HEARTBEAT_ACK` (0x0004): CM → DLM 心跳确认
- `MSG_UNREGISTER` (0x0005): DLM → CM 注销

#### 链路状态通知
- `MSG_LINK_UP` (0x0010): 链路激活
- `MSG_LINK_DOWN` (0x0011): 链路断开
- `MSG_LINK_DEGRADED` (0x0012): 链路质量下降
- `MSG_LINK_RESTORED` (0x0013): 链路质量恢复

#### 资源管理
- `MSG_ALLOCATE_REQUEST` (0x0020): CM → DLM 分配带宽
- `MSG_ALLOCATE_RESPONSE` (0x0021): DLM → CM 分配结果
- `MSG_RELEASE_REQUEST` (0x0022): CM → DLM 释放资源
- `MSG_STATS_REQUEST` (0x0030): CM → DLM 查询统计

### 2.3 消息格式示例

**REGISTER_REQUEST**:
```c
typedef struct {
    ipc_msg_header_t    header;
    char                link_name[64];       // "Inmarsat_GX_SATCOM"
    char                interface_name[16];   // "eth1"
    uint8_t             link_type;           // LINK_TYPE_SATCOM
    uint32_t            max_bandwidth_kbps;  // 2000
    uint32_t            latency_ms;          // 600
    uint32_t            cost_per_mb;         // 90
    uint8_t             priority;            // 5
    pid_t               dlm_pid;
} ipc_register_req_t;
```

**LINK_UP**:
```c
typedef struct {
    ipc_msg_header_t    header;
    uint8_t             new_state;           // LINK_STATE_ACTIVE
    uint32_t            ip_address;          // 网卡 IP
    uint32_t            current_bandwidth_kbps;
    int32_t             signal_strength_dbm; // 信号强度
    char                status_message[128];
} ipc_link_status_t;
```

## 3. DLM 实现细节

### 3.1 DLM 启动流程

```
1. 初始化上下文
   - 设置 interface_name (eth1/eth2/eth3)
   - 初始化互斥锁
   
2. 连接 CM Core
   - socket(AF_UNIX, SOCK_STREAM, 0)
   - connect("/tmp/magic_cm.sock")
   - 重试机制（最多 5 次，每次间隔 2 秒）
   
3. 发送注册请求
   - 填充 link_name, interface_name, link_type
   - 发送 max_bandwidth, latency, cost, priority
   - 等待 REGISTER_RESPONSE
   - 保存分配的 link_id
   
4. 启动工作线程
   - Heartbeat 线程：每 5 秒发送心跳
   - Monitor 线程：每 3 秒检查网卡状态
   
5. 进入运行循环
   - 主线程等待 SIGINT/SIGTERM
   
6. 优雅退出
   - 发送 UNREGISTER 消息
   - 等待线程结束
   - 关闭 socket
```

### 3.2 网卡状态监控

每个 DLM 使用 **ioctl(SIOCGIFFLAGS)** 检查网卡状态：

```c
bool dlm_check_interface_up(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq ifr;
    strncpy(ifr.ifr_name, "eth1", IFNAMSIZ - 1);
    
    ioctl(sock, SIOCGIFFLAGS, &ifr);
    close(sock);
    
    // 检查 UP 和 RUNNING 标志
    return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
}
```

### 3.3 网卡统计信息读取

从 `/sys/class/net/<iface>/statistics/` 读取：

```bash
/sys/class/net/eth1/statistics/tx_bytes
/sys/class/net/eth1/statistics/rx_bytes
/sys/class/net/eth1/statistics/tx_packets
/sys/class/net/eth1/statistics/rx_packets
/sys/class/net/eth1/statistics/tx_errors
```

## 4. CM Core 实现细节

### 4.1 链路数据结构

```c
typedef struct {
    bool                    active;
    uint32_t                link_id;
    int                     client_fd;      // DLM socket
    pid_t                   dlm_pid;
    
    char                    link_name[64];
    char                    interface_name[16];
    ipc_link_type_t         link_type;
    
    uint32_t                max_bandwidth_kbps;
    uint32_t                latency_ms;
    uint32_t                cost_per_mb;
    uint8_t                 priority;
    
    ipc_link_state_t        current_state;
    time_t                  last_heartbeat;
    uint32_t                heartbeat_miss_count;
    
    uint64_t                tx_bytes;
    uint64_t                rx_bytes;
} registered_link_t;
```

### 4.2 线程模型

```
Accept Thread:
  - accept() 新的 DLM 连接
  - 为每个连接创建 client_thread
  
Client Thread (每个 DLM 一个):
  - recv() 接收 DLM 消息
  - 处理 REGISTER, HEARTBEAT, LINK_STATUS
  
Monitor Thread:
  - 每 10 秒检查心跳超时
  - 超时 30 秒标记为 UNAVAILABLE
```

### 4.3 心跳超时检测

```c
void cm_monitor_thread_func(void)
{
    while (running) {
        sleep(10);
        
        time_t now = time(NULL);
        for (each link) {
            if ((now - link->last_heartbeat) > 30) {
                link->heartbeat_miss_count++;
                
                if (link->heartbeat_miss_count >= 3) {
                    link->current_state = LINK_STATE_UNAVAILABLE;
                }
            }
        }
    }
}
```

## 5. 编译与运行

### 5.1 编译所有模块

```bash
cd /home/zhuwuhui/freeDiameter
make -f Makefile.dlm all
```

生成的可执行文件：
- `cm_core_server` (CM Core 服务器)
- `dlm_satcom_daemon` (SATCOM DLM)
- `dlm_cellular_daemon` (CELLULAR DLM)
- `dlm_wifi_daemon` (WIFI DLM)

### 5.2 启动系统

```bash
# 方式 1: 使用启动脚本（推荐）
chmod +x startup_magic_system.sh
./startup_magic_system.sh

# 方式 2: 手动启动
./cm_core_server &
sleep 2
./dlm_satcom_daemon &
./dlm_cellular_daemon &
./dlm_wifi_daemon &
```

### 5.3 测试网卡状态变化

```bash
# 需要 root 权限
chmod +x test_network_interfaces.sh
sudo ./test_network_interfaces.sh
```

此脚本将：
1. 检查 eth1/eth2/eth3 是否存在
2. 如果不存在，自动创建虚拟网卡（dummy）
3. 按顺序测试网卡 UP/DOWN，观察 CM 和 DLM 的日志输出

### 5.4 停止系统

```bash
chmod +x shutdown_magic_system.sh
./shutdown_magic_system.sh
```

### 5.5 查看日志

```bash
# CM Core 日志
tail -f logs/cm_core.log

# DLM 日志
tail -f logs/dlm_satcom.log
tail -f logs/dlm_cellular.log
tail -f logs/dlm_wifi.log
```

## 6. 测试场景

### 6.1 场景 1: 正常注册

**预期行为**:
1. 启动 CM Core，监听 socket
2. 启动 DLM，连接成功
3. CM 收到 REGISTER_REQUEST
4. CM 分配 link_id (如 1000, 1001, 1002)
5. CM 返回 REGISTER_RESPONSE
6. DLM 开始发送心跳

**验证**:
```bash
# CM Core 日志应显示：
[CM] ✓ DLM registered successfully:
     Link ID:     1000
     Name:        Inmarsat_GX_SATCOM
     Type:        SATCOM
     Interface:   eth1
```

### 6.2 场景 2: 网卡状态变化

**操作**:
```bash
# 启用 eth1
sudo ip link set eth1 up

# 禁用 eth1
sudo ip link set eth1 down
```

**预期行为**:
1. DLM Monitor 线程检测到状态变化
2. DLM 发送 MSG_LINK_UP 或 MSG_LINK_DOWN
3. CM 收到消息，更新 registered_link_t.current_state
4. CM 打印状态变化日志

**验证**:
```bash
# CM 日志应显示：
[CM] Link Inmarsat_GX_SATCOM state changed: UNAVAILABLE → ACTIVE
     Message: Satellite link established
     Bandwidth: 2000 kbps, Signal: -80 dBm
```

### 6.3 场景 3: 心跳超时

**操作**:
```bash
# 杀死某个 DLM 进程
kill <dlm_pid>
```

**预期行为**:
1. CM 在 30 秒后检测到心跳超时
2. CM Monitor 线程增加 heartbeat_miss_count
3. 连续 3 次超时后，标记为 UNAVAILABLE

**验证**:
```bash
# CM 日志应显示：
[CM] ⚠ Link Inmarsat_GX_SATCOM heartbeat timeout (35 sec)
[CM] ✗ Link Inmarsat_GX_SATCOM marked as UNAVAILABLE
```

## 7. 链路配置参数

| 链路 | 网卡 | 最大带宽 | 延迟 | 成本(分/MB) | 优先级 | 覆盖范围 |
|-----|------|---------|-----|-----------|--------|---------|
| SATCOM | eth1 | 2 Mbps | 600ms | 90 | 5 | 全球 |
| CELLULAR | eth2 | 20 Mbps | 50ms | 20 | 8 | 陆地 |
| WIFI | eth3 | 100 Mbps | 5ms | 1 | 10 | 停机位 |

## 8. 故障处理

### 8.1 Socket 连接失败

**问题**: DLM 无法连接到 CM Core

**排查**:
```bash
# 检查 CM Core 是否运行
ps aux | grep cm_core_server

# 检查 socket 文件是否存在
ls -l /tmp/magic_cm.sock

# 检查权限
chmod 666 /tmp/magic_cm.sock
```

### 8.2 网卡不存在

**问题**: DLM 报错 "Interface eth1 not found"

**解决**:
```bash
# 使用测试脚本创建虚拟网卡
sudo ./test_network_interfaces.sh

# 或手动创建
sudo modprobe dummy numdummies=3
sudo ip link set dummy0 name eth1
sudo ip addr add 192.168.1.10/24 dev eth1
sudo ip link set eth1 up
```

### 8.3 进程僵尸/泄漏

**问题**: 进程未正常退出

**解决**:
```bash
# 强制清理
pkill -9 cm_core_server
pkill -9 dlm_satcom_daemon
pkill -9 dlm_cellular_daemon
pkill -9 dlm_wifi_daemon
rm -f /tmp/magic_cm.sock
```

## 9. 后续扩展

### 9.1 与 Diameter 集成

在 `magic_server` 的 `cmd_mccr` 函数中：

```c
// 连接到 CM Core 并请求链路选择
int cm_client_fd = connect_to_cm_core();
send_link_selection_request(cm_client_fd, bandwidth_required);
uint32_t selected_link_id = recv_link_selection_response(cm_client_fd);
```

### 9.2 链路选择策略

CM Core 可实现多种选择策略：
- **成本优先**: 选择 cost_per_mb 最低的链路（WiFi > CELLULAR > SATCOM）
- **延迟优先**: 选择 latency_ms 最低的链路（WiFi > CELLULAR > SATCOM）
- **优先级策略**: 选择 priority 最高的链路
- **负载均衡**: 根据当前流量分配

### 9.3 QoS 与流量整形

每个 DLM 可实现 `tc` 命令配置 QoS：

```c
// 在 DLM 中添加 QoS 配置
void dlm_configure_qos(uint32_t bandwidth_kbps)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "tc qdisc add dev eth1 root tbf rate %ukbit burst 32kb latency 400ms",
             bandwidth_kbps);
    system(cmd);
}
```

## 10. 性能优化

- **减少锁竞争**: 使用读写锁 (pthread_rwlock_t) 而非互斥锁
- **零拷贝**: 使用 `sendmsg()` + `MSG_ZEROCOPY`
- **批量处理**: 累积多个心跳消息后一次性发送
- **非阻塞 I/O**: 使用 `epoll` 替代 `select`

---

**文档版本**: v1.0  
**最后更新**: 2025-11-24  
**作者**: MAGIC Team
