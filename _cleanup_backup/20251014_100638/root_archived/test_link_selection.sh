#!/bin/bash

# 链路选择功能测试脚本

echo "=== freeDiameter 链路选择功能测试 ==="
echo

# 启动客户端并发送测试命令
{
    echo "help"
    sleep 2
    echo "status"
    sleep 2
    echo "login client001 password123 pilot high"
    sleep 3
    echo "status"
    sleep 2
    echo "link-select 1000 cruise"
    sleep 3
    echo "env-update 39.9042 116.4074 cruise"
    sleep 3
    echo "send-link-resource bearer001 2000"
    sleep 3
    echo "logout"
    sleep 2
    echo "exit"
} | timeout 30 ./diameter_client

echo
echo "=== 测试完成 ==="