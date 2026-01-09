# freeDiameter 跨平台分布式部署指南

## 文档信息
- **版本**: 2.0
- **创建日期**: 2024年
- **部署架构**: 三台计算机分布式部署
- **适用场景**: Ubuntu服务端 + Windows客户端 + Windows链路模拟器

---

## 1. 部署架构概述

### 1.1 三台计算机部署拓扑
```
┌─────────────────────────────────────────────────────────────────┐
│                    freeDiameter 分布式部署架构                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐ │
│  │   Ubuntu 服务端   │    │  Windows 客户端  │    │ Windows 模拟器  │ │
│  │  192.168.1.10   │    │  192.168.1.20   │    │  192.168.1.30   │ │
│  │                 │    │                 │    │                 │ │
│  │ freeDiameterd   │◄──►│ diameter_client │◄──►│ link_simulator  │ │
│  │ 端口: 3868,5868 │    │ 端口: 3868      │    │ 端口: 8001-8004 │ │
│  │ 管理: 8080      │    │ 管理: 9090      │    │ 管理: 9091      │ │
│  └─────────────────┘    └─────────────────┘    └─────────────────┘ │
│           │                       │                       │        │
│           └───────────────────────┼───────────────────────┘        │
│                                   │                                │
│                            网络交换机/路由器                         │
│                              192.168.1.1                          │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 系统组件分配

#### 计算机A - Ubuntu 服务端 (192.168.1.10)
- **操作系统**: Ubuntu 20.04 LTS 或更高版本
- **主要组件**: freeDiameter 服务端
- **核心功能**: 
  - 客户端认证和授权
  - 智能链路选择算法
  - 带宽管理和QoS控制
  - 系统监控和管理
- **网络端口**:
  - 3868: Diameter 协议端口
  - 5868: Diameter TLS 端口
  - 8080: Web 管理接口

#### 计算机B - Windows 客户端 (192.168.1.20)
- **操作系统**: Windows 10/11 或 Windows Server
- **主要组件**: Diameter 客户端应用
- **核心功能**:
  - 航空器通信终端模拟
  - 多链路自动选择
  - 消息生成和处理
  - 会话管理
- **网络端口**:
  - 3868: Diameter 客户端端口
  - 9090: 客户端管理接口

#### 计算机C - Windows 链路模拟器 (192.168.1.30)
- **操作系统**: Windows 10/11 或 Windows Server
- **主要组件**: 多链路模拟器
- **核心功能**:
  - 以太网链路模拟 (8001)
  - WiFi链路模拟 (8002)
  - 蜂窝链路模拟 (8003)
  - 卫星链路模拟 (8004)
- **网络端口**:
  - 8001-8004: 各链路模拟端口
  - 9091: 模拟器管理接口

---

## 2. 网络环境配置

### 2.1 网络规划

#### IP地址分配表
| 设备类型 | IP地址 | 子网掩码 | 网关 | DNS |
|---------|--------|----------|------|-----|
| Ubuntu 服务端 | 192.168.1.10 | 255.255.255.0 | 192.168.1.1 | 8.8.8.8 |
| Windows 客户端 | 192.168.1.20 | 255.255.255.0 | 192.168.1.1 | 8.8.8.8 |
| Windows 模拟器 | 192.168.1.30 | 255.255.255.0 | 192.168.1.1 | 8.8.8.8 |
| 网络设备 | 192.168.1.1 | 255.255.255.0 | - | - |

#### 端口映射表
| 服务 | 计算机 | 端口 | 协议 | 用途 |
|------|--------|------|------|------|
| freeDiameter | Ubuntu (10) | 3868 | TCP | Diameter 协议 |
| freeDiameter TLS | Ubuntu (10) | 5868 | TCP | Diameter TLS |
| 服务端管理 | Ubuntu (10) | 8080 | HTTP | Web 管理界面 |
| 客户端连接 | Windows (20) | 3868 | TCP | 客户端通信 |
| 客户端管理 | Windows (20) | 9090 | HTTP | 客户端管理 |
| 以太网链路 | Windows (30) | 8001 | TCP | 以太网模拟 |
| WiFi链路 | Windows (30) | 8002 | TCP | WiFi模拟 |
| 蜂窝链路 | Windows (30) | 8003 | TCP | 蜂窝模拟 |
| 卫星链路 | Windows (30) | 8004 | TCP | 卫星模拟 |
| 模拟器管理 | Windows (30) | 9091 | HTTP | 模拟器管理 |

### 2.2 防火墙配置

#### Ubuntu 服务端防火墙配置
```bash
#!/bin/bash
# ubuntu_firewall_setup.sh

echo "配置 Ubuntu 服务端防火墙..."

# 重置防火墙规则
sudo ufw --force reset

# 设置默认策略
sudo ufw default deny incoming
sudo ufw default allow outgoing

# 允许SSH连接
sudo ufw allow ssh

# 允许freeDiameter端口
sudo ufw allow 3868/tcp comment "Diameter protocol"
sudo ufw allow 5868/tcp comment "Diameter TLS"

# 允许管理接口
sudo ufw allow 8080/tcp comment "Web management"

# 允许来自客户端的连接
sudo ufw allow from 192.168.1.20 to any port 3868
sudo ufw allow from 192.168.1.20 to any port 5868

# 允许来自模拟器的连接
sudo ufw allow from 192.168.1.30 to any port 3868
sudo ufw allow from 192.168.1.30 to any port 5868

# 启用防火墙
sudo ufw enable

# 显示防火墙状态
sudo ufw status verbose

echo "Ubuntu 服务端防火墙配置完成"
```

#### Windows 客户端防火墙配置
```powershell
# windows_client_firewall_setup.ps1

Write-Host "配置 Windows 客户端防火墙..."

# 允许 Diameter 客户端程序
New-NetFirewallRule -DisplayName "freeDiameter Client" -Direction Inbound -Program "C:\freeDiameter\diameter_client.exe" -Action Allow
New-NetFirewallRule -DisplayName "freeDiameter Client" -Direction Outbound -Program "C:\freeDiameter\diameter_client.exe" -Action Allow

# 允许客户端端口
New-NetFirewallRule -DisplayName "Diameter Client Port" -Direction Inbound -Protocol TCP -LocalPort 3868 -Action Allow
New-NetFirewallRule -DisplayName "Client Management Port" -Direction Inbound -Protocol TCP -LocalPort 9090 -Action Allow

# 允许连接到服务端
New-NetFirewallRule -DisplayName "Connect to Server" -Direction Outbound -Protocol TCP -RemoteAddress 192.168.1.10 -RemotePort 3868,5868 -Action Allow

# 允许连接到模拟器
New-NetFirewallRule -DisplayName "Connect to Simulator" -Direction Outbound -Protocol TCP -RemoteAddress 192.168.1.30 -RemotePort 8001,8002,8003,8004 -Action Allow

Write-Host "Windows 客户端防火墙配置完成"
```

#### Windows 模拟器防火墙配置
```powershell
# windows_simulator_firewall_setup.ps1

Write-Host "配置 Windows 模拟器防火墙..."

# 允许链路模拟器程序
New-NetFirewallRule -DisplayName "Link Simulator" -Direction Inbound -Program "C:\freeDiameter\link_simulator.exe" -Action Allow
New-NetFirewallRule -DisplayName "Link Simulator" -Direction Outbound -Program "C:\freeDiameter\link_simulator.exe" -Action Allow

# 允许链路端口
New-NetFirewallRule -DisplayName "Ethernet Link" -Direction Inbound -Protocol TCP -LocalPort 8001 -Action Allow
New-NetFirewallRule -DisplayName "WiFi Link" -Direction Inbound -Protocol TCP -LocalPort 8002 -Action Allow
New-NetFirewallRule -DisplayName "Cellular Link" -Direction Inbound -Protocol TCP -LocalPort 8003 -Action Allow
New-NetFirewallRule -DisplayName "Satellite Link" -Direction Inbound -Protocol TCP -LocalPort 8004 -Action Allow

# 允许管理端口
New-NetFirewallRule -DisplayName "Simulator Management" -Direction Inbound -Protocol TCP -LocalPort 9091 -Action Allow

# 允许连接到服务端
New-NetFirewallRule -DisplayName "Connect to Server" -Direction Outbound -Protocol TCP -RemoteAddress 192.168.1.10 -RemotePort 3868,5868 -Action Allow

Write-Host "Windows 模拟器防火墙配置完成"
```

---

## 3. Ubuntu 服务端部署

### 3.1 系统环境准备

#### 系统要求检查
```bash
#!/bin/bash
# ubuntu_system_check.sh

echo "=== Ubuntu 服务端系统环境检查 ==="

# 检查操作系统版本
echo "1. 操作系统版本:"
lsb_release -a

# 检查系统资源
echo "2. 系统资源:"
echo "CPU: $(nproc) 核心"
echo "内存: $(free -h | grep Mem | awk '{print $2}')"
echo "磁盘: $(df -h / | tail -1 | awk '{print $4}') 可用"

# 检查网络配置
echo "3. 网络配置:"
ip addr show | grep -E "inet.*192.168.1"
ping -c 2 192.168.1.20 > /dev/null && echo "✓ 客户端网络连通" || echo "✗ 客户端网络不通"
ping -c 2 192.168.1.30 > /dev/null && echo "✓ 模拟器网络连通" || echo "✗ 模拟器网络不通"

# 检查必要的软件包
echo "4. 软件包检查:"
packages=("build-essential" "cmake" "libsctp-dev" "libgnutls28-dev" "libgcrypt20-dev" "libidn11-dev")
for pkg in "${packages[@]}"; do
    if dpkg -l | grep -q "^ii.*$pkg"; then
        echo "✓ $pkg 已安装"
    else
        echo "✗ $pkg 未安装"
    fi
done

echo "=== 系统环境检查完成 ==="
```

#### 依赖包安装
```bash
#!/bin/bash
# ubuntu_install_dependencies.sh

echo "安装 Ubuntu 服务端依赖包..."

# 更新包列表
sudo apt update

# 安装编译工具
sudo apt install -y build-essential cmake pkg-config

# 安装网络库
sudo apt install -y libsctp-dev libsctp1

# 安装加密库
sudo apt install -y libgnutls28-dev libgcrypt20-dev

# 安装其他依赖
sudo apt install -y libidn11-dev libxml2-dev

# 安装调试工具
sudo apt install -y gdb valgrind strace

# 安装网络工具
sudo apt install -y netcat-openbsd telnet tcpdump wireshark-common

# 验证安装
echo "验证关键库安装:"
pkg-config --exists gnutls && echo "✓ GnuTLS" || echo "✗ GnuTLS"
pkg-config --exists libxml-2.0 && echo "✓ libxml2" || echo "✗ libxml2"
ldconfig -p | grep -q libsctp && echo "✓ SCTP" || echo "✗ SCTP"

echo "依赖包安装完成"
```

### 3.2 freeDiameter 编译和安装

#### 编译脚本
```bash
#!/bin/bash
# ubuntu_build_freediameter.sh

echo "编译 freeDiameter 服务端..."

# 设置编译目录
BUILD_DIR="/home/zhuwuhui/freeDiameter"
cd $BUILD_DIR

# 清理之前的编译
make clean 2>/dev/null || true
rm -rf CMakeCache.txt CMakeFiles/

# 配置编译选项
cmake . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_TESTING=ON \
    -DDEFAULT_CONF_PATH=/etc/freeDiameter \
    -DDIAMID_IDNA_REJECT=ON

# 编译
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✓ freeDiameter 编译成功"
else
    echo "✗ freeDiameter 编译失败"
    exit 1
fi

# 创建安装目录
sudo mkdir -p /etc/freeDiameter
sudo mkdir -p /var/log/freeDiameter
sudo mkdir -p /var/run/freeDiameter

# 复制可执行文件
sudo cp freeDiameterd/freeDiameterd /usr/local/bin/
sudo cp exe/freeDiameterd /usr/local/bin/freeDiameterd-exe

# 复制扩展库
sudo cp extensions/server/server.fdx /usr/local/lib/
sudo cp extensions/dict_*/dict_*.fdx /usr/local/lib/

# 设置权限
sudo chmod +x /usr/local/bin/freeDiameterd*
sudo chown -R freediameter:freediameter /var/log/freeDiameter /var/run/freeDiameter 2>/dev/null || true

echo "freeDiameter 安装完成"
```

### 3.3 服务端配置

#### 主配置文件
```bash
# 创建服务端配置文件
cat > /home/zhuwuhui/freeDiameter/ubuntu_server.conf << 'EOF'
# freeDiameter 服务端配置文件 - 分布式部署版本
# 适用于三台计算机部署架构

# 基本身份配置
Identity = "server.freediameter.local";
Realm = "freediameter.local";

# 网络监听配置
ListenOn = "192.168.1.10";  # Ubuntu 服务端IP
Port = 3868;                # Diameter 标准端口
SecPort = 5868;             # TLS 端口

# TLS 配置
TLS_Cred = "/home/zhuwuhui/freeDiameter/exe/certs/server.crt", 
           "/home/zhuwuhui/freeDiameter/exe/certs/server.key";
TLS_CA = "/home/zhuwuhui/freeDiameter/exe/certs/ca.crt";
TLS_DH_File = "/home/zhuwuhui/freeDiameter/exe/certs/dh.pem";

# 客户端连接配置
ConnectPeer = "client.freediameter.local" {
    ConnectTo = "192.168.1.20";  # Windows 客户端IP
    Port = 3868;
    No_TLS;  # 初始测试不使用TLS
    Prefer_TCP;
};

# 链路模拟器连接配置
ConnectPeer = "simulator.freediameter.local" {
    ConnectTo = "192.168.1.30";  # Windows 模拟器IP
    Port = 8001;  # 以太网链路端口
    No_TLS;
    Prefer_TCP;
};

# 应用程序配置
AppServThreads = 4;
NoRelay;

# 扩展加载
LoadExtension = "/home/zhuwuhui/freeDiameter/extensions/server/server.fdx" {
    # 服务端扩展配置
    ServerMode = 1;
    
    # 客户端管理配置
    ClientConfigFile = "/home/zhuwuhui/freeDiameter/exe/client_conf/client_profiles.xml";
    
    # 链路管理配置
    LinkSimulatorEnabled = 1;
    LinkSimulatorHost = "192.168.1.30";
    LinkSimulatorPorts = "8001,8002,8003,8004";
    
    # 带宽管理配置
    BandwidthManagement = 1;
    TotalBandwidth = 100000;  # 100Mbps
    
    # 链路选择策略
    LinkSelectionStrategy = "adaptive";  # adaptive, priority, load_balance
    
    # QoS 配置
    QoSEnabled = 1;
    HighPriorityBandwidth = 40000;   # 40Mbps for high priority
    MediumPriorityBandwidth = 35000; # 35Mbps for medium priority
    LowPriorityBandwidth = 25000;    # 25Mbps for low priority
};

# 字典加载
LoadExtension = "/home/zhuwuhui/freeDiameter/extensions/dict_arinc839/dict_arinc839.fdx";

# 路由配置
LoadExtension = "/home/zhuwuhui/freeDiameter/exe/rt_default.fdx";

# 调试配置
LoadExtension = "/home/zhuwuhui/freeDiameter/exe/dbg_loglevel.fdx" {
    LogLevel = "INFO";
    LogFile = "/var/log/freeDiameter/server.log";
};

# 监控配置
LoadExtension = "/home/zhuwuhui/freeDiameter/exe/dbg_monitor.fdx" {
    MonitorPort = 8080;
    MonitorInterface = "192.168.1.10";
};
EOF
```

#### 客户端配置文件
```xml
<!-- 创建客户端配置文件 -->
cat > /home/zhuwuhui/freeDiameter/exe/client_conf/client_profiles.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<client_profiles>
    <!-- 分布式部署客户端配置 -->
    
    <!-- Windows 客户端配置 -->
    <client id="aircraft_001">
        <identity>aircraft_001@freediameter.local</identity>
        <ip_address>192.168.1.20</ip_address>
        <authentication>
            <method>psk</method>
            <psk_key>aircraft001_secret_key_2024</psk_key>
        </authentication>
        <bandwidth>
            <max_allocation>10000</max_allocation>  <!-- 10Mbps -->
            <priority>high</priority>
        </bandwidth>
        <service_types>
            <service>voice</service>
            <service>data</service>
            <service>video</service>
        </service_types>
        <qos_parameters>
            <latency_requirement>100</latency_requirement>  <!-- ms -->
            <jitter_tolerance>20</jitter_tolerance>         <!-- ms -->
            <packet_loss_tolerance>0.1</packet_loss_tolerance> <!-- % -->
        </qos_parameters>
        <link_preferences>
            <preferred_links>
                <link type="ethernet" priority="1"/>
                <link type="wifi" priority="2"/>
                <link type="cellular" priority="3"/>
                <link type="satellite" priority="4"/>
            </preferred_links>
        </link_preferences>
    </client>
    
    <!-- 测试客户端配置 -->
    <client id="test_client_001">
        <identity>test_client_001@freediameter.local</identity>
        <ip_address>192.168.1.20</ip_address>
        <authentication>
            <method>certificate</method>
            <cert_file>/home/zhuwuhui/freeDiameter/exe/certs/client.crt</cert_file>
            <key_file>/home/zhuwuhui/freeDiameter/exe/certs/client.key</key_file>
        </authentication>
        <bandwidth>
            <max_allocation>5000</max_allocation>   <!-- 5Mbps -->
            <priority>medium</priority>
        </bandwidth>
        <service_types>
            <service>data</service>
        </service_types>
        <qos_parameters>
            <latency_requirement>200</latency_requirement>
            <jitter_tolerance>50</jitter_tolerance>
            <packet_loss_tolerance>1.0</packet_loss_tolerance>
        </qos_parameters>
        <link_preferences>
            <preferred_links>
                <link type="wifi" priority="1"/>
                <link type="ethernet" priority="2"/>
                <link type="cellular" priority="3"/>
                <link type="satellite" priority="4"/>
            </preferred_links>
        </link_preferences>
    </client>
    
    <!-- 链路模拟器配置 -->
    <client id="link_simulator">
        <identity>link_simulator@freediameter.local</identity>
        <ip_address>192.168.1.30</ip_address>
        <authentication>
            <method>psk</method>
            <psk_key>simulator_secret_key_2024</psk_key>
        </authentication>
        <bandwidth>
            <max_allocation>50000</max_allocation>  <!-- 50Mbps for simulation -->
            <priority>system</priority>
        </bandwidth>
        <service_types>
            <service>simulation</service>
            <service>monitoring</service>
        </service_types>
    </client>
</client_profiles>
EOF
```

### 3.4 系统服务配置

#### systemd 服务文件
```bash
# 创建 systemd 服务文件
sudo cat > /etc/systemd/system/freediameter.service << 'EOF'
[Unit]
Description=freeDiameter Server for Aviation Communication
Documentation=man:freeDiameterd(8)
After=network.target
Wants=network.target

[Service]
Type=forking
User=root
Group=root
ExecStart=/usr/local/bin/freeDiameterd --conf /home/zhuwuhui/freeDiameter/ubuntu_server.conf --pidfile /var/run/freeDiameter/freediameter.pid
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/var/run/freeDiameter/freediameter.pid
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# 安全配置
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/freeDiameter /var/run/freeDiameter /tmp

# 资源限制
LimitNOFILE=65536
LimitNPROC=32768

[Install]
WantedBy=multi-user.target
EOF

# 重新加载 systemd 配置
sudo systemctl daemon-reload

# 启用服务
sudo systemctl enable freediameter

echo "freeDiameter 系统服务配置完成"
```

---

## 4. Windows 客户端部署

### 4.1 系统环境准备

#### 系统要求检查脚本
```powershell
# windows_client_system_check.ps1

Write-Host "=== Windows 客户端系统环境检查 ===" -ForegroundColor Green

# 检查操作系统版本
Write-Host "1. 操作系统版本:" -ForegroundColor Yellow
Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, TotalPhysicalMemory

# 检查网络配置
Write-Host "2. 网络配置:" -ForegroundColor Yellow
$networkConfig = Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -like "192.168.1.*"}
if ($networkConfig) {
    Write-Host "✓ IP地址: $($networkConfig.IPAddress)" -ForegroundColor Green
} else {
    Write-Host "✗ 未找到正确的IP地址配置" -ForegroundColor Red
}

# 测试网络连通性
Write-Host "3. 网络连通性测试:" -ForegroundColor Yellow
if (Test-Connection -ComputerName "192.168.1.10" -Count 2 -Quiet) {
    Write-Host "✓ 服务端网络连通" -ForegroundColor Green
} else {
    Write-Host "✗ 服务端网络不通" -ForegroundColor Red
}

if (Test-Connection -ComputerName "192.168.1.30" -Count 2 -Quiet) {
    Write-Host "✓ 模拟器网络连通" -ForegroundColor Green
} else {
    Write-Host "✗ 模拟器网络不通" -ForegroundColor Red
}

# 检查必要的软件
Write-Host "4. 软件环境检查:" -ForegroundColor Yellow
$requiredSoftware = @(
    @{Name="Visual C++ Redistributable"; Path="C:\Windows\System32\vcruntime140.dll"},
    @{Name=".NET Framework"; Path="C:\Windows\Microsoft.NET\Framework64\v4.0.30319"}
)

foreach ($software in $requiredSoftware) {
    if (Test-Path $software.Path) {
        Write-Host "✓ $($software.Name) 已安装" -ForegroundColor Green
    } else {
        Write-Host "✗ $($software.Name) 未安装" -ForegroundColor Red
    }
}

Write-Host "=== 系统环境检查完成 ===" -ForegroundColor Green
```

### 4.2 客户端程序部署

#### 部署目录结构创建
```powershell
# windows_client_setup.ps1

Write-Host "设置 Windows 客户端部署环境..." -ForegroundColor Green

# 创建部署目录
$deployPath = "C:\freeDiameter\client"
New-Item -ItemType Directory -Path $deployPath -Force
New-Item -ItemType Directory -Path "$deployPath\bin" -Force
New-Item -ItemType Directory -Path "$deployPath\conf" -Force
New-Item -ItemType Directory -Path "$deployPath\certs" -Force
New-Item -ItemType Directory -Path "$deployPath\logs" -Force
New-Item -ItemType Directory -Path "$deployPath\scripts" -Force

Write-Host "✓ 部署目录创建完成: $deployPath" -ForegroundColor Green

# 复制客户端程序文件
$sourceFiles = @(
    @{Source="\\192.168.1.10\freeDiameter\exe\diameter_client"; Dest="$deployPath\bin\diameter_client.exe"},
    @{Source="\\192.168.1.10\freeDiameter\exe\libfdcore.so.7"; Dest="$deployPath\bin\libfdcore.dll"},
    @{Source="\\192.168.1.10\freeDiameter\exe\libfdproto.so.7"; Dest="$deployPath\bin\libfdproto.dll"}
)

foreach ($file in $sourceFiles) {
    if (Test-Path $file.Source) {
        Copy-Item -Path $file.Source -Destination $file.Dest -Force
        Write-Host "✓ 复制文件: $(Split-Path $file.Dest -Leaf)" -ForegroundColor Green
    } else {
        Write-Host "✗ 源文件不存在: $($file.Source)" -ForegroundColor Red
    }
}

Write-Host "Windows 客户端部署环境设置完成" -ForegroundColor Green
```

#### 客户端配置文件
```bash
# 创建 Windows 客户端配置文件
cat > /home/zhuwuhui/freeDiameter/windows_client.conf << 'EOF'
# freeDiameter Windows 客户端配置文件
# 分布式部署 - 客户端专用配置

# 客户端身份配置
Identity = "aircraft_001@freediameter.local";
Realm = "freediameter.local";

# 网络配置
ListenOn = "192.168.1.20";  # Windows 客户端IP
Port = 3868;

# 服务端连接配置
ConnectPeer = "server.freediameter.local" {
    ConnectTo = "192.168.1.10";  # Ubuntu 服务端IP
    Port = 3868;
    No_TLS;  # 初始部署不使用TLS
    Prefer_TCP;
    TcpNoDelay;
};

# 链路模拟器连接配置
ConnectPeer = "simulator.freediameter.local" {
    ConnectTo = "192.168.1.30";  # Windows 模拟器IP
    Port = 8001;  # 默认连接以太网链路
    No_TLS;
    Prefer_TCP;
};

# 客户端特定配置
AppServThreads = 2;
NoRelay;

# 客户端扩展加载
LoadExtension = "C:\freeDiameter\client\bin\client.fdx" {
    # 客户端模式配置
    ClientMode = 1;
    ClientID = "aircraft_001";
    
    # 认证配置
    AuthenticationMethod = "psk";
    PSKKey = "aircraft001_secret_key_2024";
    
    # 链路选择配置
    LinkSelectionEnabled = 1;
    LinkSelectionStrategy = "adaptive";
    
    # 链路质量阈值
    LinkQualityThresholds = {
        LatencyThreshold = 100;      # ms
        JitterThreshold = 20;        # ms
        PacketLossThreshold = 0.1;   # %
        BandwidthThreshold = 1000;   # Kbps
    };
    
    # 链路优先级配置
    LinkPriorities = {
        Ethernet = 1;    # 最高优先级
        WiFi = 2;        # 第二优先级
        Cellular = 3;    # 第三优先级
        Satellite = 4;   # 最低优先级
    };
    
    # 自动重连配置
    AutoReconnect = 1;
    ReconnectInterval = 30;  # 秒
    MaxReconnectAttempts = 10;
    
    # 消息队列配置
    MessageQueueSize = 1000;
    MessageTimeout = 30;  # 秒
    
    # 会话管理配置
    SessionTimeout = 300;  # 秒
    MaxConcurrentSessions = 10;
    
    # 监控配置
    MonitoringEnabled = 1;
    MonitoringPort = 9090;
    MonitoringInterface = "192.168.1.20";
    
    # 日志配置
    LogLevel = "INFO";
    LogFile = "C:\freeDiameter\client\logs\client.log";
    LogRotation = 1;
    MaxLogSize = "10MB";
    MaxLogFiles = 5;
};

# 字典加载
LoadExtension = "C:\freeDiameter\client\bin\dict_arinc839.fdx";

# 调试配置
LoadExtension = "C:\freeDiameter\client\bin\dbg_loglevel.fdx" {
    LogLevel = "DEBUG";
    LogFile = "C:\freeDiameter\client\logs\debug.log";
};
EOF
```

### 4.3 客户端启动脚本

#### Windows 批处理启动脚本
```batch
@echo off
REM start_client.bat - Windows 客户端启动脚本

echo ========================================
echo freeDiameter Windows 客户端启动脚本
echo ========================================

REM 设置环境变量
set CLIENT_HOME=C:\freeDiameter\client
set CONFIG_FILE=%CLIENT_HOME%\conf\client.conf
set LOG_DIR=%CLIENT_HOME%\logs
set PID_FILE=%CLIENT_HOME%\client.pid

REM 检查目录
if not exist "%CLIENT_HOME%" (
    echo 错误: 客户端目录不存在 %CLIENT_HOME%
    pause
    exit /b 1
)

if not exist "%LOG_DIR%" (
    mkdir "%LOG_DIR%"
)

REM 检查配置文件
if not exist "%CONFIG_FILE%" (
    echo 错误: 配置文件不存在 %CONFIG_FILE%
    pause
    exit /b 1
)

REM 检查网络连通性
echo 检查网络连通性...
ping -n 2 192.168.1.10 >nul
if %errorlevel% neq 0 (
    echo 警告: 无法连接到服务端 192.168.1.10
)

ping -n 2 192.168.1.30 >nul
if %errorlevel% neq 0 (
    echo 警告: 无法连接到模拟器 192.168.1.30
)

REM 检查端口占用
netstat -an | findstr ":3868" >nul
if %errorlevel% equ 0 (
    echo 警告: 端口 3868 已被占用
)

REM 启动客户端
echo 启动 freeDiameter 客户端...
cd /d "%CLIENT_HOME%\bin"

REM 记录启动时间
echo %date% %time% - 客户端启动 >> "%LOG_DIR%\startup.log"

REM 启动客户端程序
start "freeDiameter Client" /min diameter_client.exe --conf "%CONFIG_FILE%" --pidfile "%PID_FILE%"

REM 等待启动
timeout /t 5 /nobreak >nul

REM 检查启动状态
tasklist | findstr "diameter_client.exe" >nul
if %errorlevel% equ 0 (
    echo ✓ 客户端启动成功
    echo 进程ID已保存到: %PID_FILE%
    echo 日志文件位置: %LOG_DIR%
) else (
    echo ✗ 客户端启动失败
    echo 请检查日志文件: %LOG_DIR%\client.log
)

echo ========================================
echo 客户端启动完成
echo 管理界面: http://192.168.1.20:9090
echo ========================================
pause
```

#### PowerShell 启动脚本
```powershell
# start_client.ps1 - Windows 客户端 PowerShell 启动脚本

param(
    [switch]$Debug,
    [switch]$TestMode,
    [string]$ConfigFile = "C:\freeDiameter\client\conf\client.conf"
)

Write-Host "========================================" -ForegroundColor Green
Write-Host "freeDiameter Windows 客户端启动脚本" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# 设置变量
$ClientHome = "C:\freeDiameter\client"
$LogDir = "$ClientHome\logs"
$BinDir = "$ClientHome\bin"
$PidFile = "$ClientHome\client.pid"

# 检查环境
Write-Host "检查部署环境..." -ForegroundColor Yellow

if (-not (Test-Path $ClientHome)) {
    Write-Host "错误: 客户端目录不存在 $ClientHome" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ConfigFile)) {
    Write-Host "错误: 配置文件不存在 $ConfigFile" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

# 网络连通性检查
Write-Host "检查网络连通性..." -ForegroundColor Yellow

$serverReachable = Test-Connection -ComputerName "192.168.1.10" -Count 2 -Quiet
$simulatorReachable = Test-Connection -ComputerName "192.168.1.30" -Count 2 -Quiet

if ($serverReachable) {
    Write-Host "✓ 服务端连通性正常" -ForegroundColor Green
} else {
    Write-Host "✗ 服务端连通性异常" -ForegroundColor Red
}

if ($simulatorReachable) {
    Write-Host "✓ 模拟器连通性正常" -ForegroundColor Green
} else {
    Write-Host "✗ 模拟器连通性异常" -ForegroundColor Red
}

# 检查端口占用
$portInUse = Get-NetTCPConnection -LocalPort 3868 -ErrorAction SilentlyContinue
if ($portInUse) {
    Write-Host "警告: 端口 3868 已被占用" -ForegroundColor Yellow
}

# 停止已运行的客户端
$existingProcess = Get-Process -Name "diameter_client" -ErrorAction SilentlyContinue
if ($existingProcess) {
    Write-Host "停止已运行的客户端进程..." -ForegroundColor Yellow
    $existingProcess | Stop-Process -Force
    Start-Sleep -Seconds 2
}

# 构建启动参数
$arguments = @("--conf", $ConfigFile)
if ($Debug) {
    $arguments += "--debug"
}
if ($TestMode) {
    $arguments += "--test-mode"
}

# 启动客户端
Write-Host "启动 freeDiameter 客户端..." -ForegroundColor Yellow

try {
    $process = Start-Process -FilePath "$BinDir\diameter_client.exe" -ArgumentList $arguments -WorkingDirectory $BinDir -PassThru -WindowStyle Minimized
    
    # 等待启动
    Start-Sleep -Seconds 3
    
    if (-not $process.HasExited) {
        Write-Host "✓ 客户端启动成功" -ForegroundColor Green
        Write-Host "进程ID: $($process.Id)" -ForegroundColor Green
        
        # 保存进程ID
        $process.Id | Out-File -FilePath $PidFile -Encoding ASCII
        
        # 记录启动日志
        $startupLog = "$LogDir\startup.log"
        "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') - 客户端启动成功 (PID: $($process.Id))" | Add-Content -Path $startupLog
        
        Write-Host "日志文件: $LogDir\client.log" -ForegroundColor Green
        Write-Host "管理界面: http://192.168.1.20:9090" -ForegroundColor Green
    } else {
        Write-Host "✗ 客户端启动失败" -ForegroundColor Red
        Write-Host "请检查日志文件: $LogDir\client.log" -ForegroundColor Red
    }
} catch {
    Write-Host "✗ 启动客户端时发生错误: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "客户端启动完成" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
```

---

## 5. Windows 链路模拟器部署

### 5.1 模拟器环境准备

#### 系统检查脚本
```powershell
# windows_simulator_system_check.ps1

Write-Host "=== Windows 链路模拟器系统环境检查 ===" -ForegroundColor Green

# 检查系统资源
Write-Host "1. 系统资源检查:" -ForegroundColor Yellow
$computerInfo = Get-ComputerInfo
Write-Host "CPU: $($computerInfo.CsProcessors.Count) 核心" -ForegroundColor White
Write-Host "内存: $([math]::Round($computerInfo.TotalPhysicalMemory / 1GB, 2)) GB" -ForegroundColor White
Write-Host "可用内存: $([math]::Round($computerInfo.AvailablePhysicalMemory / 1GB, 2)) GB" -ForegroundColor White

# 检查网络配置
Write-Host "2. 网络配置检查:" -ForegroundColor Yellow
$networkAdapter = Get-NetAdapter | Where-Object {$_.Status -eq "Up"}
foreach ($adapter in $networkAdapter) {
    $ipConfig = Get-NetIPAddress -InterfaceIndex $adapter.InterfaceIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue
    if ($ipConfig) {
        Write-Host "网卡: $($adapter.Name) - IP: $($ipConfig.IPAddress)" -ForegroundColor White
    }
}

# 检查端口可用性
Write-Host "3. 端口可用性检查:" -ForegroundColor Yellow
$requiredPorts = @(8001, 8002, 8003, 8004, 9091)
foreach ($port in $requiredPorts) {
    $portInUse = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue
    if ($portInUse) {
        Write-Host "✗ 端口 $port 已被占用" -ForegroundColor Red
    } else {
        Write-Host "✓ 端口 $port 可用" -ForegroundColor Green
    }
}

# 检查网络连通性
Write-Host "4. 网络连通性检查:" -ForegroundColor Yellow
$targets = @(
    @{Name="Ubuntu 服务端"; IP="192.168.1.10"},
    @{Name="Windows 客户端"; IP="192.168.1.20"}
)

foreach ($target in $targets) {
    if (Test-Connection -ComputerName $target.IP -Count 2 -Quiet) {
        Write-Host "✓ $($target.Name) ($($target.IP)) 连通正常" -ForegroundColor Green
    } else {
        Write-Host "✗ $($target.Name) ($($target.IP)) 连通异常" -ForegroundColor Red
    }
}

Write-Host "=== 系统环境检查完成 ===" -ForegroundColor Green
```

### 5.2 模拟器程序部署

#### 部署脚本
```powershell
# windows_simulator_setup.ps1

Write-Host "设置 Windows 链路模拟器部署环境..." -ForegroundColor Green

# 创建部署目录
$deployPath = "C:\freeDiameter\simulator"
$directories = @("bin", "conf", "logs", "data", "scripts", "tests")

foreach ($dir in $directories) {
    $fullPath = "$deployPath\$dir"
    New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
    Write-Host "✓ 创建目录: $fullPath" -ForegroundColor Green
}

# 复制模拟器程序文件
Write-Host "复制模拟器程序文件..." -ForegroundColor Yellow

$sourceFiles = @(
    @{Source="\\192.168.1.10\freeDiameter\exe\link_simulator"; Dest="$deployPath\bin\link_simulator.exe"},
    @{Source="\\192.168.1.10\freeDiameter\exe\libfdcore.so.7"; Dest="$deployPath\bin\libfdcore.dll"},
    @{Source="\\192.168.1.10\freeDiameter\exe\libfdproto.so.7"; Dest="$deployPath\bin\libfdproto.dll"}
)

foreach ($file in $sourceFiles) {
    if (Test-Path $file.Source) {
        Copy-Item -Path $file.Source -Destination $file.Dest -Force
        Write-Host "✓ 复制: $(Split-Path $file.Dest -Leaf)" -ForegroundColor Green
    } else {
        Write-Host "✗ 源文件不存在: $($file.Source)" -ForegroundColor Red
    }
}

# 创建模拟器配置文件
Write-Host "创建模拟器配置文件..." -ForegroundColor Yellow

$configContent = @"
# freeDiameter 链路模拟器配置文件
# 分布式部署 - 模拟器专用配置

# 模拟器身份配置
Identity = "link_simulator@freediameter.local";
Realm = "freediameter.local";

# 网络监听配置
ListenOn = "192.168.1.30";  # Windows 模拟器IP

# 链路端口配置
EthernetPort = 8001;
WiFiPort = 8002;
CellularPort = 8003;
SatellitePort = 8004;

# 管理端口
ManagementPort = 9091;

# 服务端连接配置
ConnectPeer = "server.freediameter.local" {
    ConnectTo = "192.168.1.10";  # Ubuntu 服务端IP
    Port = 3868;
    No_TLS;
    Prefer_TCP;
};

# 链路模拟参数
LinkSimulation = {
    # 以太网链路参数
    Ethernet = {
        Bandwidth = 100000;      # 100Mbps
        Latency = 5;            # 5ms
        Jitter = 1;             # 1ms
        PacketLoss = 0.01;      # 0.01%
        Reliability = 99.9;     # 99.9%
        MTU = 1500;             # bytes
    };
    
    # WiFi链路参数
    WiFi = {
        Bandwidth = 54000;      # 54Mbps
        Latency = 10;           # 10ms
        Jitter = 5;             # 5ms
        PacketLoss = 0.1;       # 0.1%
        Reliability = 99.5;     # 99.5%
        MTU = 1500;             # bytes
        SignalStrength = -50;   # dBm
    };
    
    # 蜂窝链路参数
    Cellular = {
        Bandwidth = 10000;      # 10Mbps
        Latency = 50;           # 50ms
        Jitter = 20;            # 20ms
        PacketLoss = 1.0;       # 1.0%
        Reliability = 98.0;     # 98.0%
        MTU = 1400;             # bytes
        SignalStrength = -80;   # dBm
    };
    
    # 卫星链路参数
    Satellite = {
        Bandwidth = 2000;       # 2Mbps
        Latency = 600;          # 600ms
        Jitter = 100;           # 100ms
        PacketLoss = 2.0;       # 2.0%
        Reliability = 95.0;     # 95.0%
        MTU = 1200;             # bytes
        SignalStrength = -100;  # dBm
    };
};

# 动态参数变化配置
DynamicSimulation = {
    Enabled = 1;
    
    # 环境变化模拟
    EnvironmentChanges = {
        WeatherEffects = 1;     # 天气影响
        InterferenceEffects = 1; # 干扰影响
        MobilityEffects = 1;    # 移动性影响
    };
    
    # 变化周期配置
    ChangeIntervals = {
        FastChanges = 10;       # 10秒 - 快速变化
        SlowChanges = 300;      # 5分钟 - 慢速变化
    };
};

# 监控和日志配置
Monitoring = {
    Enabled = 1;
    Port = 9091;
    Interface = "192.168.1.30";
    
    # 统计信息收集
    Statistics = {
        PacketCount = 1;
        ByteCount = 1;
        ErrorCount = 1;
        LatencyStats = 1;
    };
    
    # 日志配置
    Logging = {
        Level = "INFO";
        File = "C:\freeDiameter\simulator\logs\simulator.log";
        Rotation = 1;
        MaxSize = "50MB";
        MaxFiles = 10;
    };
};
"@

$configContent | Out-File -FilePath "$deployPath\conf\simulator.conf" -Encoding UTF8
Write-Host "✓ 配置文件创建完成" -ForegroundColor Green

Write-Host "Windows 链路模拟器部署环境设置完成" -ForegroundColor Green
```

### 5.3 模拟器启动脚本

#### 批处理启动脚本
```batch
@echo off
REM start_simulator.bat - Windows 链路模拟器启动脚本

echo ========================================
echo freeDiameter 链路模拟器启动脚本
echo ========================================

REM 设置环境变量
set SIMULATOR_HOME=C:\freeDiameter\simulator
set CONFIG_FILE=%SIMULATOR_HOME%\conf\simulator.conf
set LOG_DIR=%SIMULATOR_HOME%\logs
set PID_FILE=%SIMULATOR_HOME%\simulator.pid

REM 检查环境
if not exist "%SIMULATOR_HOME%" (
    echo 错误: 模拟器目录不存在 %SIMULATOR_HOME%
    pause
    exit /b 1
)

if not exist "%CONFIG_FILE%" (
    echo 错误: 配置文件不存在 %CONFIG_FILE%
    pause
    exit /b 1
)

if not exist "%LOG_DIR%" (
    mkdir "%LOG_DIR%"
)

REM 检查网络连通性
echo 检查网络连通性...
ping -n 2 192.168.1.10 >nul
if %errorlevel% neq 0 (
    echo 警告: 无法连接到服务端 192.168.1.10
)

ping -n 2 192.168.1.20 >nul
if %errorlevel% neq 0 (
    echo 警告: 无法连接到客户端 192.168.1.20
)

REM 检查端口占用
echo 检查端口占用情况...
for %%p in (8001 8002 8003 8004 9091) do (
    netstat -an | findstr ":%%p" >nul
    if %errorlevel% equ 0 (
        echo 警告: 端口 %%p 已被占用
    ) else (
        echo ✓ 端口 %%p 可用
    )
)

REM 启动模拟器
echo 启动链路模拟器...
cd /d "%SIMULATOR_HOME%\bin"

REM 记录启动时间
echo %date% %time% - 模拟器启动 >> "%LOG_DIR%\startup.log"

REM 启动模拟器程序
start "Link Simulator" /min link_simulator.exe --conf "%CONFIG_FILE%" --pidfile "%PID_FILE%"

REM 等待启动
timeout /t 5 /nobreak >nul

REM 检查启动状态
tasklist | findstr "link_simulator.exe" >nul
if %errorlevel% equ 0 (
    echo ✓ 链路模拟器启动成功
    echo 进程ID已保存到: %PID_FILE%
    echo 日志文件位置: %LOG_DIR%
    echo.
    echo 链路端口状态:
    echo   以太网链路: 8001
    echo   WiFi链路:   8002
    echo   蜂窝链路:   8003
    echo   卫星链路:   8004
    echo   管理接口:   9091
) else (
    echo ✗ 链路模拟器启动失败
    echo 请检查日志文件: %LOG_DIR%\simulator.log
)

echo ========================================
echo 模拟器启动完成
echo 管理界面: http://192.168.1.30:9091
echo ========================================
pause
```

#### PowerShell 启动脚本
```powershell
# start_simulator.ps1 - Windows 链路模拟器 PowerShell 启动脚本

param(
    [switch]$Debug,
    [switch]$TestMode,
    [string]$ConfigFile = "C:\freeDiameter\simulator\conf\simulator.conf",
    [string[]]$EnabledLinks = @("ethernet", "wifi", "cellular", "satellite")
)

Write-Host "========================================" -ForegroundColor Green
Write-Host "freeDiameter 链路模拟器启动脚本" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# 设置变量
$SimulatorHome = "C:\freeDiameter\simulator"
$LogDir = "$SimulatorHome\logs"
$BinDir = "$SimulatorHome\bin"
$PidFile = "$SimulatorHome\simulator.pid"

# 链路端口映射
$linkPorts = @{
    "ethernet" = 8001
    "wifi" = 8002
    "cellular" = 8003
    "satellite" = 8004
}

# 检查环境
Write-Host "检查部署环境..." -ForegroundColor Yellow

if (-not (Test-Path $SimulatorHome)) {
    Write-Host "错误: 模拟器目录不存在 $SimulatorHome" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ConfigFile)) {
    Write-Host "错误: 配置文件不存在 $ConfigFile" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

# 网络连通性检查
Write-Host "检查网络连通性..." -ForegroundColor Yellow

$targets = @(
    @{Name="Ubuntu 服务端"; IP="192.168.1.10"},
    @{Name="Windows 客户端"; IP="192.168.1.20"}
)

foreach ($target in $targets) {
    if (Test-Connection -ComputerName $target.IP -Count 2 -Quiet) {
        Write-Host "✓ $($target.Name) ($($target.IP)) 连通正常" -ForegroundColor Green
    } else {
        Write-Host "✗ $($target.Name) ($($target.IP)) 连通异常" -ForegroundColor Red
    }
}

# 检查端口占用
Write-Host "检查端口占用情况..." -ForegroundColor Yellow

$allPortsAvailable = $true
foreach ($link in $EnabledLinks) {
    if ($linkPorts.ContainsKey($link)) {
        $port = $linkPorts[$link]
        $portInUse = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue
        if ($portInUse) {
            Write-Host "✗ $link 链路端口 $port 已被占用" -ForegroundColor Red
            $allPortsAvailable = $false
        } else {
            Write-Host "✓ $link 链路端口 $port 可用" -ForegroundColor Green
        }
    }
}

# 检查管理端口
$mgmtPortInUse = Get-NetTCPConnection -LocalPort 9091 -ErrorAction SilentlyContinue
if ($mgmtPortInUse) {
    Write-Host "✗ 管理端口 9091 已被占用" -ForegroundColor Red
    $allPortsAvailable = $false
} else {
    Write-Host "✓ 管理端口 9091 可用" -ForegroundColor Green
}

if (-not $allPortsAvailable) {
    Write-Host "警告: 部分端口被占用，可能影响模拟器功能" -ForegroundColor Yellow
}

# 停止已运行的模拟器
$existingProcess = Get-Process -Name "link_simulator" -ErrorAction SilentlyContinue
if ($existingProcess) {
    Write-Host "停止已运行的模拟器进程..." -ForegroundColor Yellow
    $existingProcess | Stop-Process -Force
    Start-Sleep -Seconds 2
}

# 构建启动参数
$arguments = @("--conf", $ConfigFile)
if ($Debug) {
    $arguments += "--debug"
}
if ($TestMode) {
    $arguments += "--test-mode"
}

# 添加启用的链路参数
$enabledLinksStr = $EnabledLinks -join ","
$arguments += "--enabled-links", $enabledLinksStr

# 启动模拟器
Write-Host "启动链路模拟器..." -ForegroundColor Yellow
Write-Host "启用的链路: $($EnabledLinks -join ', ')" -ForegroundColor White

try {
    $process = Start-Process -FilePath "$BinDir\link_simulator.exe" -ArgumentList $arguments -WorkingDirectory $BinDir -PassThru -WindowStyle Minimized
    
    # 等待启动
    Start-Sleep -Seconds 5
    
    if (-not $process.HasExited) {
        Write-Host "✓ 链路模拟器启动成功" -ForegroundColor Green
        Write-Host "进程ID: $($process.Id)" -ForegroundColor Green
        
        # 保存进程ID
        $process.Id | Out-File -FilePath $PidFile -Encoding ASCII
        
        # 记录启动日志
        $startupLog = "$LogDir\startup.log"
        "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') - 模拟器启动成功 (PID: $($process.Id))" | Add-Content -Path $startupLog
        
        # 显示链路状态
        Write-Host "链路端口状态:" -ForegroundColor Green
        foreach ($link in $EnabledLinks) {
            if ($linkPorts.ContainsKey($link)) {
                $port = $linkPorts[$link]
                Write-Host "  $link 链路: $port" -ForegroundColor White
            }
        }
        
        Write-Host "日志文件: $LogDir\simulator.log" -ForegroundColor Green
        Write-Host "管理界面: http://192.168.1.30:9091" -ForegroundColor Green
    } else {
        Write-Host "✗ 链路模拟器启动失败" -ForegroundColor Red
        Write-Host "请检查日志文件: $LogDir\simulator.log" -ForegroundColor Red
    }
} catch {
    Write-Host "✗ 启动模拟器时发生错误: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "模拟器启动完成" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
```

---

## 6. 分布式部署配置文件

### 6.1 网络配置文件

#### hosts 文件配置
```bash
# Ubuntu 服务端 /etc/hosts 配置
cat >> /etc/hosts << 'EOF'
# freeDiameter 分布式部署主机映射
192.168.1.10    server.freediameter.local ubuntu-server
192.168.1.20    client.freediameter.local windows-client
192.168.1.30    simulator.freediameter.local windows-simulator
EOF
```

```batch
REM Windows 客户端和模拟器 C:\Windows\System32\drivers\etc\hosts 配置
echo # freeDiameter 分布式部署主机映射 >> C:\Windows\System32\drivers\etc\hosts
echo 192.168.1.10    server.freediameter.local ubuntu-server >> C:\Windows\System32\drivers\etc\hosts
echo 192.168.1.20    client.freediameter.local windows-client >> C:\Windows\System32\drivers\etc\hosts
echo 192.168.1.30    simulator.freediameter.local windows-simulator >> C:\Windows\System32\drivers\etc\hosts
```

### 6.2 证书配置

#### 证书生成脚本
```bash
#!/bin/bash
# generate_certificates.sh - 为分布式部署生成证书

echo "为分布式部署生成证书..."

CERT_DIR="/home/zhuwuhui/freeDiameter/exe/certs"
mkdir -p $CERT_DIR
cd $CERT_DIR

# 生成CA私钥
openssl genrsa -out ca.key 4096

# 生成CA证书
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/C=CN/ST=Beijing/L=Beijing/O=freeDiameter/OU=Aviation/CN=freeDiameter CA"

# 生成服务端私钥
openssl genrsa -out server.key 2048

# 生成服务端证书请求
openssl req -new -key server.key -out server.csr -subj "/C=CN/ST=Beijing/L=Beijing/O=freeDiameter/OU=Server/CN=server.freediameter.local"

# 生成服务端证书
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt

# 生成客户端私钥
openssl genrsa -out client.key 2048

# 生成客户端证书请求
openssl req -new -key client.key -out client.csr -subj "/C=CN/ST=Beijing/L=Beijing/O=freeDiameter/OU=Client/CN=client.freediameter.local"

# 生成客户端证书
openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt

# 生成模拟器私钥
openssl genrsa -out simulator.key 2048

# 生成模拟器证书请求
openssl req -new -key simulator.key -out simulator.csr -subj "/C=CN/ST=Beijing/L=Beijing/O=freeDiameter/OU=Simulator/CN=simulator.freediameter.local"

# 生成模拟器证书
openssl x509 -req -days 365 -in simulator.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out simulator.crt

# 生成DH参数文件
openssl dhparam -out dh.pem 2048

# 设置权限
chmod 600 *.key
chmod 644 *.crt *.pem

echo "证书生成完成"
ls -la $CERT_DIR
```

### 6.3 部署验证脚本

#### 完整部署验证
```bash
#!/bin/bash
# verify_distributed_deployment.sh - 验证分布式部署

echo "=========================================="
echo "freeDiameter 分布式部署验证"
echo "=========================================="

# 测试配置
UBUNTU_SERVER="192.168.1.10"
WINDOWS_CLIENT="192.168.1.20"
WINDOWS_SIMULATOR="192.168.1.30"

# 验证函数
verify_component() {
    local component_name="$1"
    local host_ip="$2"
    local port="$3"
    local description="$4"
    
    echo "验证 $component_name ($description)..."
    
    if ping -c 2 $host_ip > /dev/null 2>&1; then
        echo "  ✓ 主机 $host_ip 网络连通"
    else
        echo "  ✗ 主机 $host_ip 网络不通"
        return 1
    fi
    
    if nc -z -w 5 $host_ip $port > /dev/null 2>&1; then
        echo "  ✓ 端口 $port 服务正常"
    else
        echo "  ✗ 端口 $port 服务异常"
        return 1
    fi
    
    return 0
}

# 1. 验证 Ubuntu 服务端
echo "1. 验证 Ubuntu 服务端"
verify_component "Ubuntu 服务端" $UBUNTU_SERVER 3868 "Diameter 服务"
verify_component "Ubuntu 管理接口" $UBUNTU_SERVER 8080 "Web 管理"

# 2. 验证 Windows 客户端
echo "2. 验证 Windows 客户端"
verify_component "Windows 客户端" $WINDOWS_CLIENT 3868 "客户端服务"
verify_component "客户端管理接口" $WINDOWS_CLIENT 9090 "客户端管理"

# 3. 验证 Windows 模拟器
echo "3. 验证 Windows 模拟器"
verify_component "以太网链路" $WINDOWS_SIMULATOR 8001 "以太网模拟"
verify_component "WiFi链路" $WINDOWS_SIMULATOR 8002 "WiFi模拟"
verify_component "蜂窝链路" $WINDOWS_SIMULATOR 8003 "蜂窝模拟"
verify_component "卫星链路" $WINDOWS_SIMULATOR 8004 "卫星模拟"
verify_component "模拟器管理接口" $WINDOWS_SIMULATOR 9091 "模拟器管理"

# 4. 验证组件间通信
echo "4. 验证组件间通信"

# 客户端到服务端
echo "验证客户端到服务端通信..."
if timeout 10 bash -c "echo 'test' | nc $UBUNTU_SERVER 3868" > /dev/null 2>&1; then
    echo "  ✓ 客户端到服务端通信正常"
else
    echo "  ✗ 客户端到服务端通信异常"
fi

# 客户端到模拟器
echo "验证客户端到模拟器通信..."
if timeout 10 bash -c "echo 'test' | nc $WINDOWS_SIMULATOR 8001" > /dev/null 2>&1; then
    echo "  ✓ 客户端到模拟器通信正常"
else
    echo "  ✗ 客户端到模拟器通信异常"
fi

# 5. 验证配置文件
echo "5. 验证配置文件"

config_files=(
    "/home/zhuwuhui/freeDiameter/ubuntu_server.conf"
    "/home/zhuwuhui/freeDiameter/windows_client.conf"
    "/home/zhuwuhui/freeDiameter/exe/client_conf/client_profiles.xml"
)

for config_file in "${config_files[@]}"; do
    if [ -f "$config_file" ]; then
        echo "  ✓ 配置文件存在: $(basename $config_file)"
    else
        echo "  ✗ 配置文件缺失: $(basename $config_file)"
    fi
done

# 6. 验证证书文件
echo "6. 验证证书文件"

cert_files=(
    "/home/zhuwuhui/freeDiameter/exe/certs/ca.crt"
    "/home/zhuwuhui/freeDiameter/exe/certs/server.crt"
    "/home/zhuwuhui/freeDiameter/exe/certs/client.crt"
    "/home/zhuwuhui/freeDiameter/exe/certs/simulator.crt"
)

for cert_file in "${cert_files[@]}"; do
    if [ -f "$cert_file" ]; then
        echo "  ✓ 证书文件存在: $(basename $cert_file)"
        # 验证证书有效性
        if openssl x509 -in "$cert_file" -noout -checkend 86400 > /dev/null 2>&1; then
            echo "    ✓ 证书有效期正常"
        else
            echo "    ✗ 证书即将过期或已过期"
        fi
    else
        echo "  ✗ 证书文件缺失: $(basename $cert_file)"
    fi
done

echo "=========================================="
echo "分布式部署验证完成"
echo "=========================================="
```

---

## 7. 运维管理

### 7.1 系统监控

#### 分布式监控脚本
```bash
#!/bin/bash
# distributed_monitoring.sh - 分布式系统监控

echo "启动 freeDiameter 分布式系统监控..."

MONITOR_INTERVAL=30  # 监控间隔(秒)
LOG_DIR="/var/log/freediameter_monitoring"
ALERT_THRESHOLD_CPU=80
ALERT_THRESHOLD_MEM=85
ALERT_THRESHOLD_DISK=90

# 创建监控日志目录
mkdir -p $LOG_DIR

# 监控目标配置
declare -A TARGETS=(
    ["ubuntu_server"]="192.168.1.10:3868"
    ["windows_client"]="192.168.1.20:9090"
    ["windows_simulator"]="192.168.1.30:9091"
)

# 监控函数
monitor_target() {
    local target_name=$1
    local target_address=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local log_file="$LOG_DIR/${target_name}_$(date +%Y%m%d).log"
    
    IFS=':' read -r host port <<< "$target_address"
    
    echo "[$timestamp] 监控目标: $target_name ($target_address)" >> $log_file
    
    # 网络连通性检查
    if ping -c 1 -W 3 $host > /dev/null 2>&1; then
        echo "[$timestamp] 网络连通性: 正常" >> $log_file
        
        # 端口连通性检查
        if nc -z -w 3 $host $port > /dev/null 2>&1; then
            echo "[$timestamp] 服务端口 $port: 正常" >> $log_file
        else
            echo "[$timestamp] 服务端口 $port: 异常" >> $log_file
            # 发送告警
            echo "[$timestamp] 告警: $target_name 服务端口 $port 不可达" | tee -a $LOG_DIR/alerts.log
        fi
    else
        echo "[$timestamp] 网络连通性: 异常" >> $log_file
        # 发送告警
        echo "[$timestamp] 告警: $target_name 主机 $host 网络不可达" | tee -a $LOG_DIR/alerts.log
    fi
    
    # 如果是本地服务端，检查系统资源
    if [ "$target_name" = "ubuntu_server" ] && [ "$host" = "192.168.1.10" ]; then
        # CPU使用率
        cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | awk -F'%' '{print $1}')
        echo "[$timestamp] CPU使用率: ${cpu_usage}%" >> $log_file
        
        # 内存使用率
        mem_usage=$(free | grep Mem | awk '{printf "%.1f", $3/$2 * 100.0}')
        echo "[$timestamp] 内存使用率: ${mem_usage}%" >> $log_file
        
        # 磁盘使用率
        disk_usage=$(df -h / | awk 'NR==2 {print $5}' | sed 's/%//')
        echo "[$timestamp] 磁盘使用率: ${disk_usage}%" >> $log_file
        
        # 检查告警阈值
        if (( $(echo "$cpu_usage > $ALERT_THRESHOLD_CPU" | bc -l) )); then
            echo "[$timestamp] 告警: CPU使用率过高 ${cpu_usage}%" | tee -a $LOG_DIR/alerts.log
        fi
        
        if (( $(echo "$mem_usage > $ALERT_THRESHOLD_MEM" | bc -l) )); then
            echo "[$timestamp] 告警: 内存使用率过高 ${mem_usage}%" | tee -a $LOG_DIR/alerts.log
        fi
        
        if [ $disk_usage -gt $ALERT_THRESHOLD_DISK ]; then
            echo "[$timestamp] 告警: 磁盘使用率过高 ${disk_usage}%" | tee -a $LOG_DIR/alerts.log
        fi
    fi
}

# 主监控循环
while true; do
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 开始监控周期"
    
    for target_name in "${!TARGETS[@]}"; do
        monitor_target "$target_name" "${TARGETS[$target_name]}" &
    done
    
    # 等待所有监控任务完成
    wait
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 监控周期完成"
    sleep $MONITOR_INTERVAL
done
```

### 7.2 日志管理

#### 日志轮转配置
```bash
# 创建 logrotate 配置
sudo cat > /etc/logrotate.d/freediameter << 'EOF'
/var/log/freeDiameter/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 root root
    postrotate
        /bin/kill -HUP `cat /var/run/freeDiameter/freediameter.pid 2> /dev/null` 2> /dev/null || true
    endscript
}

/var/log/freediameter_monitoring/*.log {
    daily
    missingok
    rotate 7
    compress
    delaycompress
    notifempty
    create 644 root root
}
EOF
```

### 7.3 备份和恢复

#### 配置备份脚本
```bash
#!/bin/bash
# backup_configuration.sh - 配置文件备份

echo "备份 freeDiameter 分布式部署配置..."

BACKUP_DIR="/backup/freediameter/$(date +%Y%m%d_%H%M%S)"
mkdir -p $BACKUP_DIR

# 备份服务端配置
echo "备份服务端配置..."
cp -r /home/zhuwuhui/freeDiameter/ubuntu_server.conf $BACKUP_DIR/
cp -r /home/zhuwuhui/freeDiameter/exe/client_conf/ $BACKUP_DIR/
cp -r /home/zhuwuhui/freeDiameter/exe/certs/ $BACKUP_DIR/

# 备份客户端配置
echo "备份客户端配置..."
cp /home/zhuwuhui/freeDiameter/windows_client.conf $BACKUP_DIR/

# 备份模拟器配置
echo "备份模拟器配置..."
if [ -f "/home/zhuwuhui/freeDiameter/windows_simulator.conf" ]; then
    cp /home/zhuwuhui/freeDiameter/windows_simulator.conf $BACKUP_DIR/
fi

# 创建备份清单
cat > $BACKUP_DIR/backup_manifest.txt << EOF
freeDiameter 分布式部署配置备份
备份时间: $(date)
备份内容:
- ubuntu_server.conf (服务端主配置)
- client_conf/ (客户端配置目录)
- certs/ (证书文件目录)
- windows_client.conf (Windows客户端配置)
- windows_simulator.conf (Windows模拟器配置)
EOF

# 压缩备份
cd $(dirname $BACKUP_DIR)
tar -czf "freediameter_backup_$(date +%Y%m%d_%H%M%S).tar.gz" $(basename $BACKUP_DIR)

echo "配置备份完成: $BACKUP_DIR"
```

---

## 8. 故障排除

### 8.1 常见问题解决

#### 网络连通性问题
```bash
#!/bin/bash
# troubleshoot_network.sh - 网络问题排查

echo "freeDiameter 分布式部署网络问题排查"

# 检查网络配置
echo "1. 检查网络配置"
ip addr show | grep -E "192.168.1.(10|20|30)"
ip route show | grep 192.168.1.0

# 检查防火墙状态
echo "2. 检查防火墙状态"
sudo ufw status
sudo iptables -L -n | grep -E "(3868|5868|8001|8002|8003|8004)"

# 检查端口监听
echo "3. 检查端口监听状态"
sudo netstat -tlnp | grep -E "(3868|5868|8080)"

# 检查进程状态
echo "4. 检查进程状态"
ps aux | grep -E "(freeDiameterd|diameter_client|link_simulator)"

# 测试连通性
echo "5. 测试连通性"
for host in 192.168.1.10 192.168.1.20 192.168.1.30; do
    echo "测试 $host:"
    ping -c 2 $host
    echo "---"
done
```

#### 服务启动问题
```bash
#!/bin/bash
# troubleshoot_services.sh - 服务启动问题排查

echo "freeDiameter 服务启动问题排查"

# 检查配置文件语法
echo "1. 检查配置文件语法"
if [ -f "/home/zhuwuhui/freeDiameter/ubuntu_server.conf" ]; then
    echo "检查服务端配置文件..."
    # 这里可以添加配置文件语法检查
    grep -n "Error\|error" /home/zhuwuhui/freeDiameter/ubuntu_server.conf || echo "配置文件语法检查通过"
fi

# 检查证书文件
echo "2. 检查证书文件"
cert_dir="/home/zhuwuhui/freeDiameter/exe/certs"
for cert in ca.crt server.crt client.crt; do
    if [ -f "$cert_dir/$cert" ]; then
        echo "检查 $cert:"
        openssl x509 -in "$cert_dir/$cert" -noout -dates
    else
        echo "证书文件不存在: $cert"
    fi
done

# 检查依赖库
echo "3. 检查依赖库"
ldd /home/zhuwuhui/freeDiameter/exe/freeDiameterd | grep "not found" || echo "依赖库检查通过"

# 检查日志文件
echo "4. 检查日志文件"
if [ -f "/var/log/freeDiameter/server.log" ]; then
    echo "最近的错误日志:"
    tail -20 /var/log/freeDiameter/server.log | grep -i error
fi
```

---

## 9. 性能优化

### 9.1 系统参数优化

#### Ubuntu 服务端优化
```bash
#!/bin/bash
# optimize_ubuntu_server.sh - Ubuntu 服务端性能优化

echo "优化 Ubuntu 服务端性能参数..."

# 网络参数优化
cat >> /etc/sysctl.conf << 'EOF'
# freeDiameter 网络优化参数
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
net.core.netdev_max_backlog = 5000
net.ipv4.tcp_congestion_control = bbr
net.ipv4.tcp_slow_start_after_idle = 0
EOF

# 应用网络参数
sysctl -p

# 文件描述符限制优化
cat >> /etc/security/limits.conf << 'EOF'
# freeDiameter 文件描述符限制
root soft nofile 65536
root hard nofile 65536
* soft nofile 65536
* hard nofile 65536
EOF

echo "Ubuntu 服务端性能优化完成"
```

### 9.2 应用程序优化

#### freeDiameter 配置优化
```bash
# 在服务端配置中添加性能优化参数
cat >> /home/zhuwuhui/freeDiameter/ubuntu_server.conf << 'EOF'

# 性能优化配置
AppServThreads = 8;          # 增加应用服务线程数
TcpNoDelay;                  # 禁用 TCP Nagle 算法
SCTP_streams = 30;           # 增加 SCTP 流数量

# 缓冲区优化
TcSendBufSize = 262144;      # 256KB 发送缓冲区
TcRecvBufSize = 262144;      # 256KB 接收缓冲区

# 连接优化
ConnectRetries = 5;          # 连接重试次数
ConnectTimeout = 10;         # 连接超时时间
EOF
```

---

## 10. 总结

本指南提供了完整的 freeDiameter 跨平台分布式部署方案，实现了三台计算机的分离部署：

### 10.1 部署架构优势

1. **高可用性**: 组件分离部署，单点故障不影响整体系统
2. **可扩展性**: 各组件可独立扩展和升级
3. **真实性**: 更接近实际航空通信环境
4. **灵活性**: 支持不同操作系统和硬件平台

### 10.2 关键配置要点

1. **网络配置**: 确保三台计算机网络互通，防火墙正确配置
2. **证书管理**: 统一的证书颁发和分发机制
3. **配置同步**: 各组件配置文件的一致性和同步
4. **监控告警**: 完整的分布式监控和告警机制

### 10.3 运维建议

1. **定期备份**: 配置文件和证书的定期备份
2. **性能监控**: 持续监控系统性能和网络状态
3. **日志管理**: 集中化日志收集和分析
4. **安全更新**: 定期更新系统和应用程序

通过本指南的部署方案，可以构建一个稳定、可靠、高性能的 freeDiameter 航空通信系统分布式环境。