# freeDiameter Windows PowerShell 编译脚本

Write-Host "==========================================" -ForegroundColor Green
Write-Host "freeDiameter Windows 编译脚本" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green

# 检查 CMake
Write-Host "检查 CMake..." -ForegroundColor Yellow
try {
    $cmakeVersion = cmake --version 2>$null
    if ($LASTEXITCODE -eq 0) {
        $version = ($cmakeVersion[0] -split " ")[2]
        Write-Host "[信息] CMake 版本: $version" -ForegroundColor Blue
    } else {
        throw "CMake not found"
    }
} catch {
    Write-Host "[错误] 未找到 CMake，请先安装 CMake" -ForegroundColor Red
    Write-Host "下载地址: https://cmake.org/download/" -ForegroundColor Yellow
    Read-Host "按任意键退出"
    exit 1
}

# 检查 Visual Studio
Write-Host "检查 Visual Studio..." -ForegroundColor Yellow
try {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstances = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstances) {
            Write-Host "[信息] 找到 Visual Studio: $vsInstances" -ForegroundColor Blue
            
            # 设置 Visual Studio 环境
            $vcvarsPath = Join-Path $vsInstances "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvarsPath) {
                Write-Host "[信息] 设置 Visual Studio 环境..." -ForegroundColor Blue
                cmd /c "`"$vcvarsPath`" && set" | ForEach-Object {
                    if ($_ -match "^(.*?)=(.*)$") {
                        Set-Item "env:\$($matches[1])" $matches[2]
                    }
                }
            }
        } else {
            throw "Visual Studio not found"
        }
    } else {
        throw "vswhere.exe not found"
    }
} catch {
    Write-Host "[警告] 未找到 Visual Studio，请确保已安装 Visual Studio 2019 或更新版本" -ForegroundColor Yellow
}

# 创建构建目录
Write-Host "创建构建目录..." -ForegroundColor Yellow
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Name "build" | Out-Null
}
Set-Location "build"

# 清理之前的构建
if (Test-Path "CMakeCache.txt") {
    Write-Host "清理之前的构建..." -ForegroundColor Yellow
    Remove-Item "CMakeCache.txt" -Force
}

# 生成项目文件
Write-Host "生成项目文件..." -ForegroundColor Yellow
$generators = @(
    "Visual Studio 17 2022",
    "Visual Studio 16 2019",
    "MinGW Makefiles"
)

$success = $false
foreach ($generator in $generators) {
    Write-Host "尝试生成器: $generator" -ForegroundColor Cyan
    cmake .. -G $generator -A x64 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[成功] 使用生成器: $generator" -ForegroundColor Green
        $success = $true
        $selectedGenerator = $generator
        break
    }
}

if (!$success) {
    Write-Host "[错误] CMake 配置失败" -ForegroundColor Red
    Write-Host "请检查:" -ForegroundColor Yellow
    Write-Host "1. Visual Studio 是否正确安装" -ForegroundColor Yellow
    Write-Host "2. CMake 版本是否兼容" -ForegroundColor Yellow
    Write-Host "3. 源代码是否完整" -ForegroundColor Yellow
    Read-Host "按任意键退出"
    exit 1
}

# 编译项目
Write-Host "==========================================" -ForegroundColor Green
Write-Host "开始编译..." -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green

$buildConfigs = @("Release", "Debug")
$buildSuccess = $false

foreach ($config in $buildConfigs) {
    Write-Host "尝试 $config 模式编译..." -ForegroundColor Cyan
    cmake --build . --config $config --verbose
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[成功] $config 模式编译成功" -ForegroundColor Green
        $buildConfig = $config
        $buildSuccess = $true
        break
    } else {
        Write-Host "[失败] $config 模式编译失败" -ForegroundColor Red
    }
}

if (!$buildSuccess) {
    Write-Host "[错误] 所有编译模式都失败" -ForegroundColor Red
    Read-Host "按任意键退出"
    exit 1
}

# 复制配置文件
Write-Host "复制配置文件..." -ForegroundColor Yellow
if (Test-Path "..\config\client.conf") {
    Copy-Item "..\config\client.conf" "$buildConfig\" -Force
    Write-Host "[信息] 已复制 client.conf" -ForegroundColor Blue
}

if (Test-Path "..\certs") {
    if (!(Test-Path "$buildConfig\certs")) {
        New-Item -ItemType Directory -Path "$buildConfig\certs" | Out-Null
    }
    Copy-Item "..\certs\*" "$buildConfig\certs\" -Recurse -Force
    Write-Host "[信息] 已复制证书文件" -ForegroundColor Blue
}

# 显示结果
Write-Host "==========================================" -ForegroundColor Green
Write-Host "编译完成！" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green

$exePath = "build\$buildConfig\diameter_client.exe"
$confPath = "build\$buildConfig\client.conf"
$certsPath = "build\$buildConfig\certs\"

Write-Host "可执行文件位置: $exePath" -ForegroundColor Blue
Write-Host "配置文件位置: $confPath" -ForegroundColor Blue
Write-Host "证书目录: $certsPath" -ForegroundColor Blue

Write-Host ""
Write-Host "下一步:" -ForegroundColor Yellow
Write-Host "1. 确保 WSL Ubuntu 服务端正在运行" -ForegroundColor Yellow
Write-Host "2. 运行: diameter_client.exe --conf client.conf" -ForegroundColor Yellow
Write-Host ""

# 检查可执行文件
if (Test-Path "$buildConfig\diameter_client.exe") {
    $fileInfo = Get-Item "$buildConfig\diameter_client.exe"
    Write-Host "文件大小: $([math]::Round($fileInfo.Length / 1KB, 2)) KB" -ForegroundColor Blue
    Write-Host "创建时间: $($fileInfo.CreationTime)" -ForegroundColor Blue
} else {
    Write-Host "[警告] 未找到可执行文件" -ForegroundColor Red
}

Read-Host "按任意键退出"