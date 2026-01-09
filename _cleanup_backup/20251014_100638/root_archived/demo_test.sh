#!/bin/bash

echo "=== freeDiameter客户端测试演示 ==="
echo
echo "当前客户端已连接到服务端，连接状态为 STATE_OPEN"
echo "以下是可以在客户端命令行中输入的测试命令："
echo

echo "1. 移动凭证访问请求 (MCAR) 测试："
echo "   send-mcar airline_credential_ABC123"
echo "   send-mcar passenger_device_XYZ789"
echo "   send-mcar crew_tablet_DEF456"
echo

echo "2. 移动应用数据请求 (MADR) 测试："
echo "   send-madr CDR_ID_456789"
echo "   send-madr FLIGHT_AA1234_20241201"
echo "   send-madr PASSENGER_DATA_001"
echo

echo "3. 链路资源请求测试："
echo "   send-link-resource BEARER_001 1000"
echo "   send-link-resource WIFI_5G_CH1 2048"
echo "   send-link-resource SATCOM_LINK 512"
echo

echo "4. IP流量请求测试："
echo "   send-ip-traffic 192.168.1.100"
echo "   send-ip-traffic 10.0.0.50"
echo "   send-ip-traffic 172.16.1.200"
echo

echo "=== 实际测试场景 ==="
echo
echo "场景1: 航空公司乘客设备接入"
echo "步骤1: send-mcar passenger_device_12345"
echo "步骤2: send-madr PASSENGER_DATA_12345"
echo "步骤3: send-link-resource PASSENGER_WIFI 1024"
echo "步骤4: send-ip-traffic 192.168.100.50"
echo

echo "场景2: 机组设备管理"
echo "步骤1: send-mcar crew_tablet_PILOT01"
echo "步骤2: send-madr FLIGHT_DATA_AA1234"
echo "步骤3: send-link-resource CREW_PRIORITY 2048"
echo "步骤4: send-ip-traffic 10.1.1.100"
echo

echo "场景3: 地面服务系统"
echo "步骤1: send-mcar ground_system_GATE05"
echo "步骤2: send-madr MAINTENANCE_LOG_001"
echo "步骤3: send-link-resource GROUND_UPLINK 4096"
echo "步骤4: send-ip-traffic 172.20.1.10"
echo

echo "=== 使用说明 ==="
echo "1. 在客户端终端中输入上述任意命令"
echo "2. 观察客户端和服务端的日志输出"
echo "3. 检查消息的发送和接收情况"
echo "4. 验证Result-Code和相关AVP"
echo

echo "注意: 客户端当前正在运行，可以直接在其命令行界面中输入测试命令"