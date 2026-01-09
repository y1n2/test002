# MAGIC "No available link resources" 问题解决方案

## 问题诊断结果

### 根本原因
**架构不匹配**：DLM 原型程序使用 UDP 广播发送心跳，但 MAGIC 服务器的 LMI 模块只监听 Unix Socket，导致 DLM 无法注册到服务器。

### 详细分析

1. **DLM 进程状态**：✅ 正常运行
   - `dlm_satcom_proto` (PID 11055)
   - `dlm_wifi_proto` (PID 11617)
   - `dlm_cellular_proto` (PID 10623)
   - 正在发送 UDP 广播心跳

2. **LMI 服务器状态**：❌ 未监听 UDP 端口
   - 应监听：UDP 1947/1948/1949
   - 实际监听：Unix Socket `/tmp/magic_core.sock` 和 `/tmp/mihf.sock`
   - 导致 DLM 心跳无法被接收

3. **结果**：
   - LMI 模块中没有注册的 DLM
   - 策略引擎无法找到可用链路
   - MCCR 请求返回 "No available link resources"

## 解决方案

### 🔧 方案 1：修改代码添加 UDP 监听服务器（推荐，需要重新编译）

在 `extensions/app_magic/magic_lmi.c` 中添加 UDP 监听线程：

```c
// 在 magic_lmi_start_dgram_server() 后添加
int magic_lmi_start_udp_listener(MagicLmiContext* ctx) {
    // 创建 UDP socket
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    // 绑定到 0.0.0.0:1947 (接收所有接口的广播)
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(1947);  // 主端口
    
    bind(udp_fd, ...);
    
    // 启动接收线程处理 DLM 心跳
}
```

然后在 `app_magic.c` 的初始化中调用。

### 🚀 方案 2：快速修复 - 直接在配置文件中标记链路为可用（临时绕过）

修改 `Datalink_Profile.xml`，在初始化时强制标记链路为 ACTIVE：

```xml
<Link id="LINK_SATCOM">
    <ForceActive>true</ForceActive>  <!-- 添加此标志 -->
    ...
</Link>
```

### ⚡ 方案 3：使用 ADIF 模拟器代替 DLM 原型（推荐用于测试）

ADIF 模拟器使用 Unix Socket 通信，与当前 LMI 架构匹配：

```bash
cd /home/zhuwuhui/freeDiameter/adif_simulator/build
sudo ./adif_simulator
```

检查 ADIF 是否连接：
```bash
sudo ss -x | grep magic_core.sock
```

### 🔧 方案 4：修改 DLM 原型使用 Unix Socket（需要重新编译 DLM）

修改 `DLM_SATCOM/dlm_satcom_prototype.c` 等文件，将 UDP 广播改为 Unix Socket 连接。

## 立即可用的临时解决方案

创建一个脚本手动添加 DLM 到 LMI 缓存（仅用于测试）：

```bash
#!/bin/bash
# 注入伪造的 DLM 注册信息到 LMI

# 这需要修改代码在 LMI 初始化时预注册 DLM
# 或者使用 ADIF 模拟器替代 DLM 原型
```

## 推荐操作步骤

### 立即执行（测试）：
```bash
# 1. 停止当前的 DLM 原型
sudo pkill dlm_satcom_proto
sudo pkill dlm_wifi_proto  
sudo pkill dlm_cellular_proto

# 2. 启动 ADIF 模拟器
cd /home/zhuwuhui/freeDiameter/adif_simulator/build
sudo ./adif_simulator

# 3. 检查链路是否注册
# 在另一个终端查看服务器日志，应该看到：
# "DLM registered: LINK_SATCOM"
```

### 长期解决（生产）：
1. 在 `magic_lmi.c` 中添加 UDP 监听服务器
2. 重新编译 MAGIC 服务器
3. 重启服务

## 验证步骤

执行以下命令检查问题是否解决：

```bash
# 1. 检查 LMI 是否有注册的 DLM
# （需要添加调试接口或查看日志）

# 2. 使用客户端测试
cd /home/zhuwuhui/freeDiameter/magic_client/build
./magic_client -m ../omt_magic.conf

# 在客户端执行：
mcar
mccr create

# 应该看到成功响应而不是 "No available link resources"
```

## 总结

当前问题是 DLM 通信架构不匹配。最快的解决方法是使用 ADIF 模拟器，它已经适配了 Unix Socket 架构。长期解决需要修改代码添加 UDP 监听支持。

