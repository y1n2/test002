# MAGIC Client

MAGIC Client 是一个安全的网络访问客户端，通过认证后使用服务端分配的IP地址访问外部服务器。

## 功能特性

- **安全认证**: 基于Diameter协议的客户端认证
- **网络配置**: 自动配置网络接口和路由
- **代理服务**: 内置HTTP/HTTPS代理服务器
- **TLS加密**: 支持TLS加密通信
- **配置管理**: 灵活的配置文件系统
- **监控统计**: 连接状态和流量统计
- **自动重连**: 连接断开时自动重连
- **日志记录**: 详细的日志记录和轮转

## 系统要求

- Linux操作系统
- GCC编译器
- libconfig库
- OpenSSL库
- 管理员权限（用于网络配置）

## 安装依赖

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential libconfig-dev libssl-dev
```

### CentOS/RHEL
```bash
sudo yum install gcc make libconfig-devel openssl-devel
```

### Fedora
```bash
sudo dnf install gcc make libconfig-devel openssl-devel
```

## 编译安装

1. 克隆或下载源代码
2. 进入项目目录
3. 编译项目

```bash
cd magic_client
make
```

编译选项：
- `make debug` - 编译调试版本
- `make release` - 编译发布版本（默认）
- `make clean` - 清理编译文件

4. 安装到系统

```bash
sudo make install
```

这将安装以下文件：
- `/usr/local/bin/magic_client` - 主程序
- `/etc/magic/client.conf` - 配置文件（如果不存在）
- `/var/log/magic/` - 日志目录

## 配置

### 配置文件位置

客户端按以下顺序查找配置文件：
1. `./magic_client.conf` (当前目录)
2. `~/.magic_client.conf` (用户主目录)
3. `/etc/magic/client.conf` (系统配置)

### 基本配置

复制示例配置文件并编辑：

```bash
cp magic_client.conf.example ~/.magic_client.conf
nano ~/.magic_client.conf
```

主要配置项：

```conf
server = {
    hostname = "your.magic.server.com";
    port = 3868;
    use_tls = true;
};

auth = {
    client_id = "your_client_id";
    username = "your_username";
    password = "your_password";
    realm = "your.realm.com";
};
```

### TLS证书配置

如果启用TLS，需要配置证书文件：

```conf
server = {
    cert_file = "/path/to/client.crt";
    key_file = "/path/to/client.key";
    ca_file = "/path/to/ca.crt";
};
```

## 使用方法

### 基本使用

```bash
# 使用默认配置
magic_client

# 指定配置文件
magic_client -c /path/to/config.conf

# 命令行参数覆盖配置
magic_client -s server.com -p 3868 -u username
```

### 命令行选项

```
-c, --config FILE       配置文件路径
-s, --server HOST       服务器地址
-p, --port PORT         服务器端口
-u, --username USER     用户名
-P, --password PASS     密码
-i, --client-id ID      客户端ID
-r, --realm REALM       认证域
-v, --verbose           详细日志
-q, --quiet             静默模式
-d, --daemon            后台运行
-n, --no-tls            禁用TLS
-h, --help              帮助信息
-V, --version           版本信息
```

### 交互式命令

在控制台模式下，可以使用以下命令：

- `status` - 显示连接状态
- `stats` - 显示统计信息
- `reconnect` - 强制重连
- `quit` - 退出程序
- `help` - 显示命令帮助

### 信号处理

- `SIGINT/SIGTERM` - 优雅关闭
- `SIGHUP` - 重启客户端
- `SIGUSR1` - 打印状态信息
- `SIGUSR2` - 打印统计信息

## 代理使用

客户端成功连接后，会启动本地代理服务器（默认端口8080）。

### 浏览器配置

配置浏览器使用HTTP代理：
- 代理地址: 127.0.0.1
- 代理端口: 8080

### 命令行工具

```bash
# 使用curl通过代理访问
curl --proxy http://127.0.0.1:8080 http://example.com

# 设置环境变量
export http_proxy=http://127.0.0.1:8080
export https_proxy=http://127.0.0.1:8080
```

## 日志和监控

### 日志文件

- `/var/log/magic_client.log` - 主日志文件
- `/var/log/magic_client_stats.log` - 统计日志

### 日志级别

- `DEBUG` - 调试信息
- `INFO` - 一般信息
- `WARNING` - 警告信息
- `ERROR` - 错误信息

### 监控统计

客户端提供以下统计信息：
- 连接状态和时长
- 发送/接收字节数
- 消息计数
- 认证统计
- 错误信息

## 故障排除

### 常见问题

1. **连接失败**
   - 检查服务器地址和端口
   - 验证网络连接
   - 检查防火墙设置

2. **认证失败**
   - 验证用户名和密码
   - 检查客户端ID和域配置
   - 查看服务端日志

3. **TLS错误**
   - 验证证书文件路径
   - 检查证书有效性
   - 确认CA证书正确

4. **网络配置失败**
   - 确保以管理员权限运行
   - 检查网络接口状态
   - 验证路由表配置

### 调试模式

启用详细日志进行调试：

```bash
magic_client -v
```

或在配置文件中设置：

```conf
log = {
    log_level = "DEBUG";
};
```

### 检查连接状态

```bash
# 发送信号查看状态
kill -USR1 $(pidof magic_client)

# 查看日志
tail -f /var/log/magic_client.log
```

## 安全注意事项

1. **保护配置文件**: 配置文件包含敏感信息，设置适当的文件权限
2. **使用TLS**: 生产环境中始终启用TLS加密
3. **证书验证**: 不要在生产环境中禁用证书验证
4. **定期更新**: 保持客户端和证书更新
5. **监控日志**: 定期检查日志文件中的异常

## 开发和贡献

### 编译调试版本

```bash
make debug
```

### 代码检查

```bash
make check
```

### 代码格式化

```bash
make format
```

### 创建发布包

```bash
make dist
```

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 支持

如有问题或建议，请联系开发团队或提交Issue。