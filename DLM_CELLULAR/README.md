# DLM CELLULAR - 蜂窝链路数据链路管理器

## 概述

CELLULAR DLM 负责管理蜂窝网络（LTE/5G）链路，提供：
- 链路能力发现和参数查询
- 事件订阅和状态上报
- 资源分配和 Bearer 管理
- 物理接口控制

## 链路特性

| 参数 | 值 |
|------|-----|
| Link ID | 0x02 |
| 接口名称 | eth0 |
| IP 地址 | 10.1.1.1 |
| 最大带宽 (FL/RL) | 50/25 Mbps |
| 典型延迟 | 50 ms |
| RSSI 阈值 | -75 dBm |
| 成本因子 | 0.05 |

## 编译

```bash
gcc -Wall -Wextra -I.. -pthread -o dlm_cellular_proto dlm_cellular_prototype.c
```

## 运行

```bash
# 需要 root 权限进行网卡操作
sudo ./dlm_cellular_proto

# 后台运行并记录日志
sudo ./dlm_cellular_proto > /var/log/dlm_cellular.log 2>&1 &
```

## 配置

配置文件位于 `../DLM_CONFIG/dlm_cellular.ini`，可修改接口名称、IP 地址、带宽参数等。

## 文件说明

| 文件 | 说明 |
|------|------|
| `dlm_cellular_prototype.c` | 主程序源码 |
| `dlm_cellular_main.c` | 旧版主程序（可忽略） |

## 相关文档

- 详细说明请参阅根目录的 `DLM_OVERVIEW.md`
- MIH 协议定义请参阅 `../extensions/app_magic/mih_protocol.h`
