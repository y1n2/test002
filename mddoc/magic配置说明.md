# MAGIC 配置系统使用指南 (Configuration Guide)

本文档旨在详细说明 MAGIC (Mobile Aviation Global Internet Connectivity) 系统中配置文件的结构、参数含义及修改方法。

## 配置文件概览

MAGIC 系统的配置分为两个层面：

1.  **Diameter 基础配置**: 由 freeDiameter 核心管理，涉及网络、TLS 安全、身份认证等。
    *   详见：[freeDiameter 配置指南](MAGIC_FREEDIAMETER_CONFIG_GUIDE.md)
2.  **MAGIC 业务配置**: 由 `app_magic` 扩展管理，涉及业务策略、链路调度、用户权限等。
    *   配置文件默认位于 `extensions/app_magic/config/` 目录下。

---

## 1. 客户端配置文件 (Client_Profile.xml)

该文件用于管理接入 MAGIC 系统的所有“客户端”。一个“客户端”可以是一个物理设备（如 EFB、乘客Wi-Fi网关），也可以是一个业务系统（如 QAR 数据卸载）。

### 核心结构 `<ClientProfile>`
每个 `<ClientProfile>` 节点代表一个独立的业务实体。

#### 1.1 基础信息
```xml
<ClientProfile>
    <ProfileName>cockpit_efb_mission</ProfileName> <!-- 唯一标识符，Policy Profile 将引用此名称 -->
    <ClientID>magic-client.example.com</ClientID>  <!-- 客户端证书CN或硬件ID -->
    <Description>驾驶舱关键业务</Description>       <!-- 可读的描述 -->
    <Enabled>true</Enabled>                        <!-- 总开关：false 表示禁止此客户端接入 -->
    ...
</ClientProfile>
```

#### 1.2 认证 (Auth)
定义客户端接入网络时的身份验证信息。
*   `<Username>` / `<ClientPassword>`: 客户端向服务器发起认证时的凭证。
*   `<ServerPassword>`: 可选，客户端用于验证服务器身份（双向认证）。
*   `<SourceIP>`: 安全绑定，限制该账号只能从特定内网 IP 发起请求。

#### 1.3 带宽管理 (Bandwidth)
定义该客户端可使用的流量速率（单位：bps - bits per second）。
*   `<MaxForward>` (下行) / `<MaxReturn>` (上行): **最大峰值带宽**。系统允许突发到的最高速率。
*   `<GuaranteedForward>` / `<GuaranteedReturn>`: **承诺信息速率 (CIR)**。在网络拥塞时，系统保证分配给该客户端的最低带宽。
*   `<DefaultRequest>`: 如果客户端发起请求时未指定带宽，系统使用的默认值。

#### 1.4 服务质量 (QoS)
*   `<PriorityType>`:
    *   `1` (Block/Mission Critical): 关键业务。资源不足时，会抢占其他低优先级业务的资源。
    *   `2` (Preemption/Non-Critical): 普通业务。资源不足时，可能会被高优先级业务踢下线。
*   `<PriorityClass>`: 1-9 的整数，1 为最高优先级。
*   `<DefaultLevel>` & `<AllowedLevels>`: 定义该客户端允许使用的 DiffServ/DSCP 标记映射等级 (0=BestEffort, 1=AF, 2=EF)。

#### 1.5 链路策略 (LinkPolicy)
控制该客户端允许使用哪些物理链路。
*   `<AllowedDLMs>`: 允许使用的链路类型列表 (如 `LINK_SATCOM`, `LINK_CELLULAR`, `LINK_WIFI`)。
    *   *示例*: 驾驶舱数据可能只允许 `LINK_SATCOM` 和 `LINK_CELLULAR`，禁止 `LINK_WIFI` (出于安全性或稳定性考虑)。
*   `<PreferredDLM>`: 首选链路。如果多条链路同时可用，优先走哪条。
*   `<AllowMultiLink>`: 是否允许同时使用多条链路传输数据 (MPTCP/聚合)。
    *   `true`: 提高带宽（如乘客上网）。
    *   `false`: 提高稳定性，防止乱序（如 VoIP 语音）。

#### 1.6 流量控制 (Traffic)
*   `<Billing>` / `<Quota>`: 计费与配额（月度/日度）。
*   `<TFTTemplate>`: **流量过滤器模板 (Traffic Flow Template)**。
    *   定义防火墙规则，限制该客户端只能访问特定的 IP 或端口。
    *   *语法*: `permit out [tcp|udp|ip] from {source_ip} to [ANY | x.x.x.x/mask] port [list]`
    *   *示例*: `permit out tcp from {source_ip} to any port 80,443` (仅允许 Web 流量)

#### 1.7 地理围栏 (Location)
限制客户端只能在特定的飞行阶段或高度使用网络。
*   `<AllowedFlightPhases>`: 允许使用的飞行阶段 (如 `CRUISE` 巡航, `GATE`登机口)。
*   `<MinAltitude>` / `<MaxAltitude>`: 允许使用的高度范围（英尺）。
    *   *场景*: 乘客 Wi-Fi 可能仅在 `CRUISE` 阶段且高度 > 10000 英尺时才允许开启。

---

## 2. 中心策略配置文件 (Central_Policy_Profile.xml)

该文件是策略引擎的大脑，定义了**全局层面**如何根据飞行状态调度流量。

### 2.1 流量分类 (TrafficClassDefinitions)
将具体的 Client Profile 映射到抽象的业务类别。
```xml
<TrafficClass id="COCKPIT_DATA">
    <MatchPriorityClass>1</MatchPriorityClass> <!-- 所有 Priority=1 的客户端归为此类 -->
</TrafficClass>
```

### 2.2 切换策略 (SwitchingPolicy)
防止链路频繁切换 (Flapping) 的参数。
*   `<MinDwellTime>`: 驻留时间 (秒)。切换到新链路后，至少保持连接这么久，除非信号彻底断开。
*   `<HysteresisPercentage>`: 迟滞百分比。新链路的信号质量必须比当前链路好 X% 以上，才触发切换。

### 2.3 策略规则集 (PolicyRuleSet)
根据不同的**飞行阶段 (Flight Phase)** 定义不同的路由优先级。

#### 示例：地面作业 (GROUND_OPS)
```xml
<PolicyRuleSet id="GROUND_OPS" flight_phases="PARKED, GATE, TAXI">
    <!-- 规则: 大数据下载 (BULK_DATA) -->
    <PolicyRule traffic_class="BULK_DATA">
        <!-- 优先级 1: 免费的机场 Wi-Fi -->
        <PathPreference ranking="1" link_id="LINK_WIFI" />
        <!-- 优先级 2: 禁止使用蜂窝 (防止流量费过高) -->
        <PathPreference ranking="2" link_id="LINK_CELLULAR" action="PROHIBIT" />
        <!-- 优先级 3: 禁止使用卫星 -->
        <PathPreference ranking="3" link_id="LINK_SATCOM" action="PROHIBIT" />
    </PolicyRule>
</PolicyRuleSet>
```
*   `ranking`: 越小优先级越高。
*   `security_required="VPN"`: 强制要求该链路必须建立 VPN 隧道才能使用。
*   `action="PROHIBIT"`: 显式禁止。即使该链路可用，也不会被选中（用于成本控制）。

---

## 3. 链路配置文件 (Datalink_Profile.xml)

该文件描述了飞机上安装的物理通信设备及其属性。

### 3.1 链路定义 (DLMConfig)
*   `<DLMName>`: 链路ID (如 `LINK_SATCOM`)，供其他配置文件引用。
*   `<DLMType>`: 1=卫星, 2=ATG/蜂窝, 3=地面Wi-Fi。

### 3.2 链路参数 (Link)
*   `<MaxForwardBW>` / `<MaxReturnBW>`: 物理链路的总带宽容量 (kbps)。
*   `<Latency>`: 典型延迟 (ms)。
*   `<PacketLossRate>`: 典型丢包率 (0.0 - 1.0)。
*   `<OversubscriptionRatio>`: 超售比。允许在该链路上分配的总带宽是物理带宽的多少倍（例如 1.2 表示允许超卖 20%）。

### 3.3 覆盖范围 (Coverage)
定义该链路在什么地理位置有效。
*   `<MinLatitude>` / `<MaxLatitude>`: 纬度范围。
*   `<MinAltitude>` / `<MaxAltitude>`: 高度范围。
    *   *示例*: `LINK_WIFI` (Gatelink) 通常限制高度 `0 - 500` 英尺，确保只有在地面或极低空时才会被策略引擎认为是“可用”的。

### 3.4 负载均衡 (LoadBalance)
当该 DLM 下有多个 Modem (如双卡双待) 时如何分配流量。
*   `<Algorithm>`:
    *   `1`: RoundRobin (轮询)
    *   `2`: LeastLoaded (最小负载优先)
    *   `3`: Priority (主备模式)

---

## 修改指南

1.  **添加新业务**:
    *   在 `Client_Profile.xml` 中复制一个 `<ClientProfile>` 块。
    *   修改 `ProfileName`, `ClientID`, `Username`。
    *   根据业务需求调整 `Bandwidth`, `QoS` 和 `LinkPolicy`。
    *   **重启服务**: 修改后需重启 `freeDiameterd` 进程生效。

2.  **调整路由逻辑**:
    *   编辑 `Central_Policy_Profile.xml`。
    *   找到对应飞行阶段的 `<PolicyRuleSet>`。
    *   调整 `<PathPreference>` 的 `ranking` 顺序或添加 `action="PROHIBIT"`。

3.  **更新物理设备**:
    *   如果升级了卫星调制解调器带宽，编辑 `Datalink_Profile.xml` 中的 `<MaxForwardBW>` 和 `<MaxReturnBW>`，以便策略引擎能分配更多带宽。

## 常见问题排查

*   **Q: 客户端无法连接，提示 "Auth Failed"?**
    *   检查 `Client_Profile.xml` 中的 `<Username>`, `<ClientPassword>` 以及 `<SourceIP>` 是否与客户端实际发出的匹配。
*   **Q: 飞机起飞后 Wi-Fi 断开，但没切换到卫星?**
    *   检查 `Client_Profile.xml` 的 `<LinkPolicy>` 是否允许了 `LINK_SATCOM`。
    *   检查 `Central_Policy_Profile.xml` 在 `CLIMB` 或 `CRUISE` 阶段是否有针对该业务的规则允许使用卫星。
*   **Q: 语音通话质量差?**
    *   确保 VoIP 客户端在 `Client_Profile.xml` 中 `<PriorityType>` 设为 `1`，且 `<AllowMultiLink>` 设为 `false` (避免多链路乱序)。
    *   检查 `Datalink_Profile.xml` 中卫星链路的 `<Jitter>` 设置是否符合实际情况。

---

## 附录：核心参数字典 (Parameter Glossary)

### 1. 动作指令 (Action)
*   **PROHIBIT**: **绝对禁止**。
    *   *含义*: 强行在路由表中剔除该链路。即使该链路信号满格、带宽充足，系统也绝不会将数据流量分配上去。
    *   *场景*: 防止在漫游费用极高的卫星链路上跑大流量非关键业务（如乘客看视频）。

### 2. 优先级类型 (PriorityType)
*   **1 (Blocking / Mission Critical)**: **独占式/关键业务**。
    *   *含义*: 该业务必须被满足。如果带宽不足，系统会强制踢掉低优先级的其他连接来释放资源。
*   **2 (Preemption / Non-Critical)**: **抢占式/普通业务**。
    *   *含义*: 资源充足时允许接入；资源紧张时，随时可能被高优先级的业务抢占资源从而断开。

### 3. 服务质量等级 (QoS Level)
*   **2 (EF - Expedited Forwarding)**: **极速转发**。
    *   *用途*: 实时语音 (VoIP)、视频会议。
    *   *特征*: 极低延迟，极低抖动，但带宽通常受限。
*   **1 (AF - Assured Forwarding)**: **确保转发**。
    *   *用途*: 业务数据、网页浏览。
    *   *特征*: 保证有一定的带宽，允许少量延迟。
*   **0 (BE - Best Effort)**: **尽力而为**。
    *   *用途*: 乘客 Wi-Fi、后台下载。
    *   *特征*: 有多少带宽用多少，网络拥堵时优先丢弃。
*   *其他*: 系统最大支持扩展到 Level 4，但目前主要使用 0-2。

### 4. 链路类型 (DLMType)
*   `1`: **Satellite (卫星)** - 覆盖广，延迟高，费用高。
*   `2`: **Cellular/ATG (蜂窝)** - 延迟低，带宽大，仅限陆地上空。
*   `3`: **WiFi/Gatelink (地面)** - 仅地面可用，通常免费。

### 5. 飞行阶段 (Flight Phases)
系统能够感知的飞机状态详细列表：

*   **PARKED (停机)**: 飞机完全停止，引擎关闭。
*   **GATE (登机口)**: 飞机停靠廊桥，通常连接了地面电源和 Gatelink。
*   **TAXI (滑行)**: 飞机在跑道或滑行道上移动。
*   **TAKEOFF / TAKE_OFF (起飞)**: 飞机加速离地阶段。
    *   *注意*: 在不同配置文件中写法略有不同，系统会自动归一化。
*   **CLIMB (爬升)**: 起飞后上升至巡航高度的过程。
*   **CRUISE (巡航)**: 飞机平飞阶段，通常持续时间最长。
*   **DESCENT (下降)**: 从巡航高度下降准备着陆。
*   **APPROACH (进近)**: 对准跑道，准备降落。
*   **LANDING (着陆)**: 飞机触地滑跑阶段。
*   **MAINTENANCE (维护)**: 特殊模式，用于地面测试或数据加载 (需要手动触发)。

### 10. 流量类别参数 (Traffic Class Definition)
**概念解释**: `TrafficClass` 是连接**具体客户端**和**抽象策略**的中间层。

1.  **为什么需要它?**
    *   如果不使用 TrafficClass，我们必须为每个 Client ID 写一遍路由规则，文件会变得非常巨大且难以维护。
    *   通过 TrafficClass，我们可以将属性相似的客户端归为一类 (如 "驾驶舱数据", "乘客娱乐")，然后只需为这一类写路由规则。

2.  **匹配逻辑 (Matching Workflow)**
    *   当一个客户端连接时，策略引擎会读取它的属性 (在 `Client_Profile.xml` 中定义)。
    *   引擎会遍历 `Central_Policy_Profile.xml` 中的 `TrafficClassDefinitions` 列表。
    *   一旦找到匹配项，该客户端就被打上对应的 "标签" (TrafficClass ID)。

3.  **参数详解**
    *   **id**: 流量类别的唯一标识符 (如 `COCKPIT_DATA`)。这是被打上的标签名。
    *   **Matching Conditions (匹配条件，满足任意一条即命中)**:
        *   **MatchPriorityClass**: 如果客户端的 `PriorityClass` (优先级) 等于此值。
        *   **MatchQoSLevel**: 如果客户端的 `DefaultLevel` (QoS) 等于此值。
        *   **MatchProfileNamePattern**: 如果客户端的 `ProfileName` (配置名) 符合此通配符模式 (支持 `*` 和 `?`)。
    *   **Default**: 布尔值。设为 `true` 表示这是默认兜底类别。如果前面所有规则都没匹配上，就自动归入此类。

#### 示例图解
```text
[Client A] Priority=1, Profile="efb_v1"  ---> 命中 (MatchPriorityClass=1) ---> 归类为 [COCKPIT_DATA]
[Client B] Priority=5, Profile="pax_vip" ---> 命中 (MatchProfileNamePattern="*pax*") ---> 归类为 [PAX_DATA]
[Client C] Priority=9, Profile="unknown" ---> 未命中任何特定规则 ---> 归类为 [BEST_EFFORT] (Default=true)
```
**后续流向**: 策略引擎接着会查找 `[COCKPIT_DATA]` 对应的路由规则 (PolicyRule)，决定走卫星还是 WiFi。


### 11. 路径偏好高级参数 (Path Preference Advanced)
在 `<PathPreference>` 节点中除了 basic ranking，还有以下高级控制参数：

*   **security_required**: 安全要求标记。
    *   `VPN`: 只有当客户端建立 VPN 后才允许通过。
    *   `IPSEC`: 强制要求 IPSec 加密。
    *   *(留空)*: 无特殊安全要求。
*   **max_latency_ms**: 最大允许延迟 (毫秒)。
    *   *作用*: 即使链路可用，如果该链路当前的实时延迟超过此值，策略引擎也会将其视为不可用。
    *   *场景*: 实时交互类业务 (Interactive) 防止被调度到高延迟卫星链路。

### 12. 切换策略参数 (Switching Policy)
控制链路切换的防抖动逻辑，避免在两个信号相似的链路间频繁跳变。

*   **MinDwellTime**: 最小驻留时间 (秒)。
    *   *含义*: 刚刚切换到新链路后，系统会锁定路由表至少这么多秒，期间忽略其他更优链路，除非当前链路彻底断开。
*   **HysteresisPercentage**: 迟滞百分比 (0-100)。
    *   *含义*: 只有当新链路的评价分数 (Score) 比当前链路高出 X% 时，才触发切换。
    *   *公式*: `New_Score > Current_Score * (1 + Hysteresis/100)`

### 13. 流量过滤器模板 (TFT Template)
语法借鉴自常见的防火墙/ACL定义，用于生成底层系统的包过滤规则。

*   **基本格式**: `permit [out/in] [proto] from {source} to {dest} [port list]`
*   **{source_ip}**: 占位符，系统会自动替换为客户端实际分配的 IP 地址。
*   **any**: 匹配任意 IP。
*   **port 80,443**: 逗号分隔的端口列表。
*   **port 5000-6000**: 连字符表示端口范围。
