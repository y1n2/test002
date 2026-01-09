#!/bin/bash

# 交互式客户端测试脚本
echo "=== 链路选择功能交互测试 ==="

# 向客户端发送命令
{
    sleep 2
    echo "help"
    sleep 2
    echo "status"
    sleep 2
    echo "env-update 39.9042 116.4074 cruise"
    sleep 3
    echo "link-select 1000 cruise"
    sleep 3
    echo "env-update 39.9042 116.4074 ground"
    sleep 3
    echo "link-select 1000 ground"
    sleep 3
    echo "send-mcar test_credential"
    sleep 3
    echo "status"
    sleep 2
} | timeout 30 /home/zhuwuhui/freeDiameter/diameter_client/diameter_client simple_test.conf

echo "=== 测试完成 ==="