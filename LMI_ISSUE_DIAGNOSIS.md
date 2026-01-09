## LMI 服务器未启动问题诊断

### 问题现象
- DLM 进程正常运行并发送心跳
- freeDiameterd 进程正常运行
- **但 LMI 服务器端口（UDP 1947/1948/1949）未监听**
- 导致客户端 MCCR 请求返回 "No available link resources"

### 根本原因
MAGIC 服务器的 LMI 模块未成功初始化，可能原因：

1. **权限不足**：LMI 需要绑定 UDP 端口，可能需要 root 权限
2. **端口冲突**：端口被其他进程占用
3. **初始化失败**：app_magic.fdx 加载时 LMI 初始化报错但未中断

### 解决方案

#### 方案 1：重启服务器并查看完整日志
```bash
# 停止当前服务器
sudo pkill -f "freeDiameterd.*fd_server"

# 重新启动并查看输出
cd /home/zhuwuhui/freeDiameter
sudo freeDiameterd -c tools/cert_generator/output/server_profile/fd_server.conf -d

# 在另一个终端检查端口
sudo ss -unl | grep -E "1947|1948|1949"
```

#### 方案 2：检查 LMI 初始化代码
查看 `extensions/app_magic/app_magic.c` 中的 `magic_lmi_init()` 调用。

#### 方案 3：手动测试 UDP 端口
```bash
# 测试端口是否可以绑定
sudo nc -u -l 1947  # 如果能成功绑定说明端口可用
```

### 需要检查的日志关键词
- "LMI"
- "Failed to initialize"
- "bind"
- "1947"
- "Permission denied"

