# freeDiameter Windows 编译指南

## 概述
本指南将帮助您在 Windows 系统上编译 freeDiameter 客户端。我们提供了多种编译方法，您可以根据自己的环境选择合适的方式。

## 编译环境要求

### 方法1：使用 Visual Studio（推荐）
- **Visual Studio 2019 或更新版本**
- **CMake 3.10 或更新版本**
- **Git for Windows**（可选）

### 方法2：使用 MinGW-w64
- **MinGW-w64**
- **CMake 3.10 或更新版本**
- **MSYS2**（推荐）

### 方法3：使用 MSYS2
- **MSYS2 环境**
- **GCC 编译器**
- **CMake**

## 编译步骤

### 方法1：Visual Studio 编译

#### 1. 准备环境
```cmd
# 确保已安装 Visual Studio 和 CMake
cmake --version
```

#### 2. 创建构建目录
```cmd
cd D:\freeDiameter_Windows_Source
mkdir build
cd build
```

#### 3. 生成 Visual Studio 项目
```cmd
# 对于 Visual Studio 2019
cmake .. -G "Visual Studio 16 2019" -A x64

# 对于 Visual Studio 2022
cmake .. -G "Visual Studio 17 2022" -A x64
```

#### 4. 编译项目
```cmd
# 使用 CMake 编译
cmake --build . --config Release

# 或者打开 Visual Studio 项目文件编译
start freeDiameter_Client.sln
```

### 方法2：MinGW-w64 编译

#### 1. 安装 MinGW-w64
下载并安装 MinGW-w64：
- 访问 https://www.mingw-w64.org/
- 下载适合的安装包
- 添加到系统 PATH

#### 2. 编译
```cmd
cd D:\freeDiameter_Windows_Source
mkdir build
cd build

# 生成 Makefile
cmake .. -G "MinGW Makefiles"

# 编译
mingw32-make
```

### 方法3：MSYS2 编译

#### 1. 安装 MSYS2
```bash
# 在 MSYS2 终端中安装依赖
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-make
```

#### 2. 编译
```bash
cd /d/freeDiameter_Windows_Source
mkdir build
cd build

cmake .. -G "MSYS Makefiles"
make
```

## 编译脚本

### Windows 批处理脚本（build.bat）
```batch
@echo off
echo freeDiameter Windows 编译脚本

REM 检查 CMake
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到 CMake，请先安装 CMake
    pause
    exit /b 1
)

REM 创建构建目录
if not exist build mkdir build
cd build

REM 生成项目文件
echo 生成项目文件...
cmake .. -G "Visual Studio 16 2019" -A x64
if %errorlevel% neq 0 (
    echo 错误: CMake 配置失败
    pause
    exit /b 1
)

REM 编译项目
echo 开始编译...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo 错误: 编译失败
    pause
    exit /b 1
)

echo 编译成功！
echo 可执行文件位置: build\Release\diameter_client.exe
pause
```

### PowerShell 脚本（build.ps1）
```powershell
# freeDiameter Windows 编译脚本

Write-Host "freeDiameter Windows 编译脚本" -ForegroundColor Green

# 检查 CMake
try {
    $cmakeVersion = cmake --version
    Write-Host "CMake 版本: $($cmakeVersion[0])" -ForegroundColor Blue
} catch {
    Write-Host "错误: 未找到 CMake，请先安装 CMake" -ForegroundColor Red
    Read-Host "按任意键退出"
    exit 1
}

# 创建构建目录
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Name "build"
}
Set-Location "build"

# 生成项目文件
Write-Host "生成项目文件..." -ForegroundColor Yellow
cmake .. -G "Visual Studio 16 2019" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: CMake 配置失败" -ForegroundColor Red
    Read-Host "按任意键退出"
    exit 1
}

# 编译项目
Write-Host "开始编译..." -ForegroundColor Yellow
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 编译失败" -ForegroundColor Red
    Read-Host "按任意键退出"
    exit 1
}

Write-Host "编译成功！" -ForegroundColor Green
Write-Host "可执行文件位置: build\Release\diameter_client.exe" -ForegroundColor Blue
Read-Host "按任意键退出"
```

## 配置文件设置

编译完成后，需要配置客户端：

### 1. 复制配置文件
```cmd
copy config\client.conf build\Release\
copy certs\*.* build\Release\
```

### 2. 修改配置文件
编辑 `client.conf`，设置服务端地址：
```
# 连接到 WSL Ubuntu 服务端
ConnectPeer = "server.example.com" { ConnectTo = "127.0.0.1"; Port = 3868; No_TLS; };
```

## 运行客户端

### 1. 启动服务端
在 WSL Ubuntu 中：
```bash
cd /home/zhuwuhui/freeDiameter/exe
./start_server.sh
```

### 2. 启动客户端
在 Windows 中：
```cmd
cd D:\freeDiameter_Windows_Source\build\Release
diameter_client.exe --conf client.conf
```

## 故障排除

### 常见问题

#### 1. CMake 配置失败
- 确保安装了正确版本的 CMake
- 检查 Visual Studio 是否正确安装
- 确认系统 PATH 设置正确

#### 2. 编译错误
- 检查 Windows SDK 是否安装
- 确认编译器版本兼容性
- 查看详细错误信息

#### 3. 运行时错误
- 确保配置文件路径正确
- 检查网络连接
- 验证证书文件

#### 4. 连接失败
- 确认 WSL Ubuntu 服务端正在运行
- 检查防火墙设置
- 验证端口配置

### 调试技巧

#### 1. 详细编译输出
```cmd
cmake --build . --config Release --verbose
```

#### 2. 调试模式编译
```cmd
cmake --build . --config Debug
```

#### 3. 查看依赖
```cmd
dumpbin /dependents diameter_client.exe
```

## 总结

通过以上步骤，您应该能够成功在 Windows 系统上编译和运行 freeDiameter 客户端。如果遇到问题，请参考故障排除部分或查看详细的错误信息。

### 优势
- ✅ **原生性能**：Windows 原生编译，性能最佳
- ✅ **完全兼容**：支持所有 Windows 特性
- ✅ **易于调试**：可使用 Visual Studio 调试器
- ✅ **灵活配置**：支持多种编译环境

### 文件结构
```
D:\freeDiameter_Windows_Source\
├── src/                    # 源代码
│   ├── client/            # 客户端源代码
│   ├── libfdcore/         # 核心库源代码
│   └── libfdproto/        # 协议库源代码
├── include/               # 头文件
├── config/                # 配置文件
├── certs/                 # 证书文件
├── build/                 # 构建目录
├── docs/                  # 文档
├── CMakeLists.txt         # CMake 配置
├── build.bat              # 批处理编译脚本
└── build.ps1              # PowerShell 编译脚本
```