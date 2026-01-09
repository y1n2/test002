# MAGIC 系统 TLS 认证说明文档

本文档详细说明了 MAGIC 系统中 Diameter 协议的 TLS 认证机制，遵循 **ARINC 822A** (Gatelink) 和 **ARINC 830** (AGIE) 规范的安全要求。

---

## 1. 规范要求概述

### 1.1 协议版本要求

| 要求项 | 规范 |
| :--- | :--- |
| **最低版本** | TLS v1.2 或更高 |
| **禁用版本** | TLS v1.0, TLS v1.1, SSL v2.0, SSL v3.0 |
| **依据标准** | ARINC 822A, ARINC 830 |

> ⚠️ **安全警告**: 所有版本的 SSL 及 TLS v1.0/v1.1 均存在已知安全漏洞，在航空环境中**严格禁止使用**。

### 1.2 双向身份验证 (Mutual Authentication)

ARINC 规范**强制要求**航空器与地面网络之间执行双向身份验证：

*   **航空器 → 地面**: 航空器必须验证地面 AAA 服务器的证书，防止连接到"流氓"接入点。
*   **地面 → 航空器**: 地面服务器必须验证航空器的证书，防止非法航空器接入网络。
*   **证书标准**: 所有证书必须符合 **IETF RFC 5280** 规范的 X.509 v3 格式。

### 1.3 加密算法与密码套件 (Cipher Suites)

| 类别 | 要求 |
| :--- | :--- |
| **推荐算法** | AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305 |
| **密钥交换** | ECDHE (椭圆曲线迪菲-赫尔曼)，支持完全前向安全性 (PFS) |
| **签名算法** | SHA-256, SHA-384, SHA-512 |
| **禁止算法** | MD5, RC4, DES, 3DES, SHA-1 |

**freeDiameter 推荐的 TLS 优先级字符串 (GnuTLS 格式):**

```properties
# 符合航空安全标准的 TLS 配置
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
```

该配置的含义：
- `SECURE256:+SECURE128`: 优先使用 256 位加密，同时支持 128 位
- `-VERS-TLS1.0:-VERS-TLS1.1`: 禁用 TLS 1.0 和 1.1
- `-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128`: 禁用不安全的算法
- `%SAFE_RENEGOTIATION`: 要求安全重协商

---

## 2. 数字证书配置文件要求

ARINC 822A 附件 1 定义了两种证书配置文件，以确保互操作性。

### 2.1 AAA 服务器证书配置文件

用于地面认证服务器（MAGIC Server）。

| 字段 | 要求 |
| :--- | :--- |
| **Subject DN** | CN=magic.server.example.com, O=Airline, C=CN |
| **有效期** | 推荐 3 年 (1095 天) |
| **密钥算法** | RSA 2048 位或更高 |
| **签名算法** | SHA-256 with RSA |
| **Key Usage** | digitalSignature, keyEncipherment |
| **Extended Key Usage** | serverAuth (OID: 1.3.6.1.5.5.7.3.1) |
| **Subject Alternative Name** | 包含服务器 FQDN 和 IP 地址 |

### 2.2 客户端证书配置文件 (航空器端)

用于航空器 MAGIC Client。

| 字段 | 要求 |
| :--- | :--- |
| **Subject DN** | CN=B-1234 (航空器机尾号/注册号), O=Airline, C=CN |
| **有效期** | 推荐 1-3 年 |
| **密钥算法** | RSA 2048 位或更高 |
| **签名算法** | SHA-256 with RSA |
| **Key Usage** | digitalSignature, keyEncipherment |
| **Extended Key Usage** | clientAuth (OID: 1.3.6.1.5.5.7.3.2) |

> 📝 **说明**: 航空器证书的 `CN` 字段通常使用**机尾号 (Tail Number)** 或**注册号**，便于识别和审计。

---

## 3. 证书验证与吊销检查

认证过程中，接收方必须严格执行以下验证步骤：

### 3.1 证书合法性检查

1.  **签名验证**: 验证证书签名是否由受信任的 CA 签发。
2.  **有效期检查**: 证书必须在 `Not Before` 和 `Not After` 时间范围内。
3.  **信任链验证**: 从终端证书追溯到受信任的根 CA。

### 3.2 证书吊销检查

ARINC 规范要求必须检查证书是否已被吊销，支持两种方式：

| 方式 | 说明 | 优缺点 |
| :--- | :--- | :--- |
| **CRL (证书吊销列表)** | 定期下载 CA 发布的吊销列表 | 简单但时效性差 |
| **OCSP (在线证书状态协议)** | 实时查询证书状态 | 时效性好但依赖网络 |

**在 freeDiameter 中启用 OCSP (需 GnuTLS 3.6+):**

```properties
# 启用 OCSP Stapling
TLS_OCSP = 1;
```

> ⚠️ **注意**: 航空环境中，由于网络可能不稳定，建议同时支持 CRL 和 OCSP，并设置合理的缓存策略。

---

## 4. 证书生成流程

### 4.1 生成 CA 根证书

```bash
# 1. 生成 CA 私钥 (4096 位 RSA)
openssl genrsa -out ca.key 4096
chmod 600 ca.key

# 2. 生成自签名 CA 证书 (10 年有效期)
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/C=CN/O=Airline/CN=MAGIC-Root-CA" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign"
```

### 4.2 生成服务端证书 (AAA Server)

```bash
# 1. 生成服务端私钥 (RSA 2048)
openssl genrsa -out server.key 2048
chmod 600 server.key

# 2. 创建扩展配置文件
cat > server.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = magic.server.example.com
DNS.2 = localhost
IP.1 = 127.0.0.1
IP.2 = 192.168.1.10
EOF

# 3. 生成 CSR
openssl req -new -key server.key -out server.csr \
    -subj "/C=CN/O=Airline/OU=GroundNetwork/CN=magic.server.example.com"

# 4. 使用 CA 签署 (3 年有效期)
openssl x509 -req -days 1095 -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt -extfile server.ext -sha256
```

### 4.3 生成客户端证书 (航空器)

```bash
# 1. 生成客户端私钥
openssl genrsa -out client.key 2048
chmod 600 client.key

# 2. 创建扩展配置文件 (CN 使用机尾号)
cat > client.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = B-1234.aircraft.example.com
EOF

# 3. 生成 CSR (CN = 机尾号)
openssl req -new -key client.key -out client.csr \
    -subj "/C=CN/O=Airline/OU=Aircraft/CN=B-1234"

# 4. 使用 CA 签署 (1 年有效期)
openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out client.crt -extfile client.ext -sha256
```

---

## 5. freeDiameter 配置说明

### 5.1 服务端配置 (`magic_server.conf`)

```properties
# 身份标识 (必须与证书 CN 或 SAN 匹配)
Identity = "magic.server.example.com";
Realm = "example.com";

# TLS 证书配置
TLS_Cred = "/path/to/certs/server.crt", "/path/to/certs/server.key";
TLS_CA = "/path/to/certs/ca.crt";

# 符合 ARINC 822A/830 的 TLS 优先级配置
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:%SAFE_RENEGOTIATION";

# 安全端口 (TLS)
SecPort = 5869;

# 禁用非加密端口 (生产环境强制)
# Port = 0;
```

### 5.2 客户端配置 (`magic_client.conf`)

```properties
# 身份标识 (使用机尾号)
Identity = "B-1234.aircraft.example.com";
Realm = "example.com";

# TLS 证书配置
TLS_Cred = "/path/to/certs/client.crt", "/path/to/certs/client.key";
TLS_CA = "/path/to/certs/ca.crt";

# 符合 ARINC 822A/830 的 TLS 优先级配置
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:%SAFE_RENEGOTIATION";

# 连接到 MAGIC 服务器 (仅使用 TLS 端口)
ConnectPeer = "magic.server.example.com" { 
    ConnectTo = "192.168.1.10"; 
    Port = 5869;
    TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:%SAFE_RENEGOTIATION";
};
```

---

## 6. 应用场景

### 6.1 Gatelink (ARINC 822A)

Gatelink 是航空器在机场停机位通过 Wi-Fi 连接地面网络的标准。

*   **认证协议**: EAP-TLS (802.1X 端口访问控制)
*   **TLS 要求**: 必须使用 TLS v1.2+，双向证书认证
*   **典型流程**:
    1.  航空器关联到机场 Wi-Fi AP
    2.  AP 触发 EAP-TLS 认证
    3.  航空器和 AAA 服务器交换证书
    4.  双向验证通过后，AP 开放端口访问

### 6.2 AGIE (ARINC 830)

AGIE (Air-Ground Integration Environment) 定义了航空器与地面系统的消息交互架构。

*   **传输协议**: AMQP over TLS
*   **TLS 要求**: 所有 AMQP 消息流必须在 TLS 会话内传输
*   **安全目标**: 端到端数据机密性和完整性

---

## 7. 关键文件清单

| 文件名 | 说明 | 部署位置 | 权限 |
| :--- | :--- | :--- | :--- |
| `ca.crt` | CA 根证书公钥 | Server & Client | 644 |
| `ca.key` | CA 根证书私钥 | **仅证书生成环境** | 600 |
| `server.crt` | 服务端公钥证书 | MAGIC Server | 644 |
| `server.key` | 服务端私钥 | MAGIC Server | 600 |
| `client.crt` | 客户端公钥证书 | MAGIC Client | 644 |
| `client.key` | 客户端私钥 | MAGIC Client | 600 |

---

## 8. 常见问题排查

### 8.1 证书验证失败 (Unknown CA)

*   **现象**: `Certificate verification failed` 或 `The certificate is not trusted`
*   **原因**: 未加载正确的 `ca.crt` 或证书链不完整
*   **解决**: 确保 `TLS_CA` 指向签发证书的同一个 CA 文件

### 8.2 主机名不匹配 (Hostname Mismatch)

*   **现象**: `The certificate's owner does not match hostname`
*   **原因**: `ConnectPeer` 或 `Identity` 与证书 CN/SAN 不一致
*   **解决**: 检查证书 SAN 是否包含目标域名/IP
    ```bash
    openssl x509 -in server.crt -noout -text | grep -A1 "Subject Alternative Name"
    ```

### 8.3 协议版本不匹配

*   **现象**: `Handshake failed` 或 `No common protocol`
*   **原因**: 一端强制 TLS 1.2，另一端仅支持 TLS 1.0
*   **解决**: 确保两端 `TLS_Prio` 配置兼容，并升级 GnuTLS 库

### 8.4 密码套件协商失败

*   **现象**: `No supported cipher suites`
*   **原因**: 两端配置的加密算法没有交集
*   **解决**: 使用更宽松的 `TLS_Prio` 进行调试:
    ```properties
    TLS_Prio = "NORMAL:%SAFE_RENEGOTIATION";
    ```

---

## 9. 验证命令

### 9.1 检查证书内容

```bash
# 查看证书详情
openssl x509 -in server.crt -noout -text

# 验证证书链
openssl verify -CAfile ca.crt server.crt
openssl verify -CAfile ca.crt client.crt
```

### 9.2 测试 TLS 连接

**服务端模拟:**
```bash
openssl s_server -accept 5869 -cert server.crt -key server.key \
    -CAfile ca.crt -Verify 1 -tls1_2
```

**客户端模拟:**
```bash
openssl s_client -connect 127.0.0.1:5869 -cert client.crt -key client.key \
    -CAfile ca.crt -tls1_2
```

成功连接后应显示 `Verify return code: 0 (ok)`。

### 9.3 检查支持的密码套件

```bash
# 查看 GnuTLS 支持的算法
gnutls-cli --list

# 测试特定服务器支持的套件
gnutls-cli --print-cert -p 5869 magic.server.example.com
```

---

## 10. 实施案例：东方航空 C929 机型 TLS 认证方案

本节以**中国东方航空 (China Eastern Airlines, ICAO: CES)** 的 **COMAC C929** 机型为例，展示符合 ARINC 839 (MAGIC) 和 ARINC 842 (PKI) 规范的完整 TLS 认证方案。

### 10.1 总体认证架构

在 ARINC 839 架构中，**MAGIC Server** 部署在飞机上，作为**机载 AAA 服务器**和**资源管理器**。机载应用系统（如 EFB、客舱娱乐系统、维护终端）作为 **Diameter 客户端**，通过 TLS 安全连接向 MAGIC Server 申请通信资源。

```
┌─────────────────────────────────────────────────────────────────────┐
│                        C929 机载局域网                               │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │     EFB      │  │   客舱系统    │  │   维护系统    │              │
│  │ (Diameter客户端) │ (Diameter客户端) │ (Diameter客户端) │              │
│  │ 证书: EFB.crt │ │ 证书: IFE.crt │ │ 证书: OMT.crt │              │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘              │
│         │                 │                 │                       │
│         └─────────────────┼─────────────────┘                       │
│                           │  TLS 1.3 (mTLS)                         │
│                  ┌────────▼────────┐                                │
│                  │  MAGIC Server   │  ◀── 机载 AAA 服务器           │
│                  │  (资源管理器)    │      证书: MAGIC.crt          │
│                  └────────┬────────┘                                │
│                           │                                         │
│              ┌────────────┼────────────┐                            │
│              ▼            ▼            ▼                            │
│         ┌────────┐  ┌────────┐  ┌────────┐                         │
│         │ Satcom │  │  4G/5G │  │  WiFi  │  ◀── DLM 管理的链路      │
│         └────┬───┘  └────┬───┘  └────┬───┘                         │
│              │           │           │                              │
└──────────────┼───────────┼───────────┼──────────────────────────────┘
               │           │           │
     ══════════╪═══════════╪═══════════╪══════════  空地数据链路
               │           │           │
      (连接到东航地面网络，用于业务数据传输)
```

| 认证层 | 验证内容 | 技术实现 |
| :--- | :--- | :--- |
| **第一层: 传输层 (mTLS)** | 验证机载应用系统的合法性 | X.509 设备证书 + 双向 TLS |
| **第二层: 业务层 (MCAR)** | 验证应用服务的业务权限 | Diameter Client-Credentials AVP |

### 10.2 证书配置文件设计

#### 10.2.1 MAGIC Server 证书 (机载服务器)

MAGIC Server 代表飞机的身份，其证书必须绑定飞机注册号。

*   **Subject DN**: `CN=B-929A.MAGIC.CES, OU=Flight Ops, O=China Eastern, C=CN`
*   **SAN**:
    *   `DNS: magic.c929.internal` (机载域名)
    *   `IP: 192.168.1.1` (机载固定IP)
    *   `OtherName: 1.3.6.1.4.1.13712.842.1.1::780ABC` (Mode S 地址)
*   **EKU**: `serverAuth`

#### 10.2.2 机载客户端证书 (EFB/IFE等)

每个接入 MAGIC 的机载系统都需要独立的客户端证书。

*   **Subject DN**: `CN=EFB-01.B-929A.CES, OU=Flight Ops, O=China Eastern, C=CN`
*   **SAN**: `DNS: efb.c929.internal`
*   **EKU**: `clientAuth`

### 10.3 证书生成命令

#### 10.3.1 生成 MAGIC Server 证书

```bash
# 1. 生成私钥
openssl genrsa -out magic-server.key 2048

# 2. 扩展配置 (包含 Mode S 地址)
cat > magic-server.ext << 'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
otherName.1 = 1.3.6.1.4.1.13712.842.1.1;UTF8:780ABC
DNS.1 = magic.c929.internal
IP.1 = 192.168.1.1
EOF

# 3. 签署证书
openssl req -new -key magic-server.key -out magic-server.csr \
    -subj "/C=CN/O=China Eastern/OU=Flight Ops/CN=B-929A.MAGIC.CES"

openssl x509 -req -in magic-server.csr -CA ces_ca.crt -CAkey ces_ca.key \
    -CAcreateserial -out magic-server.crt -extfile magic-server.ext
```

#### 10.3.2 生成 EFB 客户端证书

```bash
# 1. 生成私钥
openssl genrsa -out efb-client.key 2048

# 2. 扩展配置
cat > efb-client.ext << 'EOF'
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = efb.c929.internal
IP.1 = 192.168.1.50
EOF

# 3. 签署证书
openssl req -new -key efb-client.key -out efb-client.csr \
    -subj "/C=CN/O=China Eastern/OU=Flight Ops/CN=EFB-01.B-929A.CES"

openssl x509 -req -in efb-client.csr -CA ces_ca.crt -CAkey ces_ca.key \
    -CAcreateserial -out efb-client.crt -extfile efb-client.ext
```

### 10.4 freeDiameter 配置示例

#### 10.4.1 MAGIC Server 配置 (机载)

```properties
Identity = "magic.c929.internal";
Realm = "ces.aero";
ListenOn = "192.168.1.1";
SecPort = 5869;

TLS_Cred = "/etc/magic/certs/magic-server.crt", "/etc/magic/certs/magic-server.key";
TLS_CA = "/etc/magic/certs/ces_ca.crt";
# 强制双向认证
TLS_Prio = "SECURE256:%SAFE_RENEGOTIATION";
```

#### 10.4.2 EFB 客户端配置

```properties
Identity = "efb.c929.internal";
Realm = "ces.aero";

TLS_Cred = "/data/certs/efb-client.crt", "/data/certs/efb-client.key";
TLS_CA = "/data/certs/ces_ca.crt";

ConnectPeer = "magic.c929.internal" { 
    ConnectTo = "192.168.1.1"; 
    Port = 5869;
};
```

### 10.5 认证流程 (MCAR)

1.  **TLS 握手**: EFB 连接 MAGIC Server，双方交换证书。MAGIC Server 验证 EFB 证书的合法性（由东航 CA 签发）。
2.  **MCAR 请求**: EFB 发送 `MAGIC-Client-Authentication-Request`。
    *   `User-Name`: "EFB_App_v2"
    *   `Client-Password`: (加密口令)
3.  **鉴权**: MAGIC Server 检查用户名密码，并根据当前飞行阶段（如：滑行、巡航）和链路状态，决定是否允许接入。
4.  **MCAA 响应**: 返回认证结果。

---

## 11. 参考标准

*   **ARINC 822A**: Gatelink - Ground-Based Wireless LAN at Airports
*   **ARINC 830**: Aircraft Data Interface Function (ADIF)
*   **ARINC 839**: MAGIC - Media Independent Aircraft Ground Interface for IP Communications
*   **ARINC 842**: Aviation PKI Certificate Policy
*   **ATA Spec 42**: Aviation Industry Standards for Digital Information Security
*   **IETF RFC 5280**: X.509 Public Key Infrastructure
*   **IETF RFC 8446**: TLS Protocol Version 1.3
*   **IETF RFC 6960**: Online Certificate Status Protocol (OCSP)
*   **FIPS 140-2**: Security Requirements for Cryptographic Modules
