# dict_magic_839 - MAGIC (ARINC 839-2014) Diameter Dictionary

## 概述

`dict_magic_839` 是基于 ARINC 839-2014 标准实现的 freeDiameter 词典扩展，用于航空空地通信的 MAGIC 协议。

**标准**: ARINC 839-2014  
**应用ID**: 1094202169 (0x41383339)  
**厂商ID**: 13712 (AEEC - ARINC Industry Activities)  
**状态**: ✅ 生产就绪

---

## 特性

### ✅ 完整的协议支持

- **7对命令** (14个): MCAR/MCAA, MCCR/MCCA, MNTR/MNTA, MSCR/MSCA, MSXR/MSXA, MADR/MADA, MACR/MACA
- **54个基础AVP** (Code 10001-10054): 涵盖所有带宽、链路、QoS、计费参数
- **19个分组AVP** (Code 20001-20019): 完整的请求/应答/报告参数组
- **11种枚举类型**: 40+ 个枚举值，支持自动显示可读名称

### ✅ 增强功能 (v2.0 - 2025-12-16)

- **状态码常量化**: 50+ 个 Diameter 和 MAGIC 状态码常量定义
- **枚举值定义**: 所有枚举类型的值都可在调试时显示可读名称
- **辅助函数**: 状态码到字符串的转换函数
- **完整文档**: 检查报告和使用指南

---

## 文件结构

```
dict_magic_839/
├── dict_magic.c              # 词典主文件（命令和规则定义）
├── add_avps.c                # AVP 定义（CSV自动生成 + 枚举值）
├── dict_magic_839.csv        # CSV源文件
├── dict_magic_codes.h        # 状态码常量头文件 [新增]
├── dict_magic_codes.c        # 状态码辅助函数 [新增]
├── CMakeLists.txt            # 构建配置
├── README.md                 # 本文件
├── DICT_CHECK_REPORT.md      # 详细检查报告
└── USAGE_GUIDE.md            # 使用指南
```

---

## 快速开始

### 1. 编译

```bash
cd /path/to/freeDiameter/build
make dict_magic_839
```

### 2. 安装

```bash
sudo make install
# 词典库将安装到: /usr/local/lib/freeDiameter/dict_magic_839.so
# 头文件将安装到: /usr/local/include/freeDiameter/dict_magic_codes.h
```

### 3. 在 freeDiameter 配置中加载

编辑 `freeDiameter.conf`:

```conf
# 加载 MAGIC 词典
LoadExtension = "dict_magic_839.so";
```

### 4. 在应用代码中使用

```c
#include <freeDiameter/extension.h>
#include <freeDiameter/dict_magic_codes.h>

// 构造响应消息
struct msg *ans;
struct avp *avp;
union avp_value val;

// 使用状态码常量
CHECK_FCT(fd_msg_avp_new(dict_result_code, 0, &avp));
val.u32 = DIAMETER_SUCCESS;  // 而非魔术数字 2001
CHECK_FCT(fd_msg_avp_setvalue(avp, &val));

// 使用 MAGIC 状态码
val.u32 = MAGIC_INFO_SET_LINK_QOS;  // 清晰易懂
CHECK_FCT(fd_msg_avp_setvalue(magic_status_avp, &val));

// 打印可读日志
fd_log_error("Error: %s", magic_status_code_str(code));
```

---

## 协议详情

### 命令 (Commands)

| Code | Request | Answer | 用途 |
|------|---------|--------|------|
| 100000 | MCAR | MCAA | 客户端认证 |
| 100001 | MCCR | MCCA | 通信变更请求 |
| 100002 | MNTR | MNTA | 通知报告 |
| 100003 | MSCR | MSCA | 状态变更报告 |
| 100004 | MSXR | MSXA | 状态请求 |
| 100005 | MADR | MADA | 计费数据请求 |
| 100006 | MACR | MACA | 计费控制请求 |

### 关键 AVP

#### 基础 AVP (10001-10054)
- **认证**: Client-Password (10001), Server-Password (10045)
- **带宽**: Requested-Bandwidth (10021), Granted-Bandwidth (10051)
- **QoS**: QoS-Level (10027), Priority-Class (10025), Priority-Type (10026)
- **链路**: DLM-Name (10004), Link-Number (10012), Link-Connection-Status (10014)
- **飞行参数**: Flight-Phase (10033), Altitude (10034), Airport (10035)
- **流量规则**: TFTtoGround-Rule (10030), TFTtoAircraft-Rule (10031), NAPT-Rule (10032)
- **计费**: CDR-Type (10042), CDR-Level (10043), CDR-ID (10046)
- **状态**: MAGIC-Status-Code (10053), REQ-Status-Info (10002)

#### 分组 AVP (20001-20019)
- **通信参数**: Communication-Request-Parameters (20001), Communication-Answer-Parameters (20002)
- **流量规则组**: TFTtoGround-List (20004), TFTtoAircraft-List (20005), NAPT-List (20006)
- **DLM 信息**: DLM-List (20007), DLM-Info (20008), Link-Status-Group (20011)
- **计费组**: CDRs-Active (20012), CDRs-Finished (20013), CDR-Info (20017)

### 枚举类型

| AVP | 枚举值 |
|-----|--------|
| **QoS-Level** (10027) | BE=0, AF=1, EF=2 |
| **Priority-Type** (10026) | Blocking=1, Preemption=2 |
| **DLM-Available** (10005) | YES=1, NO=2, UNKNOWN=3 |
| **Link-Connection-Status** (10014) | Disconnected=1, Connected=2, Forced_Close=3 |
| **CDR-Type** (10042) | LIST_REQUEST=1, DATA_REQUEST=2 |
| **Auto-Detect** (10038) | NO=0, YES_Symmetric=1, YES_Asymmetric=2 |

详见 [USAGE_GUIDE.md](USAGE_GUIDE.md) 获取完整列表。

---

## 状态码

### Diameter 标准码

```c
DIAMETER_SUCCESS                     2001  // 成功
DIAMETER_AUTHENTICATION_REJECTED     4001  // 认证被拒绝
DIAMETER_MISSING_AVP                 5005  // 缺少必需 AVP
DIAMETER_INVALID_AVP_VALUE           5004  // AVP 值无效
```

### MAGIC 专有码

```c
// 错误码 (1000-1037)
MAGIC_ERROR_AUTHENTICATION_FAILED    1001  // 认证失败
MAGIC_ERROR_NO_FREE_BANDWIDTH        1016  // 无剩余带宽
MAGIC_ERROR_ILLEGAL_FLIGHT_PHASE     1020  // 飞行阶段不允许

// 信息码 (1038-1048)
MAGIC_INFO_SET_LINK_QOS              1038  // QoS 已设置
MAGIC_INFO_SET_NAPT                  1044  // NAPT 已应用

// 系统错误 (2000-2010)
MAGIC_ERROR_OPEN_LINK_FAILED         2006  // 打开链路失败
MAGIC_ERROR_MAGIC_FAILURE            2009  // MAGIC 核心故障
```

完整列表参见 `dict_magic_codes.h` 或 [USAGE_GUIDE.md](USAGE_GUIDE.md)。

---

## 使用示例

### 示例 1: 发送 Communication-Change-Request

```c
struct msg *req;
struct dict_object *cmd;

// 查找命令
CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, 
                        CMD_BY_NAME, "MAGIC-Communication-Change-Request",
                        &cmd, ENOENT));

// 创建请求消息
CHECK_FCT(fd_msg_new(cmd, MSGFL_ALLOC_ETEID, &req));

// 添加 Communication-Request-Parameters
struct dict_object *avp_crp;
struct avp *crp;
CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP,
                        AVP_BY_NAME_AND_VENDOR,
                        &(struct dict_avp_request){
                            .avp_vendor = 13712,
                            .avp_name = "Communication-Request-Parameters"
                        }, &avp_crp, ENOENT));

CHECK_FCT(fd_msg_avp_new(avp_crp, 0, &crp));
// ... 添加子 AVP ...
CHECK_FCT(fd_msg_avp_add(req, MSG_BRW_LAST_CHILD, crp));
```

### 示例 2: 处理 Answer 并检查状态码

```c
struct avp *avp;
struct avp_hdr *hdr;
uint32_t result_code, magic_status;

// 获取 Result-Code
CHECK_FCT(fd_msg_search_avp(ans, dict_result_code, &avp));
CHECK_FCT(fd_msg_avp_hdr(avp, &hdr));
result_code = hdr->avp_value->u32;

if (result_code == DIAMETER_SUCCESS) {
    fd_log_notice("Request succeeded");
    
    // 检查可选的 MAGIC-Status-Code
    if (fd_msg_search_avp(ans, dict_magic_status_code, &avp) == 0) {
        CHECK_FCT(fd_msg_avp_hdr(avp, &hdr));
        magic_status = hdr->avp_value->u32;
        
        if (IS_MAGIC_INFO(magic_status)) {
            fd_log_debug("Info: %s", magic_status_code_str(magic_status));
        }
    }
} else {
    fd_log_error("Request failed: Result-Code=%u, %s",
                result_code, magic_status_code_str(result_code));
}
```

---

## 合规性

### ✅ ARINC 839-2014 标准符合度: 100%

| 规范章节 | 内容 | 状态 |
|---------|------|------|
| Section 4.1.3 | 命令定义 | ✅ 完全符合 |
| Section 4.1.4.2 | AVP 定义 | ✅ 完全符合 + 枚举值 |
| Section 1.3.2 | 状态码 | ✅ 完全符合 + 常量化 |
| Appendix B | 命令格式 | ✅ 完全符合 |

### 验证方法

1. ✅ 逐条对比规范文档与代码实现
2. ✅ CSV 自动生成确保一致性
3. ✅ 所有 AVP Code 和命令 Code 与标准完全一致
4. ✅ 枚举值定义与标准文档匹配

详见 [DICT_CHECK_REPORT.md](DICT_CHECK_REPORT.md) 获取完整检查报告。

---

## 开发指南

### 修改 AVP 定义

1. 编辑 `dict_magic_839.csv`
2. 运行 CSV 转换工具:
   ```bash
   csv_to_fd -p fdc dict_magic_839.csv > add_avps.c.new
   ```
3. 手动合并枚举值定义部分
4. 重新编译

### 添加新的状态码

1. 编辑 `dict_magic_codes.h` 添加常量定义
2. 编辑 `dict_magic_codes.c` 的 `magic_status_code_str()` 函数添加对应字符串
3. 重新编译

### 调试技巧

```bash
# 启用 freeDiameter 调试输出
freeDiameterd -d

# 查看词典是否正确加载
grep "dict_magic_839" /var/log/freeDiameter.log

# 验证枚举值显示
# 枚举值会自动显示为可读名称，如 "QoS-Level: BE (0)"
```

---

## 依赖

- **freeDiameter**: >= 1.2.0
- **编译器**: GCC >= 4.8 或 Clang >= 3.4
- **CMake**: >= 3.0

---

## 许可证

本词典扩展遵循与 freeDiameter 相同的许可证。

ARINC 839-2014 标准版权归 AEEC (Airlines Electronic Engineering Committee) 所有。

---

## 贡献

欢迎提交问题和改进建议。

---

## 文档

- **[DICT_CHECK_REPORT.md](DICT_CHECK_REPORT.md)** - 详细的合规性检查报告
- **[USAGE_GUIDE.md](USAGE_GUIDE.md)** - 完整的使用指南和示例
- **[dict_magic_codes.h](dict_magic_codes.h)** - 状态码常量定义（带注释）

---

## 更新日志

### v2.0 - 2025-12-16 (增强版)

- ✅ 新增 `dict_magic_codes.h` - 50+ 个状态码常量定义
- ✅ 新增 `dict_magic_codes.c` - 状态码字符串转换函数
- ✅ 增强 `add_avps.c` - 添加所有枚举类型的枚举值定义（40+个）
- ✅ 新增 `USAGE_GUIDE.md` - 完整使用指南
- ✅ 更新 `DICT_CHECK_REPORT.md` - 记录所有改进

### v1.0 - 2025 (初始版本)

- ✅ 完整实现 ARINC 839-2014 标准
- ✅ 54 个基础 AVP + 19 个分组 AVP
- ✅ 7 对命令定义
- ✅ CSV 自动生成工具集成

---

## 联系方式

如有问题或建议，请联系 freeDiameter 项目维护者或提交 Issue。

---

**最后更新**: 2025年12月16日  
**版本**: v2.0 (增强版)  
**状态**: ✅ 生产就绪
