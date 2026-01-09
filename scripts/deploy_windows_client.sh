#!/bin/bash

# freeDiameter Windows 客户端部署脚本
# 从 WSL Ubuntu 环境部署到 Windows D 盘
# 作者: freeDiameter 项目组
# 版本: 1.0

set -e

echo "=========================================="
echo "freeDiameter Windows 客户端部署脚本"
echo "从 WSL Ubuntu 部署到 Windows D 盘"
echo "=========================================="

# 配置变量
WINDOWS_DRIVE="/mnt/d"
DEPLOY_BASE="$WINDOWS_DRIVE/freeDiameter"
CLIENT_DIR="$DEPLOY_BASE/client"
SOURCE_DIR="/home/zhuwuhui/freeDiameter"

# 检查 Windows D 盘是否可访问
if [ ! -d "$WINDOWS_DRIVE" ]; then
    echo "错误: 无法访问 Windows D 盘 ($WINDOWS_DRIVE)"
    echo "请确保 WSL 已正确挂载 Windows 驱动器"
    exit 1
fi

echo "✓ Windows D 盘访问正常: $WINDOWS_DRIVE"

# 创建目录结构
echo "创建 Windows 客户端目录结构..."
mkdir -p "$CLIENT_DIR"/{bin,conf,certs,logs,scripts}

echo "✓ 目录结构创建完成:"
echo "  - $CLIENT_DIR/bin"
echo "  - $CLIENT_DIR/conf"
echo "  - $CLIENT_DIR/certs"
echo "  - $CLIENT_DIR/logs"
echo "  - $CLIENT_DIR/scripts"

# 复制可执行文件
echo "复制可执行文件..."
if [ -f "$SOURCE_DIR/exe/diameter_client" ]; then
    cp "$SOURCE_DIR/exe/diameter_client" "$CLIENT_DIR/bin/diameter_client.exe"
    echo "✓ diameter_client.exe"
else
    echo "警告: diameter_client 不存在，请先编译"
fi

# 复制动态库
echo "复制动态库..."
for lib in libfdcore.so.7 libfdproto.so.7; do
    if [ -f "$SOURCE_DIR/exe/$lib" ]; then
        # 将 .so 文件重命名为 .dll 以适应 Windows
        dll_name=$(echo $lib | sed 's/\.so\.7/.dll/')
        cp "$SOURCE_DIR/exe/$lib" "$CLIENT_DIR/bin/$dll_name"
        echo "✓ $dll_name"
    else
        echo "警告: $lib 不存在"
    fi
done

# 复制扩展文件
echo "复制扩展文件..."
for ext in dict_arinc839.fdx dict_nasreq.fdx dict_eap.fdx; do
    if [ -f "$SOURCE_DIR/exe/$ext" ]; then
        cp "$SOURCE_DIR/exe/$ext" "$CLIENT_DIR/bin/"
        echo "✓ $ext"
    else
        echo "警告: $ext 不存在"
    fi
done

# 创建 Windows 客户端配置文件
echo "创建 Windows 客户端配置文件..."
cat > "$CLIENT_DIR/conf/client.conf" << 'EOF'
# freeDiameter Windows 客户端配置文件
# WSL Ubuntu 部署版本

# 客户端身份标识
Identity = "aircraft_001@freediameter.local";
Realm = "freediameter.local";

# 禁用服务器功能（纯客户端模式）
NoRelay;
NoAdvertiseRelay;

# 网络配置
ListenOn = "0.0.0.0";  # 监听所有接口
Port = 3868;           # 客户端监听端口
No_IPv6;               # 使用IPv4
No_SCTP;               # Windows 使用TCP协议

# 加载ARINC-839字典扩展
LoadExtension = "bin/dict_arinc839.fdx";

# 连接到WSL Ubuntu服务端
ConnectPeer = "server.freediameter.local" {
    ConnectTo = "127.0.0.1";     # WSL Ubuntu 服务端（本地回环）
    Port = 3868;
    No_TLS;                      # 初始部署不使用TLS
    Prefer_TCP;
    TcpNoDelay;
};

# 应用线程配置
AppServThreads = 4;

# 日志配置
# LogLevel = "INFO";
# LogFile = "logs/client.log";
EOF

echo "✓ client.conf 创建完成"

# 创建简化配置文件
cat > "$CLIENT_DIR/conf/client_simple.conf" << 'EOF'
# freeDiameter Windows 客户端简化配置

Identity = "client1.arinc839.local";
Realm = "arinc839.local";

NoRelay;
Port = 3868;
No_IPv6;
No_SCTP;

LoadExtension = "bin/dict_arinc839.fdx";

ConnectPeer = "server.arinc839.local" {
    ConnectTo = "127.0.0.1";
    Port = 3868;
    No_TLS;
    Prefer_TCP;
};

AppServThreads = 2;
EOF

echo "✓ client_simple.conf 创建完成"

# 复制证书文件（如果存在）
echo "复制证书文件..."
if [ -d "$SOURCE_DIR/exe/certs" ]; then
    cp -r "$SOURCE_DIR/exe/certs/"* "$CLIENT_DIR/certs/" 2>/dev/null || true
    echo "✓ 证书文件复制完成"
else
    echo "警告: 证书目录不存在，跳过证书复制"
fi

# 创建 Windows 批处理启动脚本
echo "创建 Windows 启动脚本..."
cat > "$CLIENT_DIR/scripts/start_client.bat" << 'EOF'
@echo off
REM freeDiameter Windows 客户端启动脚本
REM WSL Ubuntu 部署版本

echo ==========================================
echo freeDiameter Windows 客户端启动
echo WSL Ubuntu 部署版本
echo ==========================================

REM 设置环境变量
set CLIENT_HOME=D:\freeDiameter\client
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

REM 检查 WSL Ubuntu 服务端连通性
echo 检查 WSL Ubuntu 服务端连通性...
ping -n 2 127.0.0.1 >nul
if %errorlevel% neq 0 (
    echo 警告: 无法连接到本地回环地址
    echo 请确保 WSL Ubuntu 服务端正在运行
)

REM 检查端口占用
netstat -an | findstr ":3868" >nul
if %errorlevel% equ 0 (
    echo 信息: 端口 3868 已有服务在监听（可能是 WSL Ubuntu 服务端）
)

REM 启动客户端
echo 启动 freeDiameter 客户端...
echo 配置文件: %CONFIG_FILE%
echo 日志目录: %LOG_DIR%
echo 连接目标: WSL Ubuntu 服务端 (127.0.0.1:3868)
echo ==========================================

diameter_client.exe --conf "%CONFIG_FILE%"

echo ==========================================
echo 客户端已退出
pause
EOF

echo "✓ start_client.bat 创建完成"

# 创建 PowerShell 启动脚本
cat > "$CLIENT_DIR/scripts/start_client.ps1" << 'EOF'
# freeDiameter Windows 客户端 PowerShell 启动脚本
# WSL Ubuntu 部署版本

param(
    [switch]$Debug,
    [string]$ConfigFile = "D:\freeDiameter\client\conf\client.conf"
)

Write-Host "==========================================" -ForegroundColor Green
Write-Host "freeDiameter Windows 客户端启动" -ForegroundColor Green
Write-Host "WSL Ubuntu 部署版本" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green

# 设置变量
$ClientHome = "D:\freeDiameter\client"
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

# WSL Ubuntu 服务端连通性检查
Write-Host "检查 WSL Ubuntu 服务端连通性..." -ForegroundColor Yellow
$serverReachable = Test-Connection -ComputerName "127.0.0.1" -Count 2 -Quiet

if ($serverReachable) {
    Write-Host "✓ WSL Ubuntu 服务端连通性正常" -ForegroundColor Green
} else {
    Write-Host "✗ WSL Ubuntu 服务端连通性异常" -ForegroundColor Red
    Write-Host "请确保 WSL Ubuntu 中的 freeDiameter 服务端正在运行" -ForegroundColor Yellow
}

# 检查端口
$portInUse = Get-NetTCPConnection -LocalPort 3868 -ErrorAction SilentlyContinue
if ($portInUse) {
    Write-Host "✓ 端口 3868 已有服务监听（WSL Ubuntu 服务端）" -ForegroundColor Green
} else {
    Write-Host "⚠ 端口 3868 未被监听，请检查 WSL Ubuntu 服务端状态" -ForegroundColor Yellow
}

# 切换到客户端目录
Set-Location $BinDir

# 启动客户端
Write-Host "启动 freeDiameter 客户端..." -ForegroundColor Yellow
Write-Host "配置文件: $ConfigFile" -ForegroundColor Cyan
Write-Host "工作目录: $BinDir" -ForegroundColor Cyan
Write-Host "连接目标: WSL Ubuntu 服务端 (127.0.0.1:3868)" -ForegroundColor Cyan

if ($Debug) {
    Write-Host "调试模式启动..." -ForegroundColor Yellow
    & ".\diameter_client.exe" --conf $ConfigFile --debug
} else {
    & ".\diameter_client.exe" --conf $ConfigFile
}

Write-Host "客户端已退出" -ForegroundColor Yellow
EOF

echo "✓ start_client.ps1 创建完成"

# 创建快速启动脚本
cat > "$CLIENT_DIR/scripts/quick_start.bat" << 'EOF'
@echo off
REM 快速启动脚本 - 使用简化配置

cd /d "D:\freeDiameter\client\bin"
echo 快速启动 freeDiameter 客户端...
echo 使用简化配置文件
diameter_client.exe --conf "..\conf\client_simple.conf"
pause
EOF

echo "✓ quick_start.bat 创建完成"

# 创建部署信息文件
cat > "$CLIENT_DIR/DEPLOYMENT_INFO.txt" << EOF
freeDiameter Windows 客户端部署信息
=====================================

部署时间: $(date)
部署来源: WSL Ubuntu ($SOURCE_DIR)
部署目标: Windows D 盘 ($CLIENT_DIR)

目录结构:
- bin/           可执行文件和库文件
- conf/          配置文件
- certs/         证书文件
- logs/          日志文件
- scripts/       启动脚本

主要文件:
- bin/diameter_client.exe     客户端主程序
- bin/libfdcore.dll          核心库
- bin/libfdproto.dll         协议库
- bin/dict_arinc839.fdx      ARINC-839 字典
- conf/client.conf           主配置文件
- conf/client_simple.conf    简化配置文件

启动方法:
1. 双击 scripts/start_client.bat
2. 运行 scripts/start_client.ps1
3. 快速启动: scripts/quick_start.bat

注意事项:
- 确保 WSL Ubuntu 中的 freeDiameter 服务端正在运行
- 客户端连接到 127.0.0.1:3868 (WSL Ubuntu 服务端)
- 日志文件保存在 logs/ 目录下

故障排除:
1. 检查 WSL Ubuntu 服务端是否运行: ps aux | grep freeDiameterd
2. 检查端口监听: netstat -tlnp | grep 3868
3. 查看客户端日志: logs/client.log
EOF

echo "✓ DEPLOYMENT_INFO.txt 创建完成"

# 创建 README 文件
cat > "$CLIENT_DIR/README.md" << 'EOF'
# freeDiameter Windows 客户端

## 快速开始

### 1. 启动 WSL Ubuntu 服务端
```bash
# 在 WSL Ubuntu 中
cd /home/zhuwuhui/freeDiameter/exe
./start_server.sh
```

### 2. 启动 Windows 客户端
```batch
# 方法一：双击启动
D:\freeDiameter\client\scripts\start_client.bat

# 方法二：命令行启动
cd D:\freeDiameter\client\bin
diameter_client.exe --conf ..\conf\client.conf
```

## 配置说明

- **主配置**: `conf/client.conf` - 完整功能配置
- **简化配置**: `conf/client_simple.conf` - 基础功能配置

## 连接信息

- **服务端地址**: 127.0.0.1:3868 (WSL Ubuntu)
- **客户端监听**: 0.0.0.0:3868
- **协议**: TCP (No_SCTP)

## 故障排除

1. **无法连接服务端**
   - 检查 WSL Ubuntu 服务端是否运行
   - 确认端口 3868 未被其他程序占用

2. **客户端启动失败**
   - 检查配置文件语法
   - 查看日志文件 `logs/client.log`

3. **依赖库缺失**
   - 确保所有 .dll 文件在 bin/ 目录下
   - 安装 Visual C++ Redistributable

## 日志查看

```powershell
# 查看最新日志
Get-Content D:\freeDiameter\client\logs\client.log -Tail 20

# 实时监控日志
Get-Content D:\freeDiameter\client\logs\client.log -Wait
```
EOF

echo "✓ README.md 创建完成"

# 设置 Windows 文件权限（如果可能）
echo "设置文件权限..."
chmod +x "$CLIENT_DIR/scripts/"*.bat 2>/dev/null || true
chmod +x "$CLIENT_DIR/scripts/"*.ps1 2>/dev/null || true
chmod +x "$CLIENT_DIR/bin/"*.exe 2>/dev/null || true

# 创建桌面快捷方式脚本
cat > "$CLIENT_DIR/scripts/create_desktop_shortcut.bat" << 'EOF'
@echo off
REM 创建桌面快捷方式

set DESKTOP=%USERPROFILE%\Desktop
set SHORTCUT_NAME=freeDiameter Client
set TARGET_PATH=D:\freeDiameter\client\scripts\start_client.bat

echo 创建桌面快捷方式...

powershell -Command "& {$WshShell = New-Object -comObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%DESKTOP%\%SHORTCUT_NAME%.lnk'); $Shortcut.TargetPath = '%TARGET_PATH%'; $Shortcut.WorkingDirectory = 'D:\freeDiameter\client\scripts'; $Shortcut.Description = 'freeDiameter Windows Client'; $Shortcut.Save()}"

if exist "%DESKTOP%\%SHORTCUT_NAME%.lnk" (
    echo ✓ 桌面快捷方式创建成功
) else (
    echo ✗ 桌面快捷方式创建失败
)

pause
EOF

echo "✓ create_desktop_shortcut.bat 创建完成"

# 显示部署摘要
echo ""
echo "=========================================="
echo "部署完成摘要"
echo "=========================================="
echo "部署目录: $CLIENT_DIR"
echo ""
echo "已创建的文件:"
echo "  可执行文件:"
echo "    - bin/diameter_client.exe"
echo "    - bin/libfdcore.dll"
echo "    - bin/libfdproto.dll"
echo "    - bin/dict_arinc839.fdx"
echo ""
echo "  配置文件:"
echo "    - conf/client.conf"
echo "    - conf/client_simple.conf"
echo ""
echo "  启动脚本:"
echo "    - scripts/start_client.bat"
echo "    - scripts/start_client.ps1"
echo "    - scripts/quick_start.bat"
echo "    - scripts/create_desktop_shortcut.bat"
echo ""
echo "  文档:"
echo "    - README.md"
echo "    - DEPLOYMENT_INFO.txt"
echo ""
echo "下一步操作:"
echo "1. 在 WSL Ubuntu 中启动服务端:"
echo "   cd /home/zhuwuhui/freeDiameter/exe && ./start_server.sh"
echo ""
echo "2. 在 Windows 中启动客户端:"
echo "   双击 D:\\freeDiameter\\client\\scripts\\start_client.bat"
echo ""
echo "3. 创建桌面快捷方式（可选）:"
echo "   双击 D:\\freeDiameter\\client\\scripts\\create_desktop_shortcut.bat"
echo ""
echo "=========================================="
echo "部署脚本执行完成！"
echo "=========================================="