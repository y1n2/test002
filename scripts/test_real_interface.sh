#!/bin/bash
##############################################################################
# 使用真实网卡测试网卡监控功能
##############################################################################

REAL_IFACE="ens37"  # 你的真实网卡名称
TEST_IFACE="eth_wifi"

echo "=========================================="
echo "   网卡监控功能快速演示"
echo "=========================================="
echo ""

# 1. 检查真实网卡状态
echo "[1] 检查真实网卡 $REAL_IFACE 状态："
if ip link show $REAL_IFACE &>/dev/null; then
    REAL_STATE=$(cat /sys/class/net/$REAL_IFACE/operstate)
    echo "   ✓ $REAL_IFACE 存在，状态: $REAL_STATE"
else
    echo "   ✗ $REAL_IFACE 不存在"
    exit 1
fi

# 2. 检查测试网卡
echo ""
echo "[2] 检查/创建测试网卡 $TEST_IFACE："
if ! ip link show $TEST_IFACE &>/dev/null; then
    echo "   创建虚拟网卡..."
    sudo ip link add $TEST_IFACE type dummy
fi
sudo ip link set $TEST_IFACE up 2>/dev/null
TEST_STATE=$(cat /sys/class/net/$TEST_IFACE/operstate 2>/dev/null || echo "unknown")
echo "   ✓ $TEST_IFACE 状态: $TEST_STATE"

# 3. 清理旧进程
echo ""
echo "[3] 清理旧进程..."
sudo pkill -9 dlm_wifi_proto freeDiameterd 2>/dev/null || true
sleep 2

# 4. 测试网卡状态检查函数
echo ""
echo "[4] 测试网卡状态检查代码："
cat << 'EOF' > /tmp/test_iface_check.c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool check_interface_status(const char *iface) {
  char path[128];
  char status[16];
  FILE *fp;
  
  snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
  fp = fopen(path, "r");
  if (!fp) return false;
  
  if (fgets(status, sizeof(status), fp) == NULL) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  
  status[strcspn(status, "\n")] = 0;
  return (strcmp(status, "up") == 0);
}

int main() {
  printf("检查 ens37: %s\n", check_interface_status("ens37") ? "UP" : "DOWN");
  printf("检查 eth_wifi: %s\n", check_interface_status("eth_wifi") ? "UP" : "DOWN");
  printf("检查 lo: %s\n", check_interface_status("lo") ? "UP" : "DOWN");
  return 0;
}
EOF

gcc -o /tmp/test_iface_check /tmp/test_iface_check.c
/tmp/test_iface_check
echo ""

# 5. 演示 DOWN/UP 检测
echo "[5] 演示网卡 DOWN/UP 检测："
echo ""
echo "   当前 $REAL_IFACE 状态: $(cat /sys/class/net/$REAL_IFACE/operstate)"
echo ""
echo "   执行: sudo ip link set $REAL_IFACE down"
read -p "   按回车继续禁用网卡..." 
sudo ip link set $REAL_IFACE down
sleep 1
echo "   禁用后状态: $(cat /sys/class/net/$REAL_IFACE/operstate)"
echo ""
echo "   执行: sudo ip link set $REAL_IFACE up"
read -p "   按回车继续启用网卡..."
sudo ip link set $REAL_IFACE up
sleep 2
echo "   启用后状态: $(cat /sys/class/net/$REAL_IFACE/operstate)"

echo ""
echo "=========================================="
echo "测试完成！"
echo "=========================================="
echo ""
echo "DLM 配置的网卡是: $TEST_IFACE"
echo "你可以用以下命令测试 DLM 监控："
echo ""
echo "  # 启动 DLM"
echo "  cd /home/zhuwuhui/freeDiameter/DLM_WIFI"
echo "  sudo ./dlm_wifi_proto"
echo ""
echo "  # 在另一个终端禁用网卡"
echo "  sudo ip link set $TEST_IFACE down"
echo "  (等待 5 秒观察 DLM 日志输出 '检测到网卡...状态变为 DOWN')"
echo ""
echo "  # 重新启用网卡"
echo "  sudo ip link set $TEST_IFACE up"
echo "  (等待 5 秒观察 DLM 日志输出 '检测到网卡...状态变为 UP')"
echo ""
