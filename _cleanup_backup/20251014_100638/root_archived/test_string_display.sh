#!/bin/bash

# 测试改进后的链路模拟器字符串显示功能

echo "=========================================="
echo "测试改进后的链路模拟器字符串显示功能"
echo "=========================================="

# 进入exe目录
cd /home/zhuwuhui/freeDiameter/exe

echo ""
echo "1. 启动链路模拟器..."
./link_simulator &
SIMULATOR_PID=$!
sleep 3

echo ""
echo "2. 启动服务端..."
./freeDiameterd -c server_test.conf -d &
SERVER_PID=$!
sleep 5

echo ""
echo "3. 测试客户端发送字符串消息..."
echo "   发送字符串: '33355'"

# 使用expect或者直接管道输入来自动化客户端测试
(
    echo 'send-madr "33355"'
    sleep 2
    echo 'send-mcar "test_string_123"'
    sleep 2
    echo 'send-madr "Hello_World_456"'
    sleep 2
    echo 'exit'
) | timeout 15 ./diameter_client simple_test.conf

echo ""
echo "4. 等待几秒钟查看链路模拟器输出..."
sleep 3

echo ""
echo "5. 清理进程..."
kill $SIMULATOR_PID 2>/dev/null
kill $SERVER_PID 2>/dev/null
pkill -f freeDiameterd 2>/dev/null
pkill -f diameter_client 2>/dev/null
pkill -f link_simulator 2>/dev/null

echo ""
echo "=========================================="
echo "测试完成！"
echo "请查看上面的输出，确认链路模拟器是否正确显示了字符串内容。"
echo "=========================================="