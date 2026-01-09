
# MAGIC 协议完整测试指导

**版本**: 2.2  
**日期**: 2025-12-31  
**适用范围**: ARINC 839 MAGIC 系统集成测试（含多会话、多客户端同 IP、多链路、多 TFT）

本文档面向测试人员，目标是提供一套“可直接照着执行”的完整步骤，用于验证：

- MCAR/MCCR/STR 基础流程
- 同一客户端（同 Origin-Host）多会话并发
- 多客户端同 IP 不同端口并发（典型：同一台机上多个进程）
- MCCR 新会话不得清理 iptables（避免影响其他会话）
- 会话关闭仅在 STR / 看门狗超时 / 致命错误时触发数据面关闭
- TFT 五元组隔离生效

---

## 1. 测试环境准备

### 1.1 系统拓扑（示例）

```
┌───────────────────────────┐      ┌──────────────────────────┐
│ Magic Client Host          │      │ MAGIC Server Host         │
│ 192.168.126.5              │      │ 192.168.126.1             │
│ - magic_client A :5890     │<---->│ freeDiameterd + app_magic │
│ - magic_client B :5892     │      │ + dataplane(iptables/ip)  │
│ - magic_client C :5894     │      └───────────┬──────────────┘
└───────────────────────────┘                  │
                                                                                             │ MIH / IPC
                                                                                             v
                                                                            ┌─────────────────┐
                                                                            │ DLM (3个)        │
                                                                            │ Cellular/WiFi/   │
                                                                            │ Satcom           │
                                                                            └─────────────────┘
```

### 1.2 启动组件

#### 1.2.1 启动 freeDiameter 服务器（MAGIC）

```bash
cd /home/zhuwuhui/freeDiameter/build
sudo freeDiameterd -c '/home/zhuwuhui/freeDiameter/tools/cert_generator/output/server_profile/fd_server.conf' -d
```

#### 1.2.2 启动 DLM 模拟器（3 条链路）

```bash
# 终端 1 - Cellular
cd /home/zhuwuhui/freeDiameter/DLM_CELLULAR
sudo ./dlm_cellular_standard

# 终端 2 - WiFi
cd /home/zhuwuhui/freeDiameter/DLM_WIFI
sudo ./dlm_wifi_standard

# 终端 3 - Satcom
cd /home/zhuwuhui/freeDiameter/DLM_SATCOM
sudo ./dlm_satcom_standard
```

#### 1.2.3 启动 ADIF 模拟器（可选：联调航电数据）

```bash
cd /home/zhuwuhui/freeDiameter/adif_simulator/build
./adif_simulator
```

### 1.3 网络与转发基本检查（重要）

在客户端主机（192.168.126.5）上：

```bash
sudo ip route replace default via 192.168.126.1 dev ens37 metric 50
ping -c 3 192.168.126.1
```

在服务器主机（192.168.126.1）上（如果服务器需要转发业务流量）：

```bash
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -L FORWARD -n --line-numbers
```

若 FORWARD 默认策略为 DROP，需要确保系统已有允许规则或由 MAGIC dataplane 正确插入规则。

### 1.4 观测与取证命令（建议先开两个窗口）

服务器侧（观测 dataplane 状态）：

```bash
sudo iptables -S OUTPUT
sudo iptables -S FORWARD
sudo iptables -t nat -S POSTROUTING
ip rule list
```

若项目使用 ipset：

```bash
sudo ipset list 2>/dev/null | head -200
```

---

## 2. 基础测试场景（单会话）

### 2.1 启动 magic_client (EFB-01)

使用 `tools/cert_generator/output` 下生成的真实配置文件启动：

```bash
cd /home/zhuwuhui/freeDiameter/magic_client
./magic_client -c /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_fd_config/fd_efb-01.conf \
               -m /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_magic_config/efb-01_magic.conf
```

在客户端 CLI 中执行：

```
magic> status
```

期望：显示已连接 Diameter，并且尚无会话。

> 注：客户端 UI 可能显示状态为 IDLE；服务器内部会话状态机可能使用 INIT/AUTHENTICATED/ACTIVE 等命名，测试以“是否能正常收发、是否能正确开关数据面”为准。

### 2.2 MCAR：纯认证（Auth Only）

- 命令：`mcar auth`
- 期望：
    - Result-Code=2001
    - 会话进入“已认证”状态（尚未开通数据面、未配置 TFT）

### 2.3 MCAR：认证并订阅状态（Subscribe）

- 命令：`mcar subscribe 7`
- 期望：
    - Result-Code=2001
    - 后续收到 MSCR 推送（与 DLM 状态/链路状态相关）

### 2.4 MCCR：Start 建链（开启数据面）

- 命令：`mccr start IP_DATA 512 2000 2 0`
- 期望：
    - 分配 Selected-Link-ID
    - 数据面生效（iptables/ip rule 有新增）
    - 如配置了 TFT（见 `efb-01_magic.conf` 中的 `TFT_GROUND.1` 等），则仅 TFT 允许的五元组能通

### 2.5 MCCR：Modify 修改（带宽/优先级/QoS）

- 命令：`mccr modify 256 512 2 0`
- 期望：
    - Result-Code=2001
    - Granted-BW 与请求匹配或被策略降级

### 2.6 MCCR：Stop 释放资源（会话仍存在）

- 命令：`mccr stop`
- 期望：
    - 会话仍可继续 STR 或再次 start
    - 数据面资源回收：对应会话的路由 / TFT 规则撤销
    - 若同 IP 无其他会话：可能进入“阻断/黑洞”状态（由 dataplane 策略决定）

### 2.7 STR：终止会话（关闭会话）

- 命令：`str`
- 期望：
    - 服务器侧删除该 Session-ID 对应的 dataplane 规则
    - 生成/关闭 CDR（如果已启用计费）

---

## 3. 多会话测试（同一个客户端）

目标：验证 ARINC 839 允许同一客户端（同 Origin-Host）通过多次 MCAR 建立多个 Session-ID，且各会话互不影响。

### 3.1 步骤

1) 启动 1 个 magic_client（单进程，使用 EFB-01 配置）。

2) 第一次 MCAR + MCCR start（会话 S1）。
   ```
   magic> mcar subscribe 7
   magic> mccr start IP_DATA 512 2000 2 0
   ```

3) 再次 MCAR（同一个客户端再次认证，创建新会话 S2），再 MCCR start。
   ```
   magic> mcar subscribe 7
   magic> mccr start VOICE 64 64 1 0
   ```

4) **查看与切换会话（v2.2 新增）**：
   ```
   magic> session list
   # 显示所有活跃会话 ID
   
   magic> session select <Session-Id-of-S1>
   # 切换回 S1
   ```

5) 验证：

- 两个会话都能发起业务流量（按各自 TFT 限制）
- 关闭 S2（STR）后，S1 业务不受影响
  ```
  magic> str <Session-Id-of-S2>
  # 或者先 select S2 再 str
  ```

### 3.2 关键观测点（iptables 不应被错误清理）

### 3.2 关键观测点（iptables 不应被错误清理）

在服务器侧做“前后对比”：

```bash
sudo iptables -S OUTPUT | grep -n '192.168.126.5'
sudo iptables -S FORWARD | grep -n '192.168.126.5'
sudo iptables -t nat -S POSTROUTING | grep -n '192.168.126.5'
ip rule list | grep -n '192.168.126.5'
```

期望：

- 当 S2 进行 MCCR start 时，不会把 S1 的规则删掉（尤其是 TFT/mangle 或 FORWARD/OUTPUT/NAT 相关规则）。
- 当 S2 STR 时，若 S1 仍存在，则不应对 192.168.126.5 添加黑洞/阻断动作。

---

## 4. 多客户端同 IP 不同端口测试（3 个客户端并发）

目标：验证“同 IP 多连接/多端口”的并发不会互相清理规则。

### 4.1 启动 3 个客户端

开 3 个终端分别执行（注意使用不同的配置文件以避免端口冲突）：

**终端 A (EFB-01, Port 5890):**
```bash
cd /home/zhuwuhui/freeDiameter/magic_client
./magic_client -c /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_fd_config/fd_efb-01.conf \
               -m /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_magic_config/efb-01_magic.conf
```

**终端 B (IFE-01, Port 5892):**
```bash
cd /home/zhuwuhui/freeDiameter/magic_client
./magic_client -c /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_fd_config/fd_ife-01.conf \
               -m /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_magic_config/ife-01_magic.conf
```

**终端 C (OMT-01, Port 5894):**
```bash
cd /home/zhuwuhui/freeDiameter/magic_client
./magic_client -c /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_fd_config/fd_omt-01.conf \
               -m /home/zhuwuhui/freeDiameter/tools/cert_generator/output/client_magic_config/omt-01_magic.conf
```

在三个客户端分别执行：

1) `mcar subscribe 7`
2) `mccr start IP_DATA 512 1500 2 0`

### 4.2 期望

- 三个客户端都能进入可通信状态
- 任意一个客户端重试 MCCR / 修改参数，不影响其他两个客户端的业务流
- 任意一个客户端 STR 后，剩余两个客户端的流量仍保持

---

## 5. TFT 流量控制测试（五元组隔离）

目标：验证：

1) 未在 TFT 白名单内的流量不能通过
2) 在 TFT 白名单内的流量可以通过
3) 同 IP 多会话/多客户端时，TFT 规则不会被错误清理

### 5.1 测试方法（通用）

1) 在 MCCR 中配置/携带 TFT（已在上述 `*_magic.conf` 中预置）。
   - EFB-01 允许: src_port=8081 -> dst_port=5880
   - IFE-01 允许: src_port=8082 -> dst_port=5880
   - OMT-01 允许: src_port=8083 -> dst_port=5880

2) 从客户端发起符合 TFT 的业务流（如 curl/nc/iperf）。
   - EFB-01: `nc -u 10.3.3.6 5880 -p 8081` (应通)
   - IFE-01: `nc -u 10.2.2.8 5880 -p 8082` (应通)

3) 再发起不符合 TFT 的业务流，确认被阻断。
   - EFB-01: `nc -u 10.3.3.6 5880 -p 9999` (应不通)

### 5.2 服务器侧验证

查看与该 Session-ID 相关的 mangle/mark 规则（如项目已实现）：

```bash
sudo iptables -t mangle -S | head -200
sudo iptables -t mangle -S | grep -E 'MARK|connmark|fwmark|session' -n
```

---

## 6. 链路切换与链路故障测试

目标：验证 DLM 链路状态变化时的会话行为（如 SUSPENDED/恢复/切换）。

### 6.1 人工模拟链路 down/up（示例）

在服务器侧（或 DLM 运行主机）执行：

```bash
sudo ip link set ens38 down
sleep 3
sudo ip link set ens38 up
```

期望：

- 客户端收到 MSCR（若已订阅）提示链路变化
- 会话可进入挂起并在链路恢复后继续（具体表现依策略/实现）

---

## 7. 会话关闭触发条件验证（STR / 看门狗超时 / 致命错误）

目标：符合你当前设计要求：

- **新会话 MCCR**：不得执行“清理 iptables”这种影响其他会话的动作
- **关闭会话**：仅在 STR / 看门狗超时 / 致命错误时执行数据面关闭动作

### 7.1 STR 关闭

对其中一个会话执行 `str`。

**方法 A：指定 Session-ID 关闭**
```
magic> session list
magic> str <Session-Id>
```

**方法 B：切换上下文后关闭**
```
magic> session select <Session-Id>
magic> str
```

期望：

- 仅删除该 Session-ID 对应的 dataplane 记录
- 若同 IP 仍有其他会话，**不得**添加黑洞/阻断动作

### 7.2 看门狗超时（若已实现）

常见做法：停止客户端心跳/不再发送任何请求，等待 server watchdog 触发。

期望：

- 超时会话被关闭并回收资源
- 若同 IP 仍有其他会话，不影响其他会话

### 7.3 致命错误

常见做法：停止某条 DLM 或模拟资源分配失败（根据你们的故障注入方式）。

期望：

- 仅影响对应会话/对应链路的资源
- 不出现“全局清 iptables”导致其他会话断流

---

## 8. 性能与压力测试（建议）

### 8.1 并发会话数

目标：验证配置的 max_concurrent_sessions 是否生效。

- 在同一客户端上重复创建会话直到超过上限
- 期望：超过后返回明确失败码或策略拒绝

### 8.2 规则规模

目标：验证 TFT 条目多时仍稳定。

- 单会话携带多条 TFT（或多会话各自携带）
- 连续 start/modify/stop/str 循环 50 次
- 期望：iptables/ip rule 不泄露、不爆炸增长

---

## 9. 故障排查（快速定位）

### 9.1 业务不通但 Diameter 成功

优先检查：

```bash
sudo iptables -L FORWARD -n --line-numbers
sudo iptables -S OUTPUT | head -200
sudo iptables -t nat -S POSTROUTING | head -200
ip rule list
ip route show table main | head -200
```

### 9.2 规则被意外清理（你这个问题的典型表现）

现象：

- 新建会话 MCCR 后，旧会话 TFT/ACCEPT/NAT 规则消失
- 或关闭其中一个会话后，其他会话断流

定位：

- 对比执行前后 `iptables -S` 输出
- 查 server 日志中是否出现 “清理/删除规则/blackhole” 相关日志

### 9.3 常见返回码提示

- `5005 MISSING_AVP`：命令参数缺失
- `5012 UNABLE_TO_COMPLY`：资源不足/策略拒绝
- `1001 AUTHENTICATION_FAILED`：账号/密码或白名单问题

