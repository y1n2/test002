# freeDiameter Windows 客户端启动指南

## 概述

本指南详细介绍如何在 Windows 系统上启动 freeDiameter 客户端，包括环境准备、配置文件设置、启动方法和故障排除。

## 目录

1. [环境准备](#环境准备)
2. [文件部署](#文件部署)
3. [配置文件设置](#配置文件设置)
4. [启动方法](#启动方法)
5. [验证连接](#验证连接)
6. [故障排除](#故障排除)
7. [常用命令](#常用命令)

## 环境准备

### 1. 系统要求

- **操作系统**: Windows 10/11 或 Windows Server 2016+
- **网络**: 确保与 Ubuntu 服务端网络连通
- **权限**: 管理员权限（用于防火墙配置）
- **依赖库**: Visual C++ Redistributable

### 2. 网络配置

#### IP 地址规划
```
Ubuntu 服务端:    192.168.1.10
Windows 客户端:   192.168.1.20
Windows 模拟器:   192.168.1.30
```

#### 防火墙配置
```powershell
# 以管理员身份运行 PowerShell
# 允许出站连接到服务端
New-NetFirewallRule -DisplayName "freeDiameter Client to Server" -Direction Outbound -Protocol TCP -RemoteAddress 192.168.1.10 -RemotePort 3868,5868 -Action Allow

# 允许客户端监听端口
New-NetFirewallRule -DisplayName "freeDiameter Client Listen" -Direction Inbound -Protocol TCP -LocalPort 3868 -Action Allow
```

## 文件部署

### 1. 目录结构

在 Windows 系统上创建以下目录结构：

```
C:\freeDiameter\client\
├── bin\                    # 可执行文件目录
│   ├── diameter_client.exe # 客户端主程序
│   ├── libfdcore.dll      # 核心库
│   ├── libfdproto.dll     # 协议库
│   └── dict_arinc839.fdx  # ARINC-839 字典扩展
├── conf\                  # 配置文件目录
│   ├── client.conf        # 主配置文件
│   └── client_simple.conf # 简化配置文件
├── certs\                 # 证书文件目录
│   ├── ca.crt            # CA 证书
│   ├── client.crt        # 客户端证书
│   └── client.key        # 客户端私钥
├── logs\                  # 日志文件目录
└── scripts\               # 启动脚本目录
    ├── start_client.bat   # 批处理启动脚本
    └── start_client.ps1   # PowerShell 启动脚本
```

### 2. 文件复制

从 Ubuntu 服务端复制必要文件：

```powershell
# 创建目录
New-Item -ItemType Directory -Path "C:\freeDiameter\client\bin" -Force
New-Item -ItemType Directory -Path "C:\freeDiameter\client\conf" -Force
New-Item -ItemType Directory -Path "C:\freeDiameter\client\certs" -Force
New-Item -ItemType Directory -Path "C:\freeDiameter\client\logs" -Force
New-Item -ItemType Directory -Path "C:\freeDiameter\client\scripts" -Force

# 从 Ubuntu 服务端复制文件（需要网络共享或 SCP）
# 示例：使用网络共享
Copy-Item "\\192.168.1.10\freeDiameter\exe\diameter_client" -Destination "C:\freeDiameter\client\bin\diameter_client.exe"
Copy-Item "\\192.168.1.10\freeDiameter\exe\libfdcore.so.7" -Destination "C:\freeDiameter\client\bin\libfdcore.dll"
Copy-Item "\\192.168.1.10\freeDiameter\exe\libfdproto.so.7" -Destination "C:\freeDiameter\client\bin\libfdproto.dll"
Copy-Item "\\192.168.1.10\freeDiameter\exe\dict_arinc839.fdx" -Destination "C:\freeDiameter\client\bin\"
```

## 配置文件设置

### 1. 主配置文件 (client.conf)

创建 `C:\freeDiameter\client\conf\client.conf`：

```ini
# freeDiameter Windows 客户端配置文件

# 客户端身份标识
Identity = "aircraft_001@freediameter.local";
Realm = "freediameter.local";

# 禁用服务器功能（纯客户端模式）
NoRelay;
NoAdvertiseRelay;

# 网络配置
ListenOn = "192.168.1.20";  # Windows 客户端IP
Port = 3868;                # 客户端监听端口
No_IPv6;                    # 使用IPv4
No_SCTP;                    # 在Windows上使用TCP协议

# TLS安全配置（可选）
# TLS_Cred = "certs/client.crt", "certs/client.key";
# TLS_CA = "certs/ca.crt";

# 加载ARINC-839字典扩展
LoadExtension = "bin/dict_arinc839.fdx";

# 连接到Ubuntu服务端
ConnectPeer = "server.freediameter.local" {
    ConnectTo = "192.168.1.10";  # Ubuntu 服务端IP
    Port = 3868;
    No_TLS;                      # 初始部署不使用TLS
    Prefer_TCP;
    TcpNoDelay;
};

# 连接到链路模拟器（可选）
ConnectPeer = "simulator.freediameter.local" {
    ConnectTo = "192.168.1.30";  # Windows 模拟器IP
    Port = 8001;
    No_TLS;
    Prefer_TCP;
};

# 应用线程配置
AppServThreads = 4;

# 日志配置
# LogLevel = "INFO";
# LogFile = "logs/client.log";
```

### 2. 简化配置文件 (client_simple.conf)

创建 `C:\freeDiameter\client\conf\client_simple.conf`：

```ini
# freeDiameter Windows 客户端简化配置

Identity = "client1.arinc839.local";
Realm = "arinc839.local";

NoRelay;
Port = 3868;
No_IPv6;
No_SCTP;

LoadExtension = "bin/dict_arinc839.fdx";

ConnectPeer = "server.arinc839.local" {
    ConnectTo = "192.168.1.10";
    Port = 3868;
    No_TLS;
    Prefer_TCP;
};

AppServThreads = 2;
```

## 启动方法

### 方法一：批处理脚本启动（推荐）

创建 `C:\freeDiameter\client\scripts\start_client.bat`：

```batch
@echo off
REM freeDiameter Windows 客户端启动脚本

echo ========================================
echo freeDiameter Windows 客户端启动
echo ========================================

REM 设置环境变量
set CLIENT_HOME=C:\freeDiameter\client
set CONFIG_FILE=%CLIENT_HOME%\conf\client.conf
set LOG_DIR=%CLIENT_HOME%\logs

REM 切换到客户端目录
cd /d "%CLIENT_HOME%\bin"

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
    echo 请检查网络连接和服务端状态
)

REM 启动客户端
echo 启动 freeDiameter 客户端...
echo 配置文件: %CONFIG_FILE%
echo 日志目录: %LOG_DIR%
echo ========================================

diameter_client.exe --conf "%CONFIG_FILE%"

echo ========================================
echo 客户端已退出
pause
```

### 方法二：PowerShell 脚本启动

创建 `C:\freeDiameter\client\scripts\start_client.ps1`：

```powershell
# freeDiameter Windows 客户端 PowerShell 启动脚本

param(
    [switch]$Debug,
    [string]$ConfigFile = "C:\freeDiameter\client\conf\client.conf"
)

Write-Host "========================================" -ForegroundColor Green
Write-Host "freeDiameter Windows 客户端启动" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# 设置变量
$ClientHome = "C:\freeDiameter\client"
$BinDir = "$ClientHome\bin"
$LogDir = "$ClientHome\logs"

# 检查环境
if (-not (Test-Path $ClientHome)) {
    Write-Host "错误: 客户端目录不存在 $ClientHome" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ConfigFile)) {
    Write-Host "错误: 配置文件不存在 $ConfigFile" -ForegroundColor Red
    exit 1
}

# 创建日志目录
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

# 网络连通性检查
Write-Host "检查网络连通性..." -ForegroundColor Yellow
$serverReachable = Test-Connection -ComputerName "192.168.1.10" -Count 2 -Quiet

if ($serverReachable) {
    Write-Host "✓ 服务端连通性正常" -ForegroundColor Green
} else {
    Write-Host "✗ 服务端连通性异常，请检查网络" -ForegroundColor Red
}

# 切换到客户端目录
Set-Location $BinDir

# 启动客户端
Write-Host "启动 freeDiameter 客户端..." -ForegroundColor Yellow
Write-Host "配置文件: $ConfigFile" -ForegroundColor Cyan
Write-Host "工作目录: $BinDir" -ForegroundColor Cyan

if ($Debug) {
    Write-Host "调试模式启动..." -ForegroundColor Yellow
    & ".\diameter_client.exe" --conf $ConfigFile --debug
} else {
    & ".\diameter_client.exe" --conf $ConfigFile
}

Write-Host "客户端已退出" -ForegroundColor Yellow
```

### 方法三：命令行直接启动

```cmd
# 打开命令提示符，切换到客户端目录
cd C:\freeDiameter\client\bin

# 直接启动客户端
diameter_client.exe --conf ..\conf\client.conf

# 或使用简化配置
diameter_client.exe --conf ..\conf\client_simple.conf
```

## 验证连接

### 1. 检查客户端进程

```powershell
# 检查客户端进程是否运行
Get-Process | Where-Object {$_.ProcessName -like "*diameter*"}

# 检查网络连接
netstat -an | findstr ":3868"
```

### 2. 测试网络连通性

```powershell
# 测试到服务端的连接
Test-NetConnection -ComputerName 192.168.1.10 -Port 3868

# 使用 telnet 测试（需要启用 telnet 客户端）
telnet 192.168.1.10 3868
```

### 3. 查看日志

```powershell
# 查看客户端日志
Get-Content C:\freeDiameter\client\logs\client.log -Tail 20

# 实时监控日志
Get-Content C:\freeDiameter\client\logs\client.log -Wait
```

## 故障排除

### 1. 常见问题

#### 问题：无法连接到服务端
**解决方案**：
```powershell
# 检查网络连通性
ping 192.168.1.10

# 检查端口是否开放
Test-NetConnection -ComputerName 192.168.1.10 -Port 3868

# 检查防火墙设置
Get-NetFirewallRule | Where-Object {$_.DisplayName -like "*freeDiameter*"}
```

#### 问题：配置文件错误
**解决方案**：
```powershell
# 验证配置文件语法
diameter_client.exe --conf C:\freeDiameter\client\conf\client.conf --check-config
```

#### 问题：缺少依赖库
**解决方案**：
```powershell
# 检查 DLL 依赖
dumpbin /dependents C:\freeDiameter\client\bin\diameter_client.exe

# 安装 Visual C++ Redistributable
# 下载并安装最新版本的 VC++ Redistributable
```

### 2. 调试模式

```powershell
# 启用详细日志
diameter_client.exe --conf C:\freeDiameter\client\conf\client.conf --debug --verbose

# 使用调试配置文件
# 在配置文件中添加：
# LogLevel = "DEBUG";
# LogFile = "logs/debug.log";
```

### 3. 网络诊断

```powershell
# 网络路由检查
tracert 192.168.1.10

# 端口扫描
nmap -p 3868 192.168.1.10

# 防火墙日志检查
Get-WinEvent -LogName "Microsoft-Windows-Windows Firewall With Advanced Security/Firewall"
```

## 常用命令

### 1. 客户端交互命令

启动客户端后，可以使用以下命令：

```
help                    # 显示帮助信息
status                  # 显示客户端状态
peers                   # 显示对等节点信息
send-madr <session-id>  # 发送 MADR 消息
send-mcar <session-id>  # 发送 MCAR 消息
quit                    # 退出客户端
```

### 2. 系统管理命令

```powershell
# 启动客户端服务
Start-Service freeDiameterClient

# 停止客户端服务
Stop-Service freeDiameterClient

# 重启客户端服务
Restart-Service freeDiameterClient

# 查看服务状态
Get-Service freeDiameterClient
```

### 3. 监控命令

```powershell
# 监控网络连接
netstat -an | findstr ":3868"

# 监控进程资源使用
Get-Process diameter_client | Select-Object CPU,WorkingSet,VirtualMemorySize

# 监控日志文件
Get-Content C:\freeDiameter\client\logs\client.log -Wait
```

## 自动化启动

### 1. 创建 Windows 服务

```powershell
# 使用 NSSM 创建服务
nssm install freeDiameterClient "C:\freeDiameter\client\bin\diameter_client.exe"
nssm set freeDiameterClient Parameters "--conf C:\freeDiameter\client\conf\client.conf"
nssm set freeDiameterClient AppDirectory "C:\freeDiameter\client\bin"
nssm set freeDiameterClient DisplayName "freeDiameter Client"
nssm set freeDiameterClient Description "freeDiameter ARINC-839 Client Service"

# 启动服务
nssm start freeDiameterClient
```

### 2. 开机自启动

```powershell
# 创建计划任务
$action = New-ScheduledTaskAction -Execute "C:\freeDiameter\client\scripts\start_client.bat"
$trigger = New-ScheduledTaskTrigger -AtStartup
$principal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -LogonType ServiceAccount
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries

Register-ScheduledTask -TaskName "freeDiameterClient" -Action $action -Trigger $trigger -Principal $principal -Settings $settings
```

## 总结

本指南提供了在 Windows 系统上启动 freeDiameter 客户端的完整流程。关键要点：

1. **环境准备**：确保网络连通性和必要的依赖库
2. **文件部署**：正确部署可执行文件、配置文件和证书
3. **配置设置**：根据网络环境调整配置文件
4. **启动方法**：使用脚本或命令行启动客户端
5. **验证测试**：确认客户端正常连接到服务端
6. **故障排除**：解决常见问题和网络连接问题

遵循本指南，您应该能够成功在 Windows 系统上启动和运行 freeDiameter 客户端。