# MAGIC 客户端测试指导手册

## 📋 目录
- [系统概述](#系统概述)
- [测试前准备](#测试前准备)
- [基本功能测试](#基本功能测试)
- [高级功能测试](#高级功能测试)
- [故障排查](#故障排查)
- [测试检查清单](#测试检查清单)

---

## 系统概述

**MAGIC (Multi-link AGgregation for Integrated Communications)** 是基于 ARINC 839-2014 标准的航空通信系统客户端，支持多链路聚合和智能路由。

### 核心特性
- ✅ **介质无关性**: 客户端只提交 QoS 需求，MAGIC 自动选择最优链路
- ✅ **本地认证**: CIC 模块支持本地配置文件认证，无需 MAGIC Core
- ✅ **7 个命令对**: MCAR/MCAA, MCCR/MCCA, MSXR/MSXA, MADR/MADA, MACR/MACA, STR/STA, ABR/ABA
- ✅ **72 个 AVP**: 完整的 ARINC 839 协议字典支持

### 系统架构
```
┌─────────────────┐         Diameter/TLS        ┌─────────────────┐
│  MAGIC Client   │ ─────────────────────────> │  MAGIC Server   │
│  (magic_client) │ <───────────────────────── │  (freeDiameterd)│
└─────────────────┘      Port: 5870 (TLS)       └─────────────────┘
        │                                               │
        │                                               │
   CLI Commands                              ┌──────────┴──────────┐
   (MCAR/MCCR/...)                          │   CIC Module        │
                                             │ (app_magic_cic.fdx) │
                                             └─────────────────────┘
                                                       │
                                                       ↓
                                             Client_Profile.xml
                                             (本地认证配置)
```

---

## 测试前准备

### 1. 环境检查

#### 检查服务器配置文件
```bash
cat /home/zhuwuhui/freeDiameter/conf/magic_server.conf
```

**关键配置项检查：**
```properties
Identity = "magic-server.example.com";      # ✅ 服务器身份
Realm = "example.com";                      # ✅ 域
SecPort = 5870;                             # ✅ TLS 端口
LoadExtension = ".../dict_magic_839.fdx";   # ✅ MAGIC 字典
LoadExtension = ".../app_magic_cic.fdx";    # ✅ CIC 模块
```

#### 检查客户端配置文件
```bash
cat /home/zhuwuhui/freeDiameter/magic_client/magic_client.conf
```

**关键配置项检查：**
```properties
Identity = "magic-client.example.com";                      # ✅ 客户端身份
ConnectPeer = "magic-server.example.com" {                 # ✅ 服务器地址
    ConnectTo = "192.168.37.136"; 
    Port = 5870; 
};
```

#### 检查客户端认证配置
```bash
cat /home/zhuwuhui/freeDiameter/extensions/app_magic_cic/config/Client_Profile.xml
```

**预配置的测试账户：**
| Client ID | Username | Password | Profile | Max BW |
|-----------|----------|----------|---------|--------|
| CLIENT001 | pilot001 | pass123  | IP_DATA | 5000000 |
| CLIENT002 | cabin001 | pass456  | IP_DATA | 2000000 |
| CLIENT003 | maint001 | pass789  | IP_DATA | 1000000 |
| CLIENT004 | guest001 | pass000  | IP_DATA | 500000  |

### 2. 启动服务器

**终端 1: freeDiameterd**
```bash
cd /home/zhuwuhui/freeDiameter

# 停止旧进程
pkill freeDiameterd

# 启动服务器
./build/freeDiameterd/freeDiameterd -c conf/magic_server.conf
```

**预期日志输出：**
```
[INFO] freeDiameter daemon starting...
[INFO] Loading extensions...
[INFO] Extension 'dict_magic_839' loaded
[CIC] ========================================
[CIC] MAGIC CIC Extension Loading
[CIC] ========================================
[CIC] ✓ Loaded 4 client profiles from .../Client_Profile.xml
[CIC] ✓ Registered MAGIC application (16777300)
[CIC] ✓ Registered MCAR handler
[CIC] ✓ Registered MCCR handler
[CIC] ✓ Registered STR handler
[CIC] ========================================
[CIC] MAGIC CIC Extension Ready
[CIC] ========================================
[INFO] freeDiameter started successfully
```

### 3. 启动客户端

**终端 2: magic_client**
```bash
cd /home/zhuwuhui/freeDiameter/magic_client/build

# 启动客户端
./magic_client
```

**预期输出：**
```
=================================================
  MAGIC Client - ARINC 839-2014 航空通信客户端
=================================================

[1/6] 初始化 freeDiameter 核心...
[2/6] 加载 freeDiameter 配置: .../magic_client.conf
[3/6] 初始化 MAGIC 协议字典...
[4/6] 加载 MAGIC 客户端配置: .../magic.conf
[5/6] 启动 freeDiameter 核心服务...
[6/6] 初始化命令行界面...

=================================================
  MAGIC 客户端已就绪
=================================================
当前状态: 未注册

MAGIC[未注册]> _
```

---

## 基本功能测试

### 测试 1: 客户端认证注册 (MCAR/MCAA)

#### 测试目的
验证客户端能够成功注册到 MAGIC 服务器，建立 Diameter 会话。

#### 测试步骤

**步骤 1: 查看帮助信息**
```bash
MAGIC[未注册]> help mcar
```

**预期输出：**
```
命令: mcar (register)
描述: 客户端认证注册 - 向MAGIC服务器注册并建立Diameter会话
用法: mcar

注意:
  - 首次使用必须先执行 mcar 命令注册
  - 认证信息从配置文件 magic.conf 中读取
  - 成功后自动创建 Diameter 会话
```

**步骤 2: 执行注册命令**
```bash
MAGIC[未注册]> mcar
```

**预期输出（成功）：**
```
[INFO] 发送请求...
[SUCCESS] 请求已发送，等待服务器应答...
[INFO] 收到应答消息 (Command-Code: 100000)
[SUCCESS] 命令执行成功 (Result-Code: 2001)
  → DIAMETER_SUCCESS

=== 会话信息 ===
  Session-Id: magic-client.example.com;1732435200;1;magic
  Auth-Session-State: NO_STATE_MAINTAINED
  Session-Timeout: 3600 秒
  Server-Password: <加密密码>

[INFO] 客户端状态已更新: 已注册
```

**步骤 3: 验证状态**
```bash
MAGIC[已注册]> status
```

**预期输出：**
```
=== MAGIC 客户端状态 ===
注册状态: 已注册
会话 ID: magic-client.example.com;1732435200;1;magic
客户端 ID: CLIENT001
用户名: pilot001
默认配置: IP_DATA

=== Diameter 连接状态 ===
本地身份: magic-client.example.com
服务器: magic-server.example.com
连接状态: 已连接
```

#### 测试检查点
- ✅ Result-Code = 2001 (DIAMETER_SUCCESS)
- ✅ Session-Id 正确生成
- ✅ Session-Timeout 返回（默认 3600 秒）
- ✅ Server-Password 返回
- ✅ 客户端状态变更为"已注册"

#### 常见错误处理

**错误 1: Result-Code 5012 (DIAMETER_MISSING_AVP)**
```
[ERROR] 命令执行失败 (Result-Code: 5012)
  → DIAMETER_MISSING_AVP (缺少必需的AVP)
```
**原因**: 配置文件中缺少认证信息  
**解决**: 检查 `magic.conf` 中的 Client-ID、Username、Password 配置

**错误 2: Result-Code 5001 (DIAMETER_AVP_UNSUPPORTED)**
```
[ERROR] 命令执行失败 (Result-Code: 5001)
  → DIAMETER_AVP_UNSUPPORTED (不支持的AVP)
```
**原因**: 服务器未加载 MAGIC 字典  
**解决**: 确认服务器配置中 `LoadExtension = ".../dict_magic_839.fdx"`

**错误 3: Result-Code 3002 (DIAMETER_UNABLE_TO_DELIVER)**
```
[ERROR] 命令执行失败 (Result-Code: 3002)
  → DIAMETER_UNABLE_TO_DELIVER (无法送达)
```
**原因**: 服务器未加载 CIC 模块或路由配置错误  
**解决**: 确认服务器配置中 `LoadExtension = ".../app_magic_cic.fdx"`

---

### 测试 2: 通信会话管理 (MCCR/MCCA)

#### 测试目的
验证客户端能够提交 QoS 需求，MAGIC 自动选择最优链路（符合 ARINC 839 介质无关性原则）。

#### 测试步骤

**步骤 1: 查看帮助信息**
```bash
MAGIC[已注册]> help mccr
```

**预期输出：**
```
📋 ARINC 839 介质无关性原则:
   客户端只提交业务需求，不能指定物理链路

命令格式:
  mccr create [profile] [min_kbps] [max_kbps] [priority] [qos]
  mccr modify [min_kbps] [max_kbps] [priority] [qos]
  mccr release

参数说明:
  profile    - 业务类型 (IP_DATA/VOICE/VIDEO)
  min_kbps   - 最小保证带宽 (kbps)
  max_kbps   - 最大期望带宽 (kbps)
  priority   - 优先级等级 (1=最高, 8=最低)
  qos        - QoS 等级 (0=尽力而为, 1=保证服务, 2=实时, 3=控制)

示例:
  mccr create IP_DATA 512 5000 2 1    # 数据业务, 512kbps-5Mbps, 优先级2, 保证服务
  mccr modify 1024 10000 1 2          # 提高需求, 1Mbps-10Mbps, 高优先级, 实时服务
  mccr release                        # 释放会话
```

**步骤 2: 创建通信会话（低带宽需求）**
```bash
MAGIC[已注册]> mccr create IP_DATA 256 512 5 0
```

**预期输出：**
```
[INFO] 发送请求...
📊 提交 QoS 业务需求到 MAGIC 策略引擎:
  业务类型: IP_DATA
  最小保证带宽: 256 kbps
  最大期望带宽: 512 kbps
  优先级等级: 5 (1=最高, 8=最低)
  QoS 等级: 0 (尽力而为)
🔄 MAGIC 将自动选择最优链路 (Satcom/LTE/WiFi)...

[SUCCESS] 请求已发送，等待服务器应答...
[INFO] 收到应答消息 (Command-Code: 100001)
[SUCCESS] 命令执行成功 (Result-Code: 2001)

=== 通信会话已建立 ===
  Granted-BW: 512000 bps
  Granted-Return-BW: 256000 bps
  Selected-DLM: Satcom_L  (MAGIC 自动选择)
```

**步骤 3: 修改会话（高带宽需求）**
```bash
MAGIC[已注册]> mccr modify 5000 10000 1 2
```

**预期输出：**
```
📊 提交新的 QoS 业务需求:
  最小保证带宽: 5000 kbps (5 Mbps)
  最大期望带宽: 10000 kbps (10 Mbps)
  优先级等级: 1 (最高优先级)
  QoS 等级: 2 (实时服务)
🔄 MAGIC 将根据新需求重新评估链路分配...

[SUCCESS] 请求已发送，等待服务器应答...
[INFO] 收到应答消息 (Command-Code: 100001)
[SUCCESS] 命令执行成功 (Result-Code: 2001)

=== 通信会话已更新 ===
  Granted-BW: 10000000 bps
  Selected-DLM: LTE_Primary  (MAGIC 自动切换到高带宽链路)
```

**步骤 4: 释放会话**
```bash
MAGIC[已注册]> mccr release
```

**预期输出：**
```
[INFO] 发送请求...
[SUCCESS] 通信会话已释放
```

#### 测试检查点
- ✅ 客户端**不能指定** DLM-Name（链路名称）
- ✅ 只提交 QoS 参数（带宽、优先级、QoS 等级）
- ✅ MAGIC 自动选择链路（Satcom/LTE/WiFi）
- ✅ 高带宽需求触发自动链路切换
- ✅ Result-Code = 2001 (SUCCESS)

#### 介质无关性验证

**❌ 错误示例（不符合 ARINC 839）：**
```bash
# 以下命令不存在（已移除）
MAGIC[已注册]> mccr create IP_DATA 5000 10000 LTE_Primary  # ❌ 不允许指定链路
```

**✅ 正确示例（符合 ARINC 839）：**
```bash
MAGIC[已注册]> mccr create IP_DATA 5000 10000 1 2  # ✅ 只提交 QoS 需求
```

---

### 测试 3: 状态查询 (MSXR/MSXA)

#### 测试步骤

```bash
MAGIC[已注册]> msxr
```

**预期输出：**
```
[INFO] 发送请求...
[SUCCESS] 请求已发送，等待服务器应答...
[INFO] 收到应答消息 (Command-Code: 100002)
[SUCCESS] 命令执行成功 (Result-Code: 2001)

=== 系统状态信息 ===
  DLM-Name: LTE_Primary
  Link-Status: 1 (在线)
  Link-Max-BW: 10000000 bps
  Current-Active-Sessions: 3
==================
```

---

### 测试 4: 会话终止 (STR/STA)

#### 测试步骤

```bash
MAGIC[已注册]> str
```

**预期输出：**
```
[INFO] 发送会话终止请求...
[SUCCESS] 会话已终止
[INFO] 客户端状态已更新: 未注册
```

**验证状态：**
```bash
MAGIC[未注册]> status
```

**预期输出：**
```
=== MAGIC 客户端状态 ===
注册状态: 未注册
会话 ID: (无)
```

---

## 高级功能测试

### 测试场景 1: 链路切换验证

#### 目的
验证 MAGIC 能够根据 QoS 需求变化自动切换链路。

#### 步骤

**1. 注册并创建低带宽会话**
```bash
MAGIC[未注册]> mcar
MAGIC[已注册]> mccr create IP_DATA 256 512 5 0
# 预期: MAGIC 选择 Satcom (低带宽、低成本)
```

**2. 提高带宽需求**
```bash
MAGIC[已注册]> mccr modify 5000 10000 1 2
# 预期: MAGIC 自动切换到 LTE (高带宽、低延迟)
```

**3. 查询当前链路状态**
```bash
MAGIC[已注册]> msxr
# 预期: DLM-Name 显示 LTE_Primary
```

**4. 降低带宽需求**
```bash
MAGIC[已注册]> mccr modify 128 256 8 0
# 预期: MAGIC 切回 Satcom (低带宽、节省成本)
```

---

### 测试场景 2: 多客户端并发测试

#### 目的
验证服务器能够同时处理多个客户端的认证和通信会话。

#### 步骤

**终端 1: 客户端 CLIENT001**
```bash
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client

MAGIC[未注册]> mcar
MAGIC[已注册]> mccr create IP_DATA 1000 5000 2 1
```

**终端 2: 客户端 CLIENT002**
```bash
# 修改配置文件使用不同的 Client-ID
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client

MAGIC[未注册]> mcar
MAGIC[已注册]> mccr create IP_DATA 500 2000 3 1
```

**验证: 服务器日志应显示两个独立会话**
```
[CIC] Client CLIENT001 authenticated successfully
[CIC] Client CLIENT002 authenticated successfully
[CIC] Active sessions: 2
```

---

### 测试场景 3: 认证失败处理

#### 目的
验证错误密码的处理机制。

#### 步骤

**1. 修改配置文件使用错误密码**
```bash
vim /home/zhuwuhui/freeDiameter/magic_client/magic.conf
# 修改: Password = "wrong_password"
```

**2. 尝试注册**
```bash
MAGIC[未注册]> mcar
```

**预期输出（失败）：**
```
[ERROR] 命令执行失败 (Result-Code: 5012)
  → DIAMETER_MISSING_AVP (认证失败)
```

**3. 恢复正确密码后重试**
```bash
# 修改配置: Password = "pass123"
MAGIC[未注册]> mcar
[SUCCESS] 命令执行成功 (Result-Code: 2001)
```

---

## 故障排查

### 问题 1: 客户端无法连接服务器

**症状：**
```
[ERROR] Connection to magic-server.example.com failed
[ERROR] Peer connection timeout
```

**排查步骤：**

1. **检查服务器是否运行**
```bash
ps aux | grep freeDiameterd
```

2. **检查端口监听**
```bash
netstat -tuln | grep 5870
# 应该看到: tcp  0  0  192.168.37.136:5870  0.0.0.0:*  LISTEN
```

3. **检查防火墙**
```bash
sudo iptables -L -n | grep 5870
```

4. **测试网络连通性**
```bash
telnet 192.168.37.136 5870
```

5. **检查证书**
```bash
openssl s_client -connect 192.168.37.136:5870 -CAfile /home/zhuwuhui/freeDiameter/certs/ca.crt
```

---

### 问题 2: Result-Code 3002 (UNABLE_TO_DELIVER)

**症状：**
```
[ERROR] 命令执行失败 (Result-Code: 3002)
  → DIAMETER_UNABLE_TO_DELIVER (无法送达)
```

**原因分析：**
- 服务器未加载 CIC 模块 (app_magic_cic.fdx)
- 路由配置错误
- 服务器 Identity 不匹配

**解决步骤：**

1. **检查服务器扩展加载**
```bash
grep "LoadExtension.*app_magic_cic" /home/zhuwuhui/freeDiameter/conf/magic_server.conf
```

应该看到：
```
LoadExtension = ".../app_magic_cic.fdx" : ".../Client_Profile.xml";
```

2. **检查服务器日志**
```bash
grep "CIC" /home/zhuwuhui/freeDiameter/logs/magic_server_*.log
```

应该看到：
```
[CIC] ✓ Loaded 4 client profiles
[CIC] ✓ Registered MCAR handler
```

3. **重启服务器**
```bash
pkill freeDiameterd
./build/freeDiameterd/freeDiameterd -c conf/magic_server.conf
```

---

### 问题 3: 字典错误 (Client-Credentials 结构错误)

**症状：**
```
[ERROR] AVP structure mismatch
[ERROR] Server-Password not expected in Client-Credentials
```

**解决：**
确认使用最新的 dict_magic_839.fdx：
```bash
cd /home/zhuwuhui/freeDiameter/build
make dict_magic_839
```

**验证字典结构：**
```bash
strings /home/zhuwuhui/freeDiameter/build/extensions/dict_magic_839.fdx | grep -A5 "Client-Credentials"
```

---

### 问题 4: 本地认证失败

**症状：**
```
[ERROR] Authentication failed
[ERROR] Client ID not found in configuration
```

**排查步骤：**

1. **检查配置文件路径**
```bash
ls -l /home/zhuwuhui/freeDiameter/extensions/app_magic_cic/config/Client_Profile.xml
```

2. **验证 XML 格式**
```bash
xmllint --noout /home/zhuwuhui/freeDiameter/extensions/app_magic_cic/config/Client_Profile.xml
```

3. **检查客户端配置**
```bash
grep -E "Client-ID|Username|Password" /home/zhuwuhui/freeDiameter/magic_client/magic.conf
```

4. **查看服务器日志**
```bash
tail -f /home/zhuwuhui/freeDiameter/logs/magic_server_*.log | grep -i auth
```

---

## 测试检查清单

### 基本功能测试
- [ ] 服务器成功启动，CIC 模块加载
- [ ] 客户端成功连接到服务器
- [ ] MCAR 注册成功 (Result-Code 2001)
- [ ] Session-Id 正确生成
- [ ] Server-Password 正确返回
- [ ] MCCR 创建通信会话成功
- [ ] 客户端不能指定链路名称（介质无关性）
- [ ] MCCR 修改会话成功
- [ ] MSXR 状态查询成功
- [ ] STR 会话终止成功

### 字典系统测试
- [ ] Client-Credentials 只包含 User-Name 和 Client-Password
- [ ] Client-Credentials 中没有 Server-Password
- [ ] Server-Password 在 MCAA 答复中作为独立 AVP

### CIC 本地认证测试
- [ ] Client_Profile.xml 加载成功（4 个客户端）
- [ ] 本地认证无需 MAGIC Core 连接
- [ ] 正确密码认证成功
- [ ] 错误密码认证失败
- [ ] 未知 Client-ID 被拒绝

### MCCR 介质无关性测试
- [ ] DLM-Name 字段已从 MCCR 中移除
- [ ] 只接受 QoS 参数（带宽、优先级、QoS 等级）
- [ ] 命令帮助显示 ARINC 839 原则说明
- [ ] 低带宽需求分配低成本链路
- [ ] 高带宽需求自动切换到高速链路

### 错误处理测试
- [ ] Result-Code 3002 正确诊断
- [ ] Result-Code 5012 正确提示
- [ ] 网络断开后重连机制
- [ ] 超时处理

### 性能测试
- [ ] 认证响应时间 < 100ms
- [ ] 通信会话创建响应时间 < 200ms
- [ ] 并发客户端支持（至少 4 个）

---

## 附录: 快速测试脚本

### 完整测试流程脚本
```bash
#!/bin/bash
# 文件: quick_test.sh

echo "========================================="
echo "  MAGIC 客户端快速测试"
echo "========================================="

# 启动客户端
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client << EOF
# 测试 1: 注册
mcar
sleep 2

# 测试 2: 查看状态
status
sleep 1

# 测试 3: 创建低带宽会话
mccr create IP_DATA 256 512 5 0
sleep 2

# 测试 4: 提高带宽需求（应触发链路切换）
mccr modify 5000 10000 1 2
sleep 2

# 测试 5: 查询系统状态
msxr
sleep 2

# 测试 6: 释放通信会话
mccr release
sleep 1

# 测试 7: 终止 Diameter 会话
str
sleep 1

# 退出
quit
EOF

echo "========================================="
echo "  测试完成"
echo "========================================="
```

---

## 技术支持

### 日志文件位置
- **服务器日志**: `/home/zhuwuhui/freeDiameter/logs/magic_server_*.log`
- **客户端日志**: 控制台输出

### 配置文件位置
- **服务器配置**: `/home/zhuwuhui/freeDiameter/conf/magic_server.conf`
- **客户端配置**: `/home/zhuwuhui/freeDiameter/magic_client/magic_client.conf`
- **MAGIC 配置**: `/home/zhuwuhui/freeDiameter/magic_client/magic.conf`
- **认证配置**: `/home/zhuwuhui/freeDiameter/extensions/app_magic_cic/config/Client_Profile.xml`

### 常用调试命令
```bash
# 查看 Diameter 消息详情
grep "DBG" /home/zhuwuhui/freeDiameter/logs/magic_server_*.log

# 查看认证记录
grep "authenticated" /home/zhuwuhui/freeDiameter/logs/magic_server_*.log

# 查看会话创建
grep "session" /home/zhuwuhui/freeDiameter/logs/magic_server_*.log

# 实时监控服务器日志
tail -f /home/zhuwuhui/freeDiameter/logs/magic_server_*.log
```

---

**文档版本**: 1.0  
**最后更新**: 2025-11-24  
**符合标准**: ARINC 839-2014
