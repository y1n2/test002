# DLM SATCOM - 卫星链路数据链路管理器

## 概述

SATCOM DLM 负责管理卫星通信链路（GEO 卫星），提供：
- 链路能力发现和参数查询
- 事件订阅和状态上报
- 资源分配和 Bearer 管理
- 物理接口控制

## 链路特性

| 参数 | 值 |
|------|-----|
| Link ID | 0x01 |
| 接口名称 | eth5 |
| IP 地址 | 10.2.2.2 |
| 最大带宽 (FL/RL) | 30/2 Mbps |
| 典型延迟 | 600 ms (GEO) |
| RSSI 阈值 | -85 dBm |
| 成本因子 | 0.20 |

## 编译

```bash
gcc -Wall -Wextra -I.. -pthread -o dlm_satcom_proto dlm_satcom_prototype.c
```

## 运行

```bash
# 需要 root 权限进行网卡操作
sudo ./dlm_satcom_proto

# 后台运行并记录日志
sudo ./dlm_satcom_proto > /var/log/dlm_satcom.log 2>&1 &
```

## 配置

配置文件位于 `../DLM_CONFIG/dlm_satcom.ini`，可修改接口名称、IP 地址、带宽参数等。

## 特殊说明

- SATCOM 链路具有高延迟特性（GEO 卫星往返约 600ms）
- 非对称带宽：前向链路远大于反向链路
- 成本较高，建议用于关键通信或其他链路不可用时的备份

## 文件说明

| 文件 | 说明 |
|------|------|
| `dlm_satcom_prototype.c` | 主程序源码 |

## 相关文档

- 详细说明请参阅根目录的 `DLM_OVERVIEW.md`
- MIH 协议定义请参阅 `../extensions/app_magic/mih_protocol.h`
