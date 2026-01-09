#!/bin/bash

# 测试中文字符串显示功能
echo "正在测试中文字符串显示功能..."

# 发送包含中文字符的MADR消息
echo 'send-madr "测试中文字符串显示功能"' > /tmp/test_input.txt

echo "测试命令已准备完成，请在客户端终端中手动输入以下命令："
echo 'send-madr "测试中文字符串显示功能"'
echo ""
echo "或者发送其他包含中文的消息："
echo 'send-madr "航空通信系统测试"'
echo 'send-madr "飞机地面通信链路"'
echo ""
echo "观察服务端和客户端的日志输出，确认中文字符是否正确显示。"