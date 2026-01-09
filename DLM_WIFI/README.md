# DLM WIFI - 无线局域网数据链路管理器

## 概述

WIFI DLM 负责管理无线局域网（IEEE 802.11）链路，提供：
- 链路能力发现和参数查询
- 事件订阅和状态上报
- 资源分配和 Bearer 管理
- 物理接口控制
- 飞行阶段感知（仅地面可用）

## 链路特性

| 参数 | 值 |
|------|-----|
| Link ID | 0x03 |
| 接口名称 | eth6 |
| IP 地址 | 10.3.3.3 |
| 最大带宽 (FL/RL) | 300/150 Mbps |
| 典型延迟 | 5 ms |
| RSSI 阈值 | -70 dBm |
| 成本因子 | 0.01 |
| 地面专用 | 是 |

## 编译

```bash
gcc -Wall -Wextra -I.. -pthread -o dlm_wifi_proto dlm_wifi_prototype.c
```

## 运行

```bash
# 需要 root 权限进行网卡操作
sudo ./dlm_wifi_proto

# 后台运行并记录日志
sudo ./dlm_wifi_proto > /var/log/dlm_wifi.log 2>&1 &
```

## 配置

配置文件位于 `../DLM_CONFIG/dlm_wifi.ini`，可修改接口名称、IP 地址、带宽参数等。

## 特殊说明

- WIFI 链路仅在飞机地面阶段可用（滑行、停机）
- 具有最高带宽和最低延迟特性
- 成本最低，适合大批量数据传输
- 飞行阶段自动禁用链路

## 文件说明

| 文件 | 说明 |
|------|------|
| `dlm_wifi_prototype.c` | 主程序源码 |

## 相关文档

- 详细说明请参阅根目录的 `DLM_OVERVIEW.md`
- MIH 协议定义请参阅 `../extensions/app_magic/mih_protocol.h`
