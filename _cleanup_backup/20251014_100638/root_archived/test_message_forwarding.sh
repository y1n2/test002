#!/bin/bash

echo "=== 测试消息转发功能 ==="
echo ""

# 创建测试命令文件
cat > /tmp/test_commands.txt << 'EOF'
help
send-madr 12345
send-link-resource bearer123 1024
send-ip-traffic 192.168.1.100 10.0.0.1 voice
exit
EOF

echo "测试命令序列："
cat /tmp/test_commands.txt
echo ""
echo "开始执行测试..."

# 向运行中的客户端发送命令
cat /tmp/test_commands.txt | while read cmd; do
    echo "发送命令: $cmd"
    echo "$cmd" > /proc/$(pgrep -f "diameter_client minimal_test.conf")/fd/0
    sleep 2
done

echo "测试命令发送完成"

# 清理临时文件
rm -f /tmp/test_commands.txt