#!/bin/bash
# freeDiameter WSL Client Startup Script
# 在 WSL 内运行客户端，连接到本地服务端

echo "=========================================="
echo "freeDiameter WSL Client Startup"
echo "=========================================="

# 设置环境变量
CLIENT_HOME="/home/zhuwuhui/freeDiameter/exe"
CONFIG_FILE="/home/zhuwuhui/freeDiameter/scripts/wsl_client.conf"
LOG_DIR="/home/zhuwuhui/freeDiameter/logs"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 检查可执行文件
if [ ! -f "$CLIENT_HOME/diameter_client" ]; then
    echo "ERROR: diameter_client not found in $CLIENT_HOME"
    exit 1
fi

# 检查配置文件
if [ ! -f "$CONFIG_FILE" ]; then
    echo "ERROR: Config file not found: $CONFIG_FILE"
    exit 1
fi

# 检查服务端是否运行
echo "Checking server status..."
if pgrep -f "freeDiameterd" > /dev/null; then
    echo "✓ freeDiameter server is running"
else
    echo "⚠ WARNING: freeDiameter server may not be running"
    echo "Please start server first: cd /home/zhuwuhui/freeDiameter/exe && ./start_server.sh"
fi

# 检查端口
if netstat -ln | grep ":3868" > /dev/null 2>&1; then
    echo "✓ Port 3868 is listening"
else
    echo "⚠ WARNING: Port 3868 is not listening"
fi

echo "=========================================="
echo "Starting freeDiameter client in WSL..."
echo "Config file: $CONFIG_FILE"
echo "Log directory: $LOG_DIR"
echo "Target server: localhost:3868"
echo "=========================================="

# 切换到客户端目录
cd "$CLIENT_HOME"

# 启动客户端
./diameter_client --conf "$CONFIG_FILE"

echo "=========================================="
echo "Client has exited"
echo "Check logs in: $LOG_DIR"
echo "=========================================="