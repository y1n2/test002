#!/bin/bash
# MAGIC Core 状态查询工具

echo "=========================================="
echo "  MAGIC Core 状态监控"
echo "=========================================="
echo ""

# 1. 检查进程
echo "📊 进程状态:"
MAGIC_PID=$(ps aux | grep magic_core_main | grep -v grep | awk '{print $2}')
if [ -n "$MAGIC_PID" ]; then
    echo "  ✓ MAGIC Core 运行中 (PID: $MAGIC_PID)"
else
    echo "  ✗ MAGIC Core 未运行"
    exit 1
fi

CM_PID=$(ps aux | grep cm_core | grep -v grep | awk '{print $2}')
if [ -n "$CM_PID" ]; then
    echo "  ✓ CM Core 运行中 (PID: $CM_PID)"
fi

echo ""

# 2. 检查 Socket 连接
echo "🔌 Socket 连接:"
if [ -S /tmp/magic_core.sock ]; then
    echo "  ✓ Socket 存在: /tmp/magic_core.sock"
    
    # 统计连接数
    CONN_COUNT=$(netstat -an | grep /tmp/magic_core.sock | grep CONNECTED | wc -l)
    echo "  ✓ 活动连接数: $CONN_COUNT"
else
    echo "  ✗ Socket 不存在"
fi

echo ""

# 3. 检查 DLM 进程
echo "📡 DLM 进程:"
DLM_COUNT=0

if ps aux | grep dlm_satcom | grep -v grep > /dev/null; then
    echo "  ✓ Satcom DLM 运行中"
    ((DLM_COUNT++))
fi

if ps aux | grep dlm_cellular | grep -v grep > /dev/null; then
    echo "  ✓ Cellular DLM 运行中"
    ((DLM_COUNT++))
fi

if ps aux | grep dlm_wifi | grep -v grep > /dev/null; then
    echo "  ✓ WiFi DLM 运行中"
    ((DLM_COUNT++))
fi

echo "  总计: $DLM_COUNT 个 DLM"
echo ""

# 4. 检查 freeDiameter 连接
echo "🌐 freeDiameter 状态:"
FD_PID=$(ps aux | grep freeDiameterd | grep -v grep | awk '{print $2}')
if [ -n "$FD_PID" ]; then
    echo "  ✓ freeDiameter 运行中 (PID: $FD_PID)"
    
    # 检查端口监听
    if netstat -tuln | grep :5870 > /dev/null; then
        echo "  ✓ 监听端口 5870"
    fi
    
    # 检查客户端连接
    CLIENT_COUNT=$(netstat -tn | grep :5870 | grep ESTABLISHED | wc -l)
    echo "  ✓ 客户端连接数: $CLIENT_COUNT"
else
    echo "  ✗ freeDiameter 未运行"
fi

echo ""

# 5. 查看 MAGIC Core 日志 (最后10行)
echo "📝 MAGIC Core 日志 (最后10行):"
echo "----------------------------------------"
if [ -f /home/zhuwuhui/freeDiameter/logs/magic_core.log ]; then
    tail -10 /home/zhuwuhui/freeDiameter/logs/magic_core.log
elif [ -f /home/zhuwuhui/freeDiameter/magic_server/build/magic_core.log ]; then
    tail -10 /home/zhuwuhui/freeDiameter/magic_server/build/magic_core.log
else
    # 如果没有日志文件，尝试从终端输出查看
    echo "  (无日志文件，MAGIC Core 可能输出到终端)"
fi
echo "----------------------------------------"
echo ""

# 6. 系统资源使用
echo "💻 资源使用:"
if [ -n "$MAGIC_PID" ]; then
    ps -p $MAGIC_PID -o pid,ppid,%cpu,%mem,vsz,rss,cmd | tail -1 | \
        awk '{printf "  CPU: %s%%, MEM: %s%%, VSZ: %s KB, RSS: %s KB\n", $3, $4, $5, $6}'
fi

echo ""
echo "=========================================="
echo "  提示:"
echo "  - 查看实时日志: tail -f logs/magic_core.log"
echo "  - 查看 DLM 状态: 在 MAGIC Core 终端查看输出"
echo "  - 查看客户端会话: 在 freeDiameter 日志中搜索 'Session-Id'"
echo "=========================================="
