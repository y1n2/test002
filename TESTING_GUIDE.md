# Diameter客户端测试指南

## 概述
本指南提供了完整的测试例子，演示如何使用freeDiameter客户端与服务端进行ARINC-839协议通信。

## 前提条件
- freeDiameter服务端正在运行
- 客户端已成功连接到服务端
- 连接状态为 `STATE_OPEN`

## 测试命令详解

### 1. 移动凭证访问请求 (MCAR)
**命令格式**: `send-mcar <credential>`

**用途**: 发送移动凭证访问请求，用于航空公司验证移动设备的访问凭证

**测试例子**:
```
send-mcar airline_credential_ABC123
send-mcar passenger_device_XYZ789
send-mcar crew_tablet_DEF456
```

**预期结果**: 
- 客户端发送MCAR消息到服务端
- 服务端返回MCAR-Answer消息
- 包含Result-Code和相关AVP

### 2. 移动应用数据请求 (MADR)
**命令格式**: `send-madr <cdr-id>`

**用途**: 发送移动应用数据请求，用于获取特定CDR ID的数据记录

**测试例子**:
```
send-madr CDR_ID_456789
send-madr FLIGHT_AA1234_20241201
send-madr PASSENGER_DATA_001
```

**预期结果**:
- 客户端发送MADR消息
- 服务端查找对应的CDR数据
- 返回MADR-Answer消息

### 3. 链路资源请求
**命令格式**: `send-link-resource <bearer-id> <bandwidth>`

**用途**: 请求分配网络链路资源，指定承载标识和带宽需求

**测试例子**:
```
send-link-resource BEARER_001 1000
send-link-resource WIFI_5G_CH1 2048
send-link-resource SATCOM_LINK 512
```

**预期结果**:
- 客户端发送链路资源请求
- 服务端评估资源可用性
- 返回资源分配结果

### 4. IP流量请求
**命令格式**: `send-ip-traffic <source-ip>`

**用途**: 发送IP流量相关请求，用于网络流量管理

**测试例子**:
```
send-ip-traffic 192.168.1.100
send-ip-traffic 10.0.0.50
send-ip-traffic 172.16.1.200
```

**预期结果**:
- 客户端发送IP流量请求
- 服务端处理流量管理逻辑
- 返回处理结果

## 完整测试序列

### 场景1: 航空公司乘客设备接入
```bash
# 1. 设备凭证验证
send-mcar passenger_device_12345

# 2. 请求数据服务
send-madr PASSENGER_DATA_12345

# 3. 申请网络资源
send-link-resource PASSENGER_WIFI 1024

# 4. 开始IP通信
send-ip-traffic 192.168.100.50
```

### 场景2: 机组设备管理
```bash
# 1. 机组设备认证
send-mcar crew_tablet_PILOT01

# 2. 获取飞行数据
send-madr FLIGHT_DATA_AA1234

# 3. 申请高优先级链路
send-link-resource CREW_PRIORITY 2048

# 4. 建立管理通信
send-ip-traffic 10.1.1.100
```

### 场景3: 地面服务系统
```bash
# 1. 地面系统认证
send-mcar ground_system_GATE05

# 2. 请求维护记录
send-madr MAINTENANCE_LOG_001

# 3. 申请数据传输链路
send-link-resource GROUND_UPLINK 4096

# 4. 开始数据同步
send-ip-traffic 172.20.1.10
```

## 监控和调试

### 查看日志
- 客户端日志显示发送的消息详情
- 服务端日志显示接收和处理过程
- 注意Result-Code和Error-Message

### 常见问题
1. **连接失败**: 检查网络配置和证书
2. **认证失败**: 验证凭证格式和权限
3. **资源不足**: 检查服务端资源配置
4. **协议错误**: 确认AVP格式正确

## 性能测试

### 批量测试脚本
```bash
#!/bin/bash
for i in {1..10}; do
    echo "send-mcar test_credential_$i"
    sleep 1
done
```

### 并发测试
可以启动多个客户端实例进行并发测试，验证服务端的处理能力。

## 结论
通过以上测试例子，可以全面验证freeDiameter客户端与服务端的ARINC-839协议通信功能。