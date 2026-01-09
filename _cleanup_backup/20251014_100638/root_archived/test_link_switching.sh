#!/bin/bash

# 链路切换测试脚本
echo "=== 链路切换测试开始 ==="

# 等待服务端和客户端稳定
sleep 2

echo "1. 测试空中状态 - 应该选择卫星链路"
echo "发送环境更新请求：设置为空中状态"

# 使用expect或直接向客户端进程发送命令
# 这里我们创建一个临时的命令文件
cat > /tmp/client_commands.txt << EOF
env_update AIR
link_selection
EOF

echo "2. 测试地面状态 - 应该选择以太网链路"
echo "发送环境更新请求：设置为地面状态"

cat >> /tmp/client_commands.txt << EOF
env_update GROUND
link_selection
EOF

echo "测试脚本准备完成"
echo "请手动在客户端CLI中执行以下命令："
echo "1. env_update AIR"
echo "2. link_selection"
echo "3. env_update GROUND" 
echo "4. link_selection"

echo "或者使用以下命令自动执行："
echo "cat /tmp/client_commands.txt"