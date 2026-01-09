# MAGIC MNTR/MNTA 通知机制测试指导

## 1. 概述

### 1.1 什么是 MNTR/MNTA？

- **MNTR** (MAGIC Notification Report Request): 服务器主动推送的通知消息
- **MNTA** (MAGIC Notification Answer): 客户端对通知的确认应答
- **Command-Code**: 100002
- **方向**: 服务器 → 客户端（单向推送，客户端自动应答）

### 1.2 通知机制的作用

MNTR是MAGIC协议中服务器**主动推送**通知给客户端的机制，用于：
- 📢 通知会话状态变化（超时、强制断开）
- 🔗 通知链路状态变化（链路中断、恢复）
- 📊 通知资源变化（带宽调整、排队位置变化）
- ⚠️ 通知服务状态（服务不可用、服务恢复）

---

## 2. MNTR 通知类型 (MAGIC-Status-Code)

根据 ARINC 839 规范，MNTR 使用 `MAGIC-Status-Code` AVP (10009) 来标识通知原因，而不是自定义的 Notification-Type。

| MAGIC-Status-Code | 名称 | 含义 | 客户端预期行为 |
|-------------------|------|------|---------------|
| 0 | SUCCESS | 成功/带宽增加 | 通过 Granted-Bandwidth 获取新带宽值 |
| 1016 | NO_FREE_BANDWIDTH | 带宽不足/被抢占 | 状态可能降级，带宽减少 |
| 1024 | SESSION_TIMEOUT | 会话超时 | 状态→IDLE，重新认证 |
| 1025 | MAGIC_SHUTDOWN | 服务关闭/强制断开 | 状态→IDLE，清理资源 |
| 2007 | LINK_ERROR | 链路错误/丢失 | 状态→AUTHENTICATED，等待恢复或切换 |
| 2010 | FORCED_REROUTING | 链路切换 | 更新网关配置，保持 ACTIVE |

---

## 3. MNTR 消息结构

### 3.1 请求消息 (MNTR)

```
MNTR (Command-Code: 100002, R-bit: 0)
├─ Session-Id               [必填] 会话标识
├─ Auth-Application-Id      [必填] 应用ID (100001)
├─ Origin-Host              [必填] 服务器主机名
├─ Origin-Realm             [必填] 服务器域名
├─ Destination-Realm        [必填] 客户端域名
├─ MAGIC-Status-Code        [可选] 通知原因 (1000-3001)
├─ Error-Message            [可选] 通知描述文本
└─ Communication-Report-Parameters [可选] 通信参数报告
   ├─ Granted-Bandwidth     带宽变化值
   ├─ Gateway-IPAddress     网关地址 (切换时)
   ├─ Selected-Link-ID      链路标识
   └─ ...
```

### 3.2 应答消息 (MNTA)

```
MNTA (Command-Code: 100002, R-bit: 0)
├─ Session-Id               [必填] 会话标识
├─ Result-Code              [必填] 结果码 (通常2001)
├─ Origin-Host              [必填] 客户端主机名
└─ Origin-Realm             [必填] 客户端域名
```

**注意**: MNTA由客户端**自动应答**，测试时无需手动发送。

---

## 4. 测试前提条件

### 4.1 环境准备

- ✅ 服务器和客户端freeDiameter进程运行
- ✅ 客户端已通过MCAR认证（状态≥AUTHENTICATED）
- ✅ 客户端MNTR处理器已注册（启动时自动完成）

### 4.2 检查MNTR处理器状态

启动客户端时应看到：
```
[客户端日志]
✓ MNTR 会话通知处理器已注册 (Command-Code=100002)
```

如果看到警告：
```
WARNING: MNTR 处理器注册失败 (可能缺少字典定义)
```

说明字典文件未正确加载，需检查 `dict_dcca_3gpp.xml` 或自定义字典。

---

## 5. 测试场景

### 场景 A: 会话超时通知 (SESSION_TIMEOUT)

**触发条件**：
- 客户端建立会话后长时间无活动
- 服务器配置的会话超时时间到期

**测试步骤**：
1. 客户端执行 `mcar auth` 建立会话
2. 等待超时时间（服务器配置，如900秒）
3. 观察客户端是否收到MNTR通知

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 1024 (SESSION_TIMEOUT)
║ Session-Id: magic.example.com;1234567890;12
║ ⚠ 会话已超时，状态将变为 IDLE
╚══════════════════════════════════════════════════════════════╝

[INFO] ✓ 已发送 MNTA 自动应答
客户端状态: AUTHENTICATED → IDLE
```

**手动触发（仅测试）**：
```bash
# 在服务器端模拟发送MNTR
# （需要在服务器代码中实现手动触发函数）
```

---

### 场景 B: 资源释放通知 (NO_FREE_BANDWIDTH)

**触发条件**：
- 服务器端手动释放客户端资源
- 资源耗尽需要回收
- 策略引擎主动回收

**测试步骤**：
1. 客户端执行 `mccr start` 建立链路
2. 服务器端触发资源回收（管理命令或策略触发）
3. 观察客户端收到通知

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 1016 (NO_FREE_BANDWIDTH)
║ Session-Id: magic.example.com;1234567890;12
║ ⚠ 带宽不足/被抢占，带宽可能降低
╚══════════════════════════════════════════════════════════════╝

客户端状态: ACTIVE → AUTHENTICATED (或降级)
```

**验证**：
```bash
# 客户端检查状态
MAGIC> status
```

---

### 场景 C: 链路中断/恢复 (LINK_ERROR / SUCCESS)

**触发条件**：
- DLM检测到链路状态变化
- 物理链路故障或恢复
- 网络接口up/down

**测试步骤**：

#### C.1 链路中断
```bash
# 1. 客户端建立会话并分配链路
MAGIC> mccr start cockpit_efb_mission 1024 5000 1 1

# 2. 服务器端模拟链路故障（在服务器机器上）
sudo ip link set ens37 down

# 3. 观察客户端收到MNTR通知
```

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 2007 (LINK_ERROR)
║ Session-Id: magic.example.com;1234567890;12
║ 通信报告参数:
║   Selected-Link-ID: LINK_SATCOM
║ ⚠ 链路/资源已释放，状态将变为 AUTHENTICATED
╚══════════════════════════════════════════════════════════════╝

客户端状态: ACTIVE → AUTHENTICATED
```

#### C.2 链路恢复
```bash
# 1. 恢复链路
sudo ip link set ens37 up

# 2. 观察客户端收到MNTR通知
```

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 0 (SUCCESS)
║ Session-Id: magic.example.com;1234567890;12
║ 通信报告参数:
║   Granted-Bandwidth: 1024000 bps  ← 带宽已恢复
║ ✓ 链路已恢复
╚══════════════════════════════════════════════════════════════╝
```

**注意**: 链路恢复后，客户端可能需要重新执行 `mccr start` 来分配资源。

---

### 场景 D: 带宽调整通知 (NO_FREE_BANDWIDTH / SUCCESS)

**触发条件**：
- 服务器端动态带宽调整
- 策略引擎根据优先级调整
- 拥塞控制触发

**测试步骤**：
```bash
# 1. 客户端建立链路
MAGIC> mccr start passenger_wifi_portal 1024 5000 2 0

# 2. 服务器端调整带宽（通过管理接口或策略）
# （服务器发送MNTR，Code=2004，带宽减少到512kbps）

# 3. 观察客户端收到通知
```

**预期结果（带宽降低）**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 1016 (NO_FREE_BANDWIDTH)
║ Session-Id: magic.example.com;1234567890;12
║ ⚠ 带宽不足/被抢占，带宽可能降低
║ 通信报告参数:
║   授予带宽: 512.00 kbps  ← 从1024降低到512
╚══════════════════════════════════════════════════════════════╝

客户端状态: ACTIVE (带宽已更新)
```

**预期结果（带宽增加）**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 0 (SUCCESS)
║ Session-Id: magic.example.com;1234567890;12
║ ℹ 带宽已增加
║ 通信报告参数:
║   授予带宽: 2048.00 kbps  ← 从512增加到2048
╚══════════════════════════════════════════════════════════════╝
```

**应用行为**：
- 客户端应用应根据新带宽调整业务流量
- 视频流可以降低码率或提升分辨率

---

### 场景 E: 链路切换 (FORCED_REROUTING)

**触发条件**：
- 链路质量下降，触发切换
- 策略引擎选择更优链路

**测试步骤**：
```bash
# 1. 客户端建立链路
MAGIC> mccr start cockpit_efb_mission 1024 5000 1 1

# 2. 服务器端触发切换
# 3. 观察客户端收到通知
```

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 2010 (FORCED_REROUTING)
║ Session-Id: magic.example.com;1234567890;12
║ ✓ 链路切换完成，请更新网关配置
║ 通信报告参数:
║   Selected-Link-ID: LINK_CELLULAR
║   Gateway-IPAddress: 192.168.200.1
╚══════════════════════════════════════════════════════════════╝

客户端状态: ACTIVE (链路已更新)
```

---

### 场景 F: 强制断开 (MAGIC_SHUTDOWN)

**触发条件**：
- 管理员手动断开客户端
- 安全策略触发（检测到异常行为）
- 维护需要强制下线

**测试步骤**：
```bash
# 1. 客户端建立会话
MAGIC> mcar connect cockpit_efb_mission 2000

# 2. 服务器端执行强制断开（管理命令）
# 3. 观察客户端收到通知
```

**预期结果**：
```
╔══════════════════════════════════════════════════════════════╗
║  🔔 收到 MNTR 会话通知                                       ║
╠══════════════════════════════════════════════════════════════╣
║ MAGIC-Status-Code: 1025 (MAGIC_SHUTDOWN)
║ Session-Id: magic.example.com;1234567890;12
║ Notification-Message: "由管理员强制断开"
║ ⚠ 链路/资源已释放，状态将变为 AUTHENTICATED
╚══════════════════════════════════════════════════════════════╝

客户端状态: ACTIVE → AUTHENTICATED
[WARN] 会话已被服务器强制终止
```

**客户端应对**：
- 清理本地资源
- 显示提示信息给用户
- 根据策略决定是否自动重连

---

## 6. 服务器端触发MNTR（开发/测试）

### 6.1 手动触发函数（需在服务器实现）

以下是服务器端发送MNTR的示例代码结构：

```c
/**
 * 向客户端发送MNTR通知
 * 
 * @param session_id 会话ID
 * @param magic_status_code MAGIC状态码 (1000-3001)
 * @param message 通知消息（可选）
 * @param comm_params 通信参数（可选，用于带宽/队列变化）
 */
int magic_send_mntr(const char* session_id,
                   uint32_t magic_status_code,
                   const char* message,
                   comm_report_params_t* comm_params)
{
    struct msg *mntr = NULL;
    struct dict_object *cmd_mntr = NULL;
    
    // 1. 创建MNTR消息
    CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND,
                            CMD_BY_CODE_R, &cmd_code, &cmd_mntr, ENOENT));
    CHECK_FCT(fd_msg_new(cmd_mntr, 0, &mntr));
    
    // 2. 添加Session-Id
    ADD_AVP_STR(mntr, g_std_dict.avp_session_id, session_id);
    
    // 3. 添加MAGIC-Status-Code
    ADD_AVP_U32(mntr, g_magic_dict.avp_magic_status_code, magic_status_code);
    
    // 4. 添加Error-Message (可选)
    if (message) {
        ADD_AVP_STR(mntr, g_std_dict.avp_error_message, message);
    }
    
    // 5. 添加Communication-Report-Parameters (可选)
    if (comm_params) {
        add_comm_report_params(mntr, comm_params);
    }
    
    // 6. 发送到客户端
    CHECK_FCT(fd_msg_send(&mntr, NULL, NULL));
    
    fd_log_notice("[MNTR] 已发送通知: Code=%u, Session=%s",
                  magic_status_code, session_id);
    return 0;
}
```

### 6.2 测试脚本示例

```bash
#!/bin/bash
# 服务器端测试脚本：触发MNTR通知

SESSION_ID="magic.example.com;1234567890;12"

# 场景1: 发送链路中断通知 (LINK_ERROR=2007)
echo "发送 LINK_ERROR 通知..."
magic_send_notification "$SESSION_ID" 2007 "SATCOM链路故障"

sleep 5

# 场景2: 发送链路恢复通知 (SUCCESS=0 + Granted-Bandwidth)
echo "发送链路恢复通知..."
magic_send_notification "$SESSION_ID" 0 "SATCOM链路已恢复" --bandwidth=1024000

sleep 5

# 场景3: 发送带宽降低通知 (NO_FREE_BANDWIDTH=1016)
echo "发送 NO_FREE_BANDWIDTH 通知..."
magic_send_notification "$SESSION_ID" 1016 "拥塞控制触发" --bandwidth=512000

# 场景4: 发送强制断开通知 (MAGIC_SHUTDOWN=1025)
echo "发送 MAGIC_SHUTDOWN 通知..."
magic_send_notification "$SESSION_ID" 1025 "系统维护"
```

---

## 7. 客户端状态转换

### 7.1 状态转换图

```
        MNTR: SESSION_TIMEOUT (1024)
        MNTR: MAGIC_SHUTDOWN (1025)
   ┌──────────────────────┐
   │                      │
   v                      │
[IDLE] ──MCAR auth──> [AUTHENTICATED] ──MCCR start──> [ACTIVE]
                           ^                              │
                           │                              │
                           └──MNTR: NO_FREE_BANDWIDTH (1016)──┘
                           └──MNTR: LINK_ERROR (2007)─────────┘
```

### 7.2 状态对应的MNTR处理

| 客户端状态 | 可能收到的MNTR | 处理动作 |
|-----------|---------------|---------|
| IDLE | - | 忽略（无会话） |
| AUTHENTICATED | SESSION_TIMEOUT (1024)<br>MAGIC_SHUTDOWN (1025) | → IDLE |
| ACTIVE | NO_FREE_BANDWIDTH (1016)<br>LINK_ERROR (2007)<br>MAGIC_SHUTDOWN (1025) | → AUTHENTICATED |
| ACTIVE | SUCCESS (0) + Granted-BW | 更新带宽，保持ACTIVE |
| ACTIVE | FORCED_REROUTING (2010) | 更新网关，保持ACTIVE |

---

## 8. 日志分析

### 8.1 客户端日志关键词

**正常收到MNTR**：
```
[INFO] 🔔 收到 MNTR 会话通知
[INFO] MAGIC-Status-Code: 2007 (LINK_ERROR)
[INFO] ✓ 已发送 MNTA 自动应答
```

**MNTR处理失败**：
```
[ERROR] MNTR 消息解析失败
[WARN] MNTR 中缺少必填AVP: MAGIC-Status-Code
```

**状态转换**：
```
[INFO] 状态变更: ACTIVE → AUTHENTICATED (MNTR触发)
[INFO] 清空分配链路: LINK_SATCOM
```

### 8.2 服务器日志关键词

**发送MNTR**：
```
[NOTICE] [MNTR] 已发送通知: Code=1008, Session=magic.example.com;123;45
[DEBUG] MNTR 发送到客户端: 192.168.126.5
```

**收到MNTA**：
```
[NOTICE] [MNTA] 收到客户端确认: Result-Code=2001
```

---

## 9. 常见问题

### Q1: 客户端收不到MNTR通知

**可能原因**：
1. MNTR处理器未注册
   - 检查启动日志是否有 "MNTR 会话通知处理器已注册"
   - 检查字典文件是否包含Command-Code 100002

2. 会话不匹配
   - 服务器发送的Session-Id与客户端不一致
   - 使用 `status` 命令检查当前Session-Id

3. 网络问题
   - Diameter连接中断
   - 检查 `netstat -an | grep 3868`

**解决方法**：
```bash
# 1. 检查MNTR处理器
grep "MNTR.*已注册" client.log

# 2. 检查Session-Id
MAGIC> status

# 3. 检查网络连接
netstat -an | grep 3868
```

### Q2: 收到MNTR但状态没有改变

**可能原因**：
- 客户端代码中状态转换逻辑未实现
- 通知类型映射错误

**检查代码**：
```c
// magic_commands.c: mntr_handler_callback
switch (magic_status_code) {
  case 2007: /* LINK_ERROR */
  case 1016: /* NO_FREE_BANDWIDTH */
  case 1024: /* SESSION_TIMEOUT */
  case 1025: /* MAGIC_SHUTDOWN */
    g_client_state = CLIENT_STATE_AUTHENTICATED;  // ← 确保这行存在
    break;
}
```

### Q3: MNTR通知过于频繁

**可能原因**：
- 链路状态抖动（up/down频繁切换）
- 服务器配置的通知阈值过低

**建议**：
- 服务器端增加防抖动逻辑
- 调整通知发送间隔
- 客户端实现通知去重

---

## 10. 压力测试

### 10.1 批量MNTR测试

**目标**: 测试客户端处理大量MNTR的能力

```bash
# 服务器端脚本
for i in {1..100}; do
    magic_send_notification "$SESSION_ID" 5 "排队测试" --position=$i
    sleep 0.1
done
```

**预期**：
- 客户端能正常接收所有100条MNTR
- 每条都有MNTA应答
- 无消息丢失或处理异常

### 10.2 并发会话MNTR测试

**目标**: 测试多个会话同时接收MNTR

```bash
# 启动多个客户端实例
for i in {1..10}; do
    ./magic_client -c client_$i.conf &
done

# 服务器向所有会话发送MNTR
for sid in $(get_all_sessions); do
    magic_send_notification "$sid" 6 "带宽调整" --bandwidth=512000
done
```

---

## 11. 最佳实践

### 11.1 客户端应用建议

1. **监听MNTR事件**
   - 注册事件回调，及时响应通知
   - 根据通知类型更新UI和业务逻辑

2. **日志记录**
   - 记录所有MNTR通知到日志文件
   - 便于问题追溯和分析

3. **用户提示**
   - 重要通知（链路中断、强制断开）显示给用户
   - 次要通知（排队位置）静默处理

4. **自动重连**
   - 收到FORCED_DISCONNECT后，等待一段时间自动重连
   - 避免频繁重连导致资源浪费

### 11.2 服务器端建议

1. **通知节流**
   - 避免短时间内发送大量重复通知
   - 合并相同类型的通知

2. **优先级管理**
   - 关键通知（强制断开）立即发送
   - 次要通知（排队位置）可批量发送

3. **日志审计**
   - 记录所有MNTR发送历史
   - 包括时间、会话、类型、原因

---

## 12. 总结

### 12.1 核心要点

- ✅ MNTR是**服务器主动推送**机制，客户端被动接收
- ✅ 客户端**自动应答MNTA**，无需手动处理
- ✅ 10种通知类型覆盖会话、链路、资源、服务全生命周期
- ✅ 通知会**触发客户端状态转换**，需正确处理

### 12.2 测试检查清单

- [ ] 启动时确认MNTR处理器注册成功
- [ ] 测试所有10种通知类型
- [ ] 验证每种通知的状态转换正确
- [ ] 检查MNTA自动应答正常发送
- [ ] 测试异常情况（网络中断、非法通知）
- [ ] 压力测试（大量通知、并发会话）
- [ ] 日志完整性检查

---

**文档版本**: 1.0  
**最后更新**: 2025-12-22  
**适用版本**: MAGIC Client v2.0+
