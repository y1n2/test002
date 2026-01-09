# MAGIC 策略引擎说明文档

## 1. 概述

MAGIC 策略引擎是 ARINC-839 MAGIC 协议实现的核心组件，负责根据飞行阶段、业务类型、链路状态和 QoS 需求自动选择最优通信链路。

**核心功能：**
- 多飞行阶段策略支持（地面、低空、高空、大洋）
- 多业务类型分类（BEST_EFFORT、INTERACTIVE、COCKPIT_DATA、BULK_DATA）
- 智能链路评分算法
- 动态链路状态感知
- 带宽保证与 QoS 控制

---

## 2. 策略引擎架构

### 2.1 核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                     MAGIC Policy Engine                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌────────────┐    ┌──────────────┐    ┌─────────────┐     │
│  │  Client    │───▶│  Rule Set    │───▶│  Link       │     │
│  │  Profile   │    │  Matching    │    │  Selection  │     │
│  └────────────┘    └──────────────┘    └─────────────┘     │
│       │                    │                    │            │
│       │                    │                    │            │
│       ▼                    ▼                    ▼            │
│  ┌────────────┐    ┌──────────────┐    ┌─────────────┐     │
│  │ Traffic    │    │ Flight Phase │    │ Link Score  │     │
│  │ Class      │    │ Detection    │    │ Calculator  │     │
│  └────────────┘    └──────────────┘    └─────────────┘     │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 决策流程

```
┌──────────────┐
│ MCCR Request │
└───────┬──────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 步骤 1: 查找客户端配置                │
│  - 验证客户端 ID                      │
│  - 检查带宽限制                        │
│  - 获取业务类型                        │
└───────┬───────────────────────────────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 步骤 2: 匹配策略规则集                │
│  - 根据飞行阶段选择规则集             │
│  - 未找到则使用默认（GROUND_OPS）     │
└───────┬───────────────────────────────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 步骤 3: 匹配策略规则                  │
│  - 根据业务类型匹配规则               │
│  - 未匹配则尝试 ALL_TRAFFIC 通配      │
└───────┬───────────────────────────────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 步骤 4: 链路评分与选择                │
│  - 遍历所有路径偏好                   │
│  - 过滤 PROHIBIT 和离线链路           │
│  - 计算每个链路评分                   │
│  - 选择得分最高的链路                 │
└───────┬───────────────────────────────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 步骤 5: 返回决策结果                  │
│  - 成功：返回链路 ID 和授予带宽       │
│  - 失败：返回错误原因                 │
└───────────────────────────────────────┘
```

---

## 3. 链路评分算法

### 3.1 评分公式

```c
score = 策略排名权重 + 带宽余量权重 + 延迟权重 - 成本权重
```

### 3.2 各因素权重

| 因素 | 权重计算 | 说明 |
|------|---------|------|
| **策略排名** | `(10 - ranking) × 1000` | 最高权重，ranking=1 得 9000 分 |
| **带宽余量** | `(max_bw - req_bw) / 100` | 余量越大加分越多，不足则扣 5000 分 |
| **低延迟奖励** | `+100` | 延迟 < 50ms 加 100 分 |
| **高延迟惩罚** | `-50` | 延迟 > 500ms 扣 50 分 |
| **成本惩罚** | `-cost_index / 10` | 成本指数越高扣分越多 |

### 3.3 评分示例

**场景：请求 10000 kbps 带宽，BEST_EFFORT 业务，GROUND_OPS 策略**

| 链路 | Ranking | 带宽余量 | 延迟 | 成本 | 总分 | 计算详情 |
|------|---------|----------|------|------|------|----------|
| **WIFI** | 1 | 90000 kbps | 5ms | 1 | **9999** | 9000 + 900 + 100 - 1 |
| **CELLULAR** | 2 | 10000 kbps | 50ms | 20 | **8098** | 8000 + 100 + 0 - 2 |
| **SATCOM** | 3 | -7952 kbps | 600ms | 90 | **-2059** | 7000 - 5000 - 50 - 9 |

**结果：WIFI 胜出（最高分 9999）**

---

## 4. 配置文件说明

### 4.1 客户端配置 (Client_Profile.xml)

```xml
<Client id="magic-client.example.com">
    <Username>B8888_EFB01</Username>
    <Password>MyPlaneSecret2025</Password>
    <DefaultProfile>IP_DATA</DefaultProfile>
    
    <!-- 关键：业务类型决定使用哪条策略规则 -->
    <TrafficClass>BEST_EFFORT</TrafficClass>
    
    <!-- 带宽上限（单位：bps） -->
    <MaxBandwidth>100000000</MaxBandwidth>        <!-- 100 Mbps -->
    <MaxReturnBandwidth>50000000</MaxReturnBandwidth>  <!-- 50 Mbps -->
    
    <PriorityClass>2</PriorityClass>
    <Enabled>true</Enabled>
</Client>
```

**业务类型影响：**
- `BEST_EFFORT`: 一般数据业务（地面优先 WIFI）
- `INTERACTIVE`: 交互式业务（优先 CELLULAR）
- `COCKPIT_DATA`: 驾驶舱数据（严格优先 CELLULAR）
- `BULK_DATA`: 大数据传输（优先 WIFI，禁止 SATCOM）

### 4.2 策略规则配置 (Central_Policy_Profile.xml)

```xml
<!-- 地面操作策略：停机坪停靠 -->
<PolicyRuleSet id="GROUND_OPS" flight_phases="PARKED">
    
    <!-- BEST_EFFORT 业务：优先使用便宜的 WIFI -->
    <PolicyRule traffic_class="BEST_EFFORT">
        <PathPreference ranking="1" link_id="LINK_WIFI" />
        <PathPreference ranking="2" link_id="LINK_CELLULAR" />
        <PathPreference ranking="3" link_id="LINK_SATCOM" />
    </PolicyRule>
    
    <!-- INTERACTIVE 业务：优先使用稳定的 CELLULAR -->
    <PolicyRule traffic_class="INTERACTIVE">
        <PathPreference ranking="1" link_id="LINK_CELLULAR" />
        <PathPreference ranking="2" link_id="LINK_WIFI" />
        <PathPreference ranking="3" link_id="LINK_SATCOM" />
    </PolicyRule>
    
    <!-- COCKPIT_DATA：仅允许安全的 CELLULAR -->
    <PolicyRule traffic_class="COCKPIT_DATA">
        <PathPreference ranking="1" link_id="LINK_CELLULAR" />
        <PathPreference ranking="2" link_id="LINK_WIFI" security_required="VPN"/>
    </PolicyRule>
</PolicyRuleSet>

<!-- 低空操作策略：滑行、起飞、进近、降落 -->
<PolicyRuleSet id="LOW_ALTITUDE_OPS" flight_phases="TAXI, TAKEOFF, APPROACH, LANDING">
    
    <!-- 所有业务：禁用 WIFI，优先 CELLULAR -->
    <PolicyRule traffic_class="ALL_TRAFFIC">
        <PathPreference ranking="1" link_id="LINK_CELLULAR" />
        <PathPreference ranking="2" link_id="LINK_SATCOM" />
        <PathPreference ranking="3" link_id="LINK_WIFI" action="PROHIBIT"/>
    </PolicyRule>
</PolicyRuleSet>

<!-- 高空操作策略：巡航、跨洋飞行 -->
<PolicyRuleSet id="HIGH_ALTITUDE_OPS" flight_phases="CRUISE, OCEANIC">
    
    <!-- 所有业务：仅允许 SATCOM -->
    <PolicyRule traffic_class="ALL_TRAFFIC">
        <PathPreference ranking="1" link_id="LINK_SATCOM" />
        <PathPreference ranking="2" link_id="LINK_CELLULAR" action="PROHIBIT"/>
        <PathPreference ranking="3" link_id="LINK_WIFI" action="PROHIBIT"/>
    </PolicyRule>
</PolicyRuleSet>
```

### 4.3 数据链路配置 (Datalink_Profile.xml)

```xml
<Link id="LINK_CELLULAR">
    <LinkName>4G/5G AGT Network</LinkName>
    <DLMDriverID>DLM_CELLULAR_PROC</DLMDriverID>
    
    <Capabilities>
        <MaxTxRateKbps>20000</MaxTxRateKbps>      <!-- 20 Mbps -->
        <TypicalLatencyMs>50</TypicalLatencyMs>   <!-- 50ms RTT -->
    </Capabilities>
    
    <PolicyAttributes>
        <CostIndex>20</CostIndex>                 <!-- 中等成本 -->
        <SecurityLevel>MEDIUM</SecurityLevel>
        <Coverage>TERRESTRIAL</Coverage>          <!-- 地面覆盖 -->
    </PolicyAttributes>
</Link>

<Link id="LINK_WIFI">
    <LinkName>Airport Gatelink</LinkName>
    <DLMDriverID>DLM_WIFI_PROC</DLMDriverID>
    
    <Capabilities>
        <MaxTxRateKbps>100000</MaxTxRateKbps>     <!-- 100 Mbps -->
        <TypicalLatencyMs>5</TypicalLatencyMs>    <!-- 5ms RTT -->
    </Capabilities>
    
    <PolicyAttributes>
        <CostIndex>1</CostIndex>                  <!-- 低成本 -->
        <SecurityLevel>LOW</SecurityLevel>
        <Coverage>GATE_ONLY</Coverage>            <!-- 仅停机坪 -->
    </PolicyAttributes>
</Link>

<Link id="LINK_SATCOM">
    <LinkName>Inmarsat Global Xpress</LinkName>
    <DLMDriverID>DLM_SATCOM_PROC</DLMDriverID>
    
    <Capabilities>
        <MaxTxRateKbps>2048</MaxTxRateKbps>       <!-- 2 Mbps -->
        <TypicalLatencyMs>600</TypicalLatencyMs>  <!-- 600ms RTT -->
    </Capabilities>
    
    <PolicyAttributes>
        <CostIndex>90</CostIndex>                 <!-- 高成本 -->
        <SecurityLevel>HIGH</SecurityLevel>
        <Coverage>GLOBAL</Coverage>               <!-- 全球覆盖 -->
    </PolicyAttributes>
</Link>
```

---

## 5. 故障处理

### 5.1 决策失败原因

| 错误原因 | Result-Code | 说明 |
|---------|-------------|------|
| 客户端未找到 | 5012 | `client_id` 不在 Client_Profile.xml 中 |
| 带宽超限 | 5012 | 请求带宽超过 `MaxBandwidth` |
| 无匹配规则 | 5012 | 飞行阶段或业务类型无对应规则 |
| 无可用链路 | 5012 | 所有链路离线或被 PROHIBIT |
| 所有链路带宽不足 | 5012 | 所有在线链路带宽 < 请求带宽 |

### 5.2 链路不可用原因

1. **链路离线** (`is_active = false`)
   - DLM 未上报 Link_Up.indication
   - 链路物理故障
   - DLM 进程未启动

2. **链路被禁止** (`action="PROHIBIT"`)
   - 策略显式禁止该链路
   - 例：地面禁用 SATCOM，高空禁用 WIFI

3. **带宽不足**
   - 链路 `MaxTxRateKbps` < 请求带宽
   - 评分计算时扣除 5000 分

---

## 6. 日志分析

### 6.1 成功决策日志

```
12:59:08  NOTI   [app_magic] === Policy Decision Start ===
12:59:08  NOTI   [app_magic]   Client: magic-client.example.com
12:59:08  NOTI   [app_magic]   Flight Phase: PARKED
12:59:08  NOTI   [app_magic]   Required BW: 10000 kbps
12:59:08  DEBUG  [app_magic]   Client Profile Found:
12:59:08  DEBUG  [app_magic]     Traffic Class: BEST_EFFORT
12:59:08  DEBUG  [app_magic]     Max BW: 100000 kbps
12:59:08  DEBUG  [app_magic]   Using Ruleset: GROUND_OPS
12:59:08  DEBUG  [app_magic]   Matched Rule: BEST_EFFORT (3 preferences)
12:59:08  DEBUG  [app_magic]     Link LINK_WIFI: score=9999 (ranking=1, bw=100000/10000 kbps, latency=5 ms)
12:59:08  DEBUG  [app_magic]     Link LINK_CELLULAR: score=8098 (ranking=2, bw=20000/10000 kbps, latency=50 ms)
12:59:08  DEBUG  [app_magic]     Link LINK_SATCOM: score=-2059 (ranking=3, bw=2048/10000 kbps, latency=600 ms)
12:59:08  NOTI   [app_magic] ✓ Policy Decision SUCCESS
12:59:08  NOTI   [app_magic]     Client: magic-client.example.com
12:59:08  NOTI   [app_magic]     Selected Link: LINK_WIFI (Airport Gatelink)
12:59:08  NOTI   [app_magic]     Granted BW: 10000/5000 kbps
12:59:08  NOTI   [app_magic]     QoS Level: 2
```

### 6.2 失败决策日志

```
12:51:16  NOTI   [app_magic] === Policy Decision Start ===
12:51:16  NOTI   [app_magic]   Client: magic-client.example.com
12:51:16  ERROR  [app_magic] Requested BW (10000 kbps) exceeds client limit (5000 kbps)
12:51:16  NOTI   [app_magic] ✗ Policy Decision FAILED: Bandwidth limit exceeded
```

---

## 7. 优化建议

### 7.1 改善链路选择

**问题：WIFI 总是优先被选中**

**原因：**
- WIFI 带宽最大（100 Mbps）
- 延迟最低（5ms）
- 成本最低（CostIndex=1）
- 在 BEST_EFFORT 策略中 ranking=1

**解决方案：**

1. **修改客户端业务类型**
   ```xml
   <!-- 改为 INTERACTIVE，优先选择 CELLULAR -->
   <TrafficClass>INTERACTIVE</TrafficClass>
   ```

2. **调整策略规则排名**
   ```xml
   <PolicyRule traffic_class="BEST_EFFORT">
       <PathPreference ranking="1" link_id="LINK_CELLULAR" />
       <PathPreference ranking="2" link_id="LINK_WIFI" />
   </PolicyRule>
   ```

3. **降低 WIFI 的可用带宽**
   ```xml
   <MaxTxRateKbps>5000</MaxTxRateKbps>  <!-- 低于请求带宽 -->
   ```

4. **临时禁用 WIFI**
   ```xml
   <PathPreference ranking="3" link_id="LINK_WIFI" action="PROHIBIT"/>
   ```

### 7.2 增强成本控制

增大成本权重：

```c
/* 修改 magic_policy.c 中的成本权重 */
score -= (link->policy_attrs.cost_index / 5);  // 从 /10 改为 /5
```

**效果：**
- SATCOM 成本 90 → 扣 18 分（原扣 9 分）
- CELLULAR 成本 20 → 扣 4 分（原扣 2 分）
- WIFI 成本 1 → 扣 0.2 分（原扣 0.1 分）

### 7.3 带宽预留机制

当前带宽计算为简单授予：

```c
resp->granted_bw_kbps = req->requested_bw_kbps;
```

**改进建议：**

```c
/* 考虑链路当前负载 */
uint32_t available_bw = link->capabilities.max_tx_rate_kbps - link->current_allocated_bw;
resp->granted_bw_kbps = MIN(req->requested_bw_kbps, available_bw);
```

---

## 8. 常见问题 (FAQ)

### Q1: 为什么修改带宽需求后仍选择同一链路？

**A:** 策略排名权重（9000 vs 8000）远大于带宽余量权重（几百分），只要 ranking=1 的链路在线且带宽满足，就会一直被选中。

**解决：** 修改客户端 `TrafficClass` 或策略规则排名。

### Q2: 如何强制选择特定链路？

**A:** 三种方法：
1. 在策略规则中将目标链路 ranking 设为 1
2. 禁止其他链路（`action="PROHIBIT"`）
3. 临时关闭其他 DLM 进程（链路变为离线）

### Q3: DLM 启动后链路仍显示离线？

**A:** 检查：
1. DLM 是否成功发送 Link_Up.indication 到 `/tmp/mihf.sock`
2. MIHF DGRAM 服务器是否运行（`lsof -U | grep mihf`）
3. 链路 ID 映射是否正确（`/tmp/dlm_cellular.sock` → `LINK_CELLULAR`）
4. 查看服务器日志：`grep "is_active" /tmp/fd_server.log`

### Q4: 如何模拟飞行阶段切换？

**A:** 当前实现中飞行阶段硬编码为 `PARKED`。

**修改方法：**
```c
/* 在 magic_cic.c 中修改 cic_handle_mccr() */
strncpy(policy_req.flight_phase, "CRUISE", sizeof(policy_req.flight_phase) - 1);
```

**未来改进：** 通过 Diameter AVP 或独立的 Flight Phase Update 命令动态切换。

---

## 9. 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 决策延迟 | < 5ms | 从 MCCR 到策略决策完成 |
| 规则匹配 | O(n) | n = 规则数量，通常 < 10 |
| 链路评分 | O(m) | m = 链路数量，通常 3-5 |
| 内存占用 | < 100 KB | 策略引擎上下文 |
| CPU 占用 | < 1% | 决策期间 |

---

## 10. 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 2.0 | 2025-11-28 | 完整策略引擎实现 |
| 1.5 | 2025-11-20 | 增加链路评分算法 |
| 1.0 | 2025-11-15 | 基础策略匹配 |

---

**文档维护：** MAGIC 开发团队  
**最后更新：** 2025-11-28

---

## 深入设计与实现细节（开发者参考）

下面内容面向开发/集成工程师，提供策略引擎的内部数据结构、关键函数伪代码、Diameter AVP 映射、运行时状态、日志与度量项、调优建议以及一组验证步骤。

### 1. 核心数据结构（概要）

- `PolicyContext`
    - `MagicConfig* config`：指向加载的配置（clients、datalinks、policy rulesets）。
    - `bool initialized`：初始化标志。
    - 运行时缓存（可选）：规则索引、Link id → 指针映射以加速查找。

- `PolicyRequest`
    - `char client_id[MAX_ID_LEN]`：来自 Diameter `Origin-Host`。
    - `uint32_t requested_bw_kbps`：请求下行带宽（kbps）。
    - `uint32_t requested_ret_bw_kbps`：请求上行带宽（kbps）。
    - `uint8_t qos_level`：QoS 等级（AVP `QoS-Level`）。
    - `char flight_phase[32]`：飞行阶段（来自 FMS 或默认）。
    - `char traffic_class[32]`：流量类别（来源于客户端配置或 AVP）。

- `PolicyResponse`
    - `bool success`：是否决策成功。
    - `char selected_link_id[MAX_ID_LEN]`：选中的链路 ID。
    - `uint32_t granted_bw_kbps` / `granted_ret_bw_kbps`：授予带宽。
    - `char reason[256]`：失败或成功原因说明（写入日志与返回给 caller）。

---

### 2. 关键函数伪代码（便于理解决策流程）

伪代码展示策略引擎的核心决策流程（简化并保留关键判断）：

```
int magic_policy_select_path(PolicyContext* ctx, const PolicyRequest* req, PolicyResponse* resp) {
        if (!ctx || !req || !resp) return -1;
        zero(resp);

        ClientProfile* client = magic_config_find_client(ctx->config, req->client_id);
        if (!client) { resp->success = false; snprintf(resp->reason,...); return -1; }

        if (req->requested_bw_kbps > client->limits.total_client_bw_kbps) {
                snprintf(resp->reason,...); return -1;
        }

        PolicyRuleSet* ruleset = magic_config_find_ruleset(ctx->config, req->flight_phase);
        if (!ruleset) ruleset = default_ruleset;

        PolicyRule* rule = find_rule_for_traffic_class(ruleset, client->traffic_class_id);
        if (!rule) rule = find_wildcard_rule(ruleset);
        if (!rule) { snprintf(resp->reason,...); return -1; }

        selected = NULL; best_score = -INF;
        for each pref in rule->preferences {
                if (pref.action == PROHIBIT) continue;
                link = magic_config_find_datalink(ctx->config, pref.link_id);
                if (!link || !link->is_active) continue;
                score = calculate_link_score(link, pref, req->requested_bw_kbps);
                if (score > best_score) update selected, best_score;
        }

        if (selected) fill resp success, selected_link_id, granted_bw; else snprintf(resp->reason,...); return 0/-1;
}
```

说明：在实际实现中，应把 `calculate_link_score` 做成可配置（例如权重可从配置文件读取），并将中间评分细节写入调试日志以便调优。

---

### 3. Diameter AVP 与内部映射（关键 AVP 列表）

下表列出 MCCR/MCAR 消息中常用 AVP 与策略引擎内部字段的映射关系（参照 ARINC vendor AVP 编号）：

- `Session-Id` (263) → 用于日志关联与会话追踪
- `Origin-Host` (264) → 映射到 `PolicyRequest.client_id`
- `Origin-Realm` / `Destination-Realm` → 常规路由/域校验
- `Communication-Request-Parameters` (20001, grouped) → 包含业务参数：
    - `Profile-Name` (10040) → 客户端请求模板/配置引用
    - `Requested-Bandwidth` (10021) → `PolicyRequest.requested_bw_kbps`（AVP 单位为 kbps 或 float，请按实现解析）
    - `Required-Bandwidth` (10023) → `PolicyRequest.requested_ret_bw_kbps`
    - `Priority-Class` (10025) → 客户端优先级
    - `QoS-Level` (10027) → `PolicyRequest.qos_level`

策略决策后，服务器在 `Communication-Answer-Parameters` (20002) 中返回：
- `DLM-Name` (10004) → 选中的 `link_id` 或 `NONE`
- `Link-Number` (10012) → 可选的内部编号
- `Granted-Bandwidth` (10051) → 授予带宽
- `Timeout` (10039) → 授予有效期

注意：实现时务必对 AVP 单位、类型（整数、浮点、字符串）做严格解析与校验，并在采样时记录解析错误以便排查。

---

### 4. 运行时状态与并发注意

- `ctx->config`：只在初始化与配置重载（若支持）时变动；运行态时多线程访问需保证只读或使用读写锁。
- 客户请求并发：策略决策函数设计应尽量无锁或使用最小临界区，仅在访问共享运行时计量（例如链路的 current_allocated_bw）时使用原子操作或保护。
- 链路状态变更（Link Up/Down）会由独立线程或消息回调更新 `link->is_active`，建议对该字段采用原子类型或在更新时使用 mutex，读取时可做原子拷贝以减少锁争用。

性能小贴士：将规则集合与 link id 做哈希映射，避免每次决策线性查找配置数组，能显著降低决策延迟。

---

### 5. 日志与度量（必须记录的字段）

关键日志（建议日志级别与信息）：

- INFO/NOTICE：策略决策开始/结束，选中链路，授予带宽，客户 id 与飞行阶段。
- DEBUG：每个候选链路的评分明细（ranking、headroom、latency、cost与最终score）。
- ERROR：因配置缺失、单位解析错误或客户端带宽超限导致的失败原因。

推荐的度量（用于监控/告警）：

- 决策延迟 P50/P95/P99（ms）
- 每分钟决策次数（throughput）
- 决策失败率（按失败原因分类）
- 链路选择分布（各链路被选中次数）
- 链路带宽分配总和与利用率（per-link allocated bw）

这些度量可导出为 Prometheus 指标或写入运营日志以便后续分析。

---

### 6. 可调参数与调优建议

1. 排名权重（ranking）影响最大：若希望带宽与延迟影响更显著，可降低 ranking 权重或增加带宽/延迟权重比重。
     - 建议参数：将 `score += (10 - pref->ranking) * 1000` 中的 1000 调低为 200–500，使带宽余量更容易改变选择。

2. 带宽余量系数：`headroom / 100` 可改为 `headroom / X`（X 越小带宽影响越大）。

3. 成本权重：调整 `cost_index` 除数（如 `/10` 改 `/5`）快速影响 SATCOM 与 CELLULAR 的成本敏感性。

4. 滞后/确认参数（见上文）应与实际数据更新频率匹配；例如心跳每 5 秒一次，时间窗建议为 10–20 秒。

调优流程建议：
 - 首先观察 `DEBUG` 评分日志，确认是哪一项（ranking、headroom、latency、cost）主导选择。
 - 修改对应权重后，用代表性流量与飞行阶段运行 100 次以上统计结果，查看链路选择分布是否达预期。

---

### 7. 验证清单（开发与集成测试必做）

1. 单元测试：对 `calculate_link_score` 覆盖边界情况（带宽恰好相等、延迟极端、cost 高低）并断言输出。
2. 集成测试：在本地模拟 DLM 上报（Link_Up/Link_Down），验证 `is_active` 变化后策略选择正确。
3. 端到端测试：通过 `magic_client` 发起 MCCR/MCAR，验证 AVP 映射、Result-Code 与 `Communication-Answer-Parameters` 内容。
4. 负载测试：在高并发请求（例如 1000 QPS）下测量决策延迟与失败率，确认无锁/最小锁策略有效。

---

文档更新完毕。我将把 TODO 标记为完成：

