# freeDiameter 跨平台分布式部署网络连通性测试指南

## 文档信息
- **版本**: 1.0
- **创建日期**: 2024年
- **适用场景**: Ubuntu服务端 + Windows客户端/链路模拟器
- **测试目标**: 验证跨平台网络连通性和系统功能

---

## 1. 网络环境准备

### 1.1 网络拓扑确认
```
Ubuntu 服务端: 192.168.1.10
├── freeDiameter 服务端 (端口: 3868, 5868)
└── 管理接口 (端口: 8080)

Windows 客户端/模拟器: 192.168.1.20
├── diameter_client (端口: 3868)
├── link_simulator (端口: 8001-8004)
│   ├── 以太网链路 (8001)
│   ├── WiFi链路 (8002)
│   ├── 蜂窝链路 (8003)
│   └── 卫星链路 (8004)
└── 管理接口 (端口: 9090)
```

### 1.2 防火墙配置验证

#### Ubuntu 服务端防火墙检查
```bash
# 检查防火墙状态
sudo ufw status

# 验证端口开放状态
sudo netstat -tlnp | grep -E "(3868|5868|8080)"

# 检查iptables规则
sudo iptables -L -n | grep -E "(3868|5868)"
```

#### Windows 防火墙检查
```powershell
# 检查Windows防火墙状态
Get-NetFirewallProfile

# 检查端口开放状态
netstat -an | findstr -E "(3868|8001|8002|8003|8004|9090)"

# 检查防火墙规则
Get-NetFirewallRule -DisplayName "*freeDiameter*"
```

---

## 2. 基础网络连通性测试

### 2.1 网络层连通性测试

#### 从 Ubuntu 服务端测试
```bash
#!/bin/bash
# test_network_from_ubuntu.sh

echo "=== Ubuntu 服务端网络连通性测试 ==="

# 测试到Windows客户端的基本连通性
echo "1. 测试基本网络连通性..."
ping -c 4 192.168.1.20
if [ $? -eq 0 ]; then
    echo "✓ 基本网络连通性正常"
else
    echo "✗ 基本网络连通性失败"
    exit 1
fi

# 测试Windows客户端端口连通性
echo "2. 测试Windows客户端端口连通性..."
for port in 3868 8001 8002 8003 8004 9090; do
    nc -z -v 192.168.1.20 $port
    if [ $? -eq 0 ]; then
        echo "✓ 端口 $port 连通正常"
    else
        echo "✗ 端口 $port 连通失败"
    fi
done

# 测试DNS解析
echo "3. 测试DNS解析..."
nslookup 192.168.1.20
if [ $? -eq 0 ]; then
    echo "✓ DNS解析正常"
else
    echo "✗ DNS解析失败"
fi

echo "=== Ubuntu 服务端网络测试完成 ==="
```

#### 从 Windows 客户端测试
```batch
@echo off
REM test_network_from_windows.bat

echo === Windows 客户端网络连通性测试 ===

REM 测试到Ubuntu服务端的基本连通性
echo 1. 测试基本网络连通性...
ping -n 4 192.168.1.10
if %errorlevel% equ 0 (
    echo ✓ 基本网络连通性正常
) else (
    echo ✗ 基本网络连通性失败
    exit /b 1
)

REM 测试Ubuntu服务端端口连通性
echo 2. 测试Ubuntu服务端端口连通性...
for %%p in (3868 5868 8080) do (
    telnet 192.168.1.10 %%p
    if %errorlevel% equ 0 (
        echo ✓ 端口 %%p 连通正常
    ) else (
        echo ✗ 端口 %%p 连通失败
    )
)

REM 测试本地端口监听状态
echo 3. 测试本地端口监听状态...
netstat -an | findstr ":3868"
netstat -an | findstr ":8001"
netstat -an | findstr ":8002"
netstat -an | findstr ":8003"
netstat -an | findstr ":8004"

echo === Windows 客户端网络测试完成 ===
```

### 2.2 应用层连通性测试

#### Diameter 协议连通性测试
```bash
#!/bin/bash
# test_diameter_connectivity.sh

echo "=== Diameter 协议连通性测试 ==="

# 测试freeDiameter服务端状态
echo "1. 检查freeDiameter服务端状态..."
ps aux | grep freeDiameterd
if [ $? -eq 0 ]; then
    echo "✓ freeDiameter服务端运行正常"
else
    echo "✗ freeDiameter服务端未运行"
fi

# 测试Diameter端口监听
echo "2. 检查Diameter端口监听..."
netstat -tlnp | grep 3868
if [ $? -eq 0 ]; then
    echo "✓ Diameter端口3868监听正常"
else
    echo "✗ Diameter端口3868监听异常"
fi

# 测试TLS端口监听
echo "3. 检查TLS端口监听..."
netstat -tlnp | grep 5868
if [ $? -eq 0 ]; then
    echo "✓ TLS端口5868监听正常"
else
    echo "✗ TLS端口5868监听异常"
fi

# 测试证书文件
echo "4. 检查证书文件..."
if [ -f "/home/zhuwuhui/freeDiameter/exe/certs/ca.crt" ]; then
    echo "✓ CA证书文件存在"
    openssl x509 -in /home/zhuwuhui/freeDiameter/exe/certs/ca.crt -text -noout | head -10
else
    echo "✗ CA证书文件不存在"
fi

echo "=== Diameter 协议连通性测试完成 ==="
```

---

## 3. 链路模拟器连通性测试

### 3.1 链路模拟器启动测试
```bash
#!/bin/bash
# test_link_simulator.sh

echo "=== 链路模拟器连通性测试 ==="

# 测试各链路端口连通性
declare -A link_ports=(
    ["以太网"]="8001"
    ["WiFi"]="8002"
    ["蜂窝"]="8003"
    ["卫星"]="8004"
)

echo "1. 测试链路端口连通性..."
for link_name in "${!link_ports[@]}"; do
    port=${link_ports[$link_name]}
    echo "测试 $link_name 链路 (端口 $port)..."
    
    # 使用telnet测试端口连通性
    timeout 5 bash -c "echo > /dev/tcp/192.168.1.20/$port" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✓ $link_name 链路端口 $port 连通正常"
    else
        echo "✗ $link_name 链路端口 $port 连通失败"
    fi
done

# 测试链路质量参数
echo "2. 测试链路质量参数..."
for link_name in "${!link_ports[@]}"; do
    port=${link_ports[$link_name]}
    echo "测试 $link_name 链路质量..."
    
    # 使用ping测试延迟
    ping_result=$(ping -c 3 192.168.1.20 | tail -1 | awk -F'/' '{print $5}')
    echo "  延迟: ${ping_result}ms"
    
    # 测试带宽 (使用iperf3如果可用)
    if command -v iperf3 &> /dev/null; then
        echo "  带宽测试: 使用iperf3测试..."
        # iperf3 -c 192.168.1.20 -p $port -t 5
    else
        echo "  带宽测试: iperf3未安装，跳过"
    fi
done

echo "=== 链路模拟器连通性测试完成 ==="
```

### 3.2 链路质量监控脚本
```bash
#!/bin/bash
# monitor_link_quality.sh

echo "=== 链路质量实时监控 ==="

# 监控配置
MONITOR_DURATION=60  # 监控时长(秒)
INTERVAL=5          # 监控间隔(秒)
TARGET_IP="192.168.1.20"

# 创建监控日志文件
LOG_FILE="/tmp/link_quality_$(date +%Y%m%d_%H%M%S).log"
echo "监控日志文件: $LOG_FILE"

# 监控函数
monitor_link() {
    local link_name=$1
    local port=$2
    local start_time=$(date +%s)
    
    echo "开始监控 $link_name 链路 (端口 $port)..."
    
    while [ $(($(date +%s) - start_time)) -lt $MONITOR_DURATION ]; do
        timestamp=$(date '+%Y-%m-%d %H:%M:%S')
        
        # 测试连通性
        if timeout 3 bash -c "echo > /dev/tcp/$TARGET_IP/$port" 2>/dev/null; then
            connectivity="UP"
        else
            connectivity="DOWN"
        fi
        
        # 测试延迟
        latency=$(ping -c 1 -W 1 $TARGET_IP 2>/dev/null | grep 'time=' | awk -F'time=' '{print $2}' | awk '{print $1}')
        if [ -z "$latency" ]; then
            latency="N/A"
        fi
        
        # 记录监控数据
        echo "$timestamp,$link_name,$port,$connectivity,$latency" >> $LOG_FILE
        
        sleep $INTERVAL
    done
}

# 启动各链路监控
declare -A links=(
    ["以太网"]="8001"
    ["WiFi"]="8002"
    ["蜂窝"]="8003"
    ["卫星"]="8004"
)

# 写入日志头
echo "时间戳,链路名称,端口,连通性,延迟(ms)" > $LOG_FILE

# 并行监控所有链路
for link_name in "${!links[@]}"; do
    monitor_link "$link_name" "${links[$link_name]}" &
done

# 等待所有监控进程完成
wait

echo "=== 链路质量监控完成 ==="
echo "监控结果已保存到: $LOG_FILE"

# 生成监控报告
echo "=== 监控报告摘要 ==="
for link_name in "${!links[@]}"; do
    up_count=$(grep "$link_name.*UP" $LOG_FILE | wc -l)
    down_count=$(grep "$link_name.*DOWN" $LOG_FILE | wc -l)
    total_count=$((up_count + down_count))
    
    if [ $total_count -gt 0 ]; then
        availability=$(echo "scale=2; $up_count * 100 / $total_count" | bc)
        echo "$link_name 链路可用性: ${availability}% (UP: $up_count, DOWN: $down_count)"
    fi
done
```

---

## 4. 端到端功能测试

### 4.1 客户端认证测试
```bash
#!/bin/bash
# test_client_authentication.sh

echo "=== 客户端认证功能测试 ==="

# 测试配置
CLIENT_ID="aircraft_001"
SERVER_IP="192.168.1.10"
SERVER_PORT="3868"

echo "1. 测试客户端配置文件..."
if [ -f "/home/zhuwuhui/freeDiameter/windows_client.conf" ]; then
    echo "✓ 客户端配置文件存在"
    grep -E "(Identity|ConnectPeer)" /home/zhuwuhui/freeDiameter/windows_client.conf
else
    echo "✗ 客户端配置文件不存在"
fi

echo "2. 测试服务端客户端配置..."
if [ -f "/home/zhuwuhui/freeDiameter/exe/client_conf/client_profiles.xml" ]; then
    echo "✓ 服务端客户端配置文件存在"
    grep -A 5 -B 5 "$CLIENT_ID" /home/zhuwuhui/freeDiameter/exe/client_conf/client_profiles.xml
else
    echo "✗ 服务端客户端配置文件不存在"
fi

echo "3. 模拟客户端认证请求..."
# 这里可以使用diameter_client工具进行实际测试
# ./diameter_client --config windows_client.conf --test-auth

echo "=== 客户端认证功能测试完成 ==="
```

### 4.2 链路选择功能测试
```bash
#!/bin/bash
# test_link_selection.sh

echo "=== 链路选择功能测试 ==="

# 测试各链路的选择逻辑
declare -A link_scenarios=(
    ["高带宽场景"]="ethernet"
    ["移动场景"]="wifi"
    ["远程场景"]="cellular"
    ["极端环境"]="satellite"
)

echo "1. 测试链路选择算法..."
for scenario in "${!link_scenarios[@]}"; do
    expected_link=${link_scenarios[$scenario]}
    echo "测试场景: $scenario (期望链路: $expected_link)"
    
    # 这里可以调用实际的链路选择测试
    # result=$(./test_link_selection --scenario "$scenario")
    # echo "  实际选择: $result"
done

echo "2. 测试链路故障切换..."
# 模拟链路故障场景
echo "模拟以太网链路故障..."
# 可以通过iptables阻断特定端口来模拟故障
# sudo iptables -A OUTPUT -p tcp --dport 8001 -j DROP

echo "验证自动切换到备用链路..."
# 检查是否自动切换到WiFi链路
# check_active_link

echo "恢复以太网链路..."
# sudo iptables -D OUTPUT -p tcp --dport 8001 -j DROP

echo "=== 链路选择功能测试完成 ==="
```

### 4.3 带宽管理测试
```bash
#!/bin/bash
# test_bandwidth_management.sh

echo "=== 带宽管理功能测试 ==="

# 测试不同优先级的带宽分配
declare -A priority_tests=(
    ["高优先级客户端"]="high"
    ["中优先级客户端"]="medium"
    ["低优先级客户端"]="low"
)

echo "1. 测试带宽分配策略..."
for client_type in "${!priority_tests[@]}"; do
    priority=${priority_tests[$client_type]}
    echo "测试 $client_type (优先级: $priority)"
    
    # 模拟带宽请求
    requested_bandwidth=5000  # 5Mbps
    echo "  请求带宽: ${requested_bandwidth}Kbps"
    
    # 这里可以调用实际的带宽分配测试
    # allocated=$(./test_bandwidth_allocation --priority $priority --request $requested_bandwidth)
    # echo "  分配带宽: ${allocated}Kbps"
done

echo "2. 测试带宽超限处理..."
# 模拟带宽超限场景
echo "模拟总带宽超限..."
# 可以通过多个客户端同时请求大量带宽来测试

echo "3. 测试带宽释放..."
# 测试客户端断开连接后带宽是否正确释放

echo "=== 带宽管理功能测试完成 ==="
```

---

## 5. 故障排除指南

### 5.1 常见网络连通性问题

#### 问题1: 基本网络不通
**症状**: ping 失败，无法建立TCP连接
**排查步骤**:
```bash
# 1. 检查网络接口配置
ip addr show
ip route show

# 2. 检查防火墙设置
sudo ufw status
sudo iptables -L -n

# 3. 检查网络服务
sudo systemctl status networking
sudo systemctl status NetworkManager

# 4. 测试网络路径
traceroute 192.168.1.20
mtr 192.168.1.20
```

**解决方案**:
- 确认IP地址配置正确
- 检查子网掩码和网关设置
- 临时关闭防火墙测试
- 检查网络设备状态

#### 问题2: 端口连接被拒绝
**症状**: "Connection refused" 错误
**排查步骤**:
```bash
# 1. 检查端口监听状态
sudo netstat -tlnp | grep 3868
sudo ss -tlnp | grep 3868

# 2. 检查进程状态
ps aux | grep freeDiameterd
sudo systemctl status freediameter

# 3. 检查端口占用
sudo lsof -i :3868
```

**解决方案**:
- 确认服务正在运行
- 检查服务绑定的IP地址
- 确认端口未被其他进程占用
- 重启相关服务

#### 问题3: TLS/SSL连接失败
**症状**: TLS握手失败，证书验证错误
**排查步骤**:
```bash
# 1. 检查证书文件
ls -la /home/zhuwuhui/freeDiameter/exe/certs/
openssl x509 -in ca.crt -text -noout

# 2. 验证证书有效性
openssl verify -CAfile ca.crt client.crt

# 3. 测试TLS连接
openssl s_client -connect 192.168.1.10:5868 -CAfile ca.crt
```

**解决方案**:
- 检查证书文件权限和路径
- 验证证书有效期
- 确认CA证书链完整
- 检查证书主题名称匹配

### 5.2 链路模拟器问题

#### 问题1: 链路模拟器无法启动
**症状**: link_simulator 进程启动失败
**排查步骤**:
```bash
# 1. 检查配置文件
cat /home/zhuwuhui/freeDiameter/windows_simulator.conf

# 2. 检查端口占用
netstat -an | grep -E "(8001|8002|8003|8004)"

# 3. 检查日志文件
tail -f /var/log/link_simulator.log

# 4. 手动启动测试
./link_simulator --config simulator.conf --debug
```

**解决方案**:
- 检查配置文件语法
- 释放被占用的端口
- 检查文件权限
- 查看详细错误日志

#### 问题2: 链路质量参数异常
**症状**: 链路延迟、丢包率等参数不符合预期
**排查步骤**:
```bash
# 1. 检查链路配置
grep -A 10 "ethernet\|wifi\|cellular\|satellite" simulator.conf

# 2. 实时监控链路状态
./monitor_link_quality.sh

# 3. 对比配置与实际参数
ping -c 10 192.168.1.20
iperf3 -c 192.168.1.20 -p 8001
```

**解决方案**:
- 调整链路参数配置
- 重启链路模拟器
- 检查网络环境干扰
- 验证测试方法正确性

### 5.3 客户端连接问题

#### 问题1: 客户端认证失败
**症状**: MCAR 请求返回认证失败
**排查步骤**:
```bash
# 1. 检查客户端配置
grep -E "(Identity|ConnectPeer)" client.conf

# 2. 检查服务端客户端配置
grep -A 5 "aircraft_001" client_profiles.xml

# 3. 查看认证日志
tail -f /var/log/freediameter-server.log | grep -i auth

# 4. 测试认证凭据
./diameter_client --test-auth --debug
```

**解决方案**:
- 确认客户端ID配置正确
- 检查认证凭据有效性
- 验证服务端客户端配置
- 检查认证方法匹配

#### 问题2: 链路选择异常
**症状**: 客户端选择了非最优链路
**排查步骤**:
```bash
# 1. 检查链路选择配置
grep -A 10 "link_selection" client.conf

# 2. 查看链路状态
./check_link_status.sh

# 3. 分析选择日志
grep -i "link.*select" /var/log/diameter_client.log

# 4. 手动测试链路质量
./test_all_links.sh
```

**解决方案**:
- 调整链路选择策略
- 更新链路质量阈值
- 检查环境检测模块
- 验证链路优先级配置

### 5.4 性能问题排查

#### 问题1: 连接延迟过高
**症状**: 消息响应时间超过预期
**排查步骤**:
```bash
# 1. 网络延迟测试
ping -c 100 192.168.1.20 | tail -1

# 2. 应用层延迟测试
time ./diameter_client --send-test-message

# 3. 系统资源检查
top -p $(pgrep freeDiameterd)
iostat -x 1 10

# 4. 网络带宽测试
iperf3 -c 192.168.1.20 -t 30
```

**解决方案**:
- 优化网络配置
- 调整系统参数
- 增加系统资源
- 优化应用配置

#### 问题2: 吞吐量不足
**症状**: 并发连接数或消息处理量低于预期
**排查步骤**:
```bash
# 1. 检查连接数限制
ulimit -n
cat /proc/sys/net/core/somaxconn

# 2. 监控系统负载
vmstat 1 10
sar -u 1 10

# 3. 分析瓶颈
perf top -p $(pgrep freeDiameterd)
strace -p $(pgrep freeDiameterd) -c
```

**解决方案**:
- 调整系统限制参数
- 优化应用线程配置
- 增加硬件资源
- 优化数据库查询

---

## 6. 自动化测试脚本

### 6.1 完整连通性测试脚本
```bash
#!/bin/bash
# comprehensive_connectivity_test.sh

echo "=========================================="
echo "freeDiameter 跨平台分布式部署连通性测试"
echo "=========================================="

# 测试配置
UBUNTU_SERVER="192.168.1.10"
WINDOWS_CLIENT="192.168.1.20"
TEST_RESULTS_DIR="/tmp/connectivity_test_$(date +%Y%m%d_%H%M%S)"

# 创建测试结果目录
mkdir -p $TEST_RESULTS_DIR
cd $TEST_RESULTS_DIR

# 测试函数
run_test() {
    local test_name="$1"
    local test_command="$2"
    local log_file="$3"
    
    echo "运行测试: $test_name"
    echo "命令: $test_command" > $log_file
    echo "开始时间: $(date)" >> $log_file
    
    if eval $test_command >> $log_file 2>&1; then
        echo "✓ $test_name - 通过"
        echo "结果: 通过" >> $log_file
    else
        echo "✗ $test_name - 失败"
        echo "结果: 失败" >> $log_file
    fi
    
    echo "结束时间: $(date)" >> $log_file
    echo "----------------------------------------" >> $log_file
}

# 1. 基础网络连通性测试
echo "1. 基础网络连通性测试"
run_test "Ubuntu到Windows ping测试" \
         "ping -c 4 $WINDOWS_CLIENT" \
         "01_ping_test.log"

run_test "Windows到Ubuntu ping测试" \
         "ping -c 4 $UBUNTU_SERVER" \
         "02_reverse_ping_test.log"

# 2. 端口连通性测试
echo "2. 端口连通性测试"
for port in 3868 5868 8080; do
    run_test "Ubuntu端口${port}连通性测试" \
             "nc -z -v $UBUNTU_SERVER $port" \
             "03_ubuntu_port_${port}_test.log"
done

for port in 3868 8001 8002 8003 8004 9090; do
    run_test "Windows端口${port}连通性测试" \
             "nc -z -v $WINDOWS_CLIENT $port" \
             "04_windows_port_${port}_test.log"
done

# 3. 服务状态测试
echo "3. 服务状态测试"
run_test "freeDiameter服务端状态检查" \
         "ps aux | grep freeDiameterd | grep -v grep" \
         "05_server_status_test.log"

run_test "链路模拟器状态检查" \
         "nc -z $WINDOWS_CLIENT 8001 && nc -z $WINDOWS_CLIENT 8002 && nc -z $WINDOWS_CLIENT 8003 && nc -z $WINDOWS_CLIENT 8004" \
         "06_simulator_status_test.log"

# 4. 证书和配置文件测试
echo "4. 证书和配置文件测试"
run_test "服务端证书文件检查" \
         "test -f /home/zhuwuhui/freeDiameter/exe/certs/ca.crt && test -f /home/zhuwuhui/freeDiameter/exe/certs/client.crt" \
         "07_cert_files_test.log"

run_test "配置文件语法检查" \
         "xmllint --noout /home/zhuwuhui/freeDiameter/ubuntu_server.conf" \
         "08_config_syntax_test.log"

# 5. 功能性测试
echo "5. 功能性测试"
if [ -x "/home/zhuwuhui/freeDiameter/exe/diameter_client" ]; then
    run_test "客户端连接测试" \
             "timeout 30 /home/zhuwuhui/freeDiameter/exe/diameter_client --config /home/zhuwuhui/freeDiameter/windows_client.conf --test-connect" \
             "09_client_connect_test.log"
fi

# 6. 性能基准测试
echo "6. 性能基准测试"
run_test "网络延迟基准测试" \
         "ping -c 100 $WINDOWS_CLIENT | tail -1" \
         "10_latency_benchmark.log"

if command -v iperf3 &> /dev/null; then
    run_test "网络带宽基准测试" \
             "timeout 30 iperf3 -c $WINDOWS_CLIENT -t 10" \
             "11_bandwidth_benchmark.log"
fi

# 生成测试报告
echo "7. 生成测试报告"
cat > test_report.html << EOF
<!DOCTYPE html>
<html>
<head>
    <title>freeDiameter 连通性测试报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .pass { color: green; }
        .fail { color: red; }
        .test-section { margin: 20px 0; }
        .log-content { background: #f5f5f5; padding: 10px; margin: 10px 0; }
    </style>
</head>
<body>
    <h1>freeDiameter 跨平台分布式部署连通性测试报告</h1>
    <p>测试时间: $(date)</p>
    <p>测试环境: Ubuntu 服务端 ($UBUNTU_SERVER) + Windows 客户端 ($WINDOWS_CLIENT)</p>
    
    <h2>测试结果摘要</h2>
    <ul>
EOF

# 统计测试结果
total_tests=0
passed_tests=0

for log_file in *.log; do
    if [ -f "$log_file" ]; then
        total_tests=$((total_tests + 1))
        if grep -q "结果: 通过" "$log_file"; then
            passed_tests=$((passed_tests + 1))
            echo "        <li class=\"pass\">✓ $(basename $log_file .log)</li>" >> test_report.html
        else
            echo "        <li class=\"fail\">✗ $(basename $log_file .log)</li>" >> test_report.html
        fi
    fi
done

cat >> test_report.html << EOF
    </ul>
    
    <h2>测试统计</h2>
    <p>总测试数: $total_tests</p>
    <p>通过测试: $passed_tests</p>
    <p>失败测试: $((total_tests - passed_tests))</p>
    <p>成功率: $(echo "scale=2; $passed_tests * 100 / $total_tests" | bc)%</p>
    
    <h2>详细测试日志</h2>
EOF

# 添加详细日志到报告
for log_file in *.log; do
    if [ -f "$log_file" ]; then
        echo "    <div class=\"test-section\">" >> test_report.html
        echo "        <h3>$(basename $log_file .log)</h3>" >> test_report.html
        echo "        <div class=\"log-content\"><pre>" >> test_report.html
        cat "$log_file" >> test_report.html
        echo "        </pre></div>" >> test_report.html
        echo "    </div>" >> test_report.html
    fi
done

cat >> test_report.html << EOF
</body>
</html>
EOF

echo "=========================================="
echo "测试完成！"
echo "测试结果目录: $TEST_RESULTS_DIR"
echo "测试报告: $TEST_RESULTS_DIR/test_report.html"
echo "总测试数: $total_tests"
echo "通过测试: $passed_tests"
echo "失败测试: $((total_tests - passed_tests))"
echo "成功率: $(echo "scale=2; $passed_tests * 100 / $total_tests" | bc)%"
echo "=========================================="
```

### 6.2 持续监控脚本
```bash
#!/bin/bash
# continuous_monitoring.sh

echo "启动 freeDiameter 系统持续监控..."

MONITOR_INTERVAL=60  # 监控间隔(秒)
LOG_DIR="/var/log/freediameter_monitoring"
ALERT_EMAIL="admin@example.com"

# 创建监控日志目录
mkdir -p $LOG_DIR

# 监控函数
monitor_system() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local status_file="$LOG_DIR/system_status_$(date +%Y%m%d).log"
    
    echo "[$timestamp] 系统状态检查" >> $status_file
    
    # 检查服务状态
    if pgrep freeDiameterd > /dev/null; then
        echo "[$timestamp] freeDiameter服务端: 运行正常" >> $status_file
    else
        echo "[$timestamp] freeDiameter服务端: 服务停止" >> $status_file
        # 发送告警
        echo "freeDiameter服务端停止运行" | mail -s "系统告警" $ALERT_EMAIL
    fi
    
    # 检查网络连通性
    if ping -c 1 192.168.1.20 > /dev/null 2>&1; then
        echo "[$timestamp] 网络连通性: 正常" >> $status_file
    else
        echo "[$timestamp] 网络连通性: 异常" >> $status_file
        # 发送告警
        echo "网络连通性异常" | mail -s "网络告警" $ALERT_EMAIL
    fi
    
    # 检查系统资源
    cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | awk -F'%' '{print $1}')
    mem_usage=$(free | grep Mem | awk '{printf "%.1f", $3/$2 * 100.0}')
    
    echo "[$timestamp] CPU使用率: ${cpu_usage}%" >> $status_file
    echo "[$timestamp] 内存使用率: ${mem_usage}%" >> $status_file
    
    # 检查磁盘空间
    disk_usage=$(df -h / | awk 'NR==2 {print $5}' | sed 's/%//')
    echo "[$timestamp] 磁盘使用率: ${disk_usage}%" >> $status_file
    
    if [ $disk_usage -gt 90 ]; then
        echo "磁盘空间不足: ${disk_usage}%" | mail -s "磁盘告警" $ALERT_EMAIL
    fi
}

# 主监控循环
while true; do
    monitor_system
    sleep $MONITOR_INTERVAL
done
```

---

## 7. 总结

本指南提供了完整的 freeDiameter 跨平台分布式部署网络连通性测试方案，包括：

1. **网络环境准备**: 网络拓扑确认和防火墙配置
2. **基础连通性测试**: 网络层和应用层连通性验证
3. **链路模拟器测试**: 多链路连通性和质量监控
4. **端到端功能测试**: 认证、链路选择、带宽管理测试
5. **故障排除指南**: 常见问题的诊断和解决方案
6. **自动化测试**: 完整的测试脚本和持续监控

通过这些测试和监控工具，可以确保 Ubuntu 服务端和 Windows 客户端/链路模拟器之间的网络连通性和系统功能正常运行。