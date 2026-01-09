#!/bin/bash
# ============================================================================
# MAGIC DLM 停止脚本
# 停止所有数据链路管理器 (SATCOM, CELLULAR, WIFI)
# ============================================================================

echo "=========================================="
echo "  MAGIC DLM 停止脚本"
echo "=========================================="
echo ""

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then
    echo "[ERROR] 请使用 sudo 运行此脚本"
    exit 1
fi

# 停止 DLM 进程
echo "[INFO] 停止 DLM 进程..."

if pkill -f dlm_satcom_proto 2>/dev/null; then
    echo "  - SATCOM DLM 已停止"
else
    echo "  - SATCOM DLM 未运行"
fi

if pkill -f dlm_cellular_proto 2>/dev/null; then
    echo "  - CELLULAR DLM 已停止"
else
    echo "  - CELLULAR DLM 未运行"
fi

if pkill -f dlm_wifi_proto 2>/dev/null; then
    echo "  - WIFI DLM 已停止"
else
    echo "  - WIFI DLM 未运行"
fi

sleep 1

# 验证
echo ""
echo "[INFO] 验证进程已停止..."
REMAINING=$(ps aux | grep "dlm_.*_proto" | grep -v grep | wc -l)
if [ "$REMAINING" -eq 0 ]; then
    echo "[SUCCESS] 所有 DLM 已停止"
else
    echo "[WARN] 仍有 $REMAINING 个 DLM 进程运行"
    ps aux | grep "dlm_.*_proto" | grep -v grep
fi

echo ""
echo "=========================================="
echo "  DLM 停止完成"
echo "=========================================="
