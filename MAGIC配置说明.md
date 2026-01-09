# MAGIC 系统配置说明文档

本文档详细说明 ARINC-839 MAGIC (Multi-Link Airborne Global Information Communications) 系统的配置文件结构及参数含义。

## 1. 配置文件概览

MAGIC 系统配置由以下四个核心文件组成：

| 配置文件 | 路径 (示例) | 作用 |
|---------|------------|------|
| **magic_server.conf** | `conf/magic_server.conf` | freeDiameter 服务器主配置，定义网络、TLS 和扩展加载 |
| **Client_Profile.xml** | `extensions/app_magic/config/Client_Profile.xml` | 定义允许接入的客户端、认证信息及带宽限制 |
| **Central_Policy_Profile.xml** | `extensions/app_magic/config/Central_Policy_Profile.xml` | 定义核心路由策略，根据飞行阶段和业务类型选择链路 |
| **Datalink_Profile.xml** | `extensions/app_magic/config/Datalink_Profile.xml` | 定义可用物理链路的属性（带宽、延迟、成本等） |

---

## 2. 服务器主配置 (magic_server.conf)

这是 freeDiameter 守护进程的主配置文件，遵循标准 freeDiameter 配置格式。

### 2.1 基础身份配置
```properties
# 服务器的 Diameter 身份标识 (FQDN 格式)
Identity = "magic-server.example.com";

# 服务器所属的 Diameter 域
Realm = "example.com";
```

### 2.2 网络与端口
```properties
# Diameter 协议监听端口 (默认 3868，这里改为 3870 避免冲突)
Port = 3870;
SecPort = 5870;

# 禁用 TCP (仅使用 SCTP)
No_TCP;

# 禁用 IPv6 (仅使用 IPv4)
No_IPv6;

# 绑定的 IP 地址
ListenOn = "127.0.0.1";
```

### 2.3 扩展模块加载
MAGIC 功能通过扩展模块加载。加载顺序至关重要。

```properties
# 1. 加载 MAGIC 协议字典 (定义 AVP 和命令代码)
# 必须最先加载，否则后续模块无法识别 MAGIC 消息
LoadExtension = "/path/to/dict_magic_839.fdx";

# 2. 加载 MAGIC 应用逻辑 (核心模块)
# 参数为客户端配置文件的路径
LoadExtension = "/path/to/app_magic.fdx" : "/path/to/Client_Profile.xml";

# 3. (可选) 加载调试消息转储模块
LoadExtension = "/path/to/dbg_msg_dumps.fdx" : "0x0080";
```

---

## 3. 客户端配置 (Client_Profile.xml)

该文件定义了所有授权接入 MAGIC 网络的客户端及其服务等级协议 (SLA)。

### 3.1 结构示例
```xml
<ClientProfiles>
    <Client id="magic-client.example.com">
        <Username>B8888_EFB01</Username>
        <Password>MyPlaneSecret2025</Password>
        <TrafficClass>BEST_EFFORT</TrafficClass>
        <MaxBandwidth>100000000</MaxBandwidth>
        <MaxReturnBandwidth>50000000</MaxReturnBandwidth>
        <PriorityClass>2</PriorityClass>
        <Enabled>true</Enabled>
    </Client>
</ClientProfiles>
```

### 3.2 参数详解

| 参数 | 必填 | 说明 |
|------|------|------|
| **id** (属性) | 是 | 客户端的 Diameter Identity (Origin-Host)，必须与请求匹配 |
| **Username** | 是 | 用于 MCAR 认证的用户名 |
| **Password** | 是 | 用于 MCAR 认证的密码 |
| **TrafficClass** | 否 | **关键参数**。定义该客户端的默认业务类型，决定使用哪条路由策略。<br>可选值：`BEST_EFFORT`, `INTERACTIVE`, `COCKPIT_DATA`, `BULK_DATA` |
| **MaxBandwidth** | 否 | 允许的最大下行带宽 (单位: bps)。超过此限制的 MCCR 请求将被拒绝 (错误码 5012) |
| **MaxReturnBandwidth** | 否 | 允许的最大上行带宽 (单位: bps) |
| **PriorityClass** | 否 | 优先级 (0-7)，数字越大优先级越高 |
| **Enabled** | 否 | 是否启用该账户 (`true`/`false`) |

---

## 4. 策略配置 (Central_Policy_Profile.xml)

这是 MAGIC 系统的"大脑"，定义了在不同场景下如何选择链路。

### 4.1 结构层级
1. **PolicyRuleSet** (对应飞行阶段)
2. **PolicyRule** (对应业务类型)
3. **PathPreference** (链路优先级列表)

### 4.2 结构示例
```xml
<CentralPolicyProfile>
    <!-- 飞行阶段：地面操作 -->
    <PolicyRuleSet id="GROUND_OPS" flight_phases="PARKED">
        
        <!-- 业务类型：一般数据 -->
        <PolicyRule traffic_class="BEST_EFFORT">
            <!-- 优先级 1: WIFI (最便宜) -->
            <PathPreference ranking="1" link_id="LINK_WIFI" />
            <!-- 优先级 2: 蜂窝网 -->
            <PathPreference ranking="2" link_id="LINK_CELLULAR" />
            <!-- 优先级 3: 卫星 (最贵) -->
            <PathPreference ranking="3" link_id="LINK_SATCOM" />
        </PolicyRule>
        
        <!-- 业务类型：驾驶舱数据 -->
        <PolicyRule traffic_class="COCKPIT_DATA">
            <!-- 强制使用蜂窝网，要求 VPN -->
            <PathPreference ranking="1" link_id="LINK_CELLULAR" />
            <PathPreference ranking="2" link_id="LINK_WIFI" security_required="VPN"/>
        </PolicyRule>
        
    </PolicyRuleSet>
</CentralPolicyProfile>
```

### 4.3 参数详解

#### `<PolicyRuleSet>`
- **id**: 规则集名称 (如 `GROUND_OPS`, `HIGH_ALTITUDE_OPS`)
- **flight_phases**: 适用的飞行阶段列表，逗号分隔。
    - 可选值: `PARKED`, `TAXI`, `TAKEOFF`, `CLIMB`, `CRUISE`, `APPROACH`, `LANDING`, `OCEANIC`

    下面为每个飞行阶段提供更详细的定义、典型触发条件（可在系统/集成时用作判断依据）、建议的策略映射及示例阈值（如适用）。这些建议既可用于策略文件设计，也可用于与 FMS/地面系统的集成定义。

    - `PARKED`（停机/停靠）
        - 含义：飞机停靠在登机口或机坪，发动机停止或空调/电源切换到地面模式，人员可以进行登机/下机与地面维护工作。
        - 典型触发条件（任选其一或组合）：
            - 速度 < 5 kt 且地面感知为停靠
            - GPS 显示与停机位坐标匹配
            - 机组或地面系统显式报告停靠状态
        - 推荐阈值（参考）：
            - 地速 < 5 kt
            - 高度 < 50 ft
        - 策略建议：优先 `LINK_WIFI`（若覆盖且信任），允许软件包下载、大数据同步与非关键后台任务。

    - `TAXI`（滑行）
        - 含义：从停机位到跑道或跑道回到停机位的过渡阶段。
        - 典型触发条件：
            - 地速在 5–40 kt
            - 切换到地面滑行频段或 ADS-B 滑行状态
        - 推荐阈值：地速 >= 5 kt && < 40 kt
        - 策略建议：如果地面 Gatelink/WiFi 在滑行区可用，可继续优先使用；对于控制类或优先级高的交互业务推荐切换到 CELLULAR（若覆盖）。

    - `TAKEOFF`（起飞）
        - 含义：从滑跑到离地并完成初始爬升的短时间窗口。
        - 典型触发条件：
            - 纵向加速度/舵面变化检测离地
            - GPS/高度变化显示快速上升
        - 推荐阈值：瞬时爬升率 > 若干值（依 FMS），或触地 -> 离地事件
        - 策略建议：暂时禁止高带宽后台任务，优先低延迟且可靠链路（CELLULAR 或内置安全链路），以保障关键业务与控制报文。

    - `CLIMB`（爬升）
        - 含义：离地后到达巡航高度之前的上升阶段。
        - 典型触发条件：
            - 高度从地面上升到设定阈值（示例：>1000 ft）
            - 爬升率稳定在正值
        - 推荐阈值：高度 > 1,000 ft && < 巡航高度（具体值依航班计划）
        - 策略建议：如果仍在陆地覆盖范围内并且蜂窝覆盖良好，优先 CELLULAR；逐步监测覆盖并在需要时准备切换到 SATCOM。

    - `CRUISE`（巡航）
        - 含义：达到并维持巡航高度之后的稳定飞行阶段。
        - 典型触发条件：高度达到巡航层并保持稳定（例如 > 10,000 ft，依据航段）
        - 推荐阈值：维持在巡航高度（例如 > 10,000 ft）且爬升率≈0
        - 策略建议：若处于陆地或近海并可检测到 CELLULAR 覆盖，可优先 CELLULAR；否则在远洋/国际航段优先 `LINK_SATCOM`。对低延迟业务可保留优先策略，带宽受限业务需降级或排队。

    - `APPROACH`（进近）
        - 含义：从巡航/下降阶段进入目的地机场空域，逐步降低高度并准备着陆。
        - 典型触发条件：进入目的地空域，下降率或高度低于某一阈值（例如 < 3,000 ft）
        - 推荐阈值：高度 < 3,000 ft
        - 策略建议：限制非关键流量，优先保证控制与关键交互业务（CELLULAR 优先，且如果 WiFi 可用只在接地后使用）。

    - `LANDING`（着陆）
        - 含义：着陆触地并完成滑回停机位前的阶段。
        - 典型触发条件：触地点传感器或 FMS 报告着陆完成
        - 推荐阈值：触地事件 + 速度/高度下降
        - 策略建议：触地后短时间内仍保持关键链路，随后可逐步恢复地面 WiFi 并允许大流量传输。

    - `OCEANIC`（远洋/海上航段）
        - 含义：长时间处于远离陆地的航段，蜂窝覆盖通常不可用。
        - 典型触发条件：航路/位置显示在海洋/远洋空域，或飞行计划进入跨洋段
        - 策略建议：优先 `LINK_SATCOM`，并限制交互式/实时业务，适配高延迟（例如增加缓冲、延迟容忍策略）。

    重要实现建议：
    - 阶段判定可以综合高度、地速、航路（flight plan）、ADS-B/ATC 信号和 FMS 状态；建议在集成时定义清晰的优先判断序列与阈值。
    - 为避免频繁切换（抖动），实现时应加入滞后/确认逻辑（例如：连续 N 次满足阈值或保持 T 秒后切换）。
    - 在 `Central_Policy_Profile.xml` 中为关键阶段（如 `LOW_ALTITUDE_OPS`/`HIGH_ALTITUDE_OPS`）保留明确规则，避免依赖动态判断导致行为不确定。

    日志与验证：
    - 在服务器日志中观察 `Policy Decision` 与 `Flight Phase` 字段以验证阶段映射是否正确。
    - 在测试中，建议逐步模拟高度/位置变化并观察链路选择与日志输出，记录每次切换的触发条件与时间戳以验证滞后行为。

## 阶段切换、滞后 (Hysteresis) 与触发源

为了避免因瞬时噪声或测量误差导致的频繁切换（抖动），建议在实现阶段切换时加入滞后与确认逻辑，并明确可用的触发源。

1) 滞后/确认（推荐实现）
    - 连续判定法：要求连续 N 次采样满足新阶段条件才执行切换（推荐 N=3）。
    - 时间窗法：要求条件在时间窗 T 内持续满足才切换（推荐 T=10 秒）。
    - 双阈值法（上下阈值）：对高度或速度等连续值使用上下阈值区分进入/退出条件。例如：进入 CLIMB：高度 > 1,200 ft；退出 CLIMB（回退到巡航前）：高度 < 800 ft。
    - 组合建议：对关键切换（如 CELLULAR ⇄ SATCOM）同时使用时间窗和双阈值以最大化稳定性。

2) 触发源优先级与说明
    - Flight Management System (FMS): 最可信的来源，优先级最高，用于提供飞行计划与阶段建议。
    - GPS/Navigation: 用于地理围栏判断（例如进入机场范围触发 PARKED/TAXI）。
    - ADS‑B / ATC 状态: 可作为辅助触发源，尤其是在地面管制交互时。
    - Crew / Manual Operator: 手工触发（例如维护模式）应优先级次之并记录审计日志。
    - 地面系统/运营指令: 可通过地面指令覆盖自动判断（需审计与确认）。
    - 注意：所有触发源都应带有时间戳与来源标识，服务器在冲突时应按优先级或最新时间戳决定。

3) 实现注意事项
    - 在接入 FMS 或地面系统时约定接口与消息格式（例如 FlightPhaseUpdate 消息或 Diameter 扩展 AVP）。
    - 在策略引擎中记录阶段变更历史（带时间戳、触发源与理由），便于审计与回溯。

## 示例 PolicyRuleSet（按阶段）

下面给出若干示例片段，供配置参考与直接复制到 `Central_Policy_Profile.xml`：

1) 停机/地面（GROUND_OPS） - 优先 WIFI（示例）

```xml
<PolicyRuleSet id="GROUND_OPS" flight_phases="PARKED">
    <PolicyRule traffic_class="BEST_EFFORT">
        <PathPreference ranking="1" link_id="LINK_WIFI" />
        <PathPreference ranking="2" link_id="LINK_CELLULAR" />
        <PathPreference ranking="3" link_id="LINK_SATCOM" />
    </PolicyRule>
</PolicyRuleSet>
```

2) 低空/起降（LOW_ALTITUDE_OPS） - 禁用 WIFI，优先 CELLULAR

```xml
<PolicyRuleSet id="LOW_ALTITUDE_OPS" flight_phases="TAXI, TAKEOFF, APPROACH, LANDING">
    <PolicyRule traffic_class="ALL_TRAFFIC">
        <PathPreference ranking="1" link_id="LINK_CELLULAR" />
        <PathPreference ranking="2" link_id="LINK_SATCOM" />
        <PathPreference ranking="3" link_id="LINK_WIFI" action="PROHIBIT" />
    </PolicyRule>
</PolicyRuleSet>
```

3) 巡航/远洋（HIGH_ALTITUDE_OPS / OCEANIC） - 强制 SATCOM

```xml
<PolicyRuleSet id="HIGH_ALTITUDE_OPS" flight_phases="CRUISE, OCEANIC">
    <PolicyRule traffic_class="ALL_TRAFFIC">
        <PathPreference ranking="1" link_id="LINK_SATCOM" />
        <PathPreference ranking="2" link_id="LINK_CELLULAR" action="PROHIBIT" />
        <PathPreference ranking="3" link_id="LINK_WIFI" action="PROHIBIT" />
    </PolicyRule>
</PolicyRuleSet>
```

## 按阶段测试用例（示例）

以下测试用例用于验证阶段映射、滞后逻辑以及策略选择是否符合预期。每个用例包含模拟步骤、期望日志输出与预期策略决策。

用例格式：
- 前置条件
- 模拟步骤
- 预期结果（客户端显示、服务器日志中 `Policy Decision` 内容）

### TC-PA-01: PARKED → 选择 WIFI
- 前置条件：服务器已启动，DLM: WIFI/CELLULAR/SATCOM 在线；客户端 `TrafficClass=BEST_EFFORT`。
- 模拟步骤：将飞机位置设为停机位（GPS 模拟），或手动触发 FlightPhaseUpdate=PARKED；在 `magic_client` 输入：`mccr modify 5000 10000 1 2`。
- 预期结果：
    - 客户端显示 `Selected-DLM = LINK_WIFI`，`Granted-BW = 10000 kbps`。
    - 服务器日志包含 `Using Ruleset: GROUND_OPS` 与 `Selected Link: LINK_WIFI` 的记录。

### TC-TA-01: TAKEOFF → 限制大流量
- 前置条件：飞机处于地面，客户端请求大流量任务准备中。
- 模拟步骤：触发 FlightPhaseUpdate=TAKEOFF（或模拟离地事件），发起 `mccr modify 5000 100000 1 2`（请求 100 Mbps）。
- 预期结果：
    - 服务器根据滞后逻辑在确认 TAKEOFF 后拒绝大流量或选择低延迟安全链路（如 CELLULAR），并在日志中记录 `Policy Decision` 与 `reason`。

### TC-CR-01: CRUISE（远洋）→ 强制 SATCOM
- 前置条件：飞行计划进入 OCEANIC 区段，蜂窝覆盖不可用或被判定不可用。
- 模拟步骤：触发 FlightPhaseUpdate=CRUISE 或 OCEANIC，客户端发起 `mccr modify 5000 2000 1 2`（请求 2 Mbps）。
- 预期结果：
    - 客户端应收到 `Selected-DLM = LINK_SATCOM`，`Granted-BW` 为请求或链路可用带宽。
    - 服务器日志包含 `Using Ruleset: HIGH_ALTITUDE_OPS` 与 `Selected Link: LINK_SATCOM`。

### TC-LF-01: 滞后/退回测试（Hysteresis）
- 前置条件：飞机在爬升临界高度附近（例如 900–1300 ft），滞后设置为连续 3 次采样确认。
- 模拟步骤：快速波动高度穿过阈值 800–1200 ft 数次，观察是否发生切换。
- 预期结果：
    - 服务器仅当高度连续满足进入新阶段的 N 次判定或持续 T 秒后才切换，日志记录切换时间与触发源说明。

---

完成上述修改后，请告知是否要我：
- 直接把某些示例策略片段写入 `Central_Policy_Profile.xml`（并重启服务验证）；
- 或先把测试用例文档化为独立的测试脚本/步骤清单以便执行。 

#### `<PolicyRule>`
- **traffic_class**: 适用的业务类型。
  - 特殊值 `ALL_TRAFFIC` 可作为该飞行阶段的默认兜底规则。

#### `<PathPreference>`
- **ranking**: 优先级排名 (1 为最高)。策略引擎首先选择 ranking=1 的链路，如果不可用则选 ranking=2。
- **link_id**: 对应 `Datalink_Profile.xml` 中的链路 ID。
- **action**: (可选) 特殊动作。
  - `PROHIBIT`: 明确禁止使用此链路 (即使链路在线也不会被选中)。
- **security_required**: (可选) 安全要求 (如 `VPN`, `IPSEC`)。

---

## 5. 数据链路配置 (Datalink_Profile.xml)

定义物理链路的属性，用于策略评分计算。

### 5.1 结构示例
```xml
<DatalinkProfiles>
    <Link id="LINK_CELLULAR">
        <LinkName>4G/5G AGT Network</LinkName>
        <DLMDriverID>DLM_CELLULAR_PROC</DLMDriverID>
        <Type>CELLULAR</Type>
        
        <Capabilities>
            <MaxTxRateKbps>20000</MaxTxRateKbps>
            <TypicalLatencyMs>50</TypicalLatencyMs>
        </Capabilities>
        
        <PolicyAttributes>
            <CostIndex>20</CostIndex>
            <SecurityLevel>MEDIUM</SecurityLevel>
        </PolicyAttributes>
    </Link>
</DatalinkProfiles>
```

### 5.2 参数详解

| 参数 | 说明 | 对评分的影响 |
|------|------|------------|
| **id** | 链路唯一标识符，必须与 DLM 上报的 ID 一致 | - |
| **DLMDriverID** | 关联的 DLM 驱动程序 ID | - |
| **MaxTxRateKbps** | 最大传输速率 (kbps) | **带宽余量权重**。如果 `MaxTxRate < 请求带宽`，评分扣 5000 分。 |
| **TypicalLatencyMs** | 典型延迟 (ms) | **延迟权重**。<br>< 50ms: +100 分<br>> 500ms: -50 分 |
| **CostIndex** | 成本指数 (0-100) | **成本权重**。数值越大，扣分越多 (CostIndex / 10)。 |
| **SecurityLevel** | 安全等级 (`LOW`, `MEDIUM`, `HIGH`) | 用于匹配策略中的安全需求。 |

---

## 6. 配置修改生效

1. **修改 XML 文件** (`Client_Profile.xml`, `Central_Policy_Profile.xml`, `Datalink_Profile.xml`)：
   - 需要**重启 freeDiameter 服务器**才能生效。
   - 配置文件在服务器启动时的初始化阶段被解析加载到内存中。

2. **修改 magic_server.conf**：
   - 需要**重启 freeDiameter 服务器**。

3. **热加载**：
   - 当前版本暂不支持配置文件的热加载。

## 7. 常见配置场景

### 场景 A: 想要优先使用 4G/5G 网络
1. 打开 `Central_Policy_Profile.xml`
2. 找到当前飞行阶段 (如 `GROUND_OPS`)
3. 在对应业务类型下，将 `LINK_CELLULAR` 的 `ranking` 设为 `1`。

### 场景 B: 想要测试带宽限制拒绝
1. 打开 `Client_Profile.xml`
2. 将测试客户端的 `MaxBandwidth` 设为一个较小的值 (如 `1000000` 即 1 Mbps)。
3. 重启服务器。
4. 发送一个请求带宽为 2 Mbps 的 MCCR 请求，应返回错误 5012。

### 场景 C: 模拟高空无网络
1. 打开 `Central_Policy_Profile.xml`
2. 找到 `HIGH_ALTITUDE_OPS`
3. 将所有链路的 `action` 设为 `PROHIBIT`，或者只保留 `LINK_SATCOM`。
