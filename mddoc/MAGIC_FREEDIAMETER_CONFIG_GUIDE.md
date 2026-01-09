# MAGIC 系统 freeDiameter 配置指南

本文档详细说明 MAGIC 系统中 freeDiameter 核心组件的配置参数及其在航空安全标准（ARINC 839/842）下的意义。

---

## 1. 基础身份标识 (Identity & Realm)

每个 Diameter 节点必须有唯一的身份标识。

*   **Identity**: 节点的完全限定域名 (FQDN)。
    *   *格式要求*: 必须包含至少一个点 `.`（如 `node.realm`），且必须使用**双引号** `"` 包围。
    *   *示例*: `Identity = "magic-server.example.com";`
    *   *核心限制*: **必须与 TLS 证书匹配**。该名称必须与 `cert/` 目录下证书的 Common Name (CN) 或 Subject Alternative Name (SAN) 完全一致，否则 TLS 握手会因身份不匹配而失败。
*   **Realm**: 节点所属的 Diameter 域。
    *   *示例*: `Realm = "example.com";`

---

## 2. 网络传输配置 (Transport)

MAGIC 系统主要运行在机载 IP 网络上，通常限制为 IPv4 和 TCP。

*   **Port / SecPort**:
    *   `Port = 3869;`: 标准 Diameter 端口（非加密）。在生产环境中通常禁用或仅用于本地测试。
    *   `SecPort = 5870;`: **TLS 加密端口**。这是 MAGIC 系统的主要通信端口。
*   **No_SCTP / No_IPv6**:
    *   `No_SCTP;`: 禁用 SCTP 协议。虽然 Diameter 支持 SCTP，但机载网络设备通常对 TCP 支持更好。
    *   `No_IPv6;`: 禁用 IPv6。目前机载网络环境以 IPv4 为主。
*   **ListenOn**: 监听的本地 IP 地址。
    *   `ListenOn = "0.0.0.0";` (监听所有网卡) 或 `ListenOn = "192.168.1.100";` (绑定特定网卡)。

---

## 3. TLS 安全配置 (ARINC 842/822A 规范)

这是配置中最关键的部分，直接决定了系统的安全性。

### 3.1 证书路径
证书文件统一存放在项目根目录的 `cert/` 文件夹下。

*   **TLS_Cred**: 节点自身的证书和私钥。
    *   `TLS_Cred = "/home/zhuwuhui/freeDiameter/cert/server.crt", "/home/zhuwuhui/freeDiameter/cert/server.key";`
*   **TLS_CA**: 信任的根证书 (CA)。用于验证对端节点的身份。
    *   `TLS_CA = "/home/zhuwuhui/freeDiameter/cert/ca.crt";`

### 3.2 安全算法 (TLS_Prio)
为了符合 **ARINC 822A (Gatelink Security)** 和 **ARINC 830** 要求，必须使用强加密套件。

```properties
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
```

*   **SECURE256 / +SECURE128**: 优先使用 256 位和 128 位加密算法（如 AES-GCM）。
*   **-VERS-TLS1.0 / -VERS-TLS1.1**: **强制禁用** 已过时的 TLS 1.0 和 1.1 版本，仅允许 TLS 1.2 及以上。
*   **-MD5 / -SHA1 / -3DES / -RC4**: 禁用已知有漏洞的哈希和加密算法。
*   **%SAFE_RENEGOTIATION**: 启用安全重协商，防止中间人攻击。

### 3.3 密钥交换 (TLS_DH_Bits)
*   `TLS_DH_Bits = 2048;`: 设置 Diffie-Hellman 密钥交换的位数。ARINC 842 建议至少 2048 位以抵御暴力破解。

---

## 4. 扩展模块加载 (Extensions)

freeDiameter 通过加载 `.fdx` 插件来实现具体业务逻辑。

*   **dict_magic_839.fdx**: **核心字典**。定义了 ARINC 839 协议中的所有 AVP（属性值对）和命令（如 MCAR, MCCR）。必须在所有节点加载。
*   **app_magic.fdx**: **应用逻辑**。仅在服务器端加载，负责处理业务请求、查询策略引擎。
    *   *参数*: 需指向 `Client_Profile.xml` 路径。
*   **dbg_msg_dumps.fdx**: **调试工具**。在日志中打印完整的 Diameter 报文内容。
    *   *参数*: `0x0080` 表示打印 AVP 的详细十六进制内容。
*   **acl_wl.fdx**: **访问控制列表**。定义允许连接到本节点的对端白名单。

---

## 5. 对端连接管理 (Peers)

### 5.1 客户端主动连接 (ConnectPeer)
在客户端配置文件中，定义如何连接到服务器。

```properties
ConnectPeer = "magic-server.example.com" { 
    ConnectTo = "192.168.126.1"; 
    Port = 5870; 
};
```
*   **ConnectTo**: 服务器的实际 IP 地址。
*   **Port**: 服务器的 TLS 端口。

### 5.2 服务器白名单 (acl_wl)
在服务器端，通过 `acl_wl.conf` 限制接入的客户端。

```properties
# acl_wl.conf 示例
"magic-client.example.com" : P ; # 允许该 Identity 的客户端接入
```

---

## 6. 性能与日志 (Performance & Logging)

*   **ThreadsPerServer**: 处理连接的线程数。
*   **AppServThreads**: 处理应用逻辑的线程数。
*   **Log Level**:
    *   `1`: 致命错误 (FATAL)
    *   `2`: 错误 (ERROR)
    *   `3`: 警告 (WARNING) - **推荐测试环境使用**
    *   `4`: 通知 (NOTICE) - **推荐生产环境使用**
    *   `6`: 调试 (DEBUG) - 仅在排查协议问题时开启

---

## 7. 常见问题排查

1.  **TLS Handshake Failed**:
    *   检查 `Identity` 是否与证书 CN 匹配。
    *   检查系统时间是否同步（证书有效期验证依赖时间）。
    *   检查 `TLS_CA` 是否正确加载了签发对端证书的根证书。
2.  **Extension Load Failed**:
    *   确保 `.fdx` 文件路径为绝对路径。
    *   检查依赖库（如 GnuTLS）是否安装。
3.  **Connection Refused**:
    *   检查防火墙是否开放了 `SecPort` (5870)。
    *   检查服务器是否在监听正确的 IP (`ListenOn`)。
