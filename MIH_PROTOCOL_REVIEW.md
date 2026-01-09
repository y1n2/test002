# MIH 协议设计审查报告
## ARINC 839 Attachment 2 合规性检查

### 📊 Executive Summary

**当前状态**: 系统存在**三套不兼容的 MIH/IPC 协议**定义

**关键发现**:
1. ✅ `extensions/app_magic/mih_protocol.h` - 完全符合 ARINC 839 Attachment 2 规范
2. ⚠️ `include/ipc/magic_mih_protocol.h` - 新创建的简化版本（有重复和不一致）
3. ❌ `include/ipc/magic_ipc_protocol.h` - 旧的自定义协议（DLM daemons 当前使用）

**建议**: 统一使用 `extensions/app_magic/mih_protocol.h`，这是唯一完全符合规范的版本

---

## 1. 协议定义对比分析

### 1.1 Bearer ID 定义

| 文件 | 定义 | 符合规范? |
|------|------|----------|
| `mih_protocol.h` | `typedef uint8_t BEARER_ID;` | ✅ 正确 (ARINC 839: UNSIGNED INT 1) |
| `magic_mih_protocol.h` | `typedef uint16_t BEARER_ID;` | ❌ 错误 (应该是 uint8_t) |

**规范依据**: ARINC 839 Section 2.4.1 - "Bearer Identifier (UNSIGNED INT 1) - Supports up to 256 different bearers"

### 1.2 QoS Parameters

#### `mih_protocol.h` (✅ 符合规范)
```c
typedef struct {
    COS_ID              cos_id;             // Class of Service
    LINK_DATA_RATE_FL   forward_link_rate;  // kbps
    LINK_DATA_RATE_RL   return_link_rate;   // kbps
    uint32_t            min_pk_tx_delay;
    uint32_t            avg_pk_tx_delay;
    uint32_t            max_pk_tx_delay;
    uint32_t            pk_delay_jitter;
    float               pk_loss_rate;       // 0.0-1.0
} QOS_PARAM;
```

#### `magic_mih_protocol.h` (⚠️ 字段不完整)
```c
typedef struct {
    uint32_t        min_bandwidth_kbps;
    uint32_t        max_bandwidth_kbps;
    uint32_t        max_latency_ms;
    uint8_t         priority;
    uint8_t         qos_class;
    uint16_t        reserved;
} mih_qos_params_t;
```

**问题**: 缺少 `COS_ID`、延迟抖动、丢包率等关键参数

### 1.3 MIH 原语定义

#### 资源请求原语对比

**`mih_protocol.h` (✅ 符合 ARINC 839 Section 2.1.2)**
```c
typedef struct {
    MIHF_ID                 destination_id;
    LINK_TUPLE_ID           link_identifier;
    RESOURCE_ACTION_TYPE    resource_action;  // REQUEST/RELEASE
    bool                    has_bearer_id;
    BEARER_ID               bearer_identifier;
    bool                    has_qos_params;
    QOS_PARAM               qos_parameters;
} MIH_Link_Resource_Request;
```

**`magic_mih_protocol.h` (⚠️ 额外字段)**
```c
typedef struct {
    mih_header_t            header;           // ✅ 必需的传输层头
    mih_link_identifier_t   link_id;
    uint8_t                 action;
    BEARER_ID               bearer_id;
    mih_qos_params_t        qos_params;
    uint32_t                session_id;       // ⚠️ 非标准扩展
    char                    client_id[64];    // ⚠️ 非标准扩展
} mih_link_resource_req_t;
```

**分析**:
- `session_id` 和 `client_id` 不是 ARINC 839 定义的字段
- 但对于 MAGIC 系统的会话管理是有用的
- 建议：保留但标注为 "MAGIC Extension"

### 1.4 Link Identifier

**`mih_protocol.h` (✅ 正确)**
```c
typedef struct {
    uint8_t  link_type;         // Ethernet, WiFi, Cellular, Satcom
    char     link_addr[32];     // Link address (MAC, IMSI, etc.)
    char     poa_addr[32];      // Point of Attachment (optional)
} LINK_TUPLE_ID;
```

**`magic_mih_protocol.h` (⚠️ 简化版)**
```c
typedef struct {
    char            link_id[64];        // Profile ID, e.g., "LINK_SATCOM"
    char            interface_name[16]; // OS interface, e.g., "eth1"
    uint8_t         link_type;
} mih_link_identifier_t;
```

**问题**: 
- 缺少 `poa_addr` (Point of Attachment)
- `link_id` 使用字符串而非标准的 MAC/IMSI 地址
- 但对于 MAGIC 的 profile-based 架构可能更实用

---

## 2. CM-DLM 通信架构

### 2.1 当前 app_magic 实现

**文件**: `extensions/app_magic/magic_dlm_manager.{h,c}`

**使用的协议**: `mih_protocol.h` ✅

**架构**:
```
┌─────────────────────────────────────────────┐
│         app_magic Extension                  │
│  (CM Core + Policy Engine + MCAR/MCCR)      │
│                                               │
│  ┌────────────────────────────────────────┐ │
│  │   DLM Manager (magic_dlm_manager.c)    │ │
│  │   - Unix Socket Server                  │ │
│  │   - MIH Primitive Handler               │ │
│  │   - Bearer Management                   │ │
│  └─────────────┬──────────────────────────┘ │
└────────────────┼──────────────────────────────┘
                 │ /tmp/magic_core.sock
                 │
      ┌──────────┼──────────┐
      │          │          │
   ┌──▼───┐  ┌──▼───┐  ┌──▼───┐
   │ DLM  │  │ DLM  │  │ DLM  │
   │SATCOM│  │ LTE  │  │ WiFi │
   └──────┘  └──────┘  └──────┘
```

### 2.2 DLM Daemon 当前实现

**文件**: `DLM_SATCOM/dlm_satcom_daemon.c`

**使用的协议**: `magic_ipc_protocol.h` ❌

**问题**: 
- 使用旧的 `ipc_register_req_t` 而非标准 MIH 原语
- socket 路径: `/tmp/magic_cm.sock` (应该是 `/tmp/magic_core.sock`)

---

## 3. 完整的 MIH 交互流程（符合 ARINC 839）

### 3.1 DLM 注册流程

```
DLM Daemon                          CM Core (app_magic)
    │                                      │
    │  MIH_Link_Register.request           │  ⚠️ 非标准扩展
    │  - link_identifier (LINK_TUPLE_ID)   │     (MAGIC 需要)
    │  - capabilities                      │
    ├─────────────────────────────────────>│
    │                                      │
    │  MIH_Link_Register.confirm           │
    │  - status (SUCCESS/FAILURE)          │
    │  - assigned_id                       │
    │<─────────────────────────────────────┤
    │                                      │
    │  MIH_Link_Up.indication              │  ✅ 标准原语
    │  - link_identifier                   │     (ARINC 839 2.2.2)
    │  - link_parameters (IP, BW, latency) │
    ├─────────────────────────────────────>│
    │                                      │
```

### 3.2 Bearer 分配流程（MCCR 触发）

```
Client          CM Core                DLM Daemon
  │                │                       │
  │  MCCR          │                       │
  ├───────────────>│                       │
  │                │                       │
  │                │  Policy Decision      │
  │                │  (select LINK_SATCOM) │
  │                │                       │
  │                │  MIH_Link_Resource.request  ✅ 标准原语
  │                │  - link_identifier          (ARINC 839 2.1.2)
  │                │  - action=REQUEST           
  │                │  - qos_parameters           
  │                ├──────────────────────>│
  │                │                       │
  │                │                       │  Allocate Bearer
  │                │                       │  (Link Layer)
  │                │                       │
  │                │  MIH_Link_Resource.confirm
  │                │  - status=SUCCESS
  │                │  - bearer_identifier=1
  │                │<──────────────────────┤
  │                │                       │
  │  MCCA          │                       │
  │  Result=2001   │                       │
  │<───────────────┤                       │
  │  Bearer-ID=1   │                       │
  │                │                       │
```

### 3.3 心跳与健康监控

```
DLM Daemon                          CM Core
    │                                      │
    │  MIH_Heartbeat (每 5 秒)             │  ⚠️ 非标准扩展
    │  - link_identifier                   │     (MAGIC 需要)
    │  - health_status                     │
    │  - tx/rx bytes                       │
    ├─────────────────────────────────────>│
    │                                      │
    │  MIH_Heartbeat_Ack                   │
    │<─────────────────────────────────────┤
    │                                      │
```

**注意**: ARINC 839 Attachment 2 没有定义心跳机制，这是 MAGIC 系统的合理扩展

---

## 4. 推荐的统一协议设计

### 4.1 核心原则

1. **严格遵循 ARINC 839 Attachment 2** 定义的 MIH 原语
2. **最小化扩展**: 只在绝对必要时添加非标准字段
3. **清晰标注**: 所有扩展字段必须标注 "MAGIC Extension"
4. **向后兼容**: 使用 optional 字段机制

### 4.2 推荐使用的头文件

**主协议**: `extensions/app_magic/mih_protocol.h`

**原因**:
- ✅ 完全符合 ARINC 839 Attachment 2 规范
- ✅ 包含所有必需的 MIH 数据类型
- ✅ 正确的 Bearer ID (uint8_t)
- ✅ 完整的 QoS 参数定义
- ✅ 标准的 LINK_TUPLE_ID

### 4.3 需要的扩展原语（标注为 MAGIC Extensions）

#### 4.3.1 MIH_Link_Register (非标准，但必需)

```c
/* MAGIC Extension: Link Registration */
typedef struct {
    LINK_TUPLE_ID           link_identifier;
    
    /* Link Capabilities */
    uint32_t                max_bandwidth_kbps;
    uint32_t                typical_latency_ms;
    uint32_t                cost_per_mb;
    uint8_t                 coverage;  // Global/Terrestrial/Gate
    uint8_t                 security_level;
    uint16_t                mtu;
    
    pid_t                   dlm_pid;
} MIH_Link_Register_Request;

typedef struct {
    STATUS                  status;
    uint32_t                assigned_id;
    char                    message[128];
} MIH_Link_Register_Confirm;
```

**理由**: ARINC 839 假设链路已知，但 MAGIC 系统需要动态 DLM 注册

#### 4.3.2 MIH_Heartbeat (非标准，但推荐)

```c
/* MAGIC Extension: DLM Health Monitoring */
typedef struct {
    LINK_TUPLE_ID           link_identifier;
    uint8_t                 health_status;  // 0=OK, 1=Warning, 2=Error
    uint64_t                tx_bytes;
    uint64_t                rx_bytes;
} MIH_Heartbeat;
```

**理由**: 系统稳定性需要，生产环境必备

#### 4.3.3 会话上下文扩展

在标准 `MIH_Link_Resource_Request` 基础上添加：

```c
typedef struct {
    /* Standard ARINC 839 fields */
    MIHF_ID                 destination_id;
    LINK_TUPLE_ID           link_identifier;
    RESOURCE_ACTION_TYPE    resource_action;
    bool                    has_bearer_id;
    BEARER_ID               bearer_identifier;
    bool                    has_qos_params;
    QOS_PARAM               qos_parameters;
    
    /* MAGIC Extension: Session Management */
    uint32_t                session_id;      // Diameter session
    char                    client_id[64];   // Aircraft/client ID
} MAGIC_MIH_Link_Resource_Request;
```

### 4.4 消息封装（传输层）

所有 MIH 原语需要传输层头部（IPC 通信）：

```c
typedef struct {
    uint16_t        primitive_type;     // MIH primitive code
    uint16_t        message_length;     // Total length including header
    uint32_t        transaction_id;     // For request/response pairing
    uint32_t        timestamp;          // Unix timestamp
} MIH_Transport_Header;
```

完整消息结构：
```
┌────────────────────────────────┐
│  MIH_Transport_Header (12 bytes)│
├────────────────────────────────┤
│  MIH Primitive Payload          │
│  (MIH_Link_Resource_Request etc)│
└────────────────────────────────┘
```

---

## 5. 迁移建议

### 5.1 文件整合计划

1. **保留**: `extensions/app_magic/mih_protocol.h` ✅
   - 这是唯一符合规范的版本
   
2. **删除**: `include/ipc/magic_mih_protocol.h` ❌
   - 重复且不符合规范
   
3. **迁移**: `include/ipc/magic_ipc_protocol.h` → 使用 `mih_protocol.h`
   - 逐步淘汰旧协议

4. **创建**: `include/ipc/mih_transport.h` (新文件)
   - 定义传输层头部和 IPC 工具函数
   - 供 DLM daemons 和 app_magic 共同使用

### 5.2 推荐的新文件结构

```
include/ipc/
├── mih_transport.h         # 传输层头部、socket 工具
└── mih_transport.c         # IPC 发送/接收函数

extensions/app_magic/
├── mih_protocol.h          # 标准 MIH 原语 (ARINC 839)
├── mih_extensions.h        # MAGIC 扩展原语
├── magic_dlm_manager.h/c   # DLM 管理器实现
└── ...

DLM_SATCOM/
├── dlm_satcom_daemon.c
└── (引用 mih_protocol.h + mih_transport.h)
```

### 5.3 代码迁移步骤

#### Step 1: 创建 `mih_extensions.h`

```c
/**
 * @file mih_extensions.h
 * @brief MAGIC System Extensions to ARINC 839 MIH Protocol
 */

#ifndef MIH_EXTENSIONS_H
#define MIH_EXTENSIONS_H

#include "mih_protocol.h"

/* Extension Primitive Codes (0x8000+ = vendor specific) */
#define MIH_EXT_LINK_REGISTER_REQUEST   0x8101
#define MIH_EXT_LINK_REGISTER_CONFIRM   0x8102
#define MIH_EXT_HEARTBEAT               0x8F01
#define MIH_EXT_HEARTBEAT_ACK           0x8F02

/* Extension data structures */
typedef struct {
    LINK_TUPLE_ID           link_identifier;
    uint32_t                max_bandwidth_kbps;
    uint32_t                typical_latency_ms;
    uint32_t                cost_per_mb;
    uint8_t                 coverage;
    uint8_t                 security_level;
    uint16_t                mtu;
    pid_t                   dlm_pid;
} MIH_EXT_Link_Register_Request;

// ... 其他扩展定义

#endif
```

#### Step 2: 创建 `mih_transport.h`

```c
#ifndef MIH_TRANSPORT_H
#define MIH_TRANSPORT_H

#include "mih_protocol.h"

#define MIH_SOCKET_PATH "/tmp/magic_core.sock"

typedef struct {
    uint16_t        primitive_type;
    uint16_t        message_length;
    uint32_t        transaction_id;
    uint32_t        timestamp;
} __attribute__((packed)) MIH_Transport_Header;

int mih_send(int sockfd, uint16_t primitive_type, const void* payload, size_t len);
int mih_recv(int sockfd, MIH_Transport_Header* header, void* payload, size_t max_len);

#endif
```

#### Step 3: 更新 DLM daemon

```c
// 旧代码 (magic_ipc_protocol.h)
ipc_register_req_t req;
ipc_init_header(&req.header, MSG_REGISTER_REQUEST, sizeof(req), 0);
strncpy(req.link_name, "Inmarsat_GX_SATCOM", sizeof(req.link_name));

// 新代码 (mih_protocol.h + mih_extensions.h)
MIH_EXT_Link_Register_Request req;
req.link_identifier.link_type = LINK_TYPE_SATCOM;
strncpy(req.link_identifier.link_addr, "eth1", 32);
req.max_bandwidth_kbps = 2000;
req.typical_latency_ms = 600;

mih_send(sockfd, MIH_EXT_LINK_REGISTER_REQUEST, &req, sizeof(req));
```

#### Step 4: 更新 app_magic DLM Manager

```c
// magic_dlm_manager.c
switch (header.primitive_type) {
    case MIH_EXT_LINK_REGISTER_REQUEST:
        handle_link_register_request(...);
        break;
        
    case MIH_LINK_RESOURCE_REQUEST:  // 标准原语
        handle_mih_link_resource_request(...);
        break;
        
    case MIH_EXT_HEARTBEAT:
        handle_heartbeat(...);
        break;
}
```

---

## 6. 合规性验证清单

### 6.1 ARINC 839 Attachment 2 必需原语

| 原语 | 状态 | 实现位置 |
|------|------|---------|
| MIH_Link_Resource.request | ✅ 已实现 | `mih_protocol.h` |
| MIH_Link_Resource.confirm | ✅ 已实现 | `mih_protocol.h` |
| LINK_Resource.request | ✅ 已实现 | `mih_protocol.h` |
| LINK_Resource.confirm | ✅ 已实现 | `mih_protocol.h` |

### 6.2 数据类型合规性

| 数据类型 | 规范定义 | 当前实现 | 状态 |
|---------|---------|---------|------|
| BEARER_ID | UNSIGNED INT 1 (uint8_t) | uint8_t | ✅ |
| QOS_PARAM | COS_ID + rates + delays | 完整结构 | ✅ |
| LINK_TUPLE_ID | link_type + addresses | 完整结构 | ✅ |
| STATUS | 0-6 枚举 | 完整枚举 | ✅ |

### 6.3 MAGIC 扩展合规性

| 扩展 | 必要性 | 标注 | 状态 |
|------|-------|------|------|
| MIH_Link_Register | ✅ 必需 | ⚠️ 需要标注 | 待更新文档 |
| MIH_Heartbeat | ✅ 推荐 | ⚠️ 需要标注 | 待更新文档 |
| session_id 字段 | ✅ 有用 | ⚠️ 需要标注 | 待更新文档 |

---

## 7. 最终建议

### 7.1 立即行动项

1. ✅ **使用 `mih_protocol.h` 作为唯一协议定义**
   - 已存在于 `extensions/app_magic/mih_protocol.h`
   - 完全符合 ARINC 839 规范
   
2. 🔧 **创建 `mih_extensions.h`**
   - 明确标注 MAGIC 系统扩展
   - 定义注册、心跳等非标准原语
   
3. 🔧 **创建 `include/ipc/mih_transport.{h,c}`**
   - 统一传输层实现
   - 供所有组件共享

4. 🔧 **更新 DLM daemons**
   - 使用标准 MIH 原语
   - 替换旧的 `magic_ipc_protocol.h`

5. 📝 **文档更新**
   - 在所有扩展原语上添加 "MAGIC Extension" 注释
   - 说明扩展的理由和用途

### 7.2 长期改进

1. **考虑完整 IEEE 802.21 支持**
   - 当前仅实现 MIH_Link_Resource 原语
   - 未来可支持 MIH_Link_Detected, MIH_Link_Going_Down 等

2. **增强 QoS 映射**
   - ARINC 839 的 COS_ID 与 Diameter QoS-Level 映射
   - 策略引擎集成

3. **MIH 事件订阅机制**
   - 支持 MIH_Event_Subscribe 原语
   - 动态链路事件通知

---

## 8. 结论

**核心结论**: 
- ✅ `extensions/app_magic/mih_protocol.h` 是**唯一符合 ARINC 839 规范**的协议定义
- ❌ `include/ipc/magic_mih_protocol.h` 应该**删除**（重复且不符合规范）
- 🔧 需要创建 `mih_extensions.h` 来标准化 MAGIC 系统扩展

**下一步**: 
选择迁移方案：
1. **快速迁移**: 先迁移一个 DLM (SATCOM) 作为 POC
2. **完整迁移**: 一次性迁移所有组件
3. **分阶段迁移**: 先 DLM daemons，再 app_magic

**预估工作量**:
- 创建 mih_extensions.h + mih_transport.{h,c}: 2 hours
- 迁移一个 DLM daemon: 2-3 hours
- 迁移所有 DLM daemons: 6-8 hours
- 测试和验证: 3-4 hours

**总计**: 约 13-17 小时完整迁移并验证
