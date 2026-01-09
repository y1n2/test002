#!/bin/bash
# 检查 MAGIC LMI 服务器状态
# 用于诊断链路资源不可用问题

echo "========================================="
echo "  MAGIC LMI 服务器状态诊断"
echo "========================================="
echo ""

echo "【1】检查 LMI 服务器端口监听"
echo "-------------------------------------"
echo "  预期端口："
echo "    - UDP 1947 (SATCOM)"
echo "    - UDP 1948 (WIFI)"
echo "    - UDP 1949 (CELLULAR)"
echo ""
echo "  实际监听："
sudo ss -unl | grep -E "1947|1948|1949" || echo "    ❌ 无 LMI 端口监听"
echo ""

echo "【2】检查 DLM 进程"
echo "-------------------------------------"
ps aux | grep -E "dlm_.*proto" | grep -v grep | awk '{print "  ✓", $11, "(PID:", $2")"}'
echo ""

echo "【3】检查 freeDiameter 服务器"
echo "-------------------------------------"
FD_PID=$(pgrep -f "freeDiameterd.*fd_server.conf" | head -1)
if [ -n "$FD_PID" ]; then
    echo "  ✓ freeDiameterd 正在运行 (PID: $FD_PID)"
else
    echo "  ❌ freeDiameterd 未运行"
fi
echo ""

echo "【4】检查 UDP 连接跟踪（DLM → LMI）"
echo "-------------------------------------"
sudo conntrack -L 2>/dev/null | grep -E "1947|1948|1949" | head -5 || echo "  ❌ 无 LMI 连接"
echo ""

echo "【5】建议操作"
echo "-------------------------------------"
echo "  如果 LMI 端口未监听，说明："
echo "    1. MAGIC 服务器的 LMI 模块未启动"
echo "    2. 或者配置中禁用了 LMI"
echo ""
echo "  解决方案："
echo "    - 检查服务器日志: journalctl -u freeDiameterd -n 100"
echo "    - 查看 LMI 初始化日志"
echo "    - 确认 app_magic.fdx 是否正确加载"
echo ""
