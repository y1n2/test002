# MCCR 命令更新 - 实现 ARINC 839 介质无关性原则

## 修改概述

根据 ARINC 839-2014 规范的**介质无关性 (Media Independence)** 核心原则，已更新 MCCR 命令，确保客户端只能提交 **QoS 业务需求**，不能指定具体的物理链路。

## 修改内容

### 1. 移除的功能
- ❌ **DLM-Name AVP**: 客户端不能指定链路模块名称（如 "SATCOM1", "LTE", "WiFi"）
- ❌ 任何直接选择或切换物理链路的选项

### 2. 新增的 QoS 参数

客户端现在只能通过以下业务参数提交需求：

| 参数 | 类型 | 说明 | 示例值 |
|------|------|------|--------|
| `profile` | 字符串 | 业务类型 | IP_DATA, VOICE, VIDEO |
| `min_bw_kbps` | 数字 | 最小保证带宽 (kbps) | 512 |
| `max_bw_kbps` | 数字 | 最大期望带宽 (kbps) | 5000 |
| `priority` | 1-8 | 优先级等级 (1=最高) | 2 |
| `qos_level` | 0-3 | QoS 服务等级 | 1 |

**QoS 等级说明：**
- 0: 最佳努力 (Best Effort)
- 1: 保证服务 (Assured)
- 2: 实时服务 (Real-Time)
- 3: 控制服务 (Control)

## 新的命令格式

### 创建会话（提交 QoS 需求）
```bash
mccr create <profile> <min_kbps> <max_kbps> <priority> <qos_level>
```

**示例：**
```bash
# 数据业务：最小512kbps，最大5Mbps，优先级2，保证服务
mccr create IP_DATA 512 5000 2 1

# 视频通话：最小2Mbps，最大10Mbps，优先级1，实时服务
mccr create VIDEO_CALL 2000 10000 1 2

# 语音通话：最小64kbps，最大128kbps，优先级3，实时服务
mccr create VOICE 64 128 3 2
```

### 修改会话（更新 QoS 需求）
```bash
mccr modify <min_kbps> <max_kbps> <priority> <qos_level>
```

**示例：**
```bash
# 提高带宽需求到 10Mbps，提升优先级
mccr modify 1024 10000 1 1

# 降低带宽需求（节省成本）
mccr modify 256 1000 5 0
```

### 释放会话
```bash
mccr release
```

## 链路切换触发机制

根据 ARINC 839 规范，链路切换由以下三种方式触发：

### 1. 外部事件触发 (Link-Driven)
**场景：** LMI 模块向 Policy Engine 报告状态变化
- 卫星信号丢失 (Link_Down)
- 飞机飞入 LTE 覆盖区 (Link_Up)
- 链路质量下降

**MAGIC 动作：**
- 触发全局路由重新计算
- 自动将受影响的客户端切换到最优链路

### 2. 内部策略触发 (Policy-Driven)
**场景：** 飞行阶段或中央策略变化
- 飞机从"巡航"进入"降落"阶段
- 策略要求"降落阶段禁用高成本链路"

**MAGIC 动作：**
- 自动终止高成本链路会话
- 将客户端切换到其他可用链路或断开非关键连接

### 3. 客户端需求变更触发 (MCCR-Driven, 间接切换)
**场景：** 客户端通过 MCCR 提交新的 QoS 需求

**示例场景：**
```
当前状态：
- 客户端在 Satcom 链路（最大 432kbps，延迟 600ms）
- 使用 IP_DATA profile，带宽 256kbps

客户端提交新需求：
mccr modify 5000 10000 1 2
(最小5Mbps，最大10Mbps，高优先级，实时服务)

MAGIC Policy Engine 处理：
1. 检查当前 Satcom 链路 → 无法满足 5Mbps 带宽需求
2. 遍历所有可用链路
3. 发现 LTE 链路（带宽 10Mbps，延迟 50ms）可以满足
4. 决策：批准新 QoS，执行链路切换
5. 在 LMI 层解除 Satcom 资源
6. 在 Network Management 层更新路由
7. 将客户端流量切换到 LTE
```

**关键点：**
- MCCR 是切换的**输入信号**，不是切换的**指令**
- 客户端表达"我需要更好的网络"
- MAGIC 自动执行链路切换和路由配置

## 客户端日志示例

### 创建会话
```
📊 提交 QoS 业务需求到 MAGIC 策略引擎:
  Profile: IP_DATA
  最小保证带宽: 512 kbps
  最大期望带宽: 5000 kbps
  优先级等级: 2 (1=最高, 8=最低)
  QoS 等级: 1 (0=最佳努力, 1=保证, 2=实时, 3=控制)

🔄 MAGIC 将自动选择最优链路 (Satcom/LTE/WiFi)...
✅ MCCR Create 请求已发送！
```

### 修改会话
```
📊 更新 QoS 业务需求:
  最小保证带宽: 1024 kbps
  最大期望带宽: 10000 kbps
  优先级: 1, QoS 等级: 2

🔄 MAGIC 将根据新需求重新评估链路分配...
✅ MCCR Modify 请求已发送！
```

## 设计优势

### 1. 客户端简洁性
- 客户端不需要了解底层链路架构
- 不需要处理链路切换逻辑
- 代码更稳定，维护成本更低

### 2. 灵活性和可扩展性
- 新增链路类型（如 5G）无需修改客户端
- 链路配置变更对客户端透明
- 支持未来技术升级

### 3. 智能决策
- MAGIC Policy Engine 综合考虑多个因素：
  - 链路可用性
  - 带宽容量
  - 延迟特性
  - 成本因素
  - 飞行阶段
  - 客户端优先级

### 4. 符合规范
- 完全遵守 ARINC 839-2014 标准
- 实现介质无关性核心原则
- 保证跨系统互操作性

## 测试场景

### 场景 1: 基本数据通信
```bash
# 1. 注册客户端
mcar

# 2. 创建数据会话（最小512kbps）
mccr create IP_DATA 512 5000 2 1

# MAGIC 自动选择：
# - 如果 LTE 可用 → 使用 LTE（低延迟）
# - 如果只有 Satcom → 使用 Satcom
# - 如果多个链路可用 → 选择成本效益最优的
```

### 场景 2: 带宽需求提升触发切换
```bash
# 1. 当前在 Satcom（432kbps）
mccr create IP_DATA 256 512 5 0

# 2. 应用需要更高带宽（触发切换）
mccr modify 5000 10000 1 2

# MAGIC 自动处理：
# - 发现 Satcom 无法满足 5Mbps
# - 自动切换到 LTE（10Mbps）
# - 更新路由表
# - 客户端无感知切换
```

### 场景 3: 链路故障自动恢复
```bash
# 当前在 LTE 链路
# LTE 突然故障（外部事件）

# MAGIC 自动处理：
# - 检测到 LTE Link_Down
# - 重新评估所有客户端
# - 将客户端切换到 Satcom
# - 发送 MNTR 通知客户端
# - 客户端继续通信（可能带宽降低）
```

## 代码位置

- **命令处理**: `magic_client/magic_commands.c` (cmd_mccr 函数)
- **AVP 构造**: `magic_client/magic_group_avp_add.c` (add_comm_req_params 函数)
- **CLI 界面**: `magic_client/cli_interface.c`

## 编译状态

✅ **编译成功** - 所有警告已清除

## 下一步

1. 测试客户端 MCCR 命令
2. 验证 MAGIC Policy Engine 链路选择逻辑
3. 测试链路切换场景（Link Up/Down）
4. 验证 QoS 需求变更触发的切换
