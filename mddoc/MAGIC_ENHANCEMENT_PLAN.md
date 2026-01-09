# MAGIC 系统增强计划与方案文档

**版本**: 1.0  
**日期**: 2025-12-28  
**文档编号**: MAGIC-PLAN-001  
**依据**: ARINC 839-2014, ARINC 842

---

## 1. 概述

本文档基于当前 MAGIC 系统代码审查结果，对照 ARINC 839-2014 规范，针对已识别的功能差距，提出具体的增强计划和技术方案。旨在将当前"基础合规"的系统升级为"完全合规"并具备高级特性的生产级系统。

## 2. 差距分析与增强目标

| 编号 | 规范对应章节 | 功能领域 | 当前状态 | 增强目标 | 优先级 |
|------|-------------|---------|---------|---------|-------|
| 1 | §2.4.2 | Federated MAGIC | 不支持多实例协作 | 支持多 MAGIC 实例间状态同步与策略协调 | TBD |
| 2 | §3.2.5.7 | Aircraft Parameters | ⚠️ 部分实现 | 完整集成 ADIF 协议，实时获取航电参数 | 高 |
| 3 | §4.2 | Link Mgmt Interface | ✅ 已实现 MIH | 实现标准化 IEEE 802.21 MIH 线级协议 | N/A |
| 4 | §3.2.4.1.3 | Ground-to-Air Mgmt | 无专用接口 | 增加地面 QoS 控制与策略推送接口 | TBD |

---

## 3. 详细增强方案

### 3.1 方案 A: Federated MAGIC (联邦架构支持) - *暂缓*

> [!NOTE]
> 根据 ARINC 839 规范，MAGIC-to-MAGIC 接口属于"Future supplements"，在 Phase 1 阶段暂不实施。以下方案仅作为未来参考。

**目标**: 实现多个 MAGIC 单元（如 AIS 域主控、PIES 域主控）之间的协同工作，共享链路状态并协调带宽使用。

#### 技术架构 (草案)
1. **MAGIC-to-MAGIC 接口 (M2M)**:
   - 基于 Diameter 协议扩展，定义新的 Application ID (MAGIC-M2M)。
2. **分布式策略引擎**:
   - 升级 `magic_policy.c`，增加"远程链路"概念。

---

### 3.2 方案 B: ADIF (Aircraft Data Interface Function) 深度集成 - ✅ **已完成**

> [!SUCCESS]
> ADIF 客户端已完整实现 (`magic_adif.c`, 702行)，**策略引擎集成已完成**。
> 现已支持实时位置数据传递和 WoW (Weight on Wheels) 感知策略。

**目标**: 实时获取飞行阶段、高度、位置等参数，用于以"场景感知"的方式进行策略决策（例如：低空禁用 SATCOM，地面禁用 WiFi）。

#### 已完成部分 ✅
1. **ADIF 客户端** (`magic_adif.c`):
   - ✅ 完整实现 **ARINC 834 ADBP** 协议
   - ✅ 线程安全的状态缓存
   - ✅ 回调机制支持

2. **CIC 集成**:
   - ✅ MCAR 时自动从 ADIF 填充 Flight-Phase (L2048)
   - ✅ MCAR/MCCR 处理时获取实时位置和 WoW 数据 (v2.2)

3. **PolicyRequest 扩展** (v2.2):
   - ✅ `aircraft_lat/lon/alt` 字段正确填充
   - ✅ `on_ground` 字段 (WoW 状态)
   - ✅ `has_adif_data` 有效标志

4. **策略引擎覆盖检查** (v2.2):
   - ✅ 已启用基于实时位置的 DLM 覆盖范围检查
   - ✅ WoW 感知策略支持 (`on_ground_only`, `airborne_only`)

5. **XML 配置支持** (v2.2):
   - ✅ `PathPreference` 新增 `on_ground_only` 属性
   - ✅ `PathPreference` 新增 `airborne_only` 属性
   - ✅ `Central_Policy_Profile.xml` 已更新示例

#### 实施步骤 (已完成 ✅)

**步骤 1**: ✅ 修改 `magic_cic.c` 的 MCAR/MCCR 处理
```c
// 在调用策略引擎前，从 ADIF 获取实时数据
AdifAircraftState adif_state;
if (adif_client_get_state(&g_ctx->adif_ctx, &adif_state) == 0 &&
    adif_state.data_valid) {
    policy_req.aircraft_lat = adif_state.position.latitude;
    policy_req.aircraft_lon = adif_state.position.longitude;
    policy_req.aircraft_alt = adif_state.position.altitude_ft * 0.3048; // ft->m
    policy_req.on_ground = adif_state.wow.on_ground;
    policy_req.has_adif_data = true;
}
```

**步骤 2**: ✅ 扩展 `PolicyRequest` 结构
```c
typedef struct {
    // ... 现有字段 ...
    double aircraft_lat;        // 飞机纬度 (度)
    double aircraft_lon;        // 飞机经度 (度)
    double aircraft_alt;        // 飞机高度 (米)
    bool   on_ground;           // Weight on Wheels 状态
    bool   has_adif_data;       // ADIF 数据有效标志
} PolicyRequest;
```

**步骤 3**: ✅ 启用策略引擎覆盖检查
- 已移除 `magic_policy.c` 中的 TODO 注释
- 覆盖检查现在基于 `has_adif_data` 有效标志生效

**步骤 4**: ✅ 添加 WoW 感知策略规则
- 已在 `PathPreference` 中添加 `on_ground_only` 和 `airborne_only` 属性
- 已在 `Central_Policy_Profile.xml` 中添加示例配置
- 策略引擎根据 `on_ground` 自动选择地面优先链路 (如 WiFi)

---

### 3.3 方案 C: 标准化 MIH 协议实现

**目标**: 将 LMI 接口从自定义 IPC 升级为符合 IEEE 802.21 规范的二进制线级协议，提高与第三方 DLM (Data Link Module) 的互操作性。

#### 技术架构
1. **MIH 帧编解码器**:
   - 实现 IEEE 802.21 TLV (Type-Length-Value) 编码格式。
   - 支持 MIH Header 解析。

2. **传输层抽象**:
   - 保持当前的 UDP/Unix Socket 传输方式，但负载载荷改为 MIH 标准帧。

3. **E-MIH (Enhanced MIH)**:
   - ARINC 839 在 Attachment 2 中定义了 `Link_Resource` 等扩展原语。
   - 确保编解码器支持 Vendor-Specific TLV 来承载这些扩展。

#### 实施步骤
1. 开发 `libmih` 静态库，处理编解码。
2. 重构 `magic_lmi.c`，替换现有的 `IpcHeader` 结构。
3. 更新 DLM 模拟器以支持新协议进行测试。

---

## 4. 实施路线图 (Roadmap)

### Phase 1: 基础增强 (Q1 2026)
- [x] 完成 **方案 B (ADIF 策略集成)** - 将实时位置/高度数据传递给策略引擎。✅ 已于 2025-12-29 完成
- [ ] 优化现有代码库，消除审查中发现的静态并发风险。

### Phase 2: 互操作性增强 (Q2 2026)
- [ ] 实施 **方案 C (MIH Protocol)**，确保能对接第三方提供的卫星/地面基站 DLM。

### Phase 3: 高级架构 (TBD)
- [ ] 实施 **方案 A (Federated MAGIC)** (待规范进一步明确)。

## 5. 资源需求

- **开发人力**: 高级 C 开发工程师 1 名，测试工程师 1 名。
- **测试环境**: 
  - 需要模拟多台虚拟机的测试网络以验证 Federal MAGIC。
  - 需要 ADIF 模拟器回放真实的飞行数据包。

---

**批准**: 
_待定_
