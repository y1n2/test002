#!/bin/bash

# freeDiameter 服务端启动脚本
# 专为 Windows 客户端连接优化
# 作者: freeDiameter 项目组

set -e

echo "=========================================="
echo "freeDiameter 服务端启动"
echo "专为 Windows 客户端连接优化"
echo "=========================================="

# 配置变量
SERVER_DIR="/home/zhuwuhui/freeDiameter/exe"
CONFIG_FILE="$SERVER_DIR/conf/ubuntu_server.conf"
LOG_FILE="$SERVER_DIR/logs/server.log"
PID_FILE="$SERVER_DIR/freediameterd.pid"

# 检查目录和文件
if [ ! -d "$SERVER_DIR" ]; then
    echo "错误: 服务端目录不存在 $SERVER_DIR"
    exit 1
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件不存在 $CONFIG_FILE"
    exit 1
fi

# 创建日志目录
mkdir -p "$SERVER_DIR/logs"

# 检查是否已有服务端运行
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "服务端已在运行 (PID: $PID)"
        echo "如需重启，请先运行: kill $PID"
        exit 0
    else
        echo "清理过期的 PID 文件..."
        rm -f "$PID_FILE"
    fi
fi

# 检查端口占用
PORT_CHECK=$(netstat -tlnp 2>/dev/null | grep ":3868" || true)
if [ -n "$PORT_CHECK" ]; then
    echo "警告: 端口 3868 已被占用"
    echo "$PORT_CHECK"
    echo "请检查是否有其他 freeDiameter 实例在运行"
fi

# 切换到服务端目录
cd "$SERVER_DIR"

echo "启动配置:"
echo "  工作目录: $SERVER_DIR"
echo "  配置文件: $CONFIG_FILE"
echo "  日志文件: $LOG_FILE"
echo "  监听地址: 0.0.0.0:3868"
echo "  客户端连接: 127.0.0.1:3868 (Windows 通过 WSL)"

# 启动服务端
echo ""
echo "启动 freeDiameter 服务端..."
echo "按 Ctrl+C 停止服务端"
echo "=========================================="

# 使用 nohup 在后台启动，但仍显示输出
./freediameterd --conf "$CONFIG_FILE" --pidfile "$PID_FILE" 2>&1 | tee "$LOG_FILE" &

SERVER_PID=$!
echo $SERVER_PID > "$PID_FILE"

echo "服务端已启动 (PID: $SERVER_PID)"
echo "日志文件: $LOG_FILE"
echo ""
echo "现在可以在 Windows 中启动客户端:"
echo "  双击: D:\\freeDiameter\\client\\scripts\\start_client.bat"
echo ""
echo "监控服务端状态:"
echo "  tail -f $LOG_FILE"
echo ""
echo "停止服务端:"
echo "  kill $SERVER_PID"
echo "  或按 Ctrl+C"

# 等待用户中断
trap "echo ''; echo '正在停止服务端...'; kill $SERVER_PID 2>/dev/null; rm -f $PID_FILE; echo '服务端已停止'; exit 0" INT TERM

# 监控服务端进程
while kill -0 $SERVER_PID 2>/dev/null; do
    sleep 1
done

echo "服务端进程已退出"
rm -f "$PID_FILE"