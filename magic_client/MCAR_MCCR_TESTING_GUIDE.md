# MAGIC 客户端 MCAR/MCCR 测试指导手册

## 概述

本文档提供 ARINC 839 MAGIC 协议客户端的完整测试指导，涵盖：
- **MCAR**: MAGIC 客户端认证请求 (3种场景 + 1种组合场景)
- **MCCR**: MAGIC 通信变更请求 (4种场景)
- **服务器推送**: MSCR (状态变更) 和 MNTR (会话通知) 处理

---

## 1. 测试前准备

### 1.1 启动服务器

```bash
# 启动 MAGIC 服务器 (PC2 地面服务器)
cd /home/zhuwuhui/freeDiameter/build
./freeDiameterd -c ../conf/magic_server_fd.conf
```

### 1.2 启动客户端

```bash
# 启动 MAGIC 客户端 (PC3 航电客户端)
cd /home/zhuwuhui/freeDiameter/magic_client
./magic_client
```

### 1.3 验证连接状态

```
MAGIC> status
```
确认客户端状态为 `IDLE (未认证)`。

---

## 2. MCAR 测试场景

MCAR (MAGIC-Client-Authentication-Request) 用于客户端认证和状态订阅。

### 2.1 场景 A: 纯认证

**目的**: 仅验证客户端身份，不建立通信链路

**命令**:
```
MAGIC> mcar auth
```

**请求内容**:
- Client-Credentials (客户端凭证)
- Session-Id, Origin-Host, Origin-Realm (标准 Diameter 头)

**预期响应**:
```
✓ MCAA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  状态转换: IDLE → AUTHENTICATED
```

**服务端状态机**:
```
IDLE → [收到 MCAR] → 验证凭证 → AUTHENTICATED
                     5步流水线:
                       1. 提取 MCAR
                       2. 验证客户端凭证
                       3. 创建会话
                       4. 添加至授权客户端
                       5. 返回 MCAA
```

**验证命令**:
```
MAGIC> status
```
确认状态显示 `AUTHENTICATED (已认证)`。

---

### 2.2 场景 B: 认证 + 订阅

**目的**: 认证并订阅服务器状态推送通知

**命令**:
```
MAGIC> mcar subscribe 3
```

**订阅级别说明**:
| 级别 | 值 | 含义 |
|------|-----|------|
| NONE | 0 | 不订阅任何状态 |
| MAGIC_Status | 1 | 仅系统状态 |
| DLM_Status | 2 | DLM一般状态 |
| MAGIC_DLM_Status | 3 | 综合状态 [推荐] |
| (Reserved) | 4,5 | 保留 (会被拒绝) |
| DLM_Link_Status | 6 | 详细链路状态 |
| All_Status | 7 | 全部状态 |

**请求内容**:
- Client-Credentials
- REQ-Status-Info = 3 (订阅级别)

**预期响应**:
```
✓ MCAA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  REQ-Status-Info: 3 (MAGIC_DLM_Status)
  (认证成功后将开始接收 MSCR 状态推送)
```

**降级检测**:
如果服务器资源不足，可能返回较低的订阅级别：
```
⚠ 服务器降级了订阅级别!
  请求级别: 7 (All_Status)
  授予级别: 3 (MAGIC_DLM_Status)
```

**验证订阅生效**:
订阅成功后，当服务器状态变化时，客户端会自动收到 MSCR 推送并显示：
```
╔══════════════════════════════════════════════════════════════╗
║  📡 收到 MSCR 状态变更推送                                   ║
║  时间: 2025-10-28 12:34:56                                   ║
╠══════════════════════════════════════════════════════════════╣
║ DLM 状态信息:
  ┌─ DLM-List ─────────────────────────────────────────────┐
  │ DLM[1] INMARSAT: ONLINE
  │ DLM[2] VSAT: DEGRADED
  └──────────────────────────────────────────────────────────┘
╚══════════════════════════════════════════════════════════════╝
→ 已发送 MSCA 确认应答 (Result-Code=2001)
```

---

### 2.3 场景 C: 0-RTT 快速接入

**目的**: 一步完成认证并建立通信链路 (Zero Round-Trip Time)

**基本命令**:
```
MAGIC> mcar connect IP_DATA 5000
```

**带非对称带宽**:
```
MAGIC> mcar connect VOICE 512 256
```

**参数说明**:
| 参数 | 描述 | 示例值 |
|------|------|--------|
| profile | 服务配置文件 | IP_DATA, VOICE, VIDEO |
| bw_kbps | 下行带宽 (kbps) | 5000 |
| ret_bw_kbps | 上行带宽 (可选) | 256 |

**请求内容**:
- Client-Credentials
- Communication-Request-Parameters:
  - Profile-Name
  - Requested-Bandwidth
  - Requested-Return-Bandwidth
  - QoS-Level, Priority-Class 等

**预期响应**:
```
✓ MCAA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  Link-ID: LINK-001-2025102812345
  Granted-Bandwidth: 5000000 bps
  Gateway-IP: 10.0.0.1
  状态转换: IDLE → ACTIVE
```

**服务端状态机**:
```
IDLE → [收到 MCAR+Comm-Params] → 验证 → 分配资源 → ACTIVE
                                   5步流水线:
                                     1. 验证凭证
                                     2. 解析通信参数
                                     3. 分配链路资源
                                     4. 配置 Bearer/QoS
                                     5. 返回带资源信息的 MCAA
```

---

### 2.4 场景 B+C: 0-RTT 接入 + 订阅 (组合场景)

**目的**: 同时建立通信并订阅状态推送

**命令**:
```
MAGIC> mcar connect IP_DATA 5000 subscribe 3
```

**参数说明**:
- 前半部分与场景 C 相同
- `subscribe <level>` 子命令追加订阅请求

**请求内容**:
- Client-Credentials
- REQ-Status-Info = 3 (订阅级别)
- Communication-Request-Parameters (通信参数)

**预期响应**:
```
✓ MCAA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  REQ-Status-Info: 3 (MAGIC_DLM_Status)
  Link-ID: LINK-002-2025102812346
  Granted-Bandwidth: 5000000 bps
  状态转换: IDLE → ACTIVE
  (认证成功后将开始接收 MSCR 状态推送)
```

---

## 3. MCCR 测试场景

MCCR (MAGIC-Communication-Change-Request) 用于管理已建立的通信链路。

**前提条件**: 客户端必须处于 `AUTHENTICATED` 或 `ACTIVE` 状态。

### 3.1 场景 A: 建立通信

**目的**: 从已认证状态建立通信链路

**命令**:
```
MAGIC> mccr start IP_DATA 512 5000
```

**参数说明**:
| 参数 | 描述 |
|------|------|
| profile | 服务配置文件 |
| req_bw | 上行请求带宽 (kbps) |
| ret_bw | 下行请求带宽 (kbps) |

**请求内容**:
- Communication-Request-Parameters
- Change-Type: START (1)

**预期响应**:
```
✓ MCCA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  Link-ID: LINK-003-2025102812347
  Granted-Bandwidth: 5120000 bps
  状态转换: AUTHENTICATED → ACTIVE
```

**服务端状态机**:
```
AUTHENTICATED → [收到 MCCR START] → 资源检查 → 分配链路 → ACTIVE
                                      4阶段流水线:
                                        Phase 1: 解析请求
                                        Phase 2: 资源可用性检查
                                        Phase 3: 资源分配
                                        Phase 4: 返回 MCCA
```

---

### 3.2 场景 B: 修改带宽

**目的**: 动态调整现有链路的带宽

**命令**:
```
MAGIC> mccr modify 1024 10000
```

**参数说明**:
- `1024`: 新的上行带宽 (kbps)
- `10000`: 新的下行带宽 (kbps)

**请求内容**:
- Communication-Request-Parameters (新参数)
- Change-Type: MODIFY (2)
- Link-ID (当前链路)

**预期响应**:
```
✓ MCCA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  Granted-Bandwidth: 10240000 bps (或降级值)
  (链路 LINK-003 带宽已调整)
```

**降级处理**:
如果请求的带宽超过可用资源，服务器可能授予较低的值：
```
⚠ 带宽被降级
  请求: 10000 kbps
  授予: 5000 kbps
```

---

### 3.3 场景 C: 切换 DLM

**目的**: 在不同数据链路管理器间切换

**命令**:
```
MAGIC> mccr switch VSAT
```

或

```
MAGIC> mccr switch 2
```

**参数说明**:
- DLM 名称 (如 INMARSAT, VSAT, L-Band)
- 或 DLM ID 数字

**请求内容**:
- DLM-Name 或 DLM-ID
- Change-Type: SWITCH (3)
- 保留当前带宽/QoS 参数

**预期响应**:
```
✓ MCCA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  New-DLM: VSAT
  Link-ID: LINK-004-2025102812348 (可能是新链路)
  (链路已切换到新 DLM)
```

---

### 3.4 场景 D: 释放链路

**目的**: 主动释放通信资源

**命令**:
```
MAGIC> mccr stop
```

**请求内容**:
- Link-ID (当前链路)
- Change-Type: STOP (4)
- Termination-Cause (可选)

**预期响应**:
```
✓ MCCA 应答接收成功
  Result-Code: 2001 (SUCCESS)
  (链路 LINK-003 已释放)
  状态转换: ACTIVE → AUTHENTICATED
```

**验证**:
```
MAGIC> status
```
确认状态为 `AUTHENTICATED` 且无活跃链路。

---

## 4. 服务器推送消息处理

### 4.1 MSCR (状态变更报告)

**触发条件**: 
- 客户端已订阅状态 (mcar subscribe)
- 服务器检测到状态变更

**自动处理流程**:
```
服务器 → [MSCR] → 客户端解析 → 显示状态 → [MSCA] → 服务器
```

**客户端输出示例**:
```
╔══════════════════════════════════════════════════════════════╗
║  📡 收到 MSCR 状态变更推送                                   ║
║  时间: 2025-10-28 12:45:00                                   ║
╠══════════════════════════════════════════════════════════════╣
║ Command-Code: 100003, Application-ID: 16777300
║ REQ-Status-Info: 3 (MAGIC_DLM_Status)
║ DLM 状态信息:
  ┌─ DLM-List ─────────────────────────────────────────────┐
  │ DLM[1] INMARSAT: ONLINE
  │ DLM[2] VSAT: ONLINE
  │ DLM[3] L-Band: DEGRADED
  └──────────────────────────────────────────────────────────┘
║ 链路状态信息:
  ┌─ Link-List ────────────────────────────────────────────┐
  │ Link #1: LINK-001 [在线]
  │   Available BW: 8192.00 kbps
  │ 总链路数: 1
  └──────────────────────────────────────────────────────────┘
╚══════════════════════════════════════════════════════════════╝
→ 已发送 MSCA 确认应答 (Result-Code=2001)
```

### 4.2 MNTR (会话通知)

**触发条件**:
- 服务器需要通知客户端会话状态变更
- 资源变化、链路故障等事件

**通知类型**:
| 类型值 | 名称 | 描述 |
|--------|------|------|
| 1 | SESSION_TIMEOUT | 会话即将超时 |
| 2 | RESOURCE_RELEASED | 资源已释放 |
| 3 | LINK_DOWN | 链路断开 |
| 4 | LINK_UP | 链路恢复 |
| 5 | QUEUE_POSITION_CHANGE | 排队位置变化 |
| 6 | BANDWIDTH_REDUCED | 带宽降低 |
| 7 | BANDWIDTH_INCREASED | 带宽增加 |
| 8 | SERVICE_UNAVAILABLE | 服务不可用 |
| 9 | SERVICE_RESTORED | 服务恢复 |
| 10 | FORCED_DISCONNECT | 强制断开 |

**客户端输出示例**:
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
║  时间: 2025-10-28 12:50:00                                   ║
╠══════════════════════════════════════════════════════════════╣
║ Command-Code: 100002, Application-ID: 16777300
║ 通知类型: 3 (LINK_DOWN)
║ ⚠ 链路/资源已释放，状态将变为 AUTHENTICATED
║ Session-Id: magic-client.aero;1234567890;1
║ 通信报告参数:
║   授予带宽: 0.00 kbps
╚══════════════════════════════════════════════════════════════╝
→ 已发送 MNTA 确认应答 (Result-Code=2001)
```

**状态自动更新**:
- LINK_DOWN/RESOURCE_RELEASED/FORCED_DISCONNECT → 状态变为 AUTHENTICATED
- LINK_UP → 状态恢复为 ACTIVE

---

## 5. 完整测试流程示例

### 5.1 标准认证流程

```
# 1. 启动客户端
./magic_client

# 2. 查看初始状态
MAGIC> status

# 3. 纯认证
MAGIC> mcar auth

# 4. 查看认证后状态
MAGIC> status

# 5. 建立通信
MAGIC> mccr start IP_DATA 512 5000

# 6. 查看活跃链路
MAGIC> status

# 7. 调整带宽
MAGIC> mccr modify 1024 8000

# 8. 释放链路
MAGIC> mccr stop

# 9. 终止会话
MAGIC> str

# 10. 退出
MAGIC> quit
```

### 5.2 0-RTT 快速接入流程

```
# 1. 一步完成认证+接入+订阅
MAGIC> mcar connect IP_DATA 5000 subscribe 3

# 2. 验证状态
MAGIC> status

# 3. 等待 MSCR 状态推送 (自动)

# 4. 修改带宽
MAGIC> mccr modify 2048 10000

# 5. 释放并退出
MAGIC> mccr stop
MAGIC> str
MAGIC> quit
```

### 5.3 订阅级别验证

```
# 测试各订阅级别
MAGIC> mcar subscribe 1    # MAGIC_Status
MAGIC> str
MAGIC> mcar subscribe 2    # DLM_Status
MAGIC> str
MAGIC> mcar subscribe 3    # MAGIC_DLM_Status (推荐)
MAGIC> str
MAGIC> mcar subscribe 6    # DLM_Link_Status
MAGIC> str
MAGIC> mcar subscribe 7    # All_Status

# 测试无效级别 (应被拒绝)
MAGIC> mcar subscribe 4    # 保留 → 验证失败
MAGIC> mcar subscribe 5    # 保留 → 验证失败
```

---

## 6. 错误码参考

### 6.1 标准 Diameter Result-Code

| 值 | 名称 | 含义 |
|----|------|------|
| 2001 | DIAMETER_SUCCESS | 请求成功 |
| 3001 | DIAMETER_COMMAND_UNSUPPORTED | 命令不支持 |
| 3002 | DIAMETER_UNABLE_TO_DELIVER | 无法传递 |
| 3003 | DIAMETER_REALM_NOT_SERVED | 域不可服务 |
| 4001 | DIAMETER_AUTHENTICATION_REJECTED | 认证被拒绝 |
| 5001 | DIAMETER_AVP_UNSUPPORTED | AVP 不支持 |
| 5012 | DIAMETER_UNABLE_TO_COMPLY | 无法满足请求 |

### 6.2 MAGIC 扩展状态码 (10053)

| 值 | 名称 | 含义 |
|----|------|------|
| 0 | SUCCESS | 操作成功 |
| 1 | INVALID_CREDENTIALS | 凭证无效 |
| 2 | RESOURCE_UNAVAILABLE | 资源不可用 |
| 3 | QUOTA_EXCEEDED | 配额超限 |
| 4 | SESSION_NOT_FOUND | 会话未找到 |
| 5 | INVALID_STATE | 状态无效 |
| 6 | INVALID_PARAMETER | 参数无效 |
| 7 | DLM_UNAVAILABLE | DLM 不可用 |
| 8 | LINK_UNAVAILABLE | 链路不可用 |

---

## 7. 故障排查

### 7.1 常见问题

**Q: 收到 "订阅级别无效" 错误**
```
A: 检查订阅级别是否为有效值 (0,1,2,3,6,7)
   级别 4 和 5 是保留值，会被拒绝
```

**Q: MSCR 推送没有收到**
```
A: 确认:
   1. 已使用 mcar subscribe 订阅
   2. 服务器端确实有状态变化事件
   3. 处理器已正确注册 (启动时应显示成功消息)
```

**Q: 0-RTT 接入失败**
```
A: 检查:
   1. 服务器资源是否可用
   2. 请求的带宽是否合理
   3. 配置文件名称是否正确
```

**Q: 带宽请求被降级**
```
A: 这是正常行为。当请求超过可用资源时，服务器会授予较低值。
   检查 Granted-Bandwidth 与 Requested-Bandwidth 的差异。
```

### 7.2 调试命令

```
# 查看完整状态
MAGIC> status

# 查看配置详情
MAGIC> config show

# 查看 Diameter 连接状态
MAGIC> peers

# 查看帮助
MAGIC> help
```

---

## 8. 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| 1.0 | 2025-10-28 | 初始版本，完整 MCAR/MCCR 测试指导 |
| 1.1 | 2025-10-28 | 添加 B+C 组合场景，MSCR/MNTR 处理器说明 |

---

*本文档基于 ARINC 839-2014 MAGIC 协议规范编写*
