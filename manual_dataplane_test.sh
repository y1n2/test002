#!/bin/bash
# 手动添加数据平面规则来测试架构是否正确
# 密码：@luoye998

SUDO_PASS="@luoye998"

echo "========================================="
echo "  手动测试数据平面规则"
echo "========================================="
echo ""

# 客户端信息
CLIENT_IP="192.168.126.5"

# efb-01 配置
EFB_DEST="10.1.1.4"
EFB_FWMARK=100
EFB_TABLE=100
EFB_INTERFACE="ens33"  # SATCOM

# omt-01 配置  
OMT_DEST="10.2.2.8"
OMT_FWMARK=101
OMT_TABLE=101
OMT_INTERFACE="ens38"  # WIFI

# ife-01 配置
IFE_DEST="10.3.3.6"
IFE_FWMARK=102
IFE_TABLE=102
IFE_INTERFACE="ens35"  # CELLULAR

echo "【步骤 1】清理现有规则"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S iptables -t mangle -F PREROUTING
echo $SUDO_PASS | sudo -S iptables -F FORWARD
echo $SUDO_PASS | sudo -S conntrack -D -s $CLIENT_IP 2>/dev/null || true
echo "✓ 清理完成"
echo ""

echo "【步骤 2】设置连接跟踪规则"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S iptables -I FORWARD 1 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
echo "✓ 连接跟踪规则已设置"
echo ""

echo "【步骤 3】添加 efb-01 规则 ($CLIENT_IP → $EFB_DEST)"
echo "-------------------------------------"
# mangle 表打标
echo $SUDO_PASS | sudo -S iptables -t mangle -A PREROUTING -s $CLIENT_IP -d $EFB_DEST -j MARK --set-mark $EFB_FWMARK
echo "  ✓ mangle: 标记为 $EFB_FWMARK"

# filter 表放行
echo $SUDO_PASS | sudo -S iptables -A FORWARD -s $CLIENT_IP -d $EFB_DEST -j ACCEPT
echo "  ✓ filter: 允许转发"
echo ""

echo "【步骤 4】添加 omt-01 规则 ($CLIENT_IP → $OMT_DEST)"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S iptables -t mangle -A PREROUTING -s $CLIENT_IP -d $OMT_DEST -j MARK --set-mark $OMT_FWMARK
echo $SUDO_PASS | sudo -S iptables -A FORWARD -s $CLIENT_IP -d $OMT_DEST -j ACCEPT
echo "  ✓ 规则已添加"
echo ""

echo "【步骤 5】添加 ife-01 规则 ($CLIENT_IP → $IFE_DEST)"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S iptables -t mangle -A PREROUTING -s $CLIENT_IP -d $IFE_DEST -j MARK --set-mark $IFE_FWMARK
echo $SUDO_PASS | sudo -S iptables -A FORWARD -s $CLIENT_IP -d $IFE_DEST -j ACCEPT
echo "  ✓ 规则已添加"
echo ""

echo "【步骤 6】添加默认阻断规则（放在最后）"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S iptables -A FORWARD -s 192.168.126.0/24 -j DROP
echo "  ✓ 默认阻断规则已添加"
echo ""

echo "【步骤 7】启用 IP 转发"
echo "-------------------------------------"
echo $SUDO_PASS | sudo -S sysctl -w net.ipv4.ip_forward=1
echo "  ✓ IP 转发已启用"
echo ""

echo "========================================="
echo "  规则配置完成"
echo "========================================="
echo ""

echo "【验证】当前 iptables 规则："
echo ""
echo "=== mangle 表 ===" 
echo $SUDO_PASS | sudo -S iptables -t mangle -L PREROUTING -n -v
echo ""
echo "=== filter FORWARD 链 ==="
echo $SUDO_PASS | sudo -S iptables -L FORWARD -n -v
echo ""

echo "【下一步】从客户端机器测试："
echo "  ping -c 3 10.1.1.4  # 测试 efb-01"
echo "  ping -c 3 10.2.2.8  # 测试 omt-01"
echo "  ping -c 3 10.3.3.6  # 测试 ife-01"
