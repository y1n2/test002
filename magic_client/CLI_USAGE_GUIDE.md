# MAGIC Client CLI 使用手册

## 概述

MAGIC Client 是一个基于 ARINC 839-2014 标准的航空电子 Diameter 通信客户端，提供交互式命令行界面(CLI)，支持完整的 MAGIC 协议命令操作。

## 功能特性

- ✅ 交互式命令行界面，支持命令历史和自动补全
- ✅ 完整支持 ARINC 839-2014 所有 7 对 MAGIC 命令
- ✅ 实时会话状态管理和显示
- ✅ 彩色输出，易于识别状态和错误
- ✅ 灵活的配置文件支持
- ✅ 详细的帮助系统

## 编译安装

### 前提条件

```bash
# 安装依赖
sudo apt-get install libreadline-dev cmake build-essential

# 确保 freeDiameter 已编译安装
cd /home/zhuwuhui/freeDiameter
mkdir -p build && cd build
cmake ..
make
sudo make install
```

### 编译客户端

```bash
cd /home/zhuwuhui/freeDiameter/magic_client   magic_client.conf
mkdir -p build && cd build
cmake ..
make

# 运行客户端
./magic_client
```

## 配置文件

### 1. FreeDiameter 主配置 (`magic_client.conf`)

```conf
# Diameter 身份
Identity = "efb-client.test.local";
Realm = "test.local";

# TLS 证书（如果需要）
TLS_Cred = "/path/to/client.cert.pem", "/path/to/client.key.pem";
TLS_CA = "/path/to/ca.cert.pem";

# 连接到服务器
ConnectPeer = "magic-server.test.local" { ConnectTo = "192.168.1.100"; Port = 3868; };

# 加载 MAGIC 字典扩展
LoadExtension = "dict_magic_839.so";
```

### 2. MAGIC 客户端配置 (`magic.conf`)

```conf
# 客户端身份
CLIENT_ID = "EFB-A320-001"
TAIL_NUMBER = "B-8888"
AIRCRAFT_TYPE = "A320"
USERNAME = "pilot01"

# 目标服务器
DESTINATION_REALM = "magic.arinc.com"
DESTINATION_HOST = "magic-server.arinc.com"

# 认证凭证
CLIENT_PASSWORD = "your_password_here"
SERVER_PASSWORD = ""  # 可选，用于双向认证

# 通信会话配置
PROFILE_NAME = "IP_DATA"
REQUESTED_BW = 5000000      # 5 Mbps 下行
REQUESTED_RETURN_BW = 1000000  # 1 Mbps 上行
REQUIRED_BW = 1000000       # 最低保障 1 Mbps
REQUIRED_RETURN_BW = 512000

# QoS 和优先级
QOS_LEVEL = 1               # 0=BE, 1=AF, 2=EF
PRIORITY_CLASS = 3
PRIORITY_TYPE = 1

# 会话控制
TIMEOUT = 300
KEEP_REQUEST = 1
AUTO_DETECT = 1
ACCOUNTING_ENABLED = 1

# 流量过滤规则（可选）
TFT_GROUND.1 = "192.168.1.0/24 any any"
TFT_AIRCRAFT.1 = "any 192.168.1.100 80"
NAPT.1 = "tcp 8080 -> 80"
```

## CLI 命令参考

### 客户端注册

#### `mcar` - 客户端认证注册

建立与 MAGIC 服务器的 Diameter 会话并完成认证。

**用法：**
```
mcar [username] [password]
mcar create_session  # 注册时立即创建通信会话
```

**示例：**
```
MAGIC[未注册]> mcar
[INFO] 开始客户端认证注册 (MCAR)...
[INFO] 创建 MCAR 请求消息...
[INFO] 添加必需 AVP...
[INFO] 发送 MCAR 请求到服务器...
  Origin-Host: efb-client.test.local
  Destination-Realm: magic.arinc.com
  Session-Id: efb-client.test.local;1700000000;12345
[SUCCESS] MCAR 请求已发送！
[INFO] 等待服务器应答...
```

### 通信会话管理

#### `mccr` - 通信会话创建/修改/释放

管理数据通信会话的生命周期和参数。

**用法：**
```
mccr create [profile] [bw_down] [bw_up]  # 创建新会话
mccr modify [bw_down] [bw_up]            # 修改带宽
mccr release                             # 释放会话
```

**示例：**
```
# 创建会话（使用配置文件参数）
MAGIC[已注册]> mccr create
[INFO] 创建新通信会话 (MCCR Create)...
[INFO] 添加通信请求参数...
  Profile: IP_DATA
  Requested BW: ↓5000000 / ↑1000000 bps
[SUCCESS] MCCR Create 请求已发送！

# 修改带宽
MAGIC[已注册+通信中]> mccr modify 10000000 2000000
[INFO] 修改通信会话参数 (MCCR Modify)...
  新带宽要求: ↓10000000 / ↑2000000 bps
[SUCCESS] MCCR Modify 请求已发送！

# 释放会话
MAGIC[已注册+通信中]> mccr release
[INFO] 释放通信会话 (MCCR Release)...
[SUCCESS] MCCR Release 请求已发送！
```

### 状态查询

#### `msxr` - 查询系统状态

主动查询 MAGIC 服务器的状态信息。

**用法：**
```
msxr [type]
```

**参数：**
- `type`: 状态类型
  - `0` - 全部状态（默认）
  - `1` - DLM（数据链路管理器）状态
  - `2` - 链路状态
  - `3` - 已注册客户端列表

**示例：**
```
MAGIC[已注册]> msxr 1
[INFO] 查询系统状态 (MSXR)...
  Status-Type: 1
[SUCCESS] MSXR 请求已发送！
[INFO] 等待服务器返回状态信息...
```

### 计费管理

#### `madr` - 计费数据查询

查询计费记录（CDR）列表或详细内容。

**用法：**
```
madr list                # 查询所有 CDR 列表
madr data <cdr_id>       # 查询指定 CDR 详情
```

**示例：**
```
MAGIC[已注册]> madr list
[INFO] 查询 CDR 列表 (MADR List)...
[SUCCESS] MADR 请求已发送！

MAGIC[已注册]> madr data CDR-12345
[INFO] 查询 CDR 详细数据 (MADR Data)...
  CDR-ID: CDR-12345
[SUCCESS] MADR 请求已发送！
```

#### `macr` - 计费控制

重启指定会话的计费记录。

**用法：**
```
macr restart <session_id>
```

**示例：**
```
MAGIC[已注册]> macr restart efb-client.test.local;1700000000;12345
[INFO] 重启 CDR (MACR)...
  Session-Id: efb-client.test.local;1700000000;12345
[SUCCESS] MACR 请求已发送！
```

### 会话终止

#### `str` - 终止 Diameter 会话

终止当前的 Diameter 会话并注销。

**用法：**
```
str [reason]
```

**参数：**
- `reason`: 终止原因码
  - `0` - 正常终止（默认）
  - `1` - 管理员强制
  - `4` - 客户端请求

**示例：**
```
MAGIC[已注册]> str
[INFO] 终止 Diameter 会话 (STR)...
  Termination-Cause: 1
  Session-Id: efb-client.test.local;1700000000;12345
[SUCCESS] STR 请求已发送！会话已终止
```

### 状态和配置

#### `status` - 显示当前状态

显示客户端的当前状态信息。

**示例：**
```
MAGIC[已注册]> status

╔══════════════════════════════════════════════╗
║          MAGIC Client Status                ║
╚══════════════════════════════════════════════╝

注册状态: 已注册
Session-Id: efb-client.test.local;1700000000;12345
通信会话: 活跃

客户端信息:
  Client-ID: EFB-A320-001
  Tail-Number: B-8888
  Aircraft-Type: A320
  Origin-Host: efb-client.test.local
  Origin-Realm: test.local

当前配置:
  Profile-Name: IP_DATA
  Requested-BW: ↓5000000 / ↑1000000 bps
  QoS-Level: 1
  Priority-Class: 3
```

#### `config` - 配置管理

显示或重新加载配置。

**用法：**
```
config [show|reload]
```

**示例：**
```
MAGIC[已注册]> config show
╔══════════════════════════════════════════════╗
║          Configuration Details              ║
╚══════════════════════════════════════════════╝

Diameter 配置:
  Vendor-ID: 13712
  Auth-App-ID: 16777300
  Destination-Realm: magic.arinc.com

带宽配置:
  Requested: ↓5000000 / ↑1000000 bps
  Required: ↓1000000 / ↑512000 bps
...
```

### 帮助和退出

#### `help` - 显示帮助

显示命令帮助信息。

**用法：**
```
help [command]  # 显示指定命令的详细帮助
help            # 显示所有命令列表
```

#### `quit` - 退出程序

退出 MAGIC 客户端（会自动发送 STR 终止会话）。

**用法：**
```
quit  # 或 exit, q
```

## 典型使用流程

### 1. 基本注册和通信

```bash
# 启动客户端
./magic_client

# 1. 注册到服务器
MAGIC[未注册]> mcar
[SUCCESS] MCAR 请求已发送！

# 2. 创建通信会话
MAGIC[已注册]> mccr create
[SUCCESS] MCCR Create 请求已发送！

# 3. 查看状态
MAGIC[已注册+通信中]> status

# 4. 查询 DLM 状态
MAGIC[已注册+通信中]> msxr 1

# 5. 完成后释放会话
MAGIC[已注册+通信中]> mccr release

# 6. 注销
MAGIC[已注册]> str

# 7. 退出
MAGIC[未注册]> quit
```

### 2. 动态调整带宽

```bash
# 1. 注册并创建会话
mcar
mccr create

# 2. 检查当前带宽
status

# 3. 增加带宽需求
mccr modify 10000000 2000000

# 4. 查看更新后的状态
status
```

### 3. 计费管理

```bash
# 1. 查询当前 CDR 列表
madr list

# 2. 查询特定 CDR 详情
madr data CDR-12345

# 3. 重启 CDR（开始新的计费记录）
macr restart efb-client.test.local;1700000000;12345
```

## 状态提示符说明

CLI 提示符会根据当前状态动态变化：

- 🔴 `MAGIC[未注册]>` - 客户端未注册
- 🟡 `MAGIC[已注册]>` - 已注册但无活跃通信会话
- 🟢 `MAGIC[已注册+通信中]>` - 已注册且有活跃通信会话

## 错误处理

### 常见错误

1. **未注册就执行其他命令**
   ```
   [ERROR] 客户端未注册！请先执行 'mcar' 命令注册
   ```
   解决：先执行 `mcar` 命令完成注册

2. **会话已存在**
   ```
   [WARN] 客户端已注册，Session-Id: xxx
   [INFO] 如需重新注册，请先执行 'str' 终止当前会话
   ```
   解决：执行 `str` 终止当前会话，然后重新 `mcar`

3. **无活跃会话**
   ```
   [ERROR] 当前无活跃通信会话！请先执行 'mccr create'
   ```
   解决：执行 `mccr create` 创建会话

## 调试技巧

### 1. 启用详细日志

在 `magic.conf` 中设置：
```conf
LOG_LEVEL = 5         # 5=DEBUG 级别
DUMP_MESSAGES = 1     # 打印完整 Diameter 消息
```

### 2. 查看 freeDiameter 日志

```bash
tail -f /var/log/freeDiameter.log
```

### 3. 检查网络连接

```bash
# 测试到服务器的连接
telnet magic-server.arinc.com 3868
```

## 注意事项

1. **认证凭证安全**：不要在配置文件中使用明文密码，建议使用环境变量或密钥管理系统

2. **会话管理**：始终在退出前执行 `str` 正常终止会话，避免服务器端资源泄漏

3. **带宽申请**：申请的带宽应在合理范围内，避免超出航空通信系统限制

4. **计费记录**：定期查询并备份 CDR，用于后续审计和结算

## 故障排查

### 问题：无法连接到服务器

1. 检查网络连接和防火墙
2. 验证服务器地址和端口配置
3. 检查 TLS 证书是否正确配置

### 问题：认证失败

1. 验证用户名和密码是否正确
2. 检查客户端证书是否有效
3. 确认服务器端用户已创建

### 问题：带宽申请被拒绝

1. 检查申请的带宽是否超出限制
2. 验证优先级和 QoS 等级配置
3. 查询 DLM 状态确认可用资源

## 技术支持

如有问题或建议，请联系：
- 邮箱: support@magic-client.com
- 文档: https://docs.magic-client.com

---

**版本**: 1.0
**日期**: 2025-11-22
**基于**: ARINC 839-2014 标准
