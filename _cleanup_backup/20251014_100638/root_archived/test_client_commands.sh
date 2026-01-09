#!/bin/bash

echo "=== 测试客户端CLI命令 ==="

# 等待客户端完全启动
sleep 2

# 测试help命令
echo "1. 测试help命令:"
echo "help" > /tmp/client_input.txt

# 测试设置空中模式
echo "2. 测试设置空中模式:"
echo "set_environment AIR 39.9042 116.4074 10000" >> /tmp/client_input.txt

# 测试链路选择请求
echo "3. 测试链路选择请求:"
echo "link_selection" >> /tmp/client_input.txt

# 测试设置地面模式
echo "4. 测试设置地面模式:"
echo "set_environment GROUND 39.9042 116.4074 0" >> /tmp/client_input.txt

# 再次测试链路选择
echo "5. 再次测试链路选择:"
echo "link_selection" >> /tmp/client_input.txt

echo "测试脚本已准备完成"
