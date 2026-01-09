# MIHF 模拟器

## 概述

MIHF（MIH Function）模拟器用于本地集成测试，可以：
- 接收 DLM 发送的 MIH 原语和指示
- 打印接收到的消息内容（格式化输出）
- 发送测试请求给 DLM
- 验证 DLM 与 MIHF 之间的通信

## 编译

```bash
gcc -Wall -Wextra -I.. -pthread -o mihf_sim mihf_simulator.c
```

## 运行

```bash
./mihf_sim
```

## 交互命令

| 命令 | 说明 |
|------|------|
| `c <n>` | 发送 Link_Capability_Discover.request |
| `p <n>` | 发送 Link_Get_Parameters.request |
| `s <n>` | 发送 Link_Event_Subscribe.request |
| `r <n>` | 发送 Link_Resource.request (分配资源) |
| `a` | 向所有 DLM 发送请求 |
| `l` | 列出 DLM 套接字状态 |
| `h` | 显示帮助 |
| `q` | 退出 |

其中 `<n>` 表示 DLM 索引：
- 1 = CELLULAR (`/tmp/dlm_cellular.sock`)
- 2 = SATCOM (`/tmp/dlm_satcom.sock`)
- 3 = WIFI (`/tmp/dlm_wifi.sock`)

## 使用示例

```bash
# 终端 1: 启动 MIHF 模拟器
./mihf_sim

# 终端 2: 启动 DLM
sudo ./DLM_CELLULAR/dlm_cellular_proto

# 在 MIHF 模拟器中测试:
MIHF> c 1    # 查询 CELLULAR 能力
MIHF> p 1    # 查询 CELLULAR 参数
MIHF> a      # 向所有 DLM 发送请求
MIHF> l      # 列出 DLM 状态
MIHF> q      # 退出
```

## 输出示例

```
╔══════════════════════════════════════════════════════════════╗
║              📡 Link_Up.indication 接收                     ║
╠══════════════════════════════════════════════════════════════╣
  链路标识: type=0x02, addr=eth0
  上线时间戳: 1732723456
  链路参数:
    - TX/RX 速率: 25000/50000 kbps
    - 信号强度: -55 dBm, 质量: 70%
    - 延迟: 50 ms, 抖动: 10 ms
    - 可用带宽: 50000 kbps
    - 链路状态: UP, 活动 Bearer: 0
╚══════════════════════════════════════════════════════════════╝
```

## 套接字路径

| 组件 | 套接字路径 |
|------|------------|
| MIHF 模拟器 | `/tmp/mihf.sock` |
| CELLULAR DLM | `/tmp/dlm_cellular.sock` |
| SATCOM DLM | `/tmp/dlm_satcom.sock` |
| WIFI DLM | `/tmp/dlm_wifi.sock` |

## 相关文档

- 详细说明请参阅根目录的 `DLM_OVERVIEW.md`
- MIH 协议定义请参阅 `../extensions/app_magic/mih_protocol.h`
