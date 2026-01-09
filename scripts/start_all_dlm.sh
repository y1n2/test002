#!/bin/bash
# ============================================================================
# MAGIC DLM 启动脚本
# 启动所有数据链路管理器 (SATCOM, CELLULAR, WIFI)
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_DIR="$(dirname "$SCRIPT_DIR")"

echo "=========================================="
echo "  MAGIC DLM 启动脚本"
echo "=========================================="
echo ""

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then
    echo "[ERROR] 请使用 sudo 运行此脚本"
    exit 1
fi

# 停止现有 DLM 进程
echo "[INFO] 停止现有 DLM 进程..."
pkill -f dlm_satcom_proto 2>/dev/null || true
pkill -f dlm_cellular_proto 2>/dev/null || true
pkill -f dlm_wifi_proto 2>/dev/null || true
sleep 1

# 配置网络接口
echo "[INFO] 配置网络接口..."

# SATCOM - ens37 -> 10.2.2.2
ip addr add 10.2.2.2/24 dev ens37 2>/dev/null || true
ip link set ens37 up
echo "  - ens37: 10.2.2.2/24 (SATCOM)"

# CELLULAR - ens33 -> 10.1.1.1 (已配置)
ip link set ens33 up
echo "  - ens33: 10.1.1.1/24 (CELLULAR)"

# WIFI - ens38 -> 10.3.3.3 (已配置)
ip link set ens38 up
echo "  - ens38: 10.3.3.3/24 (WIFI)"

echo ""

# 启动 SATCOM DLM
echo "[INFO] 启动 SATCOM DLM..."
cd "$BASE_DIR/DLM_SATCOM"
nohup ./dlm_satcom_proto ../DLM_CONFIG/dlm_satcom.ini > dlm_debug.log 2>&1 &
SATCOM_PID=$!
echo "  - SATCOM DLM PID: $SATCOM_PID"

# 启动 CELLULAR DLM
echo "[INFO] 启动 CELLULAR DLM..."
cd "$BASE_DIR/DLM_CELLULAR"
nohup ./dlm_cellular_proto ../DLM_CONFIG/dlm_cellular.ini > dlm_debug.log 2>&1 &
CELLULAR_PID=$!
echo "  - CELLULAR DLM PID: $CELLULAR_PID"

# 启动 WIFI DLM
echo "[INFO] 启动 WIFI DLM..."
cd "$BASE_DIR/DLM_WIFI"
nohup ./dlm_wifi_proto ../DLM_CONFIG/dlm_wifi.ini > dlm_debug.log 2>&1 &
WIFI_PID=$!
echo "  - WIFI DLM PID: $WIFI_PID"

echo ""
echo "[SUCCESS] 所有 DLM 已启动"
echo ""

# 等待并检查
sleep 2
echo "[INFO] 验证 DLM 进程..."
ps aux | grep "dlm_.*_proto" | grep -v grep

echo ""
echo "[INFO] 网络接口状态:"
ip addr show ens37 | grep inet
ip addr show ens33 | grep inet
ip addr show ens38 | grep inet

echo ""
echo "=========================================="
echo "  DLM 启动完成"
echo "=========================================="
