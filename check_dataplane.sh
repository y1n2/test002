#!/bin/bash
# MAGIC 数据平面诊断脚本
# 用于排查客户端 ping 失败问题

echo "========================================="
echo "  MAGIC 数据平面诊断报告"
echo "========================================="
echo ""

echo "【1】策略路由规则 (ip rule)"
echo "-------------------------------------"
sudo ip rule list | grep -E "fwmark|192.168.126"
echo ""

echo "【2】路由表配置"
echo "-------------------------------------"
echo "  路由表 100 (SATCOM - ens33):"
sudo ip route show table 100
echo ""
echo "  路由表 101 (WIFI - ens38):"
sudo ip route show table 101
echo ""
echo "  路由表 102 (CELLULAR):"
sudo ip route show table 102 2>/dev/null || echo "    未配置"
echo ""

echo "【3】iptables mangle 表 (打标规则)"
echo "-------------------------------------"
sudo iptables -t mangle -L PREROUTING -n -v | head -20
echo ""

echo "【4】iptables filter FORWARD 链"
echo "-------------------------------------"
sudo iptables -L FORWARD -n -v
echo ""

echo "【5】ipset 白名单"
echo "-------------------------------------"
echo "  control 白名单 (MCAR 阶段):"
sudo ipset list magic_control | grep "Members:" -A 10
echo ""
echo "  data 白名单 (MCCR 阶段):"
sudo ipset list magic_data | grep "Members:" -A 10
echo ""

echo "【6】网卡 IP 配置"
echo "-------------------------------------"
echo "  ens39 (客户端入口): $(ip addr show ens39 | grep 'inet ' | awk '{print $2}')"
echo "  ens33 (SATCOM):     $(ip addr show ens33 | grep 'inet ' | awk '{print $2}')"
echo "  ens38 (WIFI):       $(ip addr show ens38 | grep 'inet ' | awk '{print $2}')"
echo ""

echo "【7】连接跟踪条目"
echo "-------------------------------------"
sudo conntrack -L 2>/dev/null | grep -E "192.168.126.5|10.1.1.4|10.2.2.8|10.3.3.6" | head -10 || echo "  无相关连接"
echo ""

echo "【8】NAT 规则"
echo "-------------------------------------"
sudo iptables -t nat -L POSTROUTING -n -v | head -15
echo ""

echo "========================================="
echo "  诊断完成"
echo "========================================="
