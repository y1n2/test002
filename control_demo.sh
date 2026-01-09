#!/bin/bash

echo "=== MAGIC 控制服务器使用演示 ==="
echo

echo "1. 连接控制服务器并认证..."
echo "AUTH admin:magic123" | nc -w 2 localhost 18080
echo

echo "2. 查询系统状态..."
echo -e "AUTH admin:magic123\nSTATUS" | nc -w 2 localhost 18080
echo

echo "3. 列出客户端..."
echo -e "AUTH admin:magic123\nLIST_CLIENTS" | nc -w 2 localhost 18080
echo

echo "=== 演示完成 ==="