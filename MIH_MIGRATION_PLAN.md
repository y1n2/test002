# DLM 系统协议迁移计划

## 当前问题

系统中存在两套不兼容的 IPC 协议：

1. **旧协议** (`magic_ipc_protocol.h`):
   - Socket: `/tmp/magic_cm.sock` 
   - DLM daemons 当前使用
   - 消息类型：`MSG_REGISTER_REQUEST (0x0001)`
   - 数据结构：`ipc_register_req_t` (包含 `ipc_msg_header_t`)

2. **App_magic 协议** (`magic_dlm_manager.c`内部定义):
   - Socket: `/tmp/magic_core.sock`
   - app_magic extension 使用
   - 消息类型：`MSG_TYPE_REGISTER (0x01)`
   - 数据结构：`MsgRegister` (简化结构)

**根本冲突**：DLM 连接成功但无法通信，因为消息格式完全不同！

## 解决方案：标准 MIH 协议

按照 ARINC 839 Attachment 2 规范，使用 **IEEE 802.21 MIH 原语**：

### 新协议文件
- `include/ipc/magic_mih_protocol.h` ✅ 已创建
- `include/ipc/magic_mih_utils.c` ✅ 已创建

### MIH 原语映射

| 功能 | MIH 原语 | 代码 |
|------|----------|------|
| DLM 注册 | `MIH_Link_Register.request/confirm` | 0x0101/0x0102 |
| 链路激活 | `MIH_Link_Up.indication` | 0x0202 |
| 链路断开 | `MIH_Link_Down.indication` | 0x0203 |
| 资源分配 | `MIH_Link_Resource.request/confirm` | 0x0301/0x0302 |
| 心跳 | `MIH_Heartbeat` | 0x0F01 |

### 关键数据结构

```c
/* MIH 消息头（所有消息公用）*/
typedef struct {
    uint16_t primitive_type;    // MIH 原语类型
    uint16_t message_length;    // 消息总长度
    uint32_t transaction_id;    // 事务ID（request/confirm配对）
    uint32_t timestamp;
} mih_header_t;

/* 链路标识符 */
typedef struct {
    char     link_id[64];       // "LINK_SATCOM"
    char     interface_name[16]; // "eth1"
    uint8_t  link_type;         // LINK_TYPE_SATCOM
} mih_link_identifier_t;

/* 注册请求 */
typedef struct {
    mih_header_t            header;
    mih_link_identifier_t   link_id;
    mih_link_capabilities_t capabilities;  // 带宽、延迟、成本等
    pid_t                   dlm_pid;
} mih_link_register_req_t;

/* 资源请求（Bearer 分配）*/
typedef struct {
    mih_header_t            header;
    mih_link_identifier_t   link_id;
    uint8_t                 action;         // ALLOCATE/MODIFY/RELEASE
    BEARER_ID               bearer_id;
    mih_qos_params_t        qos_params;     // 带宽、优先级、QoS类
    uint32_t                session_id;
    char                    client_id[64];
} mih_link_resource_req_t;
```

## 迁移步骤

### Phase 1: 更新 DLM Daemons
1. 修改头文件引用：`#include "../include/ipc/magic_mih_protocol.h"`
2. 替换注册流程：
   ```c
   // 旧代码
   ipc_register_req_t req;
   ipc_init_header(&req.header, MSG_REGISTER_REQUEST, ...);
   
   // 新代码
   mih_link_register_req_t req;
   mih_init_header(&req.header, MIH_LINK_REGISTER_REQUEST, ...);
   req.link_id.link_type = LINK_TYPE_SATCOM;
   strncpy(req.link_id.link_id, "LINK_SATCOM", 64);
   ```
3. 替换链路状态报告：`MIH_Link_Up/Down.indication`
4. 替换心跳：`MIH_Heartbeat`

### Phase 2: 更新 app_magic DLM Manager
1. 包含 MIH 协议头
2. 替换消息处理 switch:
   ```c
   switch (header->primitive_type) {
       case MIH_LINK_REGISTER_REQUEST:
           handle_mih_link_register(ctx, client_fd, ...);
           break;
       case MIH_LINK_UP_INDICATION:
           handle_mih_link_up(ctx, client_fd, ...);
           break;
       case MIH_LINK_RESOURCE_REQUEST:
           handle_mih_link_resource(ctx, client_fd, ...);
           break;
   }
   ```
3. 实现 MIH 处理函数

### Phase 3: 重新编译
```bash
# 重新编译 DLM daemons
cd DLM_SATCOM && gcc -o dlm_satcom_daemon dlm_satcom_daemon.c \
    ../include/ipc/magic_mih_utils.c -pthread -I../include

# 重新编译 app_magic extension
cd build && make clean && make
```

### Phase 4: 测试
```bash
# 1. 启动 freeDiameter with app_magic
cd build && freeDiameterd -c ../conf/magic_server.conf -dd

# 2. 启动 DLM 系统
./start_dlm_system.sh

# 3. 检查注册
tail -f logs/dlm_satcom.log  # 应该看到 "MIH_Link_Register.confirm: SUCCESS"

# 4. 测试 MCCR 流程
cd diameter_client && ./connect
mcar
mccr create IP_DATA 256 512 5 0
```

## 预期结果

成功后应该看到：

```
[DLM-SATCOM] Sending MIH_Link_Register.request
[DLM-SATCOM] ✓ Received MIH_Link_Register.confirm: STATUS_SUCCESS
[DLM-SATCOM] ✓ Registered with CM Core, assigned_id=1

[app_magic] MIH_Link_Register.request from LINK_SATCOM (eth1)
[app_magic] ✓ Link registered: LINK_SATCOM, capabilities: 2000 kbps, 600ms

[app_magic] Processing MCCR for client 'test-aircraft'
[app_magic] ✓ Selected LINK_SATCOM for BEST_EFFORT traffic
[app_magic] Sending MIH_Link_Resource.request to SATCOM DLM

[DLM-SATCOM] MIH_Link_Resource.request: ALLOCATE, 512 kbps
[DLM-SATCOM] ✓ Bearer allocated: bearer_id=1
[DLM-SATCOM] Sending MIH_Link_Resource.confirm

[app_magic] ✓ Bearer allocated: bearer_id=1, granted 512/256 kbps
[app_magic] Sending MCCA with Result-Code 2001
```

## 时间估算
- Phase 1 (DLM daemons): 2-3 hours
- Phase 2 (app_magic): 3-4 hours
- Phase 3 (编译): 30 minutes
- Phase 4 (测试): 1-2 hours

**总计**: 约 7-10 小时完整迁移

## 下一步

你希望我：
1. **立即开始迁移** - 我会依次修改所有文件
2. **先迁移一个 DLM** - 作为概念验证，先迁移 SATCOM
3. **review 协议设计** - 确认 MIH 原语定义是否完全符合 ARINC 839

请告诉我你的选择！
