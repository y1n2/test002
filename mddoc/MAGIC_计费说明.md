# MAGIC 计费系统技术规范

## 文档版本

| 版本 | 日期 | 作者 | 说明 |
|-----|------|-----|------|
| 1.0 | 2025-12-23 | MAGIC Team | 初始版本 |

---

## 1. 概述

MAGIC 计费系统基于 ARINC 839 协议规范，实现对航空数据链路通信的流量统计和计费数据记录（CDR）管理。系统采用 Linux 内核 Netfilter/conntrack 机制进行实时流量监控，通过 Diameter 协议进行计费数据交换。

### 1.1 核心组件

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MAGIC 计费系统架构                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐        │
│   │ Traffic      │    │ CDR          │    │ CIC          │        │
│   │ Monitor      │◄──►│ Manager      │◄──►│ Handler      │        │
│   │ (Netlink)    │    │ (JSON 存储)  │    │ (Diameter)   │        │
│   └──────────────┘    └──────────────┘    └──────────────┘        │
│          │                   │                   │                 │
│          ▼                   ▼                   ▼                 │
│   ┌──────────────────────────────────────────────────────┐        │
│   │              Linux Kernel (nf_conntrack)              │        │
│   │                                                        │        │
│   │    nftables/iptables ──► conntrack mark ──► 计数器   │        │
│   └──────────────────────────────────────────────────────┘        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 数据流

```
客户端流量 ──► nftables 标记 ──► conntrack 计数 ──► Netlink 查询 ──► CDR 记录 ──► MADA 响应
```

---

## 2. Diameter 消息规范

### 2.1 MADR (Accounting Data Request) - 计费数据请求

**命令代码**: Application-Specific (MAGIC Vendor)

**消息方向**: Client → Server

**用途**: 请求查询当前活动的计费记录或详细流量数据

#### 请求 AVP 列表

| AVP 名称 | AVP Code | 数据类型 | M/O | 说明 |
|---------|----------|---------|-----|------|
| Origin-Host | 264 | DiameterIdentity | M | 请求方主机名 |
| Origin-Realm | 296 | DiameterIdentity | M | 请求方域名 |
| CDR-Type | 10042 | Unsigned32 | M | 请求类型 |
| CDR-Level | 10043 | Unsigned32 | O | 数据隔离级别 |
| CDR-Request-Identifier | 10044 | UTF8String | O | 指定会话/CDR ID |

#### CDR-Type 取值

| 值 | 名称 | 说明 |
|----|------|------|
| 1 | LIST_REQUEST | 仅返回 CDR 列表（不含详细流量） |
| 2 | DATA_REQUEST | 返回完整 CDR 数据（含流量统计） |

#### CDR-Level 取值

| 值 | 名称 | 说明 |
|----|------|------|
| 1 | ALL | 返回所有 CDR 记录 |
| 2 | USER_DEPENDENT | 仅返回请求方用户的 CDR |
| 3 | SESSION_DEPENDENT | 仅返回指定会话的 CDR |

### 2.2 MADA (Accounting Data Answer) - 计费数据应答

**消息方向**: Server → Client

**用途**: 返回计费数据查询结果

#### 应答 AVP 列表

| AVP 名称 | AVP Code | 数据类型 | M/O | 说明 |
|---------|----------|---------|-----|------|
| Result-Code | 268 | Unsigned32 | M | 操作结果 |
| Origin-Host | 264 | DiameterIdentity | M | 服务器主机名 |
| Origin-Realm | 296 | DiameterIdentity | M | 服务器域名 |
| CDR-Type | 10042 | Unsigned32 | M | 回显请求类型 |
| CDR-Level | 10043 | Unsigned32 | M | 回显数据级别 |
| CDRs-Active | 20012 | Grouped | O | 活动 CDR 组 |
| CDRs-Finished | 20013 | Grouped | O | 已完成 CDR 组 |

#### CDRs-Active/CDRs-Finished 子 AVP

| AVP 名称 | AVP Code | 数据类型 | 说明 |
|---------|----------|---------|------|
| CDR-Info | 20017 | Grouped | 单条 CDR 信息 |

#### CDR-Info 子 AVP

| AVP 名称 | AVP Code | 数据类型 | 说明 |
|---------|----------|---------|------|
| CDR-ID | 10046 | Unsigned32 | CDR 唯一标识 |
| CDR-Content | 10047 | UTF8String | CDR 内容字符串 |

### 2.3 CDR-Content 字段格式

CDR-Content 使用分号分隔的键值对格式：

```
CDR_ID=275;SESSION_ID=magic.server;CLIENT_ID=magic.client;STATUS=ACTIVE;DLM_NAME=CELLULAR;START_TIME=1703318400;BYTES_IN=1024000;BYTES_OUT=512000
```

| 字段 | 类型 | 说明 |
|------|------|------|
| CDR_ID | uint32 | CDR 数字标识（基于 session_id hash） |
| SESSION_ID | string | Diameter 会话 ID |
| CLIENT_ID | string | 客户端 Origin-Host |
| STATUS | enum | ACTIVE / FINISHED / ROLLOVER |
| DLM_NAME | string | 关联的数据链路名称 |
| START_TIME | timestamp | 会话开始时间（Unix 时间戳） |
| BYTES_IN | uint64 | 入站字节数（客户端→服务器） |
| BYTES_OUT | uint64 | 出站字节数（服务器→客户端） |

---

## 3. 流量监控机制

### 3.1 技术原理

系统使用 Linux Netfilter 框架进行流量监控：

1. **连接标记 (Connection Marking)**:
   - 使用 `nftables` 或 `iptables` 在 mangle 表的 PREROUTING 链添加规则
   - 基于客户端 IP 地址设置 `ct mark` (conntrack mark)
   - mark 值通过 session_id 的 DJB2 hash 算法生成

2. **流量计数 (Traffic Counting)**:
   - 内核 conntrack 模块自动维护每个连接的字节/数据包计数器
   - 需要启用 `net.netfilter.nf_conntrack_acct=1`

3. **数据查询 (Data Query)**:
   - 通过 `libnetfilter_conntrack` 库的 Netlink 接口查询
   - 根据 mark 值筛选匹配的 conntrack 条目
   - 聚合同一 mark 下所有连接的流量统计

### 3.2 Mark 值计算

```c
// DJB2 Hash 算法
uint32_t djb2_hash(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// Session ID → Mark 映射
uint32_t traffic_session_id_to_mark(const char *session_id) {
    uint32_t hash = djb2_hash(session_id);
    // 映射到 [0x100, 0xFFFF] 范围
    return TRAFFIC_MARK_BASE + (hash % (TRAFFIC_MARK_MAX - TRAFFIC_MARK_BASE + 1));
}
```

### 3.3 防火墙规则

#### nftables 规则示例

```bash
# 创建表和链
nft add table inet magic
nft add chain inet magic prerouting '{ type filter hook prerouting priority -150; policy accept; }'

# 添加标记规则
nft add rule inet magic prerouting ip saddr 192.168.126.5 ct mark set 0x113
nft add rule inet magic prerouting ip daddr 192.168.126.5 ct mark set 0x113
```

#### iptables 规则示例

```bash
# 创建自定义链
iptables -t mangle -N MAGIC_MARK
iptables -t mangle -I PREROUTING 1 -j MAGIC_MARK

# 添加标记规则
iptables -t mangle -A MAGIC_MARK -s 192.168.126.5 -j CONNMARK --set-mark 0x113
iptables -t mangle -A MAGIC_MARK -d 192.168.126.5 -j CONNMARK --set-mark 0x113
iptables -t mangle -A MAGIC_MARK -j CONNMARK --restore-mark
```

### 3.4 系统要求

#### 必需的内核模块

```bash
modprobe nf_conntrack
modprobe nf_conntrack_ipv4
modprobe nf_conntrack_ipv6
```

#### 必需的 sysctl 配置

```bash
# 【关键】启用 conntrack 流量计数
sysctl -w net.netfilter.nf_conntrack_acct=1

# 可选: 增加 conntrack 表大小
sysctl -w net.netfilter.nf_conntrack_max=131072
```

**⚠️ 重要**: 如果 `nf_conntrack_acct=0`（默认值），conntrack 不会记录字节计数，导致流量统计始终为 0。

#### 永久配置

```bash
# /etc/sysctl.d/99-magic-conntrack.conf
net.netfilter.nf_conntrack_acct = 1
net.netfilter.nf_conntrack_max = 131072
```

---

## 4. CDR 生命周期管理

### 4.1 状态机

```
                    ┌──────────────────────────────────────┐
                    │                                      │
    [会话建立]      ▼                                      │
        │      ┌────────┐    [会话结束]    ┌──────────┐   │
        └─────►│ ACTIVE │───────────────►│ FINISHED │   │
               └────────┘                 └──────────┘   │
                    │                          │          │
                    │ [CDR切分]                │ [归档]   │
                    │                          ▼          │
                    │                    ┌──────────┐     │
                    │                    │ ARCHIVED │     │
                    │                    └──────────┘     │
                    │                                     │
                    │ [MACR请求]         [创建新CDR]      │
                    │                          │          │
                    ▼                          │          │
               ┌──────────┐                    │          │
               │ ROLLOVER │────────────────────┼──────────┘
               └──────────┘    归档旧CDR       │
                                               │
                    ┌──────────────────────────┘
                    │
                    ▼
               ┌────────┐
               │ ACTIVE │  (新CDR)
               └────────┘
```

### 4.2 CDR 切分 (Rollover)

CDR 切分用于实现"不断网切账单"功能，允许在会话进行中生成新的计费记录：

1. **快照当前流量**: 记录切分时刻的累计流量
2. **归档旧 CDR**: 将当前 CDR 状态设为 ROLLOVER 并归档
3. **创建新 CDR**: 以当前流量值作为 `base_offset` 创建新 CDR
4. **流量计算**: 新 CDR 的实际流量 = 当前累计 - base_offset

#### 流量计算示例

```
时间点 T1: 创建 CDR-1, base_offset = 0
           累计流量 = 100MB
           CDR-1 流量 = 100MB - 0 = 100MB

时间点 T2: 切分请求, 累计流量 = 500MB
           归档 CDR-1 (流量 = 500MB)
           创建 CDR-2, base_offset = 500MB

时间点 T3: 累计流量 = 800MB
           CDR-2 流量 = 800MB - 500MB = 300MB
```

### 4.3 存储结构

```
/var/lib/magic/cdr/
├── active/           # 活动 CDR 文件
│   ├── 275.json
│   └── 276.json
└── archive/          # 归档 CDR 文件 (按日期组织)
    ├── 2025-12-23/
    │   ├── 270.json
    │   └── 271.json
    └── 2025-12-22/
        └── 268.json
```

### 4.4 CDR JSON 格式

```json
{
  "cdr_id": 275,
  "cdr_uuid": "6587a3c0-4f2a-4812-8a00-1703318400",
  "session_id": "magic.server;1234;5678",
  "client_id": "magic.client",
  "dlm_name": "CELLULAR",
  "status": "ACTIVE",
  "start_time": 1703318400,
  "stop_time": 0,
  "bytes_in": 1024000,
  "bytes_out": 512000,
  "packets_in": 850,
  "packets_out": 420,
  "base_offset_in": 0,
  "base_offset_out": 0,
  "overflow_count_in": 0,
  "overflow_count_out": 0
}
```

---

## 5. 客户端使用指南

### 5.1 magic_client 命令

```bash
# 进入 magic_client 交互模式
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client

# 发送 MADR 请求 (查询 CDR 列表)
magic> madr list

# 发送 MADR 请求 (查询特定 CDR 详细数据)
magic> madr data 275
```

### 5.2 命令参数

```
madr list | madr data <cdr_id>

参数:
  list    - 查询当前会话关联的所有 CDR 列表 (CDR-Type=1, CDR-Level=1)
  data    - 查询特定 CDR 的详细流量统计 (CDR-Type=2, CDR-Level=3)
  cdr_id  - 具体的 CDR ID (如 275 或 session_id)
```

### 5.3 响应示例

```
=== MADA Response ===
Result-Code: 2001 (DIAMETER_SUCCESS)
CDR-Type: 2 (DATA_REQUEST)
CDR-Level: 1 (ALL)

[CDRs-Active]
  CDR-Info:
    CDR-ID: 275
    CDR-Content: CDR_ID=275;SESSION_ID=magic.server;CLIENT_ID=magic.client;
                 STATUS=ACTIVE;DLM_NAME=CELLULAR;START_TIME=1703318400;
                 BYTES_IN=1024000;BYTES_OUT=512000
```

---

## 6. 故障排查

### 6.1 流量统计为 0

**症状**: MADA 响应中 `BYTES_IN=0; BYTES_OUT=0`

**排查步骤**:

1. **检查 conntrack 计数是否启用**:
   ```bash
   sysctl net.netfilter.nf_conntrack_acct
   # 应为 1
   ```

2. **检查防火墙规则**:
   ```bash
   # nftables
   nft list table inet magic
   
   # iptables
   iptables -t mangle -L MAGIC_MARK -v
   ```

3. **检查 conntrack 条目**:
   ```bash
   # 查看带 mark 的条目
   conntrack -L -m 0x113
   
   # 查看详细信息 (含字节计数)
   conntrack -L -o extended | grep mark=275
   ```

4. **产生测试流量**:
   ```bash
   # 从客户端 ping 服务器
   ping -c 10 -s 1000 <server_ip>
   
   # 再次查看 conntrack
   conntrack -L -o extended | grep mark
   ```

### 6.2 CDR 未创建

**症状**: MADA 响应中没有 CDRs-Active 数据

**排查步骤**:

1. **检查会话状态**:
   ```bash
   # 在 magic_client 中
   magic> msxr
   ```

2. **检查服务器日志**:
   ```bash
   tail -f /home/zhuwuhui/freeDiameter/logs/freediameter.log | grep -i cdr
   ```

3. **检查 CDR 目录**:
   ```bash
   ls -la /var/lib/magic/cdr/active/
   ```

### 6.3 常见错误码

| Result-Code | 说明 | 处理方法 |
|-------------|------|---------|
| 2001 | 成功 | - |
| 3001 | 命令不支持 | 检查消息类型 |
| 5012 | 无法处理请求 | 检查服务器状态 |
| 5030 | 用户未知 | 检查认证状态 |

---

## 7. 性能考虑

### 7.1 conntrack 表大小

对于高流量场景，需要增加 conntrack 表容量：

```bash
# 查看当前使用情况
cat /proc/sys/net/netfilter/nf_conntrack_count
cat /proc/sys/net/netfilter/nf_conntrack_max

# 增加容量
sysctl -w net.netfilter.nf_conntrack_max=262144
```

### 7.2 缓存策略

流量监控模块实现了统计缓存：

- **缓存有效期**: 1 秒 (`STATS_CACHE_TTL_SEC`)
- **作用**: 减少 Netlink 查询频率
- **配置**: 在高精度场景下可减小 TTL

### 7.3 CDR 归档策略

- **默认保留**: 24 小时 (`CDR_ARCHIVE_RETENTION_SEC`)
- **清理间隔**: 1 小时 (`CDR_CLEANUP_INTERVAL_SEC`)
- **建议**: 长期存储应将归档文件备份到外部系统

---

## 8. 安全考虑

### 8.1 数据隔离

- CDR-Level=2 (USER_DEPENDENT) 确保用户只能查看自己的记录
- 服务器根据 Origin-Host 进行过滤

### 8.2 权限要求

流量监控需要 root 权限：
- 添加 nftables/iptables 规则
- 访问 Netlink conntrack 接口

### 8.3 审计日志

所有 MADR/MADA 操作记录在 freeDiameter 日志中：
```
[app_magic] MADR (Accounting Data Request)
[app_magic]   Requester: magic.client
[app_magic]   CDR-Type: 2 (DATA), CDR-Level: 1 (ALL)
```

---

## 附录 A: 相关源文件

| 文件 | 功能 |
|------|------|
| `magic_traffic_monitor.c` | 流量监控核心实现 |
| `magic_traffic_monitor.h` | 流量监控接口定义 |
| `magic_cdr.c` | CDR 管理实现 |
| `magic_cdr.h` | CDR 数据结构定义 |
| `magic_cic.c` | MADR/MADA 消息处理 |

## 附录 B: 参考标准

- ARINC 839-2014: MAGIC Protocol Specification
- RFC 6733: Diameter Base Protocol
- Linux Netfilter Documentation

---

*文档结束*
