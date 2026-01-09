@echo off
REM Simple freeDiameter Windows Client Startup

echo Starting freeDiameter Client...

cd /d "D:\freeDiameter\client\bin"

if not exist "diameter_client.exe" (
    echo ERROR: diameter_client.exe not found
    pause
    exit /b 1
)

if not exist "..\conf\client.conf" (
    echo ERROR: client.conf not found
    pause
    exit /b 1
)

echo Connecting to WSL Ubuntu server (127.0.0.1:3868)...
diameter_client.exe --conf "..\conf\client.conf"

echo Client stopped.
pause