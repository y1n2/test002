#!/bin/bash

echo "=== 链路选择功能测试 ==="
echo "测试时间: $(date)"
echo ""

cd /home/zhuwuhui/freeDiameter/diameter_client

# 测试1: 基本链路选择功能
echo "测试1: 基本链路选择功能 (cruise阶段, 1000 Kbps)"
echo "执行命令: login test_client test_credential -> link-select 1000 cruise"
{
    echo "login test_client test_credential"
    sleep 2
    echo "link-select 1000 cruise"
    sleep 3
    echo "exit"
} | timeout 15 ./diameter_client > test1_basic.log 2>&1

echo "测试1完成，检查结果..."
if grep -q "链路选择请求发送成功" test1_basic.log; then
    echo "✓ 链路选择请求发送成功"
else
    echo "✗ 链路选择请求发送失败"
fi

if grep -q "ERROR.*Invalid parameter" test1_basic.log; then
    echo "✗ 仍存在AVP类型错误"
else
    echo "✓ 没有发现AVP类型错误"
fi

echo ""

# 测试2: 不同飞行阶段
echo "测试2: 不同飞行阶段测试 (takeoff阶段, 2000 Kbps)"
{
    echo "login test_client test_credential"
    sleep 2
    echo "link-select 2000 takeoff"
    sleep 3
    echo "exit"
} | timeout 15 ./diameter_client > test2_takeoff.log 2>&1

echo "测试2完成，检查结果..."
if grep -q "链路选择请求发送成功" test2_takeoff.log; then
    echo "✓ takeoff阶段链路选择成功"
else
    echo "✗ takeoff阶段链路选择失败"
fi

echo ""

# 测试3: 高带宽需求
echo "测试3: 高带宽需求测试 (landing阶段, 5000 Kbps)"
{
    echo "login test_client test_credential"
    sleep 2
    echo "link-select 5000 landing"
    sleep 3
    echo "exit"
} | timeout 15 ./diameter_client > test3_highbw.log 2>&1

echo "测试3完成，检查结果..."
if grep -q "链路选择请求发送成功" test3_highbw.log; then
    echo "✓ 高带宽链路选择成功"
else
    echo "✗ 高带宽链路选择失败"
fi

echo ""

# 测试4: 环境更新功能
echo "测试4: 环境更新功能测试"
{
    echo "login test_client test_credential"
    sleep 2
    echo "env-update 39.9042 116.4074 cruise"
    sleep 3
    echo "exit"
} | timeout 15 ./diameter_client > test4_envupdate.log 2>&1

echo "测试4完成，检查结果..."
if grep -q "环境更新请求发送成功" test4_envupdate.log; then
    echo "✓ 环境更新请求发送成功"
else
    echo "✗ 环境更新请求发送失败"
fi

echo ""
echo "=== 测试总结 ==="
echo "详细日志文件:"
echo "  - test1_basic.log: 基本链路选择测试"
echo "  - test2_takeoff.log: takeoff阶段测试"
echo "  - test3_highbw.log: 高带宽需求测试"
echo "  - test4_envupdate.log: 环境更新测试"

echo ""
echo "错误检查:"
for logfile in test1_basic.log test2_takeoff.log test3_highbw.log test4_envupdate.log; do
    if [ -f "$logfile" ]; then
        error_count=$(grep -c "ERROR\|Failed\|Invalid" "$logfile" 2>/dev/null || echo "0")
        echo "  $logfile: $error_count 个错误"
    fi
done