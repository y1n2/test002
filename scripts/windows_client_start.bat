@echo off
REM freeDiameter Windows Client Startup Script
REM English version to avoid encoding issues

echo ==========================================
echo freeDiameter Windows Client Startup
echo ==========================================

REM Set environment variables
set CLIENT_HOME=D:\freeDiameter\client
set CONFIG_FILE=%CLIENT_HOME%\conf\client.conf
set LOG_DIR=%CLIENT_HOME%\logs

REM Change to client directory
cd /d "%CLIENT_HOME%\bin"

REM Check if config file exists
if not exist "%CONFIG_FILE%" (
    echo ERROR: Config file not found %CONFIG_FILE%
    pause
    exit /b 1
)

REM Check WSL Ubuntu server connectivity
echo Checking WSL Ubuntu server connectivity...
ping -n 2 127.0.0.1 >nul
if %errorlevel% neq 0 (
    echo WARNING: Cannot connect to localhost
    echo Please ensure WSL Ubuntu server is running
)

REM Check port usage
netstat -an | findstr ":3868" >nul
if %errorlevel% equ 0 (
    echo INFO: Port 3868 is listening (WSL Ubuntu server)
)

REM Start client
echo Starting freeDiameter client...
echo Config file: %CONFIG_FILE%
echo Log directory: %LOG_DIR%
echo Target server: WSL Ubuntu server (127.0.0.1:3868)
echo ==========================================

diameter_client.exe --conf "%CONFIG_FILE%"

echo ==========================================
echo Client has exited
pause