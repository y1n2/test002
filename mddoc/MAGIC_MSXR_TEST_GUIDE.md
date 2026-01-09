# MAGIC MSXR/MSXA 状态查询机制测试指导

## 1. 概述

### 1.1 什么是 MSXR/MSXA？

- **MSXR** (MAGIC Status Request): 客户端主动查询系统状态的请求消息
- **MSXA** (MAGIC Status Answer): 服务器返回的状态应答消息
- **Command-Code**: 100004
- **方向**: 客户端 → 服务器（请求-应答模式）

### 1.2 状态查询的作用

MSXR是MAGIC协议中客户端**主动查询**系统状态的机制，用于：
- 📊 查询系统状态（已注册客户端列表）
- 🔗 查询DLM状态（链路管理器可用性）
- 📡 查询链路详细状态（信号强度、带宽、连接状态）
- 🔍 按需获取状态信息（无需等待MSCR推送）

### 1.3 与MSCR的区别

| 特性 | MSXR/MSXA | MSCR/MSCA |
|------|-----------|-----------|
| 触发方式 | 客户端主动查询 | 服务器主动推送 |
| 使用场景 | 按需查询 | 状态变化通知 |
| 频率控制 | 速率限制（如5秒/次） | 事件驱动 |
| 数据实时性 | 查询时刻的快照 | 变化时立即通知 |

---

## 2. Status-Type 状态类型

| 类型值 | 名称 | 含义 | 返回内容 |
|-------|------|------|---------|
| 0 | No_Status | 无状态 | 仅应答成功，不返回具体信息 |
| 1 | MAGIC_Status | 系统状态 | Registered-Clients（已注册客户端） |
| 2 | DLM_Status | DLM状态 | DLM-List（链路管理器列表） |
| 3 | MAGIC_DLM_Status | 系统+DLM状态 | Registered-Clients + DLM-List |
| 6 | DLM_Link_Status | DLM链路详细状态 | DLM-Info with Link-Status-List |
| 7 | All_Status | 全部状态 | 所有可用状态信息 |

**注意**: 
- 类型4、5保留未使用
- 类型6、7需要更高权限（`allow_detailed_status=true`）
- 服务器可能根据权限**降级**返回的Status-Type

---

## 3. MSXR 消息结构

### 3.1 请求消息 (MSXR)

```
MSXR (Command-Code: 100004, R-bit: 1)
├─ Session-Id               [必填] 会话标识
├─ Auth-Application-Id      [必填] 应用ID (100001)
├─ Origin-Host              [必填] 客户端主机名
├─ Origin-Realm             [必填] 客户端域名
├─ Destination-Realm        [必填] 服务器域名
└─ Status-Type              [必填] 状态类型 (0-7)
```

### 3.2 应答消息 (MSXA)

```
MSXA (Command-Code: 100004, R-bit: 0)
├─ Session-Id               [必填] 会话标识
├─ Result-Code              [必填] 结果码 (2001/3004/5xxx)
├─ Origin-Host              [必填] 服务器主机名
├─ Origin-Realm             [必填] 服务器域名
├─ Status-Type              [可选] 实际授予的状态类型
└─ [状态信息AVP - 根据Status-Type]
   ├─ Registered-Clients    (Type 1,3,7) 已注册客户端
   ├─ DLM-List              (Type 2,3)   DLM列表
   └─ DLM-Info              (Type 6,7)   详细链路信息
      └─ DLM-Link-Status-List
         └─ Link-Status-Group[] (多个链路)
            ├─ Link-Name
            ├─ Link-Number
            ├─ Link-Connection-Status
            ├─ Signal-Strength (dBm)
            ├─ Max-Bandwidth
            └─ Allocated-Bandwidth
```

### 3.3 Result-Code 结果码

| 结果码 | 名称 | 含义 | 客户端行为 |
|-------|------|------|-----------|
| 2001 | SUCCESS | 查询成功 | 解析状态信息 |
| 3004 | DIAMETER_TOO_BUSY | 速率限制 | 等待后重试 |
| 5001 | DIAMETER_AVP_UNSUPPORTED | AVP不支持 | 检查字典 |
| 5012 | DIAMETER_UNABLE_TO_COMPLY | 无法处理 | 检查会话状态 |

---

## 4. 测试前提条件

### 4.1 环境准备

- ✅ 服务器和客户端freeDiameter进程运行
- ✅ 客户端已通过MCAR认证（状态≥AUTHENTICATED）
- ✅ 服务器端DLM模块运行（SATCOM/WIFI/CELLULAR）

### 4.2 检查字典加载状态

启动客户端时应看到：
```
[客户端日志]
✓ Command-Code 100004 字典对象已加载
✓ Status-Type AVP (10002) 已注册
```

### 4.3 权限配置

服务器端配置文件 (`magic_server.conf`) 中客户端权限设置：
```xml
<client id="client.magic.example.com">
  <session>
    <msxr_rate_limit_sec>5</msxr_rate_limit_sec>
    <allow_detailed_status>true</allow_detailed_status>
    <allow_registered_clients>true</allow_registered_clients>
    <dlm_whitelist>SATCOM,WIFI</dlm_whitelist>
  </session>
</client>
```

**权限参数说明**：
- `msxr_rate_limit_sec`: MSXR请求速率限制（秒）
- `allow_detailed_status`: 是否允许查询详细链路状态（Type 6/7）
- `allow_registered_clients`: 是否允许查看其他客户端列表
- `dlm_whitelist`: 允许查询的DLM白名单

---

## 5. 测试场景

### 场景 A: 查询系统状态 (MAGIC_Status)

**目标**: 查看当前有哪些客户端已注册到系统

**测试步骤**：
```bash
# 1. 客户端建立会话
MAGIC> mcar auth

# 2. 查询系统状态
MAGIC> msxr 1
```

**预期结果**：
```
[INFO] 查询系统状态 (MSXR v2.1)...
  Status-Type: 1 (MAGIC_Status)
[SUCCESS] MSXR 请求已发送！
等待服务器返回状态信息...

📌 MSXR/MSXA 状态查询应答:
=== 系统状态信息 (MSXA v2.1) ===
  Status-Type: 1 (MAGIC_Status)
  Registered-Clients: client1.magic.example.com,client2.magic.example.com
==================
```

**权限测试**：
如果配置 `allow_registered_clients=false`，则不会返回Registered-Clients AVP。

---

### 场景 B: 查询DLM状态 (DLM_Status)

**目标**: 查看DLM链路管理器可用性（SATCOM/WIFI/CELLULAR）

**测试步骤**：
```bash
# 1. 客户端已认证
MAGIC> status
当前状态: AUTHENTICATED

# 2. 查询DLM状态
MAGIC> msxr 2
```

**预期结果**：
```
=== 系统状态信息 (MSXA v2.1) ===
  Status-Type: 2 (DLM_Status)
┌─ DLM-List ─────────────────────────────────────────────┐
│ DLM[1] SATCOM: ✓ONLINE
│ DLM[2] WIFI: ✓ONLINE
│ DLM[3] CELLULAR: ✗OFFLINE
└──────────────────────────────────────────────────────────┘
==================
```

**白名单测试**：
如果配置 `dlm_whitelist=SATCOM,WIFI`，则不会返回CELLULAR的状态信息。

---

### 场景 C: 查询综合状态 (MAGIC_DLM_Status)

**目标**: 同时获取系统状态和DLM状态

**测试步骤**：
```bash
MAGIC> msxr 3
```

**预期结果**：
```
=== 系统状态信息 (MSXA v2.1) ===
  Status-Type: 3 (MAGIC_DLM_Status)
  Registered-Clients: client1.magic.example.com,client2.magic.example.com
┌─ DLM-List ─────────────────────────────────────────────┐
│ DLM[1] SATCOM: ✓ONLINE
│ DLM[2] WIFI: ✓ONLINE
└──────────────────────────────────────────────────────────┘
==================
```

---

### 场景 D: 查询详细链路状态 (DLM_Link_Status)

**目标**: 获取每个DLM下所有链路的详细信息（信号强度、带宽、连接状态）

**前提**: 服务器配置 `allow_detailed_status=true`

**测试步骤**：
```bash
# 1. 请求详细链路状态
MAGIC> msxr 6
```

**预期结果**：
```
=== 系统状态信息 (MSXA v2.1) ===
  Status-Type: 6 (DLM_Link_Status)
┌─ DLM-Info ─────────────────────────────────────────────┐
│ DLM-ID: 1
│ DLM-Name: SATCOM
│ DLM-Available: ✓AVAILABLE
│ Max-Links: 4
│ Allocated-Links: 2
│ Max-Forward-BW: 10000.0 kbps
│ Allocated-Forward-BW: 3000.0 kbps
│ Link-Status-List:
│   Link[1] LINK_SATCOM_1  ✓CONNECTED  Signal:-78dBm
│   Link[2] LINK_SATCOM_2  ✗DISCONNECTED  Signal:-95dBm
└──────────────────────────────────────────────────────────┘

┌─ DLM-Info ─────────────────────────────────────────────┐
│ DLM-ID: 2
│ DLM-Name: WIFI
│ DLM-Available: ✓AVAILABLE
│ Link-Status-List:
│   Link[1] LINK_WIFI_2.4G  ✓CONNECTED  Signal:-65dBm
│   Link[2] LINK_WIFI_5G    ✓CONNECTED  Signal:-58dBm
└──────────────────────────────────────────────────────────┘
```

**权限降级测试**：
如果配置 `allow_detailed_status=false`，服务器会**降级**返回：
```
⚠ 权限降级! 请求=6 (DLM_Link_Status) → 授予=2 (DLM_Status)
  您可能没有查看详细链路状态的权限
```

---

### 场景 E: 查询全部状态 (All_Status)

**目标**: 获取所有可用的状态信息（系统+DLM+链路）

**前提**: `allow_detailed_status=true` 且 `allow_registered_clients=true`

**测试步骤**：
```bash
MAGIC> msxr 7
```

**预期结果**：
完整输出包含：
- Registered-Clients（客户端列表）
- DLM-Info[]（所有DLM的详细信息）
- Link-Status-List[]（所有链路状态）

---

### 场景 F: 速率限制测试

**目标**: 验证服务器的MSXR速率限制功能

**测试步骤**：
```bash
# 1. 快速连续发送多个MSXR请求
MAGIC> msxr 1
MAGIC> msxr 1
MAGIC> msxr 1
MAGIC> msxr 1
```

**预期结果**：
```
# 第1次请求
[SUCCESS] MSXR 请求已发送！
=== 系统状态信息 (MSXA v2.1) ===
  Status-Type: 1 (MAGIC_Status)
  ...

# 第2-4次请求（间隔 < 5秒）
[ERROR] MSXA 应答返回错误: Result-Code=3004
[WARN] 服务器忙碌或速率限制，请稍后重试
```

**服务器日志**：
```
[NOTICE] Rate limit exceeded! Returning DIAMETER_TOO_BUSY (3004)
✓ Sent MSXA (Rate Limited - 3004)
```

**等待5秒后重试**：
```bash
# 等待 5+ 秒
MAGIC> msxr 1
[SUCCESS] MSXR 请求已发送！  # 成功
```

---

### 场景 G: 权限降级测试

**目标**: 验证服务器根据权限自动降级Status-Type

**测试步骤**：
```bash
# 1. 服务器配置 allow_detailed_status=false
# 2. 客户端请求详细状态
MAGIC> msxr 6
```

**预期结果（客户端）**：
```
⚠ 权限降级! 请求=6 (DLM_Link_Status) → 授予=2 (DLM_Status)
  您可能没有查看详细链路状态的权限

┌─ DLM-List ─────────────────────────────────────────────┐
│ DLM[1] SATCOM: ✓ONLINE    ← 仅基本状态，无链路详情
└──────────────────────────────────────────────────────────┘
```

**服务器日志**：
```
[NOTICE] Permission downgrade: 6 -> 2 (detailed status not allowed)
```

**降级映射规则**：
- 请求Type 6 (DLM_Link_Status) → 降级为 Type 2 (DLM_Status)
- 请求Type 7 (All_Status) → 降级为 Type 3 (MAGIC_DLM_Status)

---

### 场景 H: 白名单过滤测试

**目标**: 验证DLM白名单功能

**测试步骤**：
```bash
# 1. 服务器配置 dlm_whitelist=SATCOM,WIFI
# 2. 客户端查询DLM状态
MAGIC> msxr 2
```

**预期结果**：
```
┌─ DLM-List ─────────────────────────────────────────────┐
│ DLM[1] SATCOM: ✓ONLINE
│ DLM[2] WIFI: ✓ONLINE
  ← CELLULAR不在返回结果中（白名单过滤）
└──────────────────────────────────────────────────────────┘
```

**服务器日志**：
```
[DEBUG] DLM whitelist filter: SATCOM (allowed)
[DEBUG] DLM whitelist filter: WIFI (allowed)
[DEBUG] DLM whitelist filter: CELLULAR (blocked by whitelist)
```

---

### 场景 I: 链路状态动态变化测试

**目标**: 验证MSXR能实时反映链路状态变化

**测试步骤**：
```bash
# 1. 查询初始状态
MAGIC> msxr 6

# 2. 服务器端停止WIFI链路
[服务器终端]
sudo ip link set ens38 down

# 3. 客户端再次查询
MAGIC> msxr 6
```

**预期结果（链路down后）**：
```
┌─ DLM-Info ─────────────────────────────────────────────┐
│ DLM-Name: WIFI
│ DLM-Available: ✗UNAVAILABLE    ← 状态变为不可用
│ Link-Status-List:
│   Link[1] LINK_WIFI_2.4G  ✗DISCONNECTED  Signal:-100dBm
└──────────────────────────────────────────────────────────┘
```

**恢复链路测试**：
```bash
# 服务器端恢复链路
sudo ip link set ens38 up

# 客户端查询
MAGIC> msxr 6
# 应看到状态恢复为AVAILABLE
```

---

## 6. 客户端实现示例

### 6.1 定时状态查询

客户端可实现定时查询，用于监控系统状态：

```c
/* 定时器示例 - 每30秒查询一次DLM状态 */
void* status_monitor_thread(void* arg) {
    while (g_running) {
        if (g_client_state >= CLIENT_STATE_AUTHENTICATED) {
            /* 发送MSXR请求 */
            char* argv[] = {"msxr", "2", NULL};
            cmd_msxr(2, argv);
        }
        
        sleep(30);  /* 等待30秒 */
    }
    return NULL;
}
```

### 6.2 UI状态显示

```c
/* 更新UI - 显示链路状态 */
void update_link_status_ui() {
    /* 查询所有链路详细状态 */
    char* argv[] = {"msxr", "7", NULL};
    cmd_msxr(2, argv);
    
    /* 解析MSXA应答后更新UI图标 */
    // ✓ SATCOM: 信号强度 -78dBm
    // ✗ WIFI: 断开
    // ✓ CELLULAR: 信号强度 -85dBm
}
```

---

## 7. 服务器端实现要点

### 7.1 速率限制实现

```c
/* 速率限制检查 */
int msxr_check_rate_limit(const char* client_id, uint32_t limit_sec) {
    time_t now = time(NULL);
    time_t* last_time = get_last_msxr_time(client_id);
    
    if (last_time && (now - *last_time) < limit_sec) {
        return -1;  /* 超频 */
    }
    
    *last_time = now;
    return 0;  /* 允许 */
}
```

### 7.2 权限降级实现

```c
/* 权限裁决 */
uint32_t apply_permission_policy(ClientProfile* profile, uint32_t requested_type) {
    /* 规则: 详细信息控制 */
    if ((requested_type == 6 || requested_type == 7) &&
        !profile->allow_detailed_status) {
        /* 降级映射 */
        return (requested_type == 6) ? 2 : 3;
    }
    
    return requested_type;  /* 无需降级 */
}
```

### 7.3 白名单过滤实现

```c
/* DLM白名单过滤 */
bool is_dlm_allowed(ClientProfile* profile, const char* dlm_name) {
    if (!profile->dlm_whitelist || strlen(profile->dlm_whitelist) == 0) {
        return true;  /* 无白名单限制 */
    }
    
    /* 检查DLM名称是否在白名单中 */
    return strstr(profile->dlm_whitelist, dlm_name) != NULL;
}
```

---

## 8. 日志分析

### 8.1 客户端日志关键词

**正常查询**：
```
[INFO] 查询系统状态 (MSXR v2.1)...
[SUCCESS] MSXR 请求已发送！
=== 系统状态信息 (MSXA v2.1) ===
```

**权限降级**：
```
⚠ 权限降级! 请求=7 (All_Status) → 授予=3 (MAGIC_DLM_Status)
  您可能没有查看详细链路状态的权限
```

**速率限制**：
```
[ERROR] MSXA 应答返回错误: Result-Code=3004
[WARN] 服务器忙碌或速率限制，请稍后重试
```

### 8.2 服务器日志关键词

**收到MSXR**：
```
[app_magic] ========================================
[app_magic] MSXR (Status Request) v2.1
[app_magic]   Client-ID: client.magic.example.com
[app_magic]   Status-Type: 6 (DLM_Link_Status)
```

**权限处理**：
```
[app_magic]   Permission downgrade: 6 -> 2 (detailed status not allowed)
[app_magic]   Registered-Clients hidden (permission denied)
[app_magic]   DLM whitelist filter: CELLULAR (blocked)
```

**速率限制**：
```
[app_magic]   Rate limit exceeded! Returning DIAMETER_TOO_BUSY (3004)
[app_magic] ✓ Sent MSXA (Rate Limited - 3004)
```

**正常应答**：
```
[app_magic]   Registered-Clients: client1,client2
[app_magic]   Adding DLM: SATCOM (Available=1)
[app_magic] ✓ Sent MSXA (Result-Code=2001)
```

---

## 9. 常见问题

### Q1: MSXA返回Result-Code=3004

**原因**: 触发了速率限制（请求过于频繁）

**解决方法**：
```bash
# 检查服务器配置的速率限制
# msxr_rate_limit_sec=5 表示最多5秒一次

# 客户端等待后重试
sleep 6
MAGIC> msxr 1
```

### Q2: 请求Type 6但只返回Type 2的内容

**原因**: 服务器降级了Status-Type（权限不足）

**解决方法**：
```bash
# 检查服务器配置
# allow_detailed_status=true/false

# 如果业务确实需要详细状态，联系管理员修改配置
```

### Q3: DLM-List中缺少某些DLM

**原因**: 触发了白名单过滤

**解决方法**：
```bash
# 检查服务器配置
# dlm_whitelist=SATCOM,WIFI
# 如果需要查询CELLULAR，需要添加到白名单

# 或者移除白名单配置（允许查询所有DLM）
```

### Q4: Registered-Clients不显示

**原因**: `allow_registered_clients=false`

**解决方法**：
```bash
# 这是安全策略，普通客户端不应看到其他客户端列表
# 如果是管理员客户端，修改配置 allow_registered_clients=true
```

### Q5: MSXR无应答

**可能原因**：
1. 客户端未认证
   - 检查状态: `status`
   - 先执行: `mcar auth`

2. 网络问题
   - 检查连接: `netstat -an | grep 3868`
   - 查看日志: 是否有连接错误

3. 字典未加载
   - 检查启动日志: 是否有"Command-Code 100004"加载成功

---

## 10. 性能测试

### 10.1 压力测试

**目标**: 测试服务器处理大量MSXR请求的能力

```bash
#!/bin/bash
# 批量MSXR压力测试脚本

for i in {1..100}; do
    echo "msxr 3" | ./magic_client -c client_$i.conf &
    sleep 0.1
done

wait
```

**预期**：
- 前几次请求正常返回（Result-Code=2001）
- 后续请求触发速率限制（Result-Code=3004）
- 服务器CPU/内存占用正常

### 10.2 状态更新延迟测试

**目标**: 测量MSXR查询到的状态与实际状态的延迟

```bash
# T0: 停止链路
sudo ip link set ens38 down

# T1: 立即查询状态
MAGIC> msxr 6

# 检查WIFI的Link-Connection-Status是否已变为DISCONNECTED
# 计算延迟: T1 - T0
```

**预期延迟**: < 1秒（取决于DLM更新频率）

---

## 11. 最佳实践

### 11.1 客户端建议

1. **合理的查询频率**
   - 不要过于频繁查询（遵守速率限制）
   - 建议间隔 ≥ 10秒
   - 使用MSCR订阅代替高频查询

2. **权限降级处理**
   - 检查返回的Status-Type是否被降级
   - 向用户提示权限不足

3. **缓存状态信息**
   - 本地缓存MSXA结果
   - 避免重复查询相同内容

4. **错误处理**
   - 检查Result-Code
   - 对3004错误实现重试机制

### 11.2 服务器端建议

1. **合理的速率限制**
   - 默认5秒/次（`msxr_rate_limit_sec=5`）
   - 管理员客户端可设置更短间隔

2. **细粒度权限控制**
   - 普通用户: Type 2/3 (基本状态)
   - 高级用户: Type 6/7 (详细状态)
   - 管理员: 完整权限 + 客户端列表

3. **白名单策略**
   - 安全敏感环境使用DLM白名单
   - 避免泄露内部链路拓扑

4. **性能优化**
   - 缓存状态信息（避免每次查询都访问DLM）
   - 异步获取DLM状态

---

## 12. 测试检查清单

### 12.1 功能测试

- [ ] Type 0 (No_Status) - 空查询正常应答
- [ ] Type 1 (MAGIC_Status) - 返回Registered-Clients
- [ ] Type 2 (DLM_Status) - 返回DLM-List
- [ ] Type 3 (MAGIC_DLM_Status) - 返回综合状态
- [ ] Type 6 (DLM_Link_Status) - 返回详细链路信息
- [ ] Type 7 (All_Status) - 返回所有状态

### 12.2 权限测试

- [ ] 速率限制 - 连续请求触发3004
- [ ] 详细状态权限 - Type 6/7降级测试
- [ ] 客户端列表权限 - Registered-Clients过滤
- [ ] DLM白名单 - 非白名单DLM不返回

### 12.3 动态测试

- [ ] 链路状态变化 - down/up能实时反映
- [ ] DLM状态变化 - AVAILABLE/UNAVAILABLE切换
- [ ] 客户端注册变化 - Registered-Clients实时更新

### 12.4 异常测试

- [ ] 未认证状态查询 - 返回错误
- [ ] 无效Status-Type - 服务器拒绝或默认处理
- [ ] 网络中断 - 超时处理

---

## 13. 与MSCR的协同使用

### 13.1 订阅 + 查询模式

**推荐做法**：

```bash
# 1. 认证时订阅MSCR推送（获取变化通知）
MAGIC> mcar subscribe 7

# 2. 业务中按需查询MSXR（获取当前快照）
MAGIC> msxr 6

# 3. 收到MSCR通知后，再查询详细信息
[收到MSCR: DLM-Available变化]
MAGIC> msxr 6  # 查询详细链路状态
```

### 13.2 使用场景对比

| 场景 | 使用MSXR | 使用MSCR |
|------|---------|---------|
| UI初始化 | ✅ 主动查询当前状态 | ❌ 需等待推送 |
| 状态变化监控 | ❌ 轮询效率低 | ✅ 事件驱动实时通知 |
| 按需诊断 | ✅ 用户点击时查询 | ❌ 无法主动触发 |
| 后台监控 | ❌ 资源浪费 | ✅ 订阅后自动通知 |

---

## 14. 总结

### 14.1 核心要点

- ✅ MSXR是**客户端主动查询**机制，与MSCR被动推送互补
- ✅ 7种Status-Type覆盖从基本到详细的多层次状态信息
- ✅ 服务器通过**速率限制、权限降级、白名单**控制信息安全
- ✅ 适用于UI初始化、按需诊断、状态快照等场景

### 14.2 关键差异

**MSXR vs MSCR**:
- MSXR: 拉模式（Pull），客户端控制查询时机
- MSCR: 推模式（Push），服务器控制推送时机
- 配合使用: MSCR订阅变化 + MSXR按需查询

### 14.3 权限体系

```
权限级别        Type    返回内容
──────────────────────────────────────
无权限          0       无状态信息
基础权限        1-3     系统/DLM基本状态
高级权限        6-7     详细链路状态
管理员权限      ALL     完整信息+客户端列表
```

---

**文档版本**: 1.0  
**最后更新**: 2025-12-22  
**适用版本**: MAGIC Client/Server v2.1+  
**相关文档**: 
- [MAGIC_MADR_TEST_GUIDE.md](MAGIC_MADR_TEST_GUIDE.md) - MADR/MADA 计费数据查询测试指导
- [MAGIC_MNTR_TEST_GUIDE.md](MAGIC_MNTR_TEST_GUIDE.md) - MNTR/MNTA通知机制测试
- [MAGIC_FIREWALL_ROUTING_GUIDE.md](MAGIC_FIREWALL_ROUTING_GUIDE.md) - 防火墙和路由指导
