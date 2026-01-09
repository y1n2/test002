#!/bin/bash

# 端口转发设置脚本
# 将服务端的3869端口转发到本地的3868端口，供客户端访问

echo "设置端口转发..."

# 检查是否已有转发规则
existing_rule=$(sudo iptables -t nat -L PREROUTING -n | grep "dpt:3869")

if [ -z "$existing_rule" ]; then
    # 添加DNAT规则：将访问3869端口的流量转发到本地3868端口
    sudo iptables -t nat -A PREROUTING -p tcp --dport 3869 -j DNAT --to-destination 127.0.0.1:3868
    
    # 添加MASQUERADE规则以确保返回流量正确路由
    sudo iptables -t nat -A POSTROUTING -p tcp --dport 3868 -j MASQUERADE
    
    # 允许转发流量
    sudo iptables -A FORWARD -p tcp --dport 3868 -j ACCEPT
    
    echo "端口转发规则已添加："
    echo "- 外部访问服务端的3869端口将被转发到本地的3868端口"
    echo "- 客户端可以通过 服务端IP:3869 连接到Diameter服务"
else
    echo "端口转发规则已存在，无需重复添加"
fi

echo ""
echo "当前NAT规则："
sudo iptables -t nat -L PREROUTING -n | grep -E "(Chain|dpt:3869)"

echo ""
echo "使用方法："
echo "1. 在服务端运行此脚本"
echo "2. 客户端配置连接到: 服务端IP:3869"
echo "3. 如需清除规则，运行: sudo iptables -t nat -F"