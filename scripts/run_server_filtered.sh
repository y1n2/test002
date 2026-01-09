#!/bin/bash
# 启动服务器并过滤日志 - 只显示关键信息

cd /home/zhuwuhui/freeDiameter/build

# 只显示以下关键日志：
# - MAGIC 相关
# - 错误和警告
# - MCAR/MSCR 等协议消息
# - 心跳超时
# - 链路状态变化
sudo ./freeDiameterd/freeDiameterd -c ../conf/magic_server.conf 2>&1 | \
    grep -E "MAGIC|ERROR|WARNING|MCAR|MSCR|MCAA|MSCA|timeout|Link status|Broadcasting|subscribed|app_magic"
