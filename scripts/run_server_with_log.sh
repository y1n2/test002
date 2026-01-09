#!/bin/bash
# 启动服务器并将日志输出到文件，同时在终端显示过滤后的关键信息

cd /home/zhuwuhui/freeDiameter/build

LOG_DIR="/home/zhuwuhui/freeDiameter/logs"
mkdir -p "$LOG_DIR"

LOG_FILE="$LOG_DIR/magic_server_$(date +%Y%m%d_%H%M%S).log"

echo "服务器日志保存到: $LOG_FILE"
echo "终端显示过滤后的关键信息..."
echo "=========================================="

# 启动服务器，全量日志写入文件，过滤后的显示在终端
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf 2>&1 | \
    tee "$LOG_FILE" | \
    grep --line-buffered -E "MAGIC|ERROR|WARNING|MCAR|MSCR|timeout|Link status|Broadcasting|subscribed|✓|⚠|✗"
