# MAGIC MACR/MACA 技术说明与测试指导

## 1. 概述

MACR (Accounting Control Request) 和 MACA (Accounting Control Answer) 是 MAGIC 系统中用于**计费控制**的 Diameter 消息对。

其核心功能是实现**"不断网切账单" (CDR Rollover)**：在不中断用户数据传输的情况下，强制结束当前的计费记录（CDR）并立即开启一个新的计费记录。这通常用于跨天结算、套餐切换或管理员手动干预。

---

## 2. 协议规范

### 2.1 MACR (Accounting Control Request)

**消息方向**: Client/Admin → Server

**用途**: 请求对指定会话执行 CDR 切分操作。

#### 核心 AVP 列表

| AVP 名称 | AVP Code | 数据类型 | M/O | 说明 |
|---------|----------|---------|-----|------|
| Session-Id | 263 | UTF8String | M | 请求方的会话 ID |
| Origin-Host | 264 | DiameterIdentity | M | 请求方主机名 |
| Origin-Realm | 296 | DiameterIdentity | M | 请求方域名 |
| Destination-Realm | 283 | DiameterIdentity | M | 目标域名 |
| **CDR-Restart-Session-Id** | 10048 | UTF8String | M | **目标被操作会话的 ID** |

### 2.2 MACA (Accounting Control Answer)

**消息方向**: Server → Client/Admin

**用途**: 返回计费控制操作的结果。

#### 核心 AVP 列表

| AVP 名称 | AVP Code | 数据类型 | M/O | 说明 |
|---------|----------|---------|-----|------|
| Result-Code | 268 | Unsigned32 | M | 操作结果 (2001=成功) |
| Origin-Host | 264 | DiameterIdentity | M | 服务器主机名 |
| Origin-Realm | 296 | DiameterIdentity | M | 服务器域名 |
| CDR-Restart-Session-Id | 10048 | UTF8String | M | 回显目标会话 ID |
| **CDRs-Updated** | 20016 | Grouped | O | 更新的 CDR 信息组 |

#### CDRs-Updated 子 AVP

| AVP 名称 | AVP Code | 数据类型 | 说明 |
|---------|----------|---------|------|
| CDR-Start-Stop-Pair | 20018 | Grouped | 包含旧/新 CDR ID 对 |
| └─ CDR-Stopped | 10049 | Unsigned32 | 被停止的旧 CDR ID |
| └─ CDR-Started | 10050 | Unsigned32 | 新启动的新 CDR ID |

---

## 3. 工作原理

当 MAGIC Server 收到 MACR 请求时，执行以下原子操作：

1. **权限校验**: 检查请求方是否有权控制目标会话（通常要求是会话所有者或具有 `allow_cdr_control` 权限的管理员）。
2. **流量快照**: 通过 Netlink 从内核获取目标会话当前的实时流量统计（`bytes_in`/`bytes_out`）。
3. **归档旧 CDR**: 将当前活跃的 CDR 状态设为 `ROLLOVER`，记录最终流量并保存到归档目录。
4. **创建新 CDR**: 立即创建一个新的 `ACTIVE` 状态 CDR，并将当前流量值设为 `base_offset`。
5. **重置计数**: 在内存中重置该会话的流量起始时间和增量计数器。
6. **返回应答**: 在 MACA 中告知客户端旧 CDR 已停止，新 CDR 已启动。

---

## 4. 测试指导

### 4.1 环境准备

1. **启动服务器**: 确保 `freeDiameterd` 已加载 `app_magic` 扩展。
2. **建立会话**: 客户端（如 `magic_client`）已成功登录并建立活动会话。
3. **产生流量**: 在会话期间产生一些网络流量（如 `ping`），确保 `bytes_in/out > 0`。

### 4.2 使用 magic_client 进行测试

`magic_client` 提供了 `macr` 命令来触发切账单操作。

#### 步骤 1: 查看当前会话
在 `magic_client` 交互界面输入：
```bash
magic> msxr
```
记录下你的 `Session-Id`（例如：`magic.client;1703318400;1`）。

#### 步骤 2: 查看当前计费信息
```bash
magic> madr data 275
```
(注：如果不知道 CDR ID，可以先执行 `madr list` 获取)
确认 `BYTES_IN` 和 `BYTES_OUT` 有数值。

#### 步骤 3: 执行 MACR 切账单
```bash
# 命令格式: macr restart <target_session_id>
magic> macr restart magic.client;1703318400;1
```

**预期输出**:
```
=== MACA Response ===
Result-Code: 2001 (DIAMETER_SUCCESS)
Target-Session: magic.client;1703318400;1
[CDRs-Updated]
  CDR-Pair: Stopped=275, Started=276
```

#### 步骤 4: 验证结果
再次查看计费信息：
```bash
magic> madr data 276
```
**预期现象**:
- `CDR_ID` 已变为新的值（如 276）。
- `BYTES_IN` 和 `BYTES_OUT` 已重置为接近 0 的值（仅包含切分后的新流量）。
- `START_TIME` 已更新为切分时刻的时间。

### 4.3 异常场景测试

| 场景 | 操作 | 预期结果 |
|------|------|---------|
| **无效会话** | `macr restart invalid_id` | Result-Code: 5002 (UNKNOWN_SESSION_ID) |
| **权限不足** | 使用普通用户切分他人会话 | Result-Code: 5003 (AUTHORIZATION_REJECTED) |
| **重复切分** | 连续快速执行两次 `macr restart` | 两次均应成功，生成两个连续的 CDR |

---

## 5. 常见问题排查

### 5.1 Result-Code 5012 (UNABLE_TO_COMPLY)
- **原因**: 服务器内部错误，通常是 CDR 文件写入失败或 Netlink 查询超时。
- **排查**: 检查服务器日志 `/var/log/freediameter.log` 和 `/var/lib/magic/cdr` 目录权限。

### 5.2 切分后流量未重置
- **原因**: 内核 `nf_conntrack_acct` 未开启，导致服务器无法获取准确的快照值。
- **排查**: 执行 `sysctl net.netfilter.nf_conntrack_acct` 确保其值为 1。

---

## 6. 相关配置

在 `Client_Profile.xml` 中，可以通过以下属性控制权限：

```xml
<SessionPolicy>
    <!-- 是否允许该客户端执行 MACR 操作控制其他会话 -->
    <AllowCDRControl>true</AllowCDRControl>
</SessionPolicy>
```

---
*MAGIC System Development Team*
