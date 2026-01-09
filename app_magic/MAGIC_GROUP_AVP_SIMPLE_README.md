# MAGIC Grouped AVP 简化版辅助函数使用说明

## 简介

`magic_group_avp_simple.h/c` 是为 MAGIC 协议设计的简化版 Grouped AVP 构造辅助库，特点如下：

### 优点
1. **零外部依赖**：无需 config.h 和 log.h，只依赖 freeDiameter 和已有的 dict_magic.h
2. **参数化设计**：直接传递参数，而非从全局配置读取
3. **简洁接口**：使用结构体参数，清晰明了
4. **标准日志**：使用 freeDiameter 的 fd_log_* 函数
5. **完全兼容**：与 add_avp.h 无缝集成

### 当前实现的函数

- ✅ `add_client_credentials_simple()` - 客户端认证凭据 (AVP 20019)
- ✅ `add_comm_req_params_simple()` - 通信请求参数 (AVP 20001)
- ✅ `add_comm_ans_params_simple()` - 通信应答参数 (AVP 20002)

## 使用示例

### 1. 添加 Client-Credentials (用于 MCAR)

```c
#include "magic_group_avp_simple.h"

/* 在 MCAR 请求中添加认证凭据 */
int result = add_client_credentials_simple(
    request_msg,
    "aircraft_001",      // username
    "secret_password",   // client_password
    NULL                 // server_password (可选)
);

if (result != 0) {
    fd_log_error("添加 Client-Credentials 失败");
    return -1;
}
```

### 2. 添加 Communication-Request-Parameters (用于 MCCR)

```c
/* 构造通信请求参数 */
comm_req_params_t req_params;
memset(&req_params, 0, sizeof(req_params));

/* 必填字段 */
req_params.profile_name = "VOICE";

/* 可选字段 */
req_params.requested_bw = 512000;          // 512 kbps 下行
req_params.requested_return_bw = 128000;   // 128 kbps 上行
req_params.priority_class = 3;             // 中等优先级
req_params.qos_level = 2;                  // QoS 级别 2
req_params.dlm_name = "KA_SAT";            // 指定链路
req_params.flight_phase = 2;               // 巡航阶段
req_params.altitude = 10000;               // 10km 高度

/* 添加到消息 */
if (add_comm_req_params_simple(request_msg, &req_params) != 0) {
    fd_log_error("添加 Communication-Request-Parameters 失败");
    return -1;
}
```

### 3. 添加 Communication-Answer-Parameters (用于 MCCA)

这是服务器在 `magic_cic.c` 中的典型用法：

```c
#include "magic_group_avp_simple.h"

/* 在 MCCR 处理器中，策略决策和 MIH 资源分配后 */
{
    comm_ans_params_t ans_params;
    memset(&ans_params, 0, sizeof(ans_params));
    
    /* 必填：选中的链路 ID */
    ans_params.selected_link_id = policy_resp.selected_link_id;  // "KA_SAT"
    
    /* 必填：分配的 Bearer ID */
    ans_params.bearer_id = mih_confirm.has_bearer_id ? 
                           mih_confirm.bearer_identifier : 0;
    
    /* 可选：授予的带宽 (bps) */
    ans_params.granted_bw = policy_resp.granted_bw_kbps * 1000;
    ans_params.granted_return_bw = policy_resp.granted_ret_bw_kbps * 1000;
    
    /* 可选：会话超时 (秒) */
    ans_params.session_timeout = 3600;  // 1 小时
    
    /* 可选：分配的 IP 地址 */
    ans_params.assigned_ip = NULL;  // 暂不分配，可传 "10.0.1.100"
    
    /* 添加到应答消息 */
    if (add_comm_ans_params_simple(ans, &ans_params) != 0) {
        fd_log_error("[app_magic] 添加 Communication-Answer-Parameters 失败");
        goto error;
    }
}
```

## 完整示例：在 magic_cic.c 中集成

### 修改前（TODO 注释）

```c
/* 使用辅助宏添加必需的标准 AVP */
ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);  /* DIAMETER_SUCCESS */

/* TODO: 添加 Communication-Answer-Parameters (包含 selected link, bearer ID 等信息) */
```

### 修改后（已实现）

```c
/* 使用辅助宏添加必需的标准 AVP */
ADD_AVP_STR(ans, g_std_dict.avp_origin_host, fd_g_config->cnf_diamid);
ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, fd_g_config->cnf_diamrlm);
ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);  /* DIAMETER_SUCCESS */

/* 添加 Communication-Answer-Parameters */
{
    comm_ans_params_t ans_params;
    memset(&ans_params, 0, sizeof(ans_params));
    ans_params.selected_link_id = policy_resp.selected_link_id;
    ans_params.bearer_id = mih_confirm.has_bearer_id ? mih_confirm.bearer_identifier : 0;
    ans_params.granted_bw = policy_resp.granted_bw_kbps * 1000;  /* kbps -> bps */
    ans_params.granted_return_bw = policy_resp.granted_ret_bw_kbps * 1000;
    ans_params.session_timeout = 3600;  /* 1 小时 */
    ans_params.assigned_ip = NULL;  /* 可选，暂不分配 */
    
    if (add_comm_ans_params_simple(ans, &ans_params) != 0) {
        fd_log_error("[app_magic] 添加 Communication-Answer-Parameters 失败");
        goto error;
    }
}
```

## API 参考

### add_client_credentials_simple()

```c
int add_client_credentials_simple(struct msg *msg, 
                                   const char *username,
                                   const char *client_password,
                                   const char *server_password);
```

**参数**:
- `msg` - 要添加 AVP 的 Diameter 消息
- `username` - 用户名（必填）
- `client_password` - 客户端密码（必填）
- `server_password` - 服务器密码（可选，传 NULL 表示不添加）

**返回值**: 0 成功, -1 失败

**使用场景**: MCAR (认证请求)

---

### add_comm_req_params_simple()

```c
int add_comm_req_params_simple(struct msg *msg, const comm_req_params_t *params);
```

**参数结构体**:
```c
typedef struct {
    const char *profile_name;       // 必填：会话类型
    uint64_t requested_bw;          // 可选：请求下行带宽(bps)，0=不添加
    uint64_t requested_return_bw;   // 可选：请求上行带宽(bps)，0=不添加
    uint32_t priority_class;        // 可选：优先级 1-8，0=不添加
    uint32_t qos_level;             // 可选：QoS等级 0-3，0=不添加
    const char *dlm_name;           // 可选：指定链路模块，NULL=不添加
    uint32_t flight_phase;          // 可选：飞行阶段，0=不添加
    uint32_t altitude;              // 可选：高度(米)，0=不添加
} comm_req_params_t;
```

**返回值**: 0 成功, -1 失败

**使用场景**: MCCR (通信请求)

---

### add_comm_ans_params_simple()

```c
int add_comm_ans_params_simple(struct msg *msg, const comm_ans_params_t *params);
```

**参数结构体**:
```c
typedef struct {
    const char *selected_link_id;   // 必填：选中的链路ID
    uint32_t bearer_id;             // 必填：分配的Bearer ID
    uint64_t granted_bw;            // 可选：授予下行带宽(bps)，0=不添加
    uint64_t granted_return_bw;     // 可选：授予上行带宽(bps)，0=不添加
    uint32_t session_timeout;       // 可选：会话超时(秒)，0=不添加
    const char *assigned_ip;        // 可选：分配的IP，NULL=不添加
} comm_ans_params_t;
```

**返回值**: 0 成功, -1 失败

**使用场景**: MCCA (通信应答)

## AVP 映射说明

由于 dict_magic.h 中的 AVP 定义与 ARINC 839 规范可能存在差异，简化版函数使用了以下映射：

| 参数字段 | 映射的 MAGIC AVP | AVP Code |
|---------|-----------------|----------|
| selected_link_id | DLM-Name | 10004 |
| bearer_id | Link-Number | 10012 |
| granted_bw | Granted-Bandwidth | 10051 |
| granted_return_bw | Granted-Return-Bandwidth | 10052 |
| session_timeout | Timeout | 10039 |
| assigned_ip | Gateway-IP | 10029 |

## 扩展开发

如需添加更多 Grouped AVP 辅助函数（如 Link-Characteristics, Bearer-Info 等），请：

1. 在 `dict_magic.h` 中添加缺失的 AVP 定义
2. 在 `dict_magic.c` 的 `magic_dict_init()` 中初始化这些 AVP
3. 取消 `magic_group_avp_simple.h` 中对应的注释
4. 在 `magic_group_avp_simple.c` 中实现函数

## 编译信息

```bash
cd /home/zhuwuhui/freeDiameter/build
make app_magic
```

编译成功输出:
```
[100%] Built target app_magic
```

## 对比：原版 vs 简化版

| 特性 | 原版 (magic_group_avp_add.c) | 简化版 (magic_group_avp_simple.c) |
|------|------------------------------|-----------------------------------|
| 外部依赖 | config.h, log.h | 无 |
| 参数来源 | 全局配置 g_cfg | 函数参数 |
| 日志函数 | LOG_E, LOG_D | fd_log_error, fd_log_debug |
| 实现函数数 | 19个 | 3个（核心功能） |
| 使用复杂度 | 需要配置文件支持 | 直接调用 |
| 适用场景 | 完整应用程序 | 库函数/集成 |

## 总结

简化版 Grouped AVP 辅助函数提供了：

✅ **即插即用**：无需修改现有配置系统  
✅ **灵活性高**：参数完全由调用者控制  
✅ **依赖最小**：只需 freeDiameter 和 dict_magic  
✅ **代码清晰**：每个字段的意图一目了然  
✅ **易于测试**：可独立测试每个函数  

推荐在 `magic_cic.c` 等核心模块中使用简化版，以保持代码的模块化和可维护性。
