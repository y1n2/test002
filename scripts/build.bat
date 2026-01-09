@echo off
chcp 65001 >nul
echo ==========================================
echo freeDiameter Windows 编译脚本
echo ==========================================

REM 检查 CMake
echo 检查 CMake...
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到 CMake，请先安装 CMake
    echo 下载地址: https://cmake.org/download/
    pause
    exit /b 1
)

REM 显示 CMake 版本
for /f "tokens=3" %%i in ('cmake --version ^| findstr "cmake version"') do set CMAKE_VERSION=%%i
echo [信息] CMake 版本: %CMAKE_VERSION%

REM 检查 Visual Studio
echo 检查 Visual Studio...
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [警告] 未找到 Visual Studio 编译器
    echo 尝试设置 Visual Studio 环境...
    
    REM 尝试找到 Visual Studio
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo [错误] 未找到 Visual Studio，请安装 Visual Studio 2019 或更新版本
        pause
        exit /b 1
    )
)

REM 创建构建目录
echo 创建构建目录...
if not exist build mkdir build
cd build

REM 清理之前的构建
if exist CMakeCache.txt (
    echo 清理之前的构建...
    del CMakeCache.txt
)

REM 生成项目文件
echo 生成项目文件...
cmake .. -G "Visual Studio 16 2019" -A x64
if %errorlevel% neq 0 (
    echo [错误] CMake 配置失败，尝试使用 Visual Studio 2022...
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo [错误] CMake 配置失败
        echo 请检查:
        echo 1. Visual Studio 是否正确安装
        echo 2. CMake 版本是否兼容
        echo 3. 源代码是否完整
        pause
        exit /b 1
    )
)

REM 编译项目
echo ==========================================
echo 开始编译...
echo ==========================================
cmake --build . --config Release --verbose
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    echo 尝试调试模式编译...
    cmake --build . --config Debug
    if %errorlevel% neq 0 (
        echo [错误] 调试模式编译也失败
        pause
        exit /b 1
    )
    echo [信息] 调试模式编译成功
    set BUILD_CONFIG=Debug
) else (
    echo [信息] 发布模式编译成功
    set BUILD_CONFIG=Release
)

REM 复制配置文件
echo 复制配置文件...
if exist "..\config\client.conf" (
    copy "..\config\client.conf" "%BUILD_CONFIG%\"
)
if exist "..\certs" (
    xcopy "..\certs" "%BUILD_CONFIG%\certs\" /E /I /Y
)

echo ==========================================
echo 编译完成！
echo ==========================================
echo 可执行文件位置: build\%BUILD_CONFIG%\diameter_client.exe
echo 配置文件位置: build\%BUILD_CONFIG%\client.conf
echo 证书目录: build\%BUILD_CONFIG%\certs\

echo.
echo 下一步:
echo 1. 确保 WSL Ubuntu 服务端正在运行
echo 2. 运行: diameter_client.exe --conf client.conf
echo.

pause