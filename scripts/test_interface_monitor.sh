#!/bin/bash
##############################################################################
# 测试网卡状态监控功能
# 
# 功能说明：
# 1. 启动服务器和 DLM_WIFI
# 2. 等待链路建立
# 3. 禁用网卡 (ip link set down)
# 4. 观察 DLM 是否发送 Link_Down
# 5. 观察服务器是否触发 MSCR 广播
# 6. 重新启用网卡
# 7. 观察链路自动恢复
##############################################################################

set -e

IFACE="eth_wifi"
SERVER_LOG="/tmp/server_iface_test.log"
DLM_LOG="/tmp/dlm_wifi_test.log"

echo "=========================================="
echo "   网卡状态监控功能测试"
echo "=========================================="
echo ""

# 清理旧进程
echo "[1/7] 清理旧进程..."
sudo pkill -9 freeDiameterd dlm_wifi_proto 2>/dev/null || true
sleep 2

# 确保网卡处于 UP 状态
echo "[2/7] 确保网卡 $IFACE 处于 UP 状态..."
if ip link show $IFACE &>/dev/null; then
    sudo ip link set $IFACE up
    echo "   ✓ 网卡 $IFACE 已启用"
else
    echo "   ✗ 错误: 网卡 $IFACE 不存在"
    echo "   提示: 请先创建虚拟网卡: sudo ip link add $IFACE type dummy"
    exit 1
fi

# 启动服务器
echo "[3/7] 启动 freeDiameter 服务器..."
cd /home/zhuwuhui/freeDiameter/build
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf > $SERVER_LOG 2>&1 &
SERVER_PID=$!
echo "   服务器 PID: $SERVER_PID"
sleep 3

# 启动 DLM_WIFI
echo "[4/7] 启动 DLM_WIFI..."
cd /home/zhuwuhui/freeDiameter/DLM_WIFI
sudo ./dlm_wifi_proto > $DLM_LOG 2>&1 &
DLM_PID=$!
echo "   DLM PID: $DLM_PID"
sleep 5

# 检查链路状态
echo "[5/7] 检查链路是否已建立..."
if grep -q "Link_Up.indication" $DLM_LOG; then
    echo "   ✓ DLM 已发送 Link_Up"
else
    echo "   ✗ DLM 未发送 Link_Up，查看日志:"
    tail -20 $DLM_LOG
fi

echo ""
echo "=========================================="
echo "现在禁用网卡 $IFACE..."
echo "=========================================="
sudo ip link set $IFACE down
echo "   ✓ 网卡已禁用"

echo ""
echo "[6/7] 等待 15 秒观察 DLM 响应..."
echo "   (DLM 每 5 秒检查一次网卡状态)"
for i in {15..1}; do
    echo -ne "   倒计时: $i 秒\r"
    sleep 1
done
echo ""

# 检查 Link_Down 是否发送
echo "[7/7] 检查 DLM 是否检测到网卡 DOWN..."
if grep -q "检测到网卡.*状态变为 DOWN" $DLM_LOG; then
    echo "   ✓ DLM 成功检测到网卡 DOWN"
    if grep -q "Link_Down.indication" $DLM_LOG; then
        echo "   ✓ DLM 已发送 Link_Down"
    else
        echo "   ✗ DLM 未发送 Link_Down"
    fi
else
    echo "   ✗ DLM 未检测到网卡 DOWN"
fi

echo ""
echo "=========================================="
echo "DLM 日志 (最后 30 行):"
echo "=========================================="
tail -30 $DLM_LOG

echo ""
echo "=========================================="
echo "服务器日志 (MAGIC 相关):"
echo "=========================================="
grep -E "(Link status|MSCR|Broadcasting)" $SERVER_LOG | tail -20 || echo "未找到相关日志"

echo ""
echo "=========================================="
echo "现在重新启用网卡 $IFACE..."
echo "=========================================="
sudo ip link set $IFACE up
echo "   ✓ 网卡已重新启用"

echo ""
echo "等待 15 秒观察链路恢复..."
for i in {15..1}; do
    echo -ne "   倒计时: $i 秒\r"
    sleep 1
done
echo ""

echo ""
echo "=========================================="
echo "最终 DLM 日志:"
echo "=========================================="
tail -30 $DLM_LOG

echo ""
echo "=========================================="
echo "测试完成！"
echo "=========================================="
echo "完整日志位置:"
echo "  - 服务器: $SERVER_LOG"
echo "  - DLM:    $DLM_LOG"
echo ""
echo "清理进程请执行:"
echo "  sudo kill $SERVER_PID $DLM_PID"
