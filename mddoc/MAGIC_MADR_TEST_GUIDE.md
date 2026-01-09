# MAGIC MADR/MADA 计费数据查询测试指导

## 1. 概述

### 1.1 什么是 MADR/MADA？

- **MADR** (MAGIC Accounting Data Request): 客户端向服务器请求计费数据（CDR - Call Detail Record）的消息。
- **MADA** (MAGIC Accounting Data Answer): 服务器返回的计费数据应答消息。
- **Command-Code**: 100005
- **方向**: 客户端 → 服务器

### 1.2 计费查询的作用

MADR 用于客户端获取其在系统中的流量使用情况和计费记录，支持：
- 📋 **查询 CDR 列表**: 获取当前活跃或已完成的计费单 ID。
- 🔍 **查询详细数据**: 获取特定计费单的详细流量统计（上行/下行流量、时长等）。

---

## 2. 消息结构与参数

### 2.1 请求消息 (MADR)

```
MADR (Command-Code: 100005, R-bit: 1)
├─ Session-Id               [必填] 会话标识
├─ Origin-Host              [必填] 客户端主机名
├─ Origin-Realm             [必填] 客户端域名
├─ Destination-Realm        [必填] 服务器域名
├─ CDR-Type                 [必填] 查询类型 (1: LIST_REQUEST, 2: DATA_REQUEST)
├─ CDR-Level                [必填] 查询层级 (1: ALL, 2: USER, 3: SESSION)
└─ CDR-Request-Identifier   [可选] 具体的 CDR ID (当 Type=2 时必填)
```

### 2.2 应答消息 (MADA)

```
MADA (Command-Code: 100005, R-bit: 0)
├─ Session-Id               [必填] 会话标识
├─ Result-Code              [必填] 结果码 (2001: SUCCESS)
├─ CDR-Type                 [可选] 返回的查询类型
├─ CDR-Level                [可选] 返回的查询层级
└─ [CDR 列表 AVP]
   ├─ CDRs-Active           活跃中的计费单列表
   ├─ CDRs-Finished         已完成的计费单列表
   ├─ CDRs-Forwarded        已转发的计费单列表
   └─ CDRs-Unknown          未知状态的计费单列表
```

---

## 3. 测试场景

### 场景 A: 查询 CDR 列表 (List Request)

**目标**: 获取当前会话关联的所有计费单 ID。

**测试步骤**：
```bash
# 1. 确保客户端已登录
MAGIC> mcar auth

# 2. 执行列表查询
MAGIC> madr list
```

**预期结果**：
```
[INFO] 查询 CDR 列表 (MADR List)...
[SUCCESS] MADR 请求已发送！

📌 MADR/MADA 计费数据应答:
┌─────────────────────────────────────────────────────────┐
│                    CDR 计费信息                         │
├─────────────────────────────────────────────────────────┤
  CDR-Type: 1 (LIST_REQUEST)
  CDR-Level: 1 (ALL)
  [ACTIVE] CDR-ID: magic-client.example.com;1766393189;1
  [ACTIVE] CDR-ID: magic-client.example.com;1766393189;2
└─────────────────────────────────────────────────────────┘
```

---

### 场景 B: 查询详细 CDR 数据 (Data Request)

**目标**: 获取特定 CDR ID 的详细流量统计信息。

**测试步骤**：
```bash
# 1. 先通过 list 获取一个 CDR ID
# 2. 查询该 ID 的详细数据
MAGIC> madr data magic-client.example.com;1766393189;1
```

**预期结果**：
```
[INFO] 查询 CDR 详细数据 (MADR Data)...
[INFO]   CDR-ID: magic-client.example.com;1766393189;1
[SUCCESS] MADR 请求已发送！

📌 MADR/MADA 计费数据应答:
┌─────────────────────────────────────────────────────────┐
│                    CDR 计费信息                         │
├─────────────────────────────────────────────────────────┤
  CDR-Type: 2 (DATA_REQUEST)
  CDR-Level: 3 (SESSION_DEPENDENT)
  CDR-Request-Id: magic-client.example.com;1766393189;1
  [ACTIVE] CDR-ID: magic-client.example.com;1766393189;1
    - Start-Time: 2025-12-22 16:40:00
    - Input-Octets: 1048576 (1.00 MB)
    - Output-Octets: 524288 (0.50 MB)
    - Duration: 300s
└─────────────────────────────────────────────────────────┘
```

---

### 场景 C: 计费控制测试 (MACR/MACA)

**目标**: 验证手动触发 CDR 重启/切换功能。

**测试步骤**：
```bash
# 1. 执行 MACR 重启命令
MAGIC> macr restart magic-client.example.com;1766393189;1;magic
```

**预期结果**：
```
[INFO] 重启 CDR (MACR)...
[INFO]   Session-Id: magic-client.example.com;1766393189;1;magic
[SUCCESS] MACR 请求已发送！

📌 MACR/MACA 计费控制结果:
┌─────────────────────────────────────────────────────────┐
│                 CDR 计费控制结果                        │
├─────────────────────────────────────────────────────────┤
  目标会话: magic-client.example.com;1766393189;1;magic
  [STOPPED] CDR-ID: magic-client.example.com;1766393189;1
  [STARTED] CDR-ID: magic-client.example.com;1766393189;3
└─────────────────────────────────────────────────────────┘
```

---

## 4. 常见问题与日志分析

### 4.1 常见错误码

| Result-Code | 含义 | 解决方法 |
|-------------|------|----------|
| 5005 | MISSING_AVP | 检查是否遗漏了 CDR-ID 或 CDR-Type |
| 5012 | UNABLE_TO_COMPLY | 服务器无法找到对应的 CDR 记录 |
| 3004 | DIAMETER_TOO_BUSY | 请求频率过快，请稍后重试 |

### 4.2 日志关键词

- **客户端**: `MADR 请求已发送！`, `CDR 计费信息`
- **服务器**: `cic_handle_madr`, `Adding CDR-Info to MADA`, `Result-Code=2001`

---

## 5. 测试检查清单

- [ ] `madr list` 能正确列出当前所有活跃的 CDR。
- [ ] `madr data <id>` 能返回非零的流量统计数据。
- [ ] 在 `mccr stop` 后，对应的 CDR 状态应从 `ACTIVE` 变为 `FINISHED`。
- [ ] `macr restart` 后，旧的 CDR 停止，新的 CDR 自动创建。

---
**文档版本**: 1.0  
**最后更新**: 2025-12-22  
