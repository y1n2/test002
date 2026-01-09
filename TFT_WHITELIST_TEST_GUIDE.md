# TFT/NAPT 白名单验证测试指南

## 测试目标
验证 MAGIC 服务器按照 ARINC 839 §1.2.2.2 要求正确实施 TFT 和 NAPT 白名单验证。

## 测试前提条件

### 1. 白名单配置
编辑 `/home/zhuwuhui/freeDiameter/tools/cert_generator/output/server_profile/Client_Profile.xml`：

```xml
<ClientProfile>
  <ProfileName>IFE-01</ProfileName>
  <!-- ... 其他配置 ... -->
  
  <Traffic>
    <EncryptionRequired>false</EncryptionRequired>
    
    <!-- 白名单：允许的目标 IP 范围 -->
    <DestIPRange>10.2.2.0/24</DestIPRange>
    
    <!-- 白名单：允许的目标端口范围 -->
    <DestPortRange>80,443,5000-6000</DestPortRange>
    
    <!-- 允许的协议 -->
    <AllowedProtocols>
      <Protocol>TCP</Protocol>
      <Protocol>UDP</Protocol>
    </AllowedProtocols>
    
    <MaxPacketSize>1500</MaxPacketSize>
  </Traffic>
</ClientProfile>
```

### 2. 重启服务器加载配置
```bash
cd /home/zhuwuhui/freeDiameter/build
sudo make install
sudo systemctl restart freediameter
# 或手动启动
sudo ./freeDiameterd/freeDiameterd -c ../tools/cert_generator/output/server_profile/fd_server.conf
```

---

## 测试用例集

### 测试集 1: TFT IP 地址范围验证

#### 用例 1.1: 合法 IP - 单 IP 在白名单内 ✅
**客户端配置** (`ife-01_magic.conf`):
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8
```

**预期结果**:
- ✅ MCCA 返回 `Result-Code=2001` (SUCCESS)
- ✅ 日志显示 `✓ TFT whitelist validation passed (toGround)`
- ✅ 会话成功建立

**验证命令**:
```bash
sudo tail -f /var/log/freeDiameter.log | grep "TFT"
```

---

#### 用例 1.2: 合法 IP - IP 范围在白名单内 ✅
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.1-10.2.2.100
```

**预期结果**:
- ✅ 验证通过
- ✅ 日志显示目标 IP 范围 `10.2.2.1-10.2.2.100` 在白名单 `10.2.2.0/24` 内

---

#### 用例 1.3: 非法 IP - 单 IP 超出白名单 ❌
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.3.3.8
```

**预期结果**:
- ❌ MCCA 返回 `Result-Code=5003` (DIAMETER_AUTHORIZATION_REJECTED)
- ❌ MCCA 包含 `MAGIC-Status-Code=1036` (MAGIC_ERROR_TFT-INVALID)
- ❌ 日志显示错误：
  ```
  [app_magic] ✗ TFT whitelist validation FAILED: Destination IP 10.3.3.8 is outside whitelist range 10.2.2.0-10.2.2.255
  ```

**验证命令**:
```bash
sudo tail -f /var/log/freeDiameter.log | grep -A 3 "TFT whitelist validation FAILED"
```

---

#### 用例 1.4: 非法 IP - IP 范围部分超出白名单 ❌
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.200-10.2.3.10
```

**预期结果**:
- ❌ 拒绝（结束 IP `10.2.3.10` 超出 `10.2.2.0/24`）
- ❌ 错误消息：`Destination IP 10.2.2.200-10.2.3.10 is outside whitelist range 10.2.2.0-10.2.2.255`

---

### 测试集 2: TFT 端口范围验证

#### 用例 2.1: 合法端口 - 单端口在白名单内 ✅
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:80
```

**预期结果**:
- ✅ 验证通过（端口 80 在白名单 `80,443,5000-6000` 中）

---

#### 用例 2.2: 合法端口 - 端口范围在白名单内 ✅
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:5100-5500
```

**预期结果**:
- ✅ 验证通过（范围 `5100-5500` 完全包含在 `5000-6000` 内）

---

#### 用例 2.3: 非法端口 - 端口不在白名单内 ❌
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:8080
```

**预期结果**:
- ❌ 拒绝（端口 8080 不在 `80,443,5000-6000` 中）
- ❌ 错误消息：`Destination port 8080-8080 is outside whitelist range 80,443,5000-6000`

---

#### 用例 2.4: 非法端口 - 端口范围部分超出白名单 ❌
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:5500-6500
```

**预期结果**:
- ❌ 拒绝（结束端口 6500 超出白名单 `5000-6000`）

---

### 测试集 3: 协议验证

#### 用例 3.1: 合法协议 - TCP 在允许协议列表中 ✅
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:80 TCP
```

**预期结果**:
- ✅ 验证通过（TCP 在 `AllowedProtocols` 中）

---

#### 用例 3.2: 非法协议 - ICMP 不在允许协议列表中 ❌
**修改白名单配置**（仅允许 TCP）:
```xml
<AllowedProtocols>
  <Protocol>TCP</Protocol>
</AllowedProtocols>
```

**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8 UDP
```

**预期结果**:
- ❌ 拒绝（UDP 不在允许列表中）
- ❌ 错误消息：`Protocol 17 is not in allowed protocols list`

---

### 测试集 4: NAPT 规则验证

#### 用例 4.1: 合法 NAPT 规则 ✅
**客户端配置** (假设客户端支持 NAPT-Rule AVP):
```properties
NAPT_RULE = tcp:8080->10.2.2.8:80
```

**预期结果**:
- ✅ 验证通过（目标 `10.2.2.8:80` 在白名单内）
- ✅ 日志显示 `✓ NAPT whitelist validation passed`

---

#### 用例 4.2: 非法 NAPT 规则 - 目标 IP 超出范围 ❌
**客户端配置**:
```properties
NAPT_RULE = tcp:8080->172.16.0.5:80
```

**预期结果**:
- ❌ 拒绝（目标 IP `172.16.0.5` 不在 `10.2.2.0/24` 内）
- ❌ MCCA 返回 `MAGIC-Status-Code=1015` (MAGIC_ERROR_ILLEGAL-PARAMETER)

---

### 测试集 5: 边界情况测试

#### 用例 5.1: 未配置白名单 - 默认行为 ⚠️
**白名单配置**（故意留空）:
```xml
<Traffic>
  <DestIPRange></DestIPRange>
  <DestPortRange></DestPortRange>
</Traffic>
```

**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 1.2.3.4:999
```

**预期结果**:
- ⚠️ 警告日志：`Warning: No dest_ip_range in whitelist, allowing all IPs`
- ✅ 验证通过（默认允许所有，安全降级模式）

---

#### 用例 5.2: TFT 语法错误 ❌
**客户端配置**:
```properties
TFT_GROUND.1 = invalid tft syntax
```

**预期结果**:
- ❌ MCCA 返回 `Result-Code=5003`
- ❌ MCCA 包含 `MAGIC-Status-Code=1036`
- ❌ 错误消息：`Failed to parse TFT rule: invalid syntax`

---

#### 用例 5.3: 同时验证 toGround 和 toAircraft ✅
**客户端配置**:
```properties
TFT_GROUND.1 = permit out ip from 192.168.126.5 to 10.2.2.8:80
TFT_AIR.1 = permit in ip from 10.2.2.8:80 to 192.168.126.5
```

**预期结果**:
- ✅ 两个 TFT 规则分别验证
- ✅ 日志显示：
  ```
  [app_magic] ✓ TFT whitelist validation passed (toGround)
  [app_magic] ✓ TFT whitelist validation passed (toAircraft)
  ```

---

## 自动化测试脚本

### 编译并启动服务器
```bash
#!/bin/bash
cd /home/zhuwuhui/freeDiameter/build
make -j4
sudo make install
sudo systemctl restart freediameter
sleep 2
```

### 测试脚本示例
```bash
#!/bin/bash
# test_tft_whitelist.sh

TEST_CASES=(
  "10.2.2.8:80:PASS:合法IP和端口"
  "10.3.3.8:80:FAIL:IP超出白名单"
  "10.2.2.8:8080:FAIL:端口超出白名单"
  "10.2.2.8:5500:PASS:端口在范围内"
)

for case in "${TEST_CASES[@]}"; do
  IFS=':' read -r ip port expected desc <<< "$case"
  
  echo "测试: $desc"
  echo "  TFT: permit out ip from 192.168.126.5 to $ip:$port"
  
  # 修改客户端配置
  sed -i "s|TFT_GROUND.1 = .*|TFT_GROUND.1 = permit out ip from 192.168.126.5 to $ip:$port|" \
    /path/to/ife-01_magic.conf
  
  # 重启客户端并检查日志
  # ... (根据实际客户端实现)
  
  # 检查日志
  if [[ "$expected" == "PASS" ]]; then
    grep "TFT whitelist validation passed" /var/log/freeDiameter.log && echo "  ✅ PASS" || echo "  ❌ FAIL"
  else
    grep "TFT whitelist validation FAILED" /var/log/freeDiameter.log && echo "  ✅ PASS (correctly rejected)" || echo "  ❌ FAIL"
  fi
  
  echo ""
done
```

---

## 验证检查清单

### 编译检查
- [ ] `make` 成功无错误
- [ ] 无链接错误（`magic_tft_validator.c` 正确编译）
- [ ] 服务器启动无崩溃

### 功能检查
- [ ] 合法 IP 通过验证
- [ ] 非法 IP 被拒绝
- [ ] 合法端口通过验证
- [ ] 非法端口被拒绝
- [ ] 协议验证正确执行
- [ ] NAPT 规则验证正确

### 日志检查
- [ ] 验证通过时有 `✓ TFT whitelist validation passed` 日志
- [ ] 验证失败时有详细错误消息：
  - TFT 字符串
  - 白名单 IP 范围
  - 白名单端口范围
  - 具体失败原因

### ARINC 839 合规性检查
- [ ] 实现了 §1.2.2.2 的内容验证（非字符串匹配）
- [ ] 超出范围时返回 `MAGIC_ERROR_TFT-INVALID (1036)`
- [ ] 返回的错误消息清晰描述失败原因

---

## 故障排查

### 问题: 编译错误 `undefined reference to 'tft_validate_against_whitelist'`
**解决方案**:
```bash
# 检查 CMakeLists.txt 是否包含 magic_tft_validator.c
grep magic_tft_validator.c extensions/app_magic/CMakeLists.txt

# 如果缺失，添加到 APP_MAGIC_SRC 列表
```

### 问题: 所有 TFT 都被拒绝
**检查**:
1. 确认 `Client_Profile.xml` 中的 `DestIPRange` 和 `DestPortRange` 已配置
2. 检查客户端 Profile 是否正确加载：
   ```bash
   grep "dest_ip_range" /var/log/freeDiameter.log
   ```
3. 验证客户端 ID 与 Profile 名称匹配

### 问题: 日志中无验证相关输出
**检查**:
1. 确认客户端发送了 TFT 参数（`TFTtoGround-Rule` AVP）
2. 检查 `ctx->profile` 是否为 NULL（未找到客户端配置）
3. 提高日志级别：
   ```bash
   fd_log_debug("[app_magic] Profile: %p, TFT: %s", 
                ctx->profile, ctx->comm_params.tft_to_ground);
   ```

---

## 预期日志示例

### 成功案例
```log
[2025-12-25 10:30:15] [app_magic] → Phase 2: Parameter & Security Check
[2025-12-25 10:30:15] [app_magic]   ✓ Profile defaults applied from: IFE-01
[2025-12-25 10:30:15] [app_magic]   ✓ Security check passed: Client IP verified
[2025-12-25 10:30:15] [tft_validator] Validating TFT: 'permit out ip from 192.168.126.5 to 10.2.2.8:80'
[2025-12-25 10:30:15] [tft_validator]   Parsed Direction: OUT
[2025-12-25 10:30:15] [tft_validator]   Parsed Source IP: 192.168.126.5
[2025-12-25 10:30:15] [tft_validator]   Parsed Dest IP: 10.2.2.8
[2025-12-25 10:30:15] [tft_validator]   Parsed Dest Port: 80-80
[2025-12-25 10:30:15] [tft_validator]   Whitelist IP: 10.2.2.0-10.2.2.255
[2025-12-25 10:30:15] [tft_validator]   Whitelist Ports: 80,443,5000-6000
[2025-12-25 10:30:15] [tft_validator] ✓ GRANTED: TFT validation passed
[2025-12-25 10:30:15] [app_magic]   ✓ TFT whitelist validation passed (toGround)
```

### 失败案例
```log
[2025-12-25 10:32:20] [app_magic] → Phase 2: Parameter & Security Check
[2025-12-25 10:32:20] [tft_validator] Validating TFT: 'permit out ip from 192.168.126.5 to 10.3.3.8:80'
[2025-12-25 10:32:20] [tft_validator]   Parsed Dest IP: 10.3.3.8
[2025-12-25 10:32:20] [tft_validator]   Whitelist IP: 10.2.2.0-10.2.2.255
[2025-12-25 10:32:20] [tft_validator] ✗ REJECTED: Destination IP 10.3.3.8 is outside whitelist range 10.2.2.0-10.2.2.255
[2025-12-25 10:32:20] [app_magic]   ✗ TFT whitelist validation FAILED: Destination IP 10.3.3.8 is outside whitelist range 10.2.2.0-10.2.2.255
[2025-12-25 10:32:20] [app_magic]     TFT: permit out ip from 192.168.126.5 to 10.3.3.8:80
[2025-12-25 10:32:20] [app_magic]     Whitelist IP: 10.2.2.0/24
[2025-12-25 10:32:20] [app_magic]     Whitelist Port: 80,443,5000-6000
[2025-12-25 10:32:20] [app_magic]   MCCA: Result-Code=5003, MAGIC-Status-Code=1036
```

---

## 结论

实施了 TFT/NAPT 白名单验证后，MAGIC 服务器现在完全符合 ARINC 839 §1.2.2.2 的要求，能够：

1. ✅ **内容验证**：解析 TFT 规则并提取 IP/端口范围进行验证（非简单字符串匹配）
2. ✅ **拒绝越界请求**：超出白名单范围时返回明确的错误码和消息
3. ✅ **安全增强**：防止客户端绕过白名单限制访问未授权网络
4. ✅ **详细日志**：提供完整的验证过程日志便于审计和调试

按照本测试指南执行测试后，应记录测试结果并形成合规性报告。
