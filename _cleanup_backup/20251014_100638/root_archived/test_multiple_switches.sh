#!/bin/bash

# 查找运行中的客户端进程
CLIENT_PID=$(pgrep -f "diameter_client")

if [ -z "$CLIENT_PID" ]; then
    echo "没有找到运行中的客户端进程"
    exit 1
fi

echo "找到客户端进程 PID: $CLIENT_PID"

# 向客户端发送多个状态切换命令
echo "开始多次状态切换测试..."

echo "1. 设置状态为 air"
echo "aircraft-status air" > /proc/$CLIENT_PID/fd/0
sleep 3

echo "2. 设置状态为 ground"  
echo "aircraft-status ground" > /proc/$CLIENT_PID/fd/0
sleep 3

echo "3. 再次设置状态为 air"
echo "aircraft-status air" > /proc/$CLIENT_PID/fd/0
sleep 3

echo "4. 再次设置状态为 ground"
echo "aircraft-status ground" > /proc/$CLIENT_PID/fd/0
sleep 3

echo "多次状态切换测试完成"