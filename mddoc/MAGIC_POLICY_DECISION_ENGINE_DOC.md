# MAGIC 策略引擎决策机制详解文档 (v2.4)

## 1. 概述 (Overview)

MAGIC (Multi-link Aggregation Gateway for Internet Connectivity) 的核心是一个智能策略引擎 (`magic_policy.c`)。它负责在多条可能的通信链路（卫星 Satcom、蜂窝网络 ATG/Cellular、机场 WiFi）中，为每一个客户端请求动态选择**最佳**的传输路径。

决策过程融合了以下关键要素：
- **静态配置**: 中央策略文件、客户端画像、链路特性
- **实时状态**: 链路在线状态、带宽占用、延迟性能
- **环境感知**: ADIF 提供的飞行阶段、地理位置、WoW (Weight on Wheels) 状态
- **业务需求**: QoS 级别、优先级类、带宽要求

---

## 2. 核心设计思想 (Decision Philosophy)

策略引擎的设计遵循以下层次化决策原则：

### 2.1 硬性约束 (Hard Constraints)
这些条件不满足则**直接淘汰**该链路，不进入评分阶段：

1. **安全策略**: 标记为 `PROHIBIT` 的链路绝不使用
2. **权限控制**: 客户端 `AllowedDLMs` 白名单之外的链路禁止
3. **物理可达性**:
   - 链路状态为 `Offline` 的不可用
   - 不在 ADIF 覆盖范围内的链路排除
4. **性能阈值**: 
   - 延迟超过 `max_latency_ms` 限制的链路淘汰（返回 -999999 分）
   - 带宽不足的链路扣除 5000 分
5. **飞行阶段匹配** (v2.2+):
   - `on_ground_only`: 仅地面可用，飞机空中时排除
   - `airborne_only`: 仅空中可用，飞机在地面时排除

### 2.2 软性优化 (Soft Optimization)
通过评分机制选择最优解：

1. **策略排名优先**: 管理员配置的 `ranking` 权重最高（1000 倍系数）
2. **负载均衡**: 带宽余量大的链路加分
3. **性能偏好**: 低延迟链路加分，链路类型稳定性加分
4. **客户偏好**: 客户端配置的 `PreferredDLM` 获得额外奖励
5. **稳定性保护**: 防抖动机制避免频繁切换

---

## 3. 关键数据结构详解

### 3.1 策略请求 (PolicyRequest)
来自 CIC 的请求，包含决策所需的全部输入：

```c
typedef struct {
    char client_id[64];              // 客户端标识 (Origin-Host)
    char profile_name[64];           // 配置文件名称
    char flight_phase[32];           // 飞行阶段 (GATE/TAXI/CRUISE...)
    
    uint32_t requested_bw_kbps;      // 请求前向带宽
    uint32_t requested_ret_bw_kbps;  // 请求返回带宽
    uint32_t required_bw_kbps;       // 必需前向带宽
    uint32_t required_ret_bw_kbps;   // 必需返回带宽
    
    uint8_t priority_class;          // 优先级类 (1-9, 1最高)
    uint8_t qos_level;               // QoS级别
    
    // v2.2: ADIF 飞机状态数据
    bool has_adif_data;              // ADIF 数据是否有效
    bool on_ground;                  // Weight on Wheels 状态
    double aircraft_lat;             // 飞机纬度
    double aircraft_lon;             // 飞机经度
    double aircraft_alt;             // 飞机高度 (米)
} PolicyRequest;
```

### 3.2 策略响应 (PolicyResponse)
返回给 CIC 的决策结果：

```c
typedef struct {
    bool success;                    // 决策是否成功
    char selected_link_id[64];       // 选中的链路 ID
    char matched_traffic_class[64];  // 动态分类结果
    uint32_t granted_bw_kbps;        // 授予的带宽
    uint32_t granted_ret_bw_kbps;    // 授予的返回带宽
    char reason[256];                // 失败原因
} PolicyResponse;
```

---

## 4. 决策过程详解 (The Logic Flow)

当一个客户端发起连接请求 (Session Request) 时，策略引擎 (`magic_policy_select_path`) 执行以下 6 步逻辑：

### 步骤 1: 客户端准入与校验
*   **输入**: Client ID, 请求带宽。
*   **逻辑**:
    1.  检查客户端是否存在且启用。
    2.  检查请求带宽是否超过 `Client_Profile` 中的 `MaxForward`。
*   **结果**: 如果超限，直接拒绝连接。

### 步骤 2: 环境感知与规则集加载
*   **输入**: 当前飞行阶段 (`flight_phase`)。
*   **逻辑**: 在 `Central_Policy_Profile` 中查找匹配当前阶段的 `<PolicyRuleSet>`。
    *   *例如：如果现在是 `GATE` (登机口)，加载地面策略集；如果是 `CRUISE` (巡航)，加载高空策略集。*
*   **思想**: **上下文感知**。同样的业务在地面和空中可能有完全不同的选路逻辑。

### 步骤 3: 动态流量分类 (Dynamic Classification)
*   **输入**: 客户端的 `PriorityClass`, `QoSLevel`, `ProfileName`。
*   **逻辑**: 
    1.  **规则匹配**: 遍历 `TrafficClassDefinitions`，按以下顺序尝试匹配：
        *   **PriorityClass**: 如果定义了且相等，匹配成功。
        *   **QoSLevel**: 如果定义了且相等，匹配成功。
        *   **ProfileName**: 支持通配符 (`*`, `?`) 匹配。
    2.  **默认回退**: 如果没有匹配到任何定义，使用默认类别 (通常为 `BEST_EFFORT`)。
    3.  **策略查找**: 使用分类结果在当前规则集 (`PolicyRuleSet`) 中查找对应规则。
        *   **精确匹配**: 查找 `traffic_class` 完全一致的规则。
        *   **通配回退**: 如果没找到，查找 `ALL_TRAFFIC` 规则。
        *   **优先级回退**: 如果还没找到，尝试查找 `PRIORITY_X` (X为优先级数值) 规则。
*   **思想**: **多级回退机制**确保任何请求都能找到对应的策略，避免因配置遗漏导致拒绝服务。

### 步骤 4: 候选链路过滤 (The Sieve)
*   **逻辑**: 拿到所有物理链路，进行层层筛选：
    1.  **配置过滤**: 策略规则中标记为 `PROHIBIT` 的链路 → **剔除**。
    2.  **权限过滤**: 客户端 `AllowedDLMs` 名单中没有的链路 → **剔除**。
    3.  **状态过滤**: 链路当前处于 Down 状态或未初始化 → **剔除**。
    4.  **覆盖过滤**: (v2.0) 结合 ADIF 数据，检查飞机经纬度和高度是否在 DLM 定义的 `CoverageConfig` 范围内。
        *   高度转换: 1米 ≈ 3.28084英尺。
    5.  **性能过滤**: (v2.0) 如果策略要求 `max_latency_ms` (如300ms)，而链路实际延迟超过此值 → **剔除** (返回 -999999 分)。
    6.  **WoW过滤**: (v2.2) 
        *   `on_ground_only`: 飞机在空中时剔除。
        *   `airborne_only`: 飞机在地面时剔除。

### 步骤 5: 链路评分与竞选 (Scoring & Ranking)
*   **逻辑**: 对剩下的候选链路打分，分数高者得。
    $$ TotalScore = BaseScore + BWScore + LatencyScore + TypeScore + BonusScore - LoadPenalty $$
    
    *   **BaseScore (策略排名)**: `(10 - ranking) * 1000`。
        *   *Ranking 1 = 9000分, Ranking 2 = 8000分*。这是基准分。
    *   **BWScore (带宽余量)**: `(链路最大带宽 - 请求带宽) / 100`。
        *   *每 100kbps 余量 +1 分*。
        *   *如果带宽不足 (Max < Req)，直接扣除 5000 分*。
    *   **LatencyScore (延迟)**: 
        *   `< 50ms`: +100 分
        *   `> 500ms`: -50 分
    *   **TypeScore (类型)**: 卫星+5，混合+4，蜂窝+3。
    *   **BonusScore (首选奖励)**: 如果是客户端 `PreferredDLM`，+500分。
    *   **LoadPenalty (负载均衡)**: `active_sessions * 600`。
        *   *关键逻辑*: 每多一个活跃会话扣 600 分。这意味着 **2 个活跃会话 (1200分) 的惩罚足以抵消 1 级 Ranking (1000分) 的优势**。这实现了强力的跨链路负载均衡。

### 步骤 6: 防抖动检查 (Stability Check)
*   **场景**: 如果客户端已经连接在 Link A 上，现在评分发现 Link B 分数略高。
*   **逻辑**:
    1.  **最小驻留时间**: 检查在 Link A 上呆的时间是否超过 `MinDwellTime` (如60秒)。没超过则不切。
    2.  **信号迟滞 (Hysteresis)**: 
        *   使用 **可用带宽百分比** 作为信号质量指标。
        *   公式: `NewBW% < CurrentBW% + (CurrentBW% * Hysteresis% / 100)`
        *   *含义*: 新链路的质量必须比当前链路好出 `Hysteresis%` (如10%) 以上，否则视为抖动，不进行切换。
*   **思想**: **稳定性大于微小的性能提升**。

---

## 5. 动态策略执行 (Dynamic Policy Enforcement)

除了初始选路，MAGIC 还通过 ADIF 接口实时监控飞机状态，并对已建立的会话进行持续合规性检查。

### 5.1 触发机制
当 ADIF 报告以下状态变更时，触发全量会话检查 (`on_adif_state_changed`)：
*   **WoW (Weight on Wheels)**: 飞机起飞或降落。
*   **飞行阶段 (Flight Phase)**: 如从 CLIMB 进入 CRUISE。
*   **位置/高度**: 经纬度或高度发生显著变化。

### 5.2 检查逻辑 (Session Validation)
对每一个 `ACTIVE` 状态的会话，执行 `check_session_activation_conditions`：

1.  **飞行阶段合规性**:
    *   将 ADIF 实时阶段映射为配置阶段 (如 `FLIGHT_PHASE_TAKEOFF` -> `CFG_FLIGHT_PHASE_TAKE_OFF`)。
    *   检查客户端 Profile 中的 `AllowedFlightPhases` 是否包含当前阶段。
    *   *违规后果*: 如果当前阶段不被允许，**立即终止会话**。

2.  **高度/机场限制 (v2.3)**:
    *   检查会话建立时指定的 `Altitude` 限制 (如 "10000-") 是否仍满足。
    *   检查 `Airport` 限制是否满足。

3.  **资源释放**:
    *   一旦判定违规，立即通过 MIH 发送 `RESOURCE_ACTION_RELEASE` 释放链路资源。
    *   清除数据平面路由。
    *   (TODO) 向客户端发送 STR (Session Termination Request)。

---

## 6. 典型决策案例演示

### 案例 A: 飞机在登机口 (GATE)，机务下载数据
*   **环境**: 地面，有 WiFi (Gatelink)，有 4G，有卫星。
*   **客户端**: 维护终端 (Priority: Low, Allowed: WiFi, Cellular)。
*   **过程**:
    1.  规则集锁定 `GROUND_OPS`。
    2.  策略定义 WiFi 为 Ranking 1，4G 为 Ranking 2，卫星为 PROHIBIT。
    3.  **评分**: WiFi 获得 Ranking 1 的高分 (9000分) + 带宽极大 + 低延迟奖励。
    4.  **结果**: 选中 **WiFi**。如果 WiFi 断了，回退到 4G。绝不会走卫星（因为 PROHIBIT 且客户端不允许）。

### 案例 B: 飞机巡航中 (CRUISE)，飞行员使用 EFB
*   **环境**: 高空，只能用卫星 (Satcom) 或 ATG (Cellular)。
*   **客户端**: EFB (Priority: High, Allowed: Satcom, WiFi)。注意：EFB配置里可能不允许 Cellular。
*   **过程**:
    1.  规则集锁定 `HIGH_ALTITUDE_OPS`。
    2.  分类为 `COCKPIT_DATA`。
    3.  **过滤**: 
        *   WiFi: 高度超限/不在覆盖区 → 剔除。
        *   Cellular: 客户端 `AllowedDLMs` 不包含 → 剔除。
    4.  **结果**: 唯一候选者 **Satcom** 自动中标。即使 ATG 速度更快，由于安全策略限制（EFB不走公网 ATG），依然走卫星。

### 案例 C: VoIP 语音通话 (对延迟敏感)
*   **环境**: 降落阶段 (APPROACH)，卫星和 ATG 均可用。
*   **规则**: `LOW_ALTITUDE_OPS` 中定义 INTERACTIVE 类流量 `max_latency_ms="300"`。
*   **过程**:
    1.  **过滤**: 卫星链路延迟 600ms > 300ms 阈值 → **直接剔除**。
    2.  **结果**: 选中 **ATG/Cellular** (延迟 < 100ms)。保证了语音不卡顿。

---

## 7. 总结

MAGIC 策略引擎通过 **"分层过滤 + 加权评分"** 的机制，完美平衡了复杂的航空通信需求。它既能通过 XML 配置实现灵活的业务逻辑调整，又能通过硬编码的算法保证决策的实时性和稳定性。同时，结合 ADIF 的**动态策略执行**机制，确保了通信服务始终符合航空安全和运营规范。
