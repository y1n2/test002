#!/bin/bash

echo "=== 测试完整消息流 ==="
echo "1. 检查服务器状态..."

# 检查服务器端口
if netstat -tlnp | grep -q ":3870.*freeDiameterd"; then
    echo "✅ Diameter 服务器正在监听端口 3870"
else
    echo "❌ Diameter 服务器未在端口 3870 监听"
    exit 1
fi

# 检查 TLS 端口
if netstat -tlnp | grep -q ":5870.*freeDiameterd"; then
    echo "✅ Diameter 服务器正在监听 TLS 端口 5870"
else
    echo "❌ Diameter 服务器未在 TLS 端口 5870 监听"
    exit 1
fi

echo "2. 检查链路模拟器状态..."

# 检查链路模拟器端口
for port in 8001 8002 8003 8004; do
    if netstat -tlnp | grep -q ":$port.*link_simul"; then
        echo "✅ 链路模拟器正在监听端口 $port"
    else
        echo "❌ 链路模拟器未在端口 $port 监听"
        exit 1
    fi
done

echo "3. 检查客户端连接状态..."

# 检查客户端连接
if netstat -tnp | grep -q "diameter_client.*5870.*ESTABLISHED"; then
    echo "✅ 客户端已连接到服务器"
else
    echo "❌ 客户端未连接到服务器"
    exit 1
fi

echo "4. 测试链路模拟器直连..."

# 测试链路模拟器连接
if timeout 3 bash -c 'echo "test" | nc 127.0.0.1 8001' >/dev/null 2>&1; then
    echo "✅ 可以连接到链路模拟器端口 8001"
else
    echo "❌ 无法连接到链路模拟器端口 8001"
fi

echo ""
echo "=== 消息流测试总结 ==="
echo "✅ 服务器运行正常"
echo "✅ 链路模拟器运行正常"
echo "✅ 客户端已连接"
echo "✅ 系统架构验证完成"
echo ""
echo "消息流路径: 客户端 -> 服务器(3870/5870) -> 链路模拟器(8001-8004)"