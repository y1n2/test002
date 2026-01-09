# MAGIC 系统测试手册

## 目录

1. [系统概述](#1-系统概述)
2. [测试环境准备](#2-测试环境准备)
3. [组件启动顺序](#3-组件启动顺序)
4. [控制平面测试](#4-控制平面测试)
5. [数据平面测试](#5-数据平面测试)
6. [端到端集成测试](#6-端到端集成测试)
7. [故障场景测试](#7-故障场景测试)
8. [常用命令参考](#8-常用命令参考)
9. [问题排查](#9-问题排查)

---

## 1. 系统概述

### 1.1 架构图

```
                                    MAGIC 系统架构
┌─────────────────────────────────────────────────────────────────────────────┐
│                              控制平面                                        │
│  ┌──────────────┐    Diameter    ┌─────────────────────────────────────┐    │
│  │ 机载客户端   │ ◄────────────► │         MAGIC Server               │    │
│  │ diameter_client│   MCAR/MCCR  │         (app_magic.fdx)             │    │
│  └──────────────┘                │  ┌─────────┐ ┌─────────┐ ┌────────┐ │    │
│                                  │  │  CIC    │ │ Policy  │ │Session │ │    │
│                                  │  │ Handler │ │ Engine  │ │  Mgr   │ │    │
│                                  │  └─────────┘ └─────────┘ └────────┘ │    │
│                                  │  ┌─────────┐ ┌─────────┐ ┌────────┐ │    │
│                                  │  │   LMI   │ │Dataplane│ │ Config │ │    │
│                                  │  │Interface│ │ Router  │ │ Parser │ │    │
│                                  │  └────┬────┘ └────┬────┘ └────────┘ │    │
│                                  └───────┼──────────┼──────────────────┘    │
│                                          │          │                        │
│  ┌───────────────────────────────────────┼──────────┼───────────────────┐   │
│  │              DLM 原型 (MIH 接口)      │          │                   │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌┴─────────────┐               │   │
│  │  │ CELLULAR DLM │  │  SATCOM DLM  │  │   WIFI DLM   │               │   │
│  │  │ /tmp/dlm_    │  │ /tmp/dlm_    │  │ /tmp/dlm_    │               │   │
│  │  │ cellular.sock│  │ satcom.sock  │  │ wifi.sock    │               │   │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │   │
│  └─────────┼─────────────────┼─────────────────┼────────────────────────┘   │
│            │                 │                 │                             │
└────────────┼─────────────────┼─────────────────┼─────────────────────────────┘
             │                 │                 │
┌────────────┼─────────────────┼─────────────────┼─────────────────────────────┐
│            ▼                 ▼                 ▼        数据平面             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                       │
│  │    eth2      │  │    eth1      │  │    eth3      │   出口接口            │
│  │  CELLULAR    │  │   SATCOM     │  │    WIFI      │                       │
│  │  table 101   │  │  table 100   │  │  table 102   │                       │
│  └──────────────┘  └──────────────┘  └──────────────┘                       │
│                           ▲                                                  │
│                           │ ip rule from <client_ip> lookup <table>         │
│                    ┌──────┴───────┐                                         │
│                    │    eth4      │   入口接口                              │
│                    │192.168.10.102│   (客户端连接)                          │
│                    └──────────────┘                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 组件说明

| 组件 | 描述 | 通信方式 |
|------|------|----------|
| **MAGIC Server** | 核心服务，运行 freeDiameter + app_magic.fdx | Diameter (TCP/SCTP) |
| **diameter_client** | 机载应用客户端模拟器 | Diameter |
| **DLM 原型** | 数据链路管理器 (CELLULAR/SATCOM/WIFI) | Unix Domain Socket (UDP) |
| **MIHF 模拟器** | MIH 功能模拟器 (调试用) | Unix Domain Socket |

### 1.3 关键套接字路径

| 路径 | 用途 |
|------|------|
| `/tmp/mihf.sock` | MAGIC LMI 数据报服务器 (接收 DLM 消息) |
| `/tmp/magic_core.sock` | MAGIC LMI 流式服务器 (传统模式) |
| `/tmp/dlm_cellular.sock` | CELLULAR DLM 套接字 |
| `/tmp/dlm_satcom.sock` | SATCOM DLM 套接字 |
| `/tmp/dlm_wifi.sock` | WIFI DLM 套接字 |

---

## 2. 测试环境准备

### 2.1 网络接口配置

测试前需要配置以下网络接口：

```bash
# 查看当前接口
ip a

# 创建虚拟接口 (如果物理接口不存在)
sudo ip link add ens37 type dummy  # SATCOM
sudo ip link add ens33 type dummy  # CELLULAR
sudo ip link add ens38 type dummy  # WIFI
sudo ip link add ens39 type dummy  # 入口 (客户端侧)

# 配置 IP 地址
sudo ip addr add 10.0.1.1/24 dev ens37   # SATCOM 网关
sudo ip addr add 10.0.2.1/24 dev ens33   # CELLULAR 网关
sudo ip addr add 10.0.3.1/24 dev ens38   # WIFI 网关
sudo ip addr add 192.168.126.1/24 dev ens39  # 入口接口 (服务器入口 IP)

# 启用接口
sudo ip link set ens37 up
sudo ip link set ens33 up
sudo ip link set ens38 up
sudo ip link set ens39 up
```

### 2.2 验证接口

```bash
ip link show | grep -E "eth[1-4]"
# 应显示 4 个 UP 状态的接口
```

### 2.3 目录结构

```
/home/zhuwuhui/freeDiameter/
├── build/
│   ├── freeDiameterd/freeDiameterd     # MAGIC 服务器
│   ├── extensions/app_magic.fdx        # MAGIC 扩展
│   └── DLM_PROTOTYPES/
│       ├── dlm_cellular_proto          # CELLULAR DLM
│       ├── dlm_satcom_proto            # SATCOM DLM
│       └── dlm_wifi_proto              # WIFI DLM
├── diameter_client/
│   └── diameter_client                 # Diameter 客户端
├── MIHF_SIMULATOR/
│   └── mihf_simulator                  # MIHF 模拟器
└── conf/
    └── magic_server.conf               # 服务器配置
```

### 2.4 编译所有组件

```bash
cd /home/zhuwuhui/freeDiameter/build

# 编译 MAGIC 服务器和扩展
make freeDiameterd app_magic

# 编译 DLM 原型
make dlm_cellular_proto dlm_satcom_proto dlm_wifi_proto

# 编译 Diameter 客户端
cd ../diameter_client && make

# 编译 MIHF 模拟器
cd ../MIHF_SIMULATOR && make
```

---

## 3. 组件启动顺序

### 3.1 推荐启动顺序

```
1. MAGIC Server (freeDiameterd)
2. DLM 原型 (CELLULAR, SATCOM, WIFI)
3. Diameter 客户端
```

### 3.2 终端布局建议

打开 5 个终端窗口：

| 终端 | 用途 |
|------|------|
| T1 | MAGIC Server |
| T2 | CELLULAR DLM |
| T3 | SATCOM DLM |
| T4 | WIFI DLM |
| T5 | Diameter 客户端 / 命令操作 |

### 3.3 启动命令

**终端 T1 - MAGIC Server:**
```bash
cd /home/zhuwuhui/freeDiameter/build
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf
```

**终端 T2 - CELLULAR DLM:**
```bash
cd /home/zhuwuhui/freeDiameter/build/DLM_PROTOTYPES
./dlm_cellular_proto
```

**终端 T3 - SATCOM DLM:**
```bash
cd /home/zhuwuhui/freeDiameter/build/DLM_PROTOTYPES
./dlm_satcom_proto
```

**终端 T4 - WIFI DLM:**
```bash
cd /home/zhuwuhui/freeDiameter/build/DLM_PROTOTYPES
./dlm_wifi_proto
```

### 3.4 验证启动状态

```bash
# 检查进程
ps aux | grep -E "freeDiameter|dlm_.*_proto"

# 检查套接字文件
ls -la /tmp/*.sock

# 预期输出:
# /tmp/mihf.sock          (MAGIC 数据报服务器)
# /tmp/magic_core.sock    (MAGIC 流式服务器)
# /tmp/dlm_cellular.sock  (CELLULAR DLM)
# /tmp/dlm_satcom.sock    (SATCOM DLM)
# /tmp/dlm_wifi.sock      (WIFI DLM)
```

---

## 4. 控制平面测试

### 4.1 测试 DLM 链路注册

**目标:** 验证 DLM 能够向 MAGIC 注册链路

**步骤:**
1. 启动 MAGIC Server
2. 启动任一 DLM (如 CELLULAR)
3. 在 DLM 终端输入 `u` 发送 Link_Up.indication

**DLM 命令:**
```
u    - 发送 Link_Up.indication (链路上线)
d    - 发送 Link_Down.indication (链路下线)
p    - 发送 Link_Parameters_Report.indication (参数报告)
q    - 退出
```

**预期结果:**
- MAGIC Server 日志显示收到 Link_Up.indication
- 链路被注册到 LMI 上下文

### 4.2 测试客户端认证 (MCAR)

**目标:** 验证客户端能够完成 Diameter 认证

**步骤:**
1. 确保 MAGIC Server 运行中
2. 启动 diameter_client
3. 客户端自动发送 MCAR

**预期日志 (MAGIC Server):**
```
[app_magic] ========================================
[app_magic] MCAR (Client Authentication Request)
[app_magic] ========================================
[app_magic]   Client: magic-client.example.com
[app_magic]   Username: B8888_EFB01
[app_magic] ✓ Client authenticated
[app_magic] ✓ Sent MCAA
```

**预期日志 (diameter_client):**
```
[MAGIC] 认证成功 (Result-Code: 2001)
```

### 4.3 测试通信资源请求 (MCCR)

**目标:** 验证 MCCR 处理和策略决策

**步骤:**
1. 完成 MCAR 认证
2. 在 diameter_client 中发送 MCCR
3. 观察策略决策结果

**diameter_client 命令:**
```
mccr <bandwidth_kbps>    - 发送 MCCR 请求
例如: mccr 5000
```

**预期日志 (MAGIC Server):**
```
[app_magic] ========================================
[app_magic] MCCR (Communication Change Request)
[app_magic] ========================================
[app_magic]   Session-Id: magic-client.example.com;...
[app_magic]   Client: magic-client.example.com
[app_magic]   Requested BW: 5000.00/1000.00 kbps
[app_magic] ✓ Policy Decision:
[app_magic]     Link: LINK_CELLULAR
[app_magic]     BW: 5000/1000 kbps
[app_magic]     Reason: Selected based on policy ranking
[app_magic] ✓ Dataplane route added: 192.168.10.10 → LINK_CELLULAR
[app_magic] ✓ Sent MCCA
```

### 4.4 测试会话终止 (STR)

**目标:** 验证会话清理和路由规则删除

**步骤:**
1. 建立活动会话
2. 在 diameter_client 中发送 STR
3. 观察资源释放

**diameter_client 命令:**
```
str    - 发送会话终止请求
quit   - 断开连接并退出
```

**预期日志 (MAGIC Server):**
```
[app_magic] ========================================
[app_magic] STR (Session Termination Request)
[app_magic] ========================================
[app_magic]   Session: magic-client.example.com;...
[app_magic] ✓ Dataplane route removed for session
[app_magic] ✓ Sent STA
```

---

## 5. 数据平面测试

### 5.1 验证路由表创建

**目标:** 确认 MAGIC 启动时创建了正确的路由表

**命令:**
```bash
# 查看策略路由规则
ip rule list

# 查看各链路路由表
ip route show table 100   # SATCOM
ip route show table 101   # CELLULAR  
ip route show table 102   # WIFI
```

**预期输出:**
```
# ip rule list (初始状态，无客户端规则)
0:      from all lookup local
32766:  from all lookup main
32767:  from all lookup default

# ip route show table 100
default dev eth1 scope link

# ip route show table 101
default dev eth2 scope link

# ip route show table 102
default dev eth3 scope link
```

### 5.2 验证客户端路由规则

**目标:** MCCR 成功后，验证添加了正确的策略路由

**步骤:**
1. 发送 MCCR 请求
2. 检查 ip rule

**命令:**
```bash
ip rule list | grep "192.168.10"
```

**预期输出:**
```
1000:   from 192.168.10.10 lookup 101
```

### 5.3 测试数据包路由

**目标:** 验证客户端数据包被正确路由

**步骤:**
1. 建立会话 (分配到 CELLULAR)
2. 从客户端 IP 发送测试包
3. 观察数据包走向

**命令:**
```bash
# 在入口接口抓包
sudo tcpdump -i eth4 -n host 192.168.10.10

# 在出口接口抓包 (应该看到转发的包)
sudo tcpdump -i eth2 -n

# 发送测试 UDP 包 (需要配置客户端 IP)
# 方法1: 使用 netcat
echo "test" | nc -u -s 192.168.10.10 8.8.8.8 53

# 方法2: 使用 ping (需要配置源地址)
ping -I 192.168.10.10 8.8.8.8
```

### 5.4 测试链路切换

**目标:** 验证链路切换时路由规则更新

**步骤:**
1. 建立会话 (分配到 CELLULAR)
2. 模拟 CELLULAR 链路下线
3. 发送新 MCCR 或触发策略重评估
4. 验证路由切换到 SATCOM

**验证:**
```bash
# 切换前
ip rule list | grep 192.168.10.10
# 1000: from 192.168.10.10 lookup 101 (CELLULAR)

# 切换后
ip rule list | grep 192.168.10.10
# 1001: from 192.168.10.10 lookup 100 (SATCOM)
```

---

## 6. 端到端集成测试

### 6.1 测试场景 1: 完整会话生命周期

**场景描述:** 客户端认证 → 请求资源 → 使用链路 → 终止会话

**步骤:**

```bash
# 步骤 1: 启动所有组件
# (按照 3.3 节启动)

# 步骤 2: 启动 DLM 并发送 Link_Up
# 在 CELLULAR DLM 终端输入:
u

# 步骤 3: 启动客户端
cd /home/zhuwuhui/freeDiameter/diameter_client
./diameter_client -c client.conf

# 步骤 4: 发送 MCCR
mccr 5000

# 步骤 5: 验证路由
ip rule list | grep 192.168.10

# 步骤 6: 发送测试数据 (另一个终端)
echo "test data" | nc -u -s 192.168.10.10 8.8.8.8 12345

# 步骤 7: 终止会话
str

# 步骤 8: 验证路由已删除
ip rule list | grep 192.168.10
```

### 6.2 测试场景 2: 多客户端并发

**场景描述:** 多个客户端同时连接，验证资源隔离

**步骤:**
1. 启动 3 个 diameter_client 实例 (不同配置)
2. 各客户端发送 MCCR
3. 验证各自的路由规则独立

### 6.3 测试场景 3: 链路故障切换

**场景描述:** 主链路故障时自动切换到备用链路

**步骤:**
```bash
# 1. 建立会话 (使用 CELLULAR)
# 2. 在 CELLULAR DLM 发送 Link_Down
d

# 3. 客户端发送新 MCCR
mccr 5000

# 4. 验证切换到 SATCOM
ip rule list | grep 192.168.10
```

---

## 7. 故障场景测试

### 7.1 DLM 断开重连

**测试目标:** DLM 断开后重新连接，链路状态正确恢复

**步骤:**
1. 启动 MAGIC 和 DLM
2. 发送 Link_Up
3. 终止 DLM 进程
4. 重新启动 DLM
5. 重新发送 Link_Up

### 7.2 客户端异常断开

**测试目标:** 客户端异常断开时资源正确释放

**步骤:**
1. 建立会话
2. 强制终止 diameter_client (Ctrl+C)
3. 等待会话超时
4. 验证路由规则已删除

### 7.3 所有链路不可用

**测试目标:** 无可用链路时正确返回错误

**步骤:**
1. 不启动任何 DLM
2. 客户端发送 MCCR
3. 验证返回 UNABLE_TO_COMPLY

---

## 8. 常用命令参考

### 8.1 MAGIC Server

```bash
# 启动 (前台)
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf

# 启动 (后台)
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf &

# 停止
sudo pkill freeDiameterd
```

### 8.2 DLM 原型命令

| 命令 | 描述 |
|------|------|
| `u` | 发送 Link_Up.indication |
| `d` | 发送 Link_Down.indication |
| `g` | 发送 Link_Going_Down.indication |
| `p` | 发送 Link_Parameters_Report.indication |
| `s` | 显示状态 |
| `h` | 帮助 |
| `q` | 退出 |

### 8.3 diameter_client 命令

| 命令 | 描述 |
|------|------|
| `mcar` | 发送认证请求 |
| `mccr <bw>` | 发送通信资源请求 |
| `str` | 发送会话终止请求 |
| `status` | 显示当前状态 |
| `help` | 帮助 |
| `quit` | 退出 |

### 8.4 网络诊断命令

```bash
# 查看路由规则
ip rule list

# 查看特定路由表
ip route show table 100
ip route show table 101
ip route show table 102

# 查看套接字文件
ls -la /tmp/*.sock

# 抓包
sudo tcpdump -i eth4 -n
sudo tcpdump -i eth2 -n udp

# 检查进程
ps aux | grep -E "freeDiameter|dlm_|diameter_client"

# 查看日志
tail -f /var/log/syslog | grep -i magic
```

### 8.5 MIHF 模拟器命令

```bash
# 启动
cd /home/zhuwuhui/freeDiameter/MIHF_SIMULATOR
./mihf_simulator

# 命令
c <n>  - 发送 Capability_Discover.request (n=1,2,3 对应 DLM)
p <n>  - 发送 Get_Parameters.request
s <n>  - 发送 Event_Subscribe.request
r <n>  - 发送 Resource.request
a      - 向所有 DLM 发送请求
l      - 列出 DLM 状态
h      - 帮助
q      - 退出
```

---

## 9. 问题排查

### 9.1 常见问题

#### 问题 1: MAGIC Server 启动失败

**症状:** `bind() failed` 或 `Address already in use`

**解决:**
```bash
# 清理残留套接字
sudo rm -f /tmp/mihf.sock /tmp/magic_core.sock

# 检查端口占用
sudo netstat -tlnp | grep 3868

# 终止旧进程
sudo pkill freeDiameterd
```

#### 问题 2: DLM 连接失败

**症状:** DLM 报告无法连接到 MIHF

**解决:**
```bash
# 检查 MAGIC 是否运行
ps aux | grep freeDiameter

# 检查套接字是否存在
ls -la /tmp/mihf.sock

# 检查权限
sudo chmod 777 /tmp/mihf.sock
```

#### 问题 3: 路由规则未创建

**症状:** MCCR 成功但 ip rule 中没有规则

**解决:**
```bash
# 检查数据平面是否启用
# 查看 MAGIC 日志中是否有 "Dataplane initialized"

# 检查接口是否存在
ip link show eth1 eth2 eth3

# 手动测试规则添加
sudo ip rule add from 192.168.10.10 lookup 101 priority 1000
```

#### 问题 4: 策略决策失败

**症状:** MCCR 返回 UNABLE_TO_COMPLY

**解决:**
```bash
# 检查是否有活动链路
# 查看 MAGIC 日志中链路注册情况

# 确保 DLM 发送了 Link_Up
# 在 DLM 终端输入 u

# 检查配置文件
cat /home/zhuwuhui/freeDiameter/extensions/app_magic/config/Datalink_Profile.xml
```

### 9.2 日志级别调整

编辑 `magic_server.conf`:
```
# 调试级别 (0-4, 4 最详细)
LogLevel = 4;
```

### 9.3 核心转储分析

```bash
# 启用核心转储
ulimit -c unlimited

# 设置转储路径
echo "/tmp/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern

# 分析
gdb ./freeDiameterd/freeDiameterd /tmp/core.freeDiameterd.*
```

---

## 附录 A: 配置文件位置

| 配置文件 | 路径 |
|----------|------|
| MAGIC 服务器 | `conf/magic_server.conf` |
| 数据链路配置 | `extensions/app_magic/config/Datalink_Profile.xml` |
| 策略配置 | `extensions/app_magic/config/Central_Policy_Profile.xml` |
| 客户端配置 | `extensions/app_magic/config/Client_Profile.xml` |

## 附录 B: 测试检查清单

- [ ] 网络接口已配置 (eth1-eth4)
- [ ] 所有组件已编译
- [ ] MAGIC Server 启动成功
- [ ] 至少一个 DLM 运行并注册链路
- [ ] diameter_client 认证成功
- [ ] MCCR 处理成功
- [ ] 路由规则已创建
- [ ] 数据包正确路由
- [ ] STR 清理资源成功

---

**文档版本:** 1.0  
**最后更新:** 2025-11-27  
**作者:** MAGIC 系统开发团队
