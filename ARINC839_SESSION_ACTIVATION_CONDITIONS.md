# ARINC 839 会话激活条件验证实现报告

## 概述

本文档描述了 MAGIC 服务端针对 ARINC 839 规范中会话激活条件 (Session Activation Conditions) 验证逻辑的实现情况。

根据 ARINC 839 规范 §3.2.4.1.1.1.2，MAGIC 服务器在处理客户端认证和通信请求时，必须验证客户端是否有权在当前飞行条件下激活会话或进行通信。

## 规范要求

### 1. Flight-Phase AVP (§1.1.1.6.4.1)
- **AVP Code**: 10033
- **格式**: UTF8String
- **用途**: 限制会话在特定飞行阶段的激活
- **示例值**: "GROUND", "TAXI", "TAKEOFF", "CLIMB", "CRUISE", "DESCENT", "APPROACH", "LANDING"

### 2. Altitude AVP (§1.1.1.6.4.2)
- **AVP Code**: 10034
- **格式**: UTF8String
- **用途**: 限制会话在特定高度范围的激活
- **语法**: `[not] min-max[,min-max]*` 
- **示例**: 
  - `"1000-10000"` - 白名单：仅在 1000-10000 ft 允许
  - `"not 0-1000,35000-"` - 黑名单：禁止在 0-1000 ft 和 35000 ft 以上

### 3. Airport AVP (§1.1.1.6.4.3)
- **AVP Code**: 10035
- **格式**: UTF8String
- **用途**: 限制会话在特定机场的激活
- **语法**: `[not] ICAO[,ICAO]*`
- **示例**:
  - `"MUC,FRA,BER"` - 白名单：仅在 MUC/FRA/BER 机场允许
  - `"not JFK,LAX"` - 黑名单：禁止在 JFK/LAX 机场

## 实现详情

### 代码位置
- **文件**: `extensions/app_magic/magic_cic.c`

### MCAR 处理 (客户端认证请求)

在 MCAR 处理流程中添加了 **Step 3b: ARINC 839 会话激活条件验证**：

```
MCAR 处理流程:
  Step 1: 格式解析与安全校验 (mcar_step1_validation)
  Step 2: 身份鉴权 (mcar_step2_auth)
  Step 3: 订阅处理 (mcar_step3_subscription)
  Step 3b: ARINC 839 会话激活条件验证 (mcar_step3b_session_conditions) ← 新增
  Step 4: 0-RTT 资源申请 (mcar_step4_allocation)
  Step 5: 构建并发送应答 (mcar_step5_finalize)
```

### MCCR 处理 (通信变更请求)

在 MCCR Phase 2 的安全检查中添加了验证逻辑：

```
MCCR 处理流程:
  Phase 1: 会话验证 (mccr_phase1_session_validation)
  Phase 2: 参数与安全检查 (mccr_phase2_param_security)
           ├── 2.3.5.1 验证 Flight-Phase AVP
           ├── 2.3.5.2 验证 Altitude AVP 白/黑名单
           └── 2.3.5.3 验证 Airport AVP 白/黑名单
  Phase 3: 意图路由 (mccr_phase3_intent_routing)
  Phase 4: 执行与响应 (mccr_phase4_execute)
```

### 数据结构扩展

`CommReqParams` 结构体新增字段：

```c
/* Altitude AVP 解析结果 */
bool altitude_is_blacklist;             /* true=黑名单模式, false=白名单模式 */
int altitude_ranges[10][2];             /* 高度范围数组 [min, max], max=-1 表示无上限 */
int altitude_range_count;               /* 有效高度范围数量 */

/* Airport AVP 解析结果 */
bool airport_is_blacklist;              /* true=黑名单模式, false=白名单模式 */
char airport_codes[20][8];              /* 机场代码数组 (3-4字符ICAO代码) */
int airport_code_count;                 /* 有效机场代码数量 */
```

### 解析函数

#### `parse_altitude_avp()`
解析 Altitude AVP 字符串到结构化数据：
- 支持白名单/黑名单模式 (`not` 前缀)
- 支持多个高度范围，逗号分隔
- 支持开放上界 (`20000-` 表示 20000 ft 以上)

#### `parse_airport_avp()`
解析 Airport AVP 字符串到结构化数据：
- 支持白名单/黑名单模式 (`not` 前缀)
- 支持多个机场代码，逗号分隔
- 自动去除空格

### ADIF 集成

验证逻辑通过 ADIF (飞机数据接口功能) 获取实时飞机状态：
- **Weight on Wheels (WoW)**: 判断飞机是否在地面
- **高度**: 从 `aircraft_state.position.altitude_ft` 获取
- **飞行阶段**: 从 `aircraft_state.flight_phase.phase` 获取

### 飞行阶段映射

ADIF 飞行阶段到配置飞行阶段的映射：

| ADIF Phase | Config Phase |
|------------|--------------|
| FLIGHT_PHASE_GATE | CFG_FLIGHT_PHASE_GATE |
| FLIGHT_PHASE_TAXI | CFG_FLIGHT_PHASE_TAXI |
| FLIGHT_PHASE_TAKEOFF | CFG_FLIGHT_PHASE_TAKE_OFF |
| FLIGHT_PHASE_CLIMB | CFG_FLIGHT_PHASE_CLIMB |
| FLIGHT_PHASE_CRUISE | CFG_FLIGHT_PHASE_CRUISE |
| FLIGHT_PHASE_DESCENT | CFG_FLIGHT_PHASE_DESCENT |
| FLIGHT_PHASE_APPROACH | CFG_FLIGHT_PHASE_APPROACH |
| FLIGHT_PHASE_LANDING | CFG_FLIGHT_PHASE_LANDING |

## 错误代码

当会话激活条件验证失败时，返回以下错误代码：

| Magic Status Code | 含义 |
|-------------------|------|
| 1020 | SESSION_DENIED_FLIGHT_PHASE - 飞行阶段限制 |
| 1021 | SESSION_DENIED_ALTITUDE - 高度限制 |
| 1022 | SESSION_DENIED_AIRPORT - 机场限制 |

## 待完成项

### 1. 机场代码获取
当前实现中，机场代码获取使用占位符。需要实现以下方式之一：
- 从 ADIF 位置数据反向地理编码获取最近机场
- 从机场数据库查询飞机坐标附近的机场
- 从会话/配置文件中预设的机场信息

### 2. 高度和机场的持续验证
当前动态验证仅支持飞行阶段检查。需要在会话中保存原始 `CommReqParams`，以便在 ADIF 状态变化时验证高度范围和机场限制。

## 动态策略调整 (v2.4)

### 功能概述
MAGIC服务端实现了基于ADIF飞机状态变化的动态策略调整功能。当飞机状态（WoW、高度、飞行阶段等）发生变化时，系统会自动检查所有活动会话是否仍满足激活条件，并终止不合规的会话。

### 实现机制

#### 1. ADIF状态变化回调
- **回调函数**: `on_adif_state_changed()` ([magic_cic.c](extensions/app_magic/magic_cic.c#L4997))
- **注册位置**: [app_magic.c](extensions/app_magic/app_magic.c#L275)
- **触发时机**: 当ADIF客户端接收到新的飞机状态数据时自动触发

#### 2. 会话激活条件验证
- **验证函数**: `check_session_activation_conditions()` ([magic_cic.c](extensions/app_magic/magic_cic.c#L4937))
- **验证内容**:
  - 飞行阶段限制：检查当前飞行阶段是否在客户端配置的允许阶段列表中
  - 未来扩展：高度范围和机场白/黑名单（需保存会话的CommReqParams）

#### 3. 不合规会话处理
当会话不再满足激活条件时，系统执行以下操作：
1. 设置会话状态为 `SESSION_STATE_TERMINATED`
2. 通过 MIH 释放链路资源 (`RESOURCE_ACTION_RELEASE`)
3. 清除数据平面路由表项
4. 记录日志说明终止原因

### 工作流程

```
ADIF 状态变化
    ↓
on_adif_state_changed() 回调触发
    ↓
获取所有活动会话 (ACTIVE + AUTHENTICATED)
    ↓
对每个会话执行:
    ├─ 获取客户端配置文件
    ├─ 验证飞行阶段是否允许
    └─ 如果违反条件:
        ├─ 终止会话
        ├─ 释放链路资源
        └─ 清除路由
    ↓
记录验证结果
```

### 日志示例

```
[app_magic] ========================================
[app_magic] ADIF State Changed - Validating Sessions
[app_magic] WoW=0, Alt=25000.0 ft, Phase=CRUISE
[app_magic] Checking 3 active sessions
[app_magic]   ✗ Session abc123 violates activation conditions, terminating
[app_magic]   ✓ Session def456 activation conditions OK
[app_magic]   ✓ Session ghi789 activation conditions OK
[app_magic] Session validation complete: 1/3 terminated
[app_magic] ========================================
```

### API 新增

#### magic_session.h
```c
int magic_session_get_active_sessions(SessionManager* mgr, 
                                     ClientSession** sessions, 
                                     int max_count);
```
获取所有活跃会话（ACTIVE和AUTHENTICATED状态），用于动态策略验证。

### 配置要求

客户端配置文件中的飞行阶段限制通过 `<Session>` 元素的 `<AllowedPhases>` 配置：

```xml
<Client_Profile>
  <Session>
    <AllowedPhases>
      <Phase>GATE</Phase>
      <Phase>TAXI</Phase>
      <Phase>CRUISE</Phase>
    </AllowedPhases>
  </Session>
</Client_Profile>
```

## 测试建议

### 1. Flight-Phase 静态测试
```bash
# 在地面状态发送只允许空中的请求
./test_client --flight-phase "CRUISE" --adif-phase "GATE"
# 预期: 拒绝，返回 1020
```

### 2. Altitude 静态测试
```bash
# 当前高度 5000ft，请求黑名单 0-10000ft
./test_client --altitude "not 0-10000" --adif-alt 5000
# 预期: 拒绝，返回 1021
```

### 3. Airport 静态测试
```bash
# 当前机场 MUC，请求黑名单 MUC,FRA
./test_client --airport "not MUC,FRA" --adif-airport "MUC"
# 预期: 拒绝，返回 1022
```

### 4. 动态策略调整测试
```bash
# 步骤1: 启动 ADIF 模拟器，设置飞机在地面
./adif_simulator --wow on_ground --phase GATE

# 步骤2: 客户端建立会话（配置允许地面阶段）
./magic_client connect --profile "ground_profile"
# 预期: 成功建立会话

# 步骤3: 模拟飞机起飞，改变状态为空中
./adif_simulator set --wow airborne --phase TAKEOFF
# 预期: MAGIC服务端检测到状态变化，终止不符合条件的会话
# 日志显示: "Session xxx violates activation conditions, terminating"

# 步骤4: 验证会话已被终止
./magic_client status
# 预期: 显示会话状态为 TERMINATED
```

### 5. 飞行阶段限制动态测试
```bash
# 配置客户端仅允许CRUISE阶段
# Client_Profile.xml: <AllowedPhases><Phase>CRUISE</Phase></AllowedPhases>

# 在CRUISE阶段建立会话
./adif_simulator set --phase CRUISE
./magic_client connect
# 预期: 成功

# 切换到DESCENT阶段
./adif_simulator set --phase DESCENT
# 预期: 会话被自动终止，日志显示飞行阶段违规
```

## 参考规范

- ARINC 839-5: Mobile Air-Ground Interoperability and Connectivity (MAGIC)
- ARINC 834: Aircraft Data Interface Function (ADIF)

## 版本历史

| 版本 | 日期 | 描述 |
|------|------|------|
| 1.0 | 2025-01 | 初始实现：Flight-Phase, Altitude, Airport AVP 验证 |
| 2.4 | 2025-01 | 新增动态策略调整：ADIF状态变化触发会话验证和自动终止 |
