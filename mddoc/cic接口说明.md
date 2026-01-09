# MAGIC CIC 模块设计说明

> **版本**: 2.0  
> **最后更新**: 2025-12-14  
> **规范依据**: ARINC 839-2014

## 1. 模块概述

### 1.1 定位

CIC (Client Interface Component) 是 MAGIC 系统的**核心入口模块**，负责处理来自客户端的所有 Diameter 消息。它是 ARINC 839 架构中 Server 端的客户端接口组件。

```
┌─────────────────────────────────────────────────────────────────────┐
│                         MAGIC Client                                │
└─────────────────────────────────────────────────────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Diameter Protocol                              │
│                (MCAR/MCCR/STR/MSXR/MADR/MACR)                      │
└─────────────────────────────────────────────────────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        CIC Module                                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │ MCAR Handler│  │ MCCR Handler│  │ STR Handler │  ...             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                  │
│         └────────────────┼────────────────┘                         │
│                          ▼                                          │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │              Session Manager (magic_session.c)               │   │
│  └──────────────────────────────────────────────────────────────┘   │
│         │                │                  │                       │
│         ▼                ▼                  ▼                       │
│  ┌────────────┐  ┌──────────────┐  ┌─────────────────┐              │
│  │  Policy    │  │     LMI      │  │   Dataplane     │              │
│  │  Engine    │  │  (MIH/DLM)   │  │   Routing       │              │
│  └────────────┘  └──────────────┘  └─────────────────┘              │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 文件结构

| 文件 | 行数 | 描述 |
|------|------|------|
| `magic_cic.c` | ~4200 | 主实现文件 |
| `magic_cic.h` | ~73 | 公共接口头文件 |

### 1.3 命令处理器

| 命令 | Code | 处理函数 | 描述 |
|------|------|----------|------|
| MCAR/MCAA | 100000 | `cic_handle_mcar` | 客户端认证 |
| MCCR/MCCA | 100001 | `cic_handle_mccr` | 通信变更 |
| STR/STA | 275 | `cic_handle_str` | 会话终止 |
| MNTR/MNTA | 100002 | `cic_handle_mntr` | 通知报告 |
| MSCR/MSCA | 100003 | `cic_handle_mscr` | 状态变更 |
| MSXR/MSXA | 100004 | `cic_handle_msxr` | 状态查询 |
| MADR/MADA | 100005 | `cic_handle_madr` | 计费数据 |
| MACR/MACA | 100006 | `cic_handle_macr` | 计费控制 |

---

## 2. MCAR 处理器详解

### 2.1 功能

处理客户端认证请求，支持三种接入场景：

| 场景 | 条件 | 结果状态 |
|------|------|----------|
| **A** (Auth Only) | 仅认证 | `AUTHENTICATED` |
| **B** (Auth + Subscribe) | 包含 `REQ-Status-Info` | `AUTHENTICATED` + 订阅 |
| **C** (0-RTT Access) | 包含 `Communication-Request-Parameters` | `ACTIVE` (直接分配资源) |

### 2.2 五步流水线

```
┌──────────────────────────────────────────────────────────────────┐
│                        MCAR Request                              │
└──────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│ Step 1: mcar_step1_validation                                    │
│ ─────────────────────────────                                    │
│ • 提取 Session-Id, Origin-Host, Origin-Realm                     │
│ • 解析 Client-Credentials (User-Name, Client-Password)           │
│ • 检测重复会话 (会话冲突处理)                                     │
│ • 提取 Communication-Request-Parameters (场景 C)                 │
│ • 确定接入场景 (A/B/C)                                           │
└──────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│ Step 2: mcar_step2_auth                                          │
│ ────────────────────────                                         │
│ • 查找客户端配置 (Client_Profile.xml)                             │
│ • 验证用户名/密码                                                 │
│ • 创建会话对象 (magic_session_create)                             │
│ • 设置状态为 AUTHENTICATED                                       │
│ • 注册到 control 白名单 (iptables/nftables)                       │
└──────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│ Step 3: mcar_step3_subscription (场景 B)                         │
│ ──────────────────────────────────────                           │
│ • 解析 REQ-Status-Info 订阅级别                                   │
│ • 权限检查 (详细状态可能需要高权限)                                │
│ • 注册订阅到会话                                                  │
└──────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│ Step 4: mcar_step4_allocation (场景 C)                           │
│ ────────────────────────────────────                             │
│ • 从 Profile 填充缺失的默认参数                                   │
│ • 带宽削峰 (检查是否超限)                                         │
│ • 调用 Policy Engine 选择链路                                     │
│ • 发送 MIH_Link_Resource.request 到 DLM                          │
│ • 更新会话状态为 ACTIVE                                          │
│ • 配置数据平面路由                                                │
└──────────────────────────────────────────────────────────────────┘
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│ Step 5: mcar_step5_finalize                                      │
│ ───────────────────────────                                      │
│ • 构建 MCAA 应答消息                                             │
│ • 添加 Auth-Application-Id, Result-Code                          │
│ • 添加 Server-Password (双向认证)                                 │
│ • 添加 Communication-Answer-Parameters (场景 C)                   │
│ • 发送应答                                                        │
└──────────────────────────────────────────────────────────────────┘
```

### 2.3 关键数据结构

```c
typedef struct {
  /* 从请求中提取的信息 */
  char session_id[128];       /* 会话 ID */
  char client_id[MAX_ID_LEN]; /* 客户端 ID (Origin-Host) */
  char username[MAX_ID_LEN];  /* 用户名 */
  char password[MAX_ID_LEN];  /* 密码 */
  
  /* 解析后的参数 */
  CommReqParams comm_params;   /* 通信请求参数 */
  bool has_comm_req_params;    /* 是否包含 Comm-Req-Params */
  bool has_req_status_info;    /* 是否包含 REQ-Status-Info */
  uint32_t req_status_info;    /* 订阅级别 */
  
  /* 认证结果 */
  ClientProfile *profile;      /* 客户端配置 */
  ClientSession *session;      /* 会话对象 */
  bool auth_success;           /* 认证是否成功 */
  
  /* 资源分配结果 */
  bool resource_allocated;     /* 资源是否分配成功 */
  PolicyResponse policy_resp;  /* 策略引擎响应 */
  MIH_Link_Resource_Confirm mih_confirm; /* MIH 确认 */
  
  /* 应答参数 */
  uint32_t result_code;        /* Diameter Result-Code */
  uint32_t magic_status_code;  /* MAGIC-Status-Code */
  const char *error_message;   /* 错误消息 */
} McarProcessContext;
```

---

## 3. MCCR 处理器详解

### 3.1 功能

处理通信变更请求，支持四种操作意图：

| 意图 | 条件 | 动作 |
|------|------|------|
| **START** | 会话已认证但无链路 | 首次分配链路资源 |
| **MODIFY** | 会话已有链路 + 参数变更 | 修改链路参数 |
| **STOP** | 会话已有链路 + Keep-Request=0 | 释放链路资源 |
| **QUEUE** | 资源不足 + Keep-Request=1 | 加入等待队列 |

### 3.2 四阶段流水线

```
Phase 1: Session Validation     → 验证会话存在且有效
Phase 2: Param & Security Check → 解析参数并做 TFT 安全校验
Phase 3: Intent Routing         → 判断操作类型 (Start/Modify/Stop/Queue)
Phase 4: Execution & Response   → 执行操作并发送应答
```

### 3.3 排队管理器

```c
#define MCCR_QUEUE_MAX_SIZE 64    /* 最大排队请求数 */
#define MCCR_QUEUE_TIMEOUT_SEC 30 /* 排队超时时间 */

typedef struct {
  bool in_use;
  MccxQueueState state;       /* PENDING/PROCESSING/COMPLETED/EXPIRED */
  char session_id[128];
  CommReqParams params;
  time_t enqueue_time;
  time_t expire_time;
  uint32_t priority;          /* 数字越小优先级越高 */
} MccxQueueEntry;
```

### 3.4 链路回退机制

```
1. 首选链路请求失败
        ▼
2. 重试 (最多 MCCR_RETRY_MAX_COUNT=3 次)
        ▼
3. 询问 Policy Engine 获取候选链路列表
        ▼
4. 依次尝试每个候选链路 (最多 MCCR_FALLBACK_MAX_LINKS=4 个)
        ▼
5. 所有链路都失败 → 返回 NO_BW 错误
```

---

## 4. 其他处理器

### 4.1 STR 处理器

```
功能: 处理会话终止请求
流程:
  1. 查找会话
  2. 释放链路资源 (MIH_Link_Resource.request RELEASE)
  3. 删除数据平面路由
  4. 从 control/forward 白名单移除
  5. 删除会话
  6. 发送 STA 应答
```

### 4.2 MSXR 处理器

```
功能: 处理状态查询请求
支持的 Status-Type:
  • 0 - No_Status (无状态)
  • 1 - MAGIC_Policy_Status (策略状态)
  • 2 - MAGIC_DLM_No_List (DLM 简要信息)
  • 3 - MAGIC_DLM_Status (DLM 详细状态)
  • 6 - Link_Status (链路状态, 单个 DLM)
  • 7 - All_Status (所有详细信息)

速率限制: 同一客户端每 5 秒最多一次查询
```

### 4.3 MADR/MACR 处理器

```
MADR 功能: 查询 CDR (Call Detail Records)
  • 支持按类型、级别过滤
  • 返回 Active/Finished/Forwarded/Unknown CDR 列表

MACR 功能: 计费控制（不断网切账单）
  • Snapshot: 快照当前流量数据
  • Archive: 归档旧 CDR
  • Create: 创建新 CDR
  • 返回 CDR-Updated 列表
```

---

## 5. 辅助函数

### 5.1 AVP 解析

| 函数 | 描述 |
|------|------|
| `parse_comm_req_params` | 解析 Communication-Request-Parameters |
| `extract_string_from_grouped_avp` | 从 Grouped AVP 提取字符串 |
| `extract_uint32_from_grouped_avp` | 从 Grouped AVP 提取 U32 |
| `extract_float32_from_grouped_avp` | 从 Grouped AVP 提取 Float32 |

### 5.2 参数处理

| 函数 | 描述 |
|------|------|
| `comm_req_params_init` | 初始化参数为默认值 |
| `comm_req_params_fill_from_profile` | 从配置 Profile 填充缺失参数 |
| `handle_unknown_avps` | 处理未知 AVP（日志记录） |

### 5.3 速率限制

| 函数 | 描述 |
|------|------|
| `msxr_check_rate_limit` | 检查 MSXR 请求速率 |

---

## 6. 初始化与清理

### 6.1 初始化流程

```c
int magic_cic_init(MagicContext *ctx) {
    // 1. 保存全局上下文指针
    g_ctx = ctx;
    
    // 2. 初始化字典句柄
    magic_dict_init();
    
    // 3. 注册应用支持 (Vendor-Specific)
    fd_disp_app_support(g_magic_dict.app, g_magic_dict.vendor, 1, 0);
    
    // 4. 注册命令处理器
    fd_disp_register(cic_handle_mcar, DISP_HOW_CC, &when, NULL);
    fd_disp_register(cic_handle_mccr, DISP_HOW_CC, &when, NULL);
    fd_disp_register(cic_handle_str,  DISP_HOW_CC, &when, NULL);
    // ... 其他处理器
    
    return 0;
}
```

### 6.2 清理流程

```c
void magic_cic_cleanup(MagicContext *ctx) {
    g_ctx = NULL;
    fd_log_notice("[app_magic] CIC module cleanup complete");
}
```

---

## 7. 线程安全

### 7.1 已实现的保护

| 组件 | 锁类型 | 保护范围 |
|------|--------|----------|
| `SessionManager` | `pthread_mutex_t` | 会话数组的增删查 |
| `MccxQueueManager` | `pthread_mutex_t` | 排队队列的操作 |
| `MSXR Rate Limit` | `pthread_mutex_t` | 速率限制条目 |

### 7.2 已知限制

**Check-then-Act 问题**：

```c
// 问题代码模式
ClientSession *session = magic_session_find_by_id(mgr, session_id);
// 返回指针后锁已释放，其他线程可能修改/删除 session
if (session->state == ACTIVE) {  // 可能访问已释放内存
    ...
}
```

**建议方案**：
- 引入引用计数
- 使用读写锁
- 操作期间持有锁

---

## 8. 错误处理

### 8.1 Result-Code 映射

| Diameter Code | 含义 | 使用场景 |
|---------------|------|----------|
| 2001 | SUCCESS | 请求成功 |
| 3004 | TOO_BUSY | 服务器繁忙/速率限制 |
| 5001 | AVP_UNSUPPORTED | 不支持的 AVP |
| 5004 | INVALID_AVP_VALUE | AVP 值无效 |
| 5012 | UNABLE_TO_COMPLY | 无法处理 |

### 8.2 MAGIC-Status-Code

| Code | 名称 | 描述 |
|------|------|------|
| 1000 | INTERNAL_ERROR | 内部错误 |
| 1001 | AUTH_FAILED | 认证失败 |
| 1010 | NO_BW | 无可用带宽 |
| 1020 | TFT_REJECTED | TFT 规则被拒绝 |

---

## 9. 维护指南

### 9.1 添加新命令处理器

1. 在 `magic_cic.c` 中实现处理函数
2. 在 `magic_cic_init` 中注册处理器
3. 更新 `dict_magic.h` 添加命令句柄

### 9.2 调试技巧

```bash
# 查看 CIC 日志
sudo journalctl -u freeDiameter | grep app_magic

# Wireshark 过滤
diameter.cmd.code == 100000  # MCAR
diameter.cmd.code == 100001  # MCCR
```

### 9.3 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 认证失败 | 密码不匹配 | 检查 `Client_Profile.xml` |
| 无带宽分配 | 策略限制或 DLM 不可用 | 检查策略规则和 LMI 连接 |
| 会话不存在 | 会话已过期或被清理 | 检查会话超时配置 |

---

## 10. 依赖关系

```
magic_cic.c
    ├── app_magic.h           (MagicContext)
    ├── dict_magic.h          (字典句柄)
    ├── magic_session.h       (会话管理)
    ├── magic_policy.h        (策略引擎)
    ├── magic_config.h        (配置管理)
    ├── magic_lmi.h           (LMI/MIH 接口)
    ├── magic_dataplane.h     (数据平面路由)
    ├── magic_cdr.h           (CDR 管理)
    └── magic_group_avp_simple.h (AVP 构建)
```

---

## 11. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2024-01-01 | 初始版本，支持 MCAR/MCCR/STR |
| 2.0 | 2025-12-14 | 添加 MSXR/MADR/MACR，修复线程安全问题 |
