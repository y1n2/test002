# MAGIC Client 快速启动指南

## 问题诊断

如果您遇到"程序显示界面后自动退出"的问题，原因是：

**readline 库需要交互式终端（TTY）才能正常工作**

## 解决方案

### 方法 1：直接在终端运行（推荐）

```bash
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client -c '/home/zhuwuhui/freeDiameter/magic_client/magic_client.conf' -dd
```

**注意事项：**
- ✅ 必须在真实的终端窗口中直接运行
- ❌ 不要通过 SSH 后台运行 (`nohup`, `&`, `screen` 需要特殊配置)
- ❌ 不要重定向标准输入 (`< /dev/null` 会导致 readline 失败)
- ❌ 不要在非交互式脚本中调用

### 方法 2：使用启动脚本

```bash
cd /home/zhuwuhui/freeDiameter/magic_client
./start_magic_client.sh
```

### 方法 3：通过 SSH 远程运行

如果需要通过 SSH 运行，请使用：

```bash
ssh -t user@host "cd /path/to/build && ./magic_client -c config.conf -dd"
```

`-t` 参数强制分配伪终端（PTY）。

### 方法 4：在 screen 或 tmux 中运行

```bash
# 使用 screen
screen -S magic_client
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client -c '../magic_client.conf' -dd

# 使用 tmux
tmux new -s magic_client
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client -c '../magic_client.conf' -dd
```

## 验证程序正常运行

成功启动后，您应该看到：

```
╔══════════════════════════════════════════════╗
║                                              ║
║      MAGIC Client - ARINC 839-2014          ║
║      航空电子 Diameter 通信客户端            ║
║                                              ║
╚══════════════════════════════════════════════╝

输入 help 查看所有命令
输入 mcar 开始客户端注册

MAGIC[未注册]> _
```

此时光标应该在提示符后闪烁，等待您输入命令。

## 常用命令

```bash
# 查看帮助
MAGIC> help

# 查看状态
MAGIC> status

# 客户端认证
MAGIC> mcar

# 退出程序
MAGIC> quit
# 或按 Ctrl+D
```

## 故障排查

### 症状：程序立即退出，没有提示符

**原因：** readline 检测到标准输入不是 TTY

**检查方法：**
```bash
# 在运行程序的终端中执行
tty
# 应该输出类似 /dev/pts/0，而不是 "not a tty"
```

**解决方法：**
1. 确保在真实终端中运行，不是通过管道或文件重定向
2. 如果使用 SSH，加上 `-t` 参数
3. 如果使用 screen/tmux，确保会话是连接状态

### 症状：连接失败 "DIAMETER_UNABLE_TO_COMPLY"

**原因：** 服务器拒绝连接，可能服务器已有活动连接

**解决方法：**
1. 检查服务器状态：
   ```bash
   # 在服务器端查看连接
   ps aux | grep freediameterd
   netstat -antp | grep 5870
   ```

2. 重启服务器：
   ```bash
   # 在服务器端
   killall freediameterd
   # 然后重新启动服务器
   ```

3. 修改客户端配置，使用不同的 Identity

### 症状：TLS 认证失败

**检查证书：**
```bash
# 验证证书有效性
openssl verify -CAfile /path/to/ca.crt /path/to/client.crt

# 查看证书信息
openssl x509 -in /path/to/client.crt -text -noout

# 检查证书是否过期
openssl x509 -in /path/to/client.crt -noout -dates
```

## 服务器要求

确保服务器正确配置：

1. **服务器必须运行** - 在另一台机器或同一机器的不同端口
2. **NoRelay 配置** - 客户端配置文件应包含 `NoRelay;`
3. **证书匹配** - 客户端和服务器证书必须由同一个 CA 签发
4. **防火墙开放** - 端口 5870 (TLS) 或 3868 (TCP) 必须可访问

## 开发调试

如果需要查看详细的调试信息：

```bash
# 最详细的调试输出
./magic_client -c config.conf -dddd

# 只显示错误和警告
./magic_client -c config.conf

# 中等详细程度
./magic_client -c config.conf -dd
```

## 移除调试输出

调试完成后，移除代码中的 `[DEBUG]` 输出：

编辑这些文件并删除包含 `[DEBUG]` 的 printf 语句：
- `magic_client/magic_client.c`
- `magic_client/cli_interface.c`

然后重新编译：
```bash
cd /home/zhuwuhui/freeDiameter/magic_client/build
make
```

---

**更新时间**：2025-11-22  
**适用版本**：MAGIC Client 1.0
