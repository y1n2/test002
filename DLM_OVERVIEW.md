# DLM（Data Link Manager）说明文档

## 概述

本项目实现一组原型级的 DLM（数据链路管理器），用于在 MAGIC 航空通信系统里对不同链路技术（SATCOM / CELLULAR / WIFI）进行能力上报、参数查询、事件订阅、资源管理及链路状态指示。文档面向开发者、测试与运维人员，说明软件功能、架构、运行流程与运维要点。

## 组件与文件

| 文件/目录 | 说明 |
|-----------|------|
| `DLM_COMMON/dlm_common.h` | 公共数据结构与工具函数（配置管理、状态模拟、bearer 管理、接口控制等） |
| `DLM_COMMON/dlm_config_parser.h` | INI 配置文件解析器 |
| `DLM_CONFIG/dlm_cellular.ini` | CELLULAR DLM 配置文件 |
| `DLM_CONFIG/dlm_satcom.ini` | SATCOM DLM 配置文件 |
| `DLM_CONFIG/dlm_wifi.ini` | WIFI DLM 配置文件 |
| `DLM_SATCOM/dlm_satcom_prototype.c` | SATCOM（卫星）链路 DLM 原型实现。接口默认：`eth5`，IP `10.2.2.2` |
| `DLM_CELLULAR/dlm_cellular_prototype.c` | CELLULAR（蜂窝）链路 DLM 原型实现。接口默认：`eth0`，IP `10.1.1.1` |
| `DLM_WIFI/dlm_wifi_prototype.c` | WIFI 链路 DLM 原型实现。接口默认：`eth6`，IP `10.3.3.3` |
| `MIHF_SIMULATOR/mihf_simulator.c` | MIHF 模拟器，用于本地集成测试 |
| `extensions/app_magic/mih_protocol.h` | MIH（Media Independent Handover）协议的数据结构与原语定义（Request/Confirm/Indication） |

设计原则
- 原型阶段遵循“仅报告不操作”原则：大部分 MIH 原语仅返回或上报状态、模拟资源分配，不做真实流量/QoS 级别的系统变更。
- 但对于链路物理控制（Link_Up / Link_Down），DLM 会执行本地网卡操作（`ip link set up/down`）及 IP 地址配置/删除，以便在实验环境中验证链路切换流程。
- 采用 Unix 域套接字（默认路径 `/tmp/mihf.sock`）与 MIHF（上层 CM core）通信。若 MIHF 未运行，DLM 会记录警告并继续运行。

主要功能（已实现）
- 链路能力发现（Link_Capability_Discover.request / .confirm）
- 链路参数查询（Link_Get_Parameters.request / .confirm）
- 事件订阅（Link_Event_Subscribe.request / .confirm）
- 资源请求/释放（MIH 的 Link_Resource 原语）——原型中模拟分配并维护 bearer 表
- 链路异步指示（Link_Detected / Link_Up / Link_Going_Down / Link_Down / Link_Parameters_Report）
- 状态模拟：周期性更新 RSSI、带宽使用、发送参数报告
- 物理链路控制：在 Link_Up 时执行 `ip link set <iface> up` 并 `ip addr add <IP>/<mask> dev <iface>`；在 Link_Down 时删除 IP 并关闭接口

运行与部署
1. 编译（示例）：
   - CELLULAR: `gcc -I. -lpthread -o DLM_CELLULAR/dlm_cellular_proto DLM_CELLULAR/dlm_cellular_prototype.c`
   - SATCOM: `gcc -I. -lpthread -o DLM_SATCOM/dlm_satcom_proto DLM_SATCOM/dlm_satcom_prototype.c`
   - WIFI: `gcc -I. -lpthread -o DLM_WIFI/dlm_wifi_proto DLM_WIFI/dlm_wifi_prototype.c`
2. 启动顺序建议：先启动 MIHF（如果有实现），再启动 DLM。
   - 若没有 MIHF，可以直接运行 DLM 可执行文件来观察行为（它会在 MIHF 套接字缺失时打印警告）。
3. 启动示例（后台运行）：
```bash
sudo ./DLM_CELLULAR/dlm_cellular_proto &
sudo ./DLM_SATCOM/dlm_satcom_proto &
sudo ./DLM_WIFI/dlm_wifi_proto &
```
注意：物理网卡操作需要 root 权限，且在真实系统上运行会影响现有网络配置，请在受控实验环境或容器/虚拟机中测试。

配置说明（已编码在源文件）
- CELLULAR: 接口 `eth0`，IP `10.1.1.1`，掩码 `255.255.255.0`
- SATCOM: 接口 `eth5`，IP `10.2.2.2`，掩码 `255.255.255.0`
- WIFI: 接口 `eth6`，IP `10.3.3.3`，掩码 `255.255.255.0`
这些常量在各自的 `dlm_*_prototype.c` 文件顶部定义，测试时可直接修改源文件或把配置抽象为外部配置文件（建议作为后续改进）。

消息与数据结构
- 使用 `mih_protocol.h` 中定义的原语码（例如 `MIH_LINK_CAPABILITY_DISCOVER_REQ`、`MIH_LINK_UP_IND` 等）和数据结构进行进程间通信。
- 在原型实现中，DLM 通过 Unix 域数据报套接字发送和接收完整结构体（以 C 结构的二进制形式），调用方（MIHF）需使用相同头文件进行解析。

日志与调试
- DLM 在控制台打印详细日志，包括启动信息、每次原语处理、参数上报、物理链路操作（包含执行的 ip 命令）以及错误/警告。
- 建议将输出重定向到日志文件，例如：
```bash
sudo ./DLM_CELLULAR/dlm_cellular_proto > /var/log/dlm_cellular.log 2>&1 &
```

测试建议
- 单元测试：对 `dlm_common` 中的工具函数（如 bearer 分配/释放、RSSI 模拟）添加单元测试。
- 集成测试：在一个干净的虚拟机中部署 MIHF 模拟端（可编写一个简单的 Unix 域 socket 接收器），然后启动 DLM，验证原语交互。
- 场景测试：模拟 RSSI 下降触发 `Link_Going_Down`，验证 CM 对资源迁移或切换的响应流程。

已知限制与后续改进
- 当前实现为原型：消息以 C 结构二进制形式传输，跨平台/版本兼容性差，建议改为 TLV/JSON 或 protobuf 格式。
- 配置硬编码在源文件中，建议提取为 `ini`/`yaml` 配置文件并支持命令行参数覆盖。
- 权限和安全：当前使用 Unix 域套接字与本地命令执行（system），需在生产或更严格环境中替换为受控 API，并增加鉴权。
- MIHF 模拟：仓库中未包含 MIHF 完整实现，测试时需准备一个对端（或使用日志验证）。

文档维护
- 建议把本文件 `DLM_OVERVIEW.md` 放入仓库根目录并随代码一起维护。
- 对每次接口或结构变更，请同步更新 `extensions/app_magic/mih_protocol.h` 与本说明文档的“消息与数据结构”部分。

常见命令快速参考
- 查看当前网络接口与地址： `ip addr show <iface>`
- 添加 IP（DLM 启动时执行）： `sudo ip addr add 10.1.1.1/24 dev eth0`
- 删除 IP（DLM 停止时执行）： `sudo ip addr del 10.1.1.1/24 dev eth0`

联系方式与贡献
- 如需扩展或出现问题，请联系负责本模块的开发者并创建 issue。

---
说明文档已生成：`DLM_OVERVIEW.md`（仓库根目录）。如需我把配置抽象为配置文件、添加示例 MIHF 模拟器或把说明翻译/格式化为 README.md，我可以继续实现。