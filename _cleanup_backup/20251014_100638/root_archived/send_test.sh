#!/bin/bash

# 发送测试消息到客户端
echo "发送测试消息: 33355"
echo 'send-madr "33355"' > /tmp/client_input.txt

echo "发送测试消息: 12345"
echo 'send-madr "12345"' >> /tmp/client_input.txt

echo "发送测试消息: TestString"
echo 'send-mcar "TestString"' >> /tmp/client_input.txt

echo "测试命令已准备好，请手动在客户端中执行这些命令："
cat /tmp/client_input.txt