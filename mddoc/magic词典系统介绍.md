# MAGIC Diameter 词典系统设计说明

## 1. 概述

本文档描述了 MAGIC (Multi-link Aggregation Gateway for Internet Connectivity) 系统中 Diameter 协议词典的设计与实现，符合 **ARINC 839-2014** 规范。

### 1.1 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    freeDiameter Core                            │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────┐    ┌──────────────────────────────┐   │
│  │   dict_magic_839     │    │        app_magic             │   │
│  │   (词典扩展模块)      │    │      (应用逻辑模块)           │   │
│  ├──────────────────────┤    ├──────────────────────────────┤   │
│  │ • Vendor 定义        │    │ • 应用支持注册               │   │
│  │ • Application 定义   │◄───│ • 命令处理器                 │   │
│  │ • AVP 类型定义       │    │ • 业务逻辑                   │   │
│  │ • Command 定义       │    │ • 会话管理                   │   │
│  │ • 规则约束           │    │ • 策略决策                   │   │
│  └──────────────────────┘    └──────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 职责分离原则

| 模块 | 职责 | 文件位置 |
|------|------|----------|
| **dict_magic_839** | 协议元素定义（纯字典） | `extensions/dict_magic_839/` |
| **app_magic** | 应用逻辑实现（使用字典） | `extensions/app_magic/` |

---

## 2. 协议标识符

### 2.1 Vendor ID

```c
Vendor ID: 13712  // AEEC (Airlines Electronic Engineering Committee)
Vendor Name: "AEEC (ARINC)"
```

### 2.2 Application ID

```c
Application ID: 1094202169  // 0x41383339, ASCII = "A839"
Application Name: "MAGIC-ARINC839"
```

> **注意**: Application ID 必须使用 ARINC 839 官方分配值 `1094202169`，而非测试值 `16777300`。

---

## 3. 命令定义 (Appendix B)

### 3.1 命令代码分配

| 命令对 | Code | 请求名称 | 应答名称 | 用途 |
|--------|------|----------|----------|------|
| MCAR/MCAA | 100000 | Client-Authentication-Request | Client-Authentication-Answer | 客户端认证 |
| MCCR/MCCA | 100001 | Communication-Change-Request | Communication-Change-Answer | 通信参数变更 |
| MNTR/MNTA | 100002 | Notification-Report | Notification-Answer | 事件通知 |
| MSCR/MSCA | 100003 | Status-Change-Report | Status-Change-Answer | 状态变更广播 |
| MSXR/MSXA | 100004 | Status-Request | Status-Answer | 状态查询 |
| MADR/MADA | 100005 | Accounting-Data-Request | Accounting-Data-Answer | 计费数据请求 |
| MACR/MACA | 100006 | Accounting-Control-Request | Accounting-Control-Answer | 计费控制 |

### 3.2 命令结构示例 (MCAR)

```
MCAR ::= < Diameter Header: 100000, REQ, PXY >
         < Session-Id >
         { Origin-Host }
         { Origin-Realm }
         { Destination-Realm }
         { Auth-Application-Id }      ; 必选 ✓
         [ Session-Timeout ]
         [ Client-Credentials ]
         [ Auth-Session-State ]
         [ Authorization-Lifetime ]
         [ Auth-Grace-Period ]
         [ Destination-Host ]
         [ REQ-Status-Info ]
         [ Communication-Request-Parameters ]
```

**规则说明**:
- `< >` = `RULE_FIXED_HEAD` (固定在头部)
- `{ }` = `RULE_REQUIRED` (必选)
- `[ ]` = `RULE_OPTIONAL` (可选)

---

## 4. AVP 定义 (Attachment 1)

### 4.1 AVP 编码范围

| 类型 | Code 范围 | 数量 | 示例 |
|------|-----------|------|------|
| 基础 AVP | 10001-10054 | 54 | Client-Password, DLM-Name, QoS-Level |
| 分组 AVP | 20001-20019 | 19 | Communication-Request-Parameters |

### 4.2 基础 AVP 类型定义

```c
// UTF8String 类型
{ 10001, 13712, "Client-Password",  AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_OCTETSTRING }
{ 10004, 13712, "DLM-Name",         AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_OCTETSTRING }
{ 10040, 13712, "Profile-Name",     AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_OCTETSTRING }

// Unsigned32 类型
{ 10002, 13712, "REQ-Status-Info",  AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_UNSIGNED32 }
{ 10053, 13712, "MAGIC-Status-Code",AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_UNSIGNED32 }

// Float32 类型
{ 10006, 13712, "DLM-Max-Bandwidth",AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_FLOAT32 }
{ 10051, 13712, "Granted-Bandwidth",AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_FLOAT32 }

// Enumerated 类型 (底层为 Integer32)
{ 10005, 13712, "DLM-Available",    AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_INTEGER32 }
{ 10027, 13712, "QoS-Level",        AVP_FLAG_VENDOR|AVP_FLAG_MANDATORY, AVP_TYPE_INTEGER32 }
```

### 4.3 分组 AVP 结构

#### Communication-Request-Parameters (20001)

```
Communication-Request-Parameters ::= < AVP Header: 20001 >
    { Profile-Name }
    [ Requested-Bandwidth ]
    [ Requested-Return-Bandwidth ]
    [ Required-Bandwidth ]
    [ Required-Return-Bandwidth ]
    [ Priority-Type ]
    [ QoS-Level ]
    [ DLM-Name ]
    [ Flight-Phase ]
    [ Altitude ]
    [ Airport ]
    ...
```

#### DLM-Info (20008)

```
DLM-Info ::= < AVP Header: 20008 >
    { DLM-Name }
    { DLM-Available }
    { DLM-Max-Links }
    { DLM-Max-Bandwidth }
    [ DLM-Max-Return-Bandwidth ]
    { DLM-Allocated-Links }
    { DLM-Allocated-Bandwidth }
    [ DLM-Allocated-Return-Bandwidth ]
    { DLM-QoS-Level-List }
    [ DLM-Link-Status-List ]
```

---

## 5. 实现细节

### 5.1 文件结构

```
extensions/dict_magic_839/
├── CMakeLists.txt           # 构建配置
├── dict_magic.c             # 主入口：注册 Vendor, App, Commands, Rules
├── add_avps.c               # AVP 定义（由 CSV 生成）
└── dict_magic_839.csv       # AVP 定义源文件
```

### 5.2 初始化流程

```c
// dict_magic.c
static int dict_magic_arinc839_entry(char *conffile) {
    // 1. 注册 Vendor
    struct dict_vendor_data vendor_data = { 13712, "AEEC (ARINC)" };
    CHECK_dict_new(DICT_VENDOR, &vendor_data, NULL, NULL);
    
    // 2. 注册 Application
    struct dict_application_data app_data = { 1094202169, "MAGIC-ARINC839" };
    CHECK_dict_new(DICT_APPLICATION, &app_data, NULL, &magic_app);
    
    // 3. 加载 AVP 定义
    CHECK_FCT(add_avps());
    
    // 4. 定义分组 AVP 规则
    // ... Communication-Request-Parameters, DLM-Info, etc.
    
    // 5. 定义命令结构
    // ... MCAR/MCAA, MCCR/MCCA, etc.
    
    return 0;
}
```

### 5.3 规则定义宏

```c
struct local_rules_definition {
    char           *avp_name;   // AVP 名称
    enum rule_position position; // 约束类型
    int             min;         // 最小出现次数
    int             max;         // 最大出现次数
};

// 使用示例
struct local_rules_definition rules[] = {
    { "Session-Id",         RULE_FIXED_HEAD, -1, 1 },  // 固定头部，1次
    { "Origin-Host",        RULE_REQUIRED,   -1, 1 },  // 必选，1次
    { "Auth-Application-Id",RULE_REQUIRED,   -1, 1 },  // 必选，1次
    { "Client-Credentials", RULE_OPTIONAL,   -1, 1 },  // 可选，0-1次
    { "TFTtoGround-Rule",   RULE_REQUIRED,    1, 255}, // 必选，1-255次
};
PARSE_loc_rules(rules, cmd);
```

---

## 6. 应用支持注册

### 6.1 正确位置

应用支持注册必须在 **app_magic** 模块中完成，而非词典模块：

```c
// app_magic/magic_cic.c - magic_cic_init()
int magic_cic_init(MagicContext *ctx) {
    // 初始化字典句柄
    CHECK_FCT(magic_dict_init());
    
    // 注册 Vendor-Specific 应用支持
    // 参数: (app对象, vendor对象, 支持认证, 支持计费)
    CHECK_FCT(fd_disp_app_support(
        g_magic_dict.app,      // MAGIC Application
        g_magic_dict.vendor,   // AEEC Vendor (13712)
        1,                     // Auth = Yes
        0                      // Acct = No
    ));
    
    // 注册命令处理器...
}
```

### 6.2 影响

正确传入 vendor 参数后：

- CER/CEA 消息中正确包含 `Vendor-Specific-Application-Id`
- 其他 ARINC 839 节点能识别此为 Vendor-Specific 应用
- 确保与标准实现的互操作性

---

## 7. 符合性检查清单

| 检查项 | 规范章节 | 状态 |
|--------|----------|------|
| Application ID = 1094202169 | 4.1.2 | ✅ |
| Vendor ID = 13712 (AEEC) | 4.1.1 | ✅ |
| Client-Password 为 UTF8String | 1.1.1.2.1 | ✅ |
| MCAR 包含 Auth-Application-Id | Appendix B-1.1 | ✅ |
| MCAA 包含 Auth-Application-Id | Appendix B-1.2 | ✅ |
| Server-Password 为必选 | Appendix B-1.2 | ✅ |
| 所有 AVP 使用正确 Vendor 标志 | Attachment 1 | ✅ |

---

## 8. 参考文档

- **ARINC 839-2014**: MAGIC Protocol Specification
  - Attachment 1: Data Definitions
  - Appendix B: Command Structure
- **RFC 6733**: Diameter Base Protocol
- **freeDiameter**: [libfdproto API Reference](http://www.freediameter.net/doc/)

---

## 9. 版本记录

| 版本 | 日期 | 修改内容 |
|------|------|----------|
| 1.0 | 2025-12-14 | 初始版本，完成 ARINC 839 规范合规性修正 |
