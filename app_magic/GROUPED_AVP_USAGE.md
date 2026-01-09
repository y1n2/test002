# Grouped AVP 辅助函数使用指南

## 概述

`magic_group_avp_simple.c` 提供了完整的 ARINC 839 MAGIC 协议 Grouped AVP 辅助函数，基于 `dict_magic_839` 词典系统实现。

## 新增函数列表

### 1. TFT (Traffic Flow Template) 管理

#### `add_tft_to_ground_list_simple()`
添加地面侧流量过滤模板列表 (Code 20004)

```c
const char *tft_rules[] = {
    "permit in ip from 192.168.1.0/24 to any",
    "deny in ip from any to 10.0.0.0/8"
};
add_tft_to_ground_list_simple(msg, tft_rules, 2);
```

#### `add_tft_to_aircraft_list_simple()`
添加飞机侧流量过滤模板列表 (Code 20005)

```c
const char *tft_rules[] = {
    "permit in ip from 10.0.0.1 to any",
};
add_tft_to_aircraft_list_simple(msg, tft_rules, 1);
```

### 2. NAPT (Network Address Port Translation) 管理

#### `add_napt_list_simple()`
添加网络地址端口转换规则列表 (Code 20006)

```c
const char *napt_rules[] = {
    "192.168.1.100:8080 -> 10.0.0.1:80",
    "192.168.1.101:8443 -> 10.0.0.2:443"
};
add_napt_list_simple(msg, napt_rules, 2);
```

### 3. DLM 列表管理

#### 创建和填充 DLM-List
```c
/* 步骤 1: 创建 DLM-List 容器 */
struct avp *dlm_list;
create_dlm_list_simple(msg, &dlm_list);

/* 步骤 2: 添加多个 DLM 信息 */
dlm_info_t dlm1 = {
    .dlm_name = "SAT1",
    .dlm_available = 0,  // YES
    .dlm_max_links = 4,
    .dlm_max_bw = 5120.0,  // kbps
    .dlm_max_return_bw = 2048.0,
    .dlm_alloc_links = 1,
    .dlm_alloc_bw = 1024.0,
    .dlm_alloc_return_bw = 512.0,
    .qos_levels = {0, 1, 2},  // BE, AF, EF
    .qos_level_count = 3
};
add_dlm_info_simple(dlm_list, &dlm1);

dlm_info_t dlm2 = {
    .dlm_name = "LTE1",
    .dlm_available = 0,
    .dlm_max_links = 8,
    .dlm_max_bw = 10240.0,
    .dlm_max_return_bw = 5120.0,
    .dlm_alloc_links = 0,
    .dlm_alloc_bw = 0.0,
    .dlm_alloc_return_bw = 0.0,
    .qos_levels = {0, 1},
    .qos_level_count = 2
};
add_dlm_info_simple(dlm_list, &dlm2);

/* 步骤 3: 完成并添加到消息 */
finalize_dlm_list_simple(msg, dlm_list);
```

### 4. 链路状态列表管理

#### 创建和填充 DLM-Link-Status-List
```c
/* 为 DLM-Info 创建链路状态列表 */
struct avp *link_status_list;
create_dlm_link_status_list_simple(dlm_info_avp, &link_status_list);

/* 添加多个链路状态 */
link_status_t link1 = {
    .link_name = "SAT1-Beam1",
    .link_number = 1,
    .link_available = 1,  // YES
    .qos_level = 1,  // AF
    .link_conn_status = 1,  // Connected
    .link_login_status = 2,  // Logged on
    .link_max_bw = 2048.0,
    .link_max_return_bw = 1024.0,
    .link_alloc_bw = 1024.0,
    .link_alloc_return_bw = 512.0,
    .link_error_string = NULL
};
add_link_status_simple(link_status_list, &link1);

/* 完成并添加到 DLM-Info */
finalize_dlm_link_status_list_simple(dlm_info_avp, link_status_list);
```

### 5. QoS 参数管理

#### `add_qos_params_inline()`
添加内联 QoS 参数（使用现有 AVP）

```c
/* Priority-Type: 2=Preemption, Priority-Class: "5", QoS-Level: 1=AF */
add_qos_params_inline(msg, 2, "5", 1);
```

### 6. 链路特征管理

#### `add_link_characteristics_inline()`
添加链路特征信息

```c
add_link_characteristics_inline(msg, "SAT1-Beam1", 1, 2048.0, 1);
/* 参数: link_name, link_number, max_bw(kbps), qos_level */
```

### 7. Bearer 信息管理

#### `add_bearer_info_inline()`
添加 Bearer 信息

```c
add_bearer_info_inline(msg, 1, 1, 2);
/* 参数: bearer_id, link_conn_status(0/1/2), link_login_status(1/2) */
```

## 完整示例：构建 MSXA 应答消息

```c
/* 创建应答消息 */
struct msg *ans;
fd_msg_new_answer_from_req(fd_g_config->cnf_dict, &qry, 0);
ans = qry;

/* 添加标准 AVP */
ADD_AVP_STR(ans, g_std_dict.avp_origin_host, "magic-server.example.com");
ADD_AVP_STR(ans, g_std_dict.avp_origin_realm, "example.com");
ADD_AVP_U32(ans, g_std_dict.avp_result_code, 2001);

/* 创建 DLM-List */
struct avp *dlm_list;
create_dlm_list_simple(ans, &dlm_list);

/* 添加 DLM 信息（SAT1） */
dlm_info_t sat1 = {
    .dlm_name = "SAT1",
    .dlm_available = 0,
    .dlm_max_links = 4,
    .dlm_max_bw = 5120.0,
    .dlm_max_return_bw = 2048.0,
    .dlm_alloc_links = 2,
    .dlm_alloc_bw = 2048.0,
    .dlm_alloc_return_bw = 1024.0,
    .qos_levels = {0, 1, 2},
    .qos_level_count = 3
};
add_dlm_info_simple(dlm_list, &sat1);

/* 添加 DLM 信息（LTE1） */
dlm_info_t lte1 = {
    .dlm_name = "LTE1",
    .dlm_available = 0,
    .dlm_max_links = 8,
    .dlm_max_bw = 10240.0,
    .dlm_max_return_bw = 5120.0,
    .dlm_alloc_links = 1,
    .dlm_alloc_bw = 1024.0,
    .dlm_alloc_return_bw = 512.0,
    .qos_levels = {0, 1},
    .qos_level_count = 2
};
add_dlm_info_simple(dlm_list, &lte1);

/* 完成 DLM-List */
finalize_dlm_list_simple(ans, dlm_list);

/* 发送应答 */
fd_msg_send(&ans, NULL, NULL);
```

## TFT 规则语法

TFT (Traffic Flow Template) 规则遵循 ARINC 839 协议规范：

```
<action> <direction> <protocol> from <source> to <destination> [port <ports>]
```

### 示例：
```c
"permit in ip from 192.168.1.0/24 to any"
"deny in ip from any to 10.0.0.0/8"
"permit out tcp from any to any port 80,443"
"permit in udp from 192.168.1.100 to 10.0.0.1 port 53"
```

## 错误处理

所有函数返回：
- `0` = 成功
- `-1` = 失败（通过 `fd_log_error` 记录详细错误）

### 常见错误：
1. **参数为 NULL** - 必填参数不能为空
2. **AVP 创建失败** - 内存不足或字典未初始化
3. **无效的规则数量** - QoS 等级列表必须包含 1-3 个元素

## 与词典系统集成

所有函数依赖于 `dict_magic_839` 词典扩展，确保：
1. `dict_magic_839.fdx` 已加载
2. `g_magic_dict` 全局句柄已初始化
3. 所有 AVP 定义在 `dict_magic.h` 中声明

## 编译验证

```bash
cd /home/zhuwuhui/freeDiameter/build
make app_magic

# 检查符号
nm extensions/app_magic.fdx | grep add_tft
nm extensions/app_magic.fdx | grep add_napt
nm extensions/app_magic.fdx | grep add_qos
```

## 性能注意事项

1. **批量添加** - 使用 `create_*` / `finalize_*` 模式批量添加 AVP，避免多次消息修改
2. **内存管理** - 函数失败时自动清理已分配的 AVP，无需手动释放
3. **字符串生命周期** - 传入的字符串指针必须在函数调用期间有效（函数内部会复制数据）

## 参考文档

- ARINC 839-2014 协议规范 Appendix B
- freeDiameter API 文档
- `dict_magic.h` - AVP 代码定义
- `add_avp.h` - 基础 AVP 添加宏

## 版本历史

- **v1.0** (2025-12-01): 初始实现，包含所有基础 Grouped AVP 辅助函数
  - TFT/NAPT 列表管理
  - DLM 和链路状态管理
  - 内联 QoS/Bearer/链路特征函数
