# MAGIC 系统测试指导书

## 1. 测试环境准备

### 1.1 软件依赖
- freeDiameter (包含 MAGIC 扩展)
- CMake 3.10+
- GCC/G++
- Flex/Bison
- GnuTLS

### 1.2 目录结构
```
/home/zhuwuhui/freeDiameter/
├── build/                  # 编译输出目录
├── conf/                   # 配置文件目录
│   ├── magic_server.conf   # 服务器配置
│   └── magic_client.conf   # 客户端配置
├── extensions/
│   └── app_magic/          # MAGIC 扩展源码
│       ├── config/         # 策略配置文件
│       │   ├── Central_Policy_Profile.xml
│       │   ├── Client_Profile.xml
│       │   └── Datalink_Profile.xml
│       └── ...
└── ...
```

### 1.3 启动顺序
1. **清理环境**
   ```bash
   pkill -9 freeDiameterd
   pkill -9 dlm
   pkill -9 magic_client
   rm -f /tmp/*.sock
   ```

2. **启动服务器**
   ```bash
   cd /home/zhuwuhui/freeDiameter/build
   ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf > /tmp/fd_server.log 2>&1 &
   ```

3. **启动 DLM 模拟器**
   ```bash
   # 启动 Cellular DLM (LTE)
   cd /home/zhuwuhui/freeDiameter/build/extensions/app_magic/link_simulator
   ./dlm_cellular > /tmp/dlm_cellular.log 2>&1 &
   
   # 启动 WiFi DLM
   ./dlm_wifi > /tmp/dlm_wifi.log 2>&1 &
   
   # 启动 Satcom DLM
   ./dlm_satcom > /tmp/dlm_satcom.log 2>&1 &
   ```

4. **启动客户端**
   ```bash
   cd /home/zhuwuhui/freeDiameter/build/magic_client
   ./magic_client
   ```

---

## 2. 测试用例详细指导

### 测试用例 1: 基础连接与注册 (MCAR)

**目标：** 验证客户端能否成功连接服务器并完成 MAGIC 注册。

**前置条件：** 服务器已启动，DLM 已启动。

**测试步骤：**
1. 启动 `magic_client`
2. 在客户端 CLI 输入：`connect`
3. 在客户端 CLI 输入：`mcar`

**预期结果：**
- 客户端显示 `[SUCCESS] MCAR 注册成功`
- 服务器日志显示 `Processing MCAR` 和 `DIAMETER_SUCCESS`
- 客户端状态变为 `[已注册]`

**故障排查：**
- 如果连接失败，检查证书路径和 IP 配置
- 如果注册失败，检查 `Client_Profile.xml` 中的用户名密码

---

### 测试用例 2: 链路状态感知 (Link Management)

**目标：** 验证服务器能否正确感知 DLM 上报的链路状态。

**前置条件：** 服务器运行中，DLM 进程运行中。

**测试步骤：**
1. 查看服务器日志：`tail -f /tmp/fd_server.log`
2. 观察 `Link_Up` 消息处理

**预期结果：**
- 日志显示 `[app_magic] ✓ Link LINK_CELLULAR is now ONLINE`
- 日志显示 `[app_magic] ✓ Link LINK_WIFI is now ONLINE`
- 日志显示 `[app_magic] ✓ Link LINK_SATCOM is now ONLINE`
- 所有链路 `is_active=1`

**故障排查：**
- 如果链路未上线，检查 `/tmp/mihf.sock` 是否存在
- 检查 DLM 日志 `/tmp/dlm_*.log` 是否有错误

---

### 测试用例 3: 策略决策 - 默认业务 (Best Effort)

**目标：** 验证默认业务类型下，系统优先选择 WIFI 链路。

**前置条件：** 客户端已注册，所有链路在线。

**测试输入：**
```bash
# 请求 10 Mbps 带宽，优先级 1，QoS 等级 2
MAGIC[已注册]> mccr modify 5000 10000 1 2
```

**预期结果：**
- 客户端显示 `[SUCCESS] 命令执行成功`
- 客户端显示 `Selected-DLM = LINK_WIFI`
- 客户端显示 `Granted-BW = 10000 kbps`

**原理分析：**
- 策略：`GROUND_OPS` -> `BEST_EFFORT`
- 排名：WIFI (1) > CELLULAR (2) > SATCOM (3)
- 结果：WIFI 得分最高

---

### 测试用例 4: 策略决策 - 交互式业务 (Interactive)

**目标：** 验证交互式业务类型下，系统优先选择 CELLULAR 链路。

**前置条件：** 
- 修改 `Client_Profile.xml`，将 `TrafficClass` 改为 `INTERACTIVE`
- 重启服务器

**测试输入：**
```bash
MAGIC[已注册]> mccr modify 5000 10000 1 2
```

**预期结果：**
- 客户端显示 `Selected-DLM = LINK_CELLULAR`
- 客户端显示 `Granted-BW = 10000 kbps`

**原理分析：**
- 策略：`GROUND_OPS` -> `INTERACTIVE`
- 排名：CELLULAR (1) > WIFI (2) > SATCOM (3)
- 结果：CELLULAR 得分最高

---

### 测试用例 5: 带宽限制测试

**目标：** 验证当请求带宽超过客户端限制时，请求被拒绝。

**前置条件：** 
- `Client_Profile.xml` 中 `MaxBandwidth` 设为 5000000 (5 Mbps)
- 重启服务器

**测试输入：**
```bash
# 请求 10 Mbps (超过 5 Mbps 限制)
MAGIC[已注册]> mccr modify 5000 10000 1 2
```

**预期结果：**
- 客户端显示 `[ERROR] 命令执行失败 (Result-Code: 5012)`
- 服务器日志显示 `Requested BW (10000 kbps) exceeds client limit`

---

### 测试用例 6: 链路故障切换 (Failover)

**目标：** 验证当首选链路不可用时，系统自动选择次优链路。

**前置条件：** 
- 恢复 `BEST_EFFORT` 配置（WIFI 优先）
- 确保 WIFI 和 CELLULAR 都在线

**测试步骤：**
1. 模拟 WIFI 故障：`pkill -9 dlm_wifi`
2. 等待几秒让心跳超时（或手动触发 Link_Down）
3. 发送 MCCR 请求：`mccr modify 5000 10000 1 2`

**预期结果：**
- 客户端显示 `Selected-DLM = LINK_CELLULAR`
- 服务器日志显示 `Link LINK_WIFI: Offline`

**原理分析：**
- WIFI 离线，策略引擎跳过
- CELLULAR 成为在线链路中得分最高的

---

## 3. 常用调试命令

### 3.1 查看日志
```bash
# 实时查看服务器日志
tail -f /tmp/fd_server.log

# 过滤策略决策日志
grep "Policy Decision" /tmp/fd_server.log

# 过滤链路状态日志
grep "Link.*is now" /tmp/fd_server.log
```

### 3.2 检查端口与进程
```bash
# 检查 freeDiameter 端口
netstat -anp | grep 3870

# 检查 UNIX 域套接字
lsof -U | grep "dlm\|mihf"
```

### 3.3 抓包分析
```bash
# 抓取 Diameter 流量 (SCTP)
tcpdump -i lo port 3870 -w magic_trace.pcap
```

---

## 4. 配置文件修改指南

### 4.1 修改客户端带宽限制
编辑 `extensions/app_magic/config/Client_Profile.xml`:
```xml
<MaxBandwidth>20000000</MaxBandwidth> <!-- 修改为 20 Mbps -->
```

### 4.2 修改业务类型
编辑 `extensions/app_magic/config/Client_Profile.xml`:
```xml
<TrafficClass>INTERACTIVE</TrafficClass> <!-- 修改为 INTERACTIVE -->
```

### 4.3 修改策略优先级
编辑 `extensions/app_magic/config/Central_Policy_Profile.xml`:
```xml
<PolicyRule traffic_class="BEST_EFFORT">
    <!-- 交换排名 -->
    <PathPreference ranking="1" link_id="LINK_CELLULAR" />
    <PathPreference ranking="2" link_id="LINK_WIFI" />
</PolicyRule>
```

---

**测试报告模板：**

| 测试 ID | 测试名称 | 输入参数 | 预期结果 | 实际结果 | 状态 | 备注 |
|---------|----------|----------|----------|----------|------|------|
| TC-01 | MCAR 注册 | 无 | 注册成功 | 注册成功 | PASS | |
| TC-02 | 链路感知 | 无 | 3链路在线 | 3链路在线 | PASS | |
| TC-03 | 默认策略 | 10M/BestEffort | 选 WIFI | 选 WIFI | PASS | |
| TC-04 | 交互策略 | 10M/Interactive | 选 CELLULAR | 选 CELLULAR | PASS | |
| TC-05 | 带宽限制 | 10M > 5M | 拒绝 (5012) | 拒绝 (5012) | PASS | |
| TC-06 | 故障切换 | Kill WIFI | 选 CELLULAR | 选 CELLULAR | PASS | |
