# MAGIC 系统证书配置与生成指南

本文档提供符合 **ARINC 839 (MAGIC)** 和 **ARINC 842 (PKI)** 标准的 TLS 证书生成指南，包括证书扩展配置文件设计和完整的生成步骤。

---

## 1. 证书体系概述

MAGIC 系统采用三层证书体系：

```
┌─────────────────────────────────────────────────────────┐
│                   根 CA 证书                             │
│   Subject: CN=MAGIC-Root-CA, O=Airline, C=CN           │
│   有效期: 10 年                                          │
│   用途: 签发所有中间和终端证书                            │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼                         ▼
┌──────────────────┐     ┌──────────────────┐
│  服务器证书       │     │  客户端证书       │
│  (MAGIC Server)  │     │  (EFB/IFE/OMT)   │
├──────────────────┤     ├──────────────────┤
│ EKU: serverAuth  │     │ EKU: clientAuth  │
│ 有效期: 3 年      │     │ 有效期: 1-3 年    │
│ 包含 Mode S 地址  │     │ 绑定设备标识      │
└──────────────────┘     └──────────────────┘
```

---

## 2. 证书扩展配置文件设计

### 2.1 CA 证书扩展配置 (`ca.ext`)

```ini
# ═══════════════════════════════════════════════════════════
# CA 根证书扩展配置
# 用途: 自签名根 CA，用于签发所有机载和地面证书
# ═══════════════════════════════════════════════════════════

# 基础约束 (关键扩展)
basicConstraints = critical, CA:TRUE, pathlen:1

# 密钥用法 (关键扩展)
# - keyCertSign: 允许签发其他证书
# - cRLSign: 允许签发证书吊销列表
keyUsage = critical, keyCertSign, cRLSign

# 主题密钥标识符 (自动生成)
subjectKeyIdentifier = hash

# 授权密钥标识符 (自签名时等于 subjectKeyIdentifier)
authorityKeyIdentifier = keyid:always, issuer
```

**关键参数说明**:
- `CA:TRUE`: 标识为 CA 证书
- `pathlen:1`: 允许签发一级中间 CA（可选）
- `keyCertSign`: 必须存在才能签发证书

---

### 2.2 MAGIC Server 证书扩展配置 (`magic-server.ext`)

```ini
# ═══════════════════════════════════════════════════════════
# MAGIC Server 证书扩展配置 (机载 AAA 服务器)
# 用途: 机载 Diameter 服务器身份认证
# 符合: ARINC 842 AAA Server Profile
# ═══════════════════════════════════════════════════════════

# 授权密钥标识符 (指向签发此证书的 CA)
authorityKeyIdentifier = keyid, issuer

# 基础约束 (非 CA 证书)
basicConstraints = CA:FALSE

# 密钥用法 (关键扩展)
# - digitalSignature: TLS 握手签名
# - keyEncipherment: RSA 密钥交换
keyUsage = critical, digitalSignature, keyEncipherment

# 扩展密钥用法 (关键扩展)
# - serverAuth (OID 1.3.6.1.5.5.7.3.1): TLS Web 服务器认证
extendedKeyUsage = serverAuth

# 主题别名 (SAN) - 关键扩展
# 必须包含以下内容:
# 1. 飞机 Mode S 地址 (ARINC 842 OID: 1.3.6.1.4.1.13712.842.1.1)
# 2. 机载网络域名
# 3. 机载局域网 IP 地址
subjectAltName = @alt_names

[alt_names]
# Mode S 地址 (24位十六进制，如 780ABC 代表飞机唯一标识)
otherName.1 = 1.3.6.1.4.1.13712.842.1.1;UTF8:780ABC

# 机载域名
DNS.1 = magic.c929.internal
DNS.2 = B-929A.aircraft.ces.aero

# 机载局域网 IP (ARINC 664 规范)
IP.1 = 192.168.1.1
IP.2 = 10.0.0.1
```

**重要说明**:
- **Mode S 地址**: 航空器全球唯一物理标识，类似 MAC 地址，在证书生成时必须从航空器系统配置中获取
- **DNS 名称**: 必须与 `freeDiameter.conf` 中的 `Identity` 字段严格匹配
- **IP 地址**: 必须包含服务器监听的所有 IP

---

### 2.3 机载客户端证书扩展配置 (`efb-client.ext`)

```ini
# ═══════════════════════════════════════════════════════════
# 机载客户端证书扩展配置 (EFB/IFE/OMT)
# 用途: 机载应用系统身份认证
# 符合: ARINC 842 Client Certificate Profile
# ═══════════════════════════════════════════════════════════

authorityKeyIdentifier = keyid, issuer
basicConstraints = CA:FALSE

# 密钥用法
keyUsage = critical, digitalSignature, keyEncipherment

# 扩展密钥用法
# - clientAuth (OID 1.3.6.1.5.5.7.3.2): TLS 客户端认证
extendedKeyUsage = clientAuth

# 主题别名
subjectAltName = @alt_names

[alt_names]
# 客户端设备标识
DNS.1 = efb-01.c929.internal
DNS.2 = EFB-01.B-929A.CES

# 客户端 IP (机载局域网分配)
IP.1 = 192.168.126.5
IP.1 = 192.168.126.101
IP.1 = 192.168.126.102
```

**应用场景**:
- EFB (Electronic Flight Bag): 电子飞行包
- IFE (In-Flight Entertainment): 客舱娱乐系统
- OMT (Onboard Maintenance Terminal): 机载维护终端
- 每个应用系统需要独立的客户端证书

---

## 3. 证书生成完整流程

### 3.1 准备工作

```bash
# 创建证书工作目录
mkdir -p /home/zhuwuhui/freeDiameter/certs
cd /home/zhuwuhui/freeDiameter/certs

# 设置权限掩码（确保私钥文件默认权限为 600）
umask 077
```

---

### 3.2 生成 CA 根证书

#### 步骤 1: 生成 CA 私钥

```bash
# 生成 4096 位 RSA 私钥 (ARINC 842 推荐)
openssl genrsa -out ca.key 4096

# 设置私钥权限（仅所有者可读写）
chmod 600 ca.key
```

#### 步骤 2: 创建 CA 扩展配置文件

```bash
cat > ca.ext << 'EOF'
basicConstraints = critical, CA:TRUE, pathlen:1
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always, issuer
EOF
```

#### 步骤 3: 生成自签名 CA 证书

```bash
# 生成 CA 证书 (有效期 10 年)
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/C=CN/O=China Eastern Airlines/OU=Aviation PKI/CN=CES-MAGIC-Root-CA" \
    -extensions v3_ca -extfile ca.ext -sha256

# 设置证书权限（所有人可读）
chmod 644 ca.crt
```

#### 步骤 4: 验证 CA 证书

```bash
# 查看证书详情
openssl x509 -in ca.crt -noout -text

# 验证证书自签名
openssl verify -CAfile ca.crt ca.crt
```

**预期输出**:
```
ca.crt: OK
```

---

### 3.3 生成 MAGIC Server 证书 (机载服务器)

#### 步骤 1: 生成服务器私钥

```bash
# 生成 2048 位 RSA 私钥
openssl genrsa -out magic-server.key 2048
chmod 600 magic-server.key
```

#### 步骤 2: 创建服务器证书扩展配置

```bash
cat > magic-server.ext << 'EOF'
authorityKeyIdentifier = keyid, issuer
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
# ⚠️ 重要: 根据实际航空器信息修改
otherName.1 = 1.3.6.1.4.1.13712.842.1.1;UTF8:780ABC
DNS.1 = magic.c929.internal
DNS.2 = B-929A.aircraft.ces.aero
IP.1 = 192.168.1.1
EOF
```

> **重要提示**: 必须修改以下字段：
> - `780ABC`: 替换为实际的 Mode S 地址
> - `B-929A`: 替换为实际的飞机注册号
> - `192.168.1.1`: 替换为实际的机载服务器 IP

#### 步骤 3: 生成证书签名请求 (CSR)

```bash
# Subject DN 格式: CN=<机尾号>.MAGIC.<ICAO代码>
openssl req -new -key magic-server.key -out magic-server.csr \
    -subj "/C=CN/O=China Eastern Airlines/OU=Flight Operations/CN=B-929A.MAGIC.CES"
```

#### 步骤 4: 使用 CA 签署证书

```bash
# 签发服务器证书 (有效期 3 年)
openssl x509 -req -in magic-server.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out magic-server.crt -days 1095 \
    -extfile magic-server.ext -sha256

chmod 644 magic-server.crt
```

#### 步骤 5: 验证证书

```bash
# 验证证书链
openssl verify -CAfile ca.crt magic-server.crt

# 检查 SAN 扩展
openssl x509 -in magic-server.crt -noout -text | grep -A5 "Subject Alternative Name"
```

**预期输出**:
```
magic-server.crt: OK

X509v3 Subject Alternative Name:
    othername:<unsupported>, DNS:magic.c929.internal, DNS:B-929A.aircraft.ces.aero, IP Address:192.168.1.1
```

---

### 3.4 生成 EFB 客户端证书

#### 步骤 1: 生成客户端私钥

```bash
openssl genrsa -out efb-client.key 2048
chmod 600 efb-client.key
```

#### 步骤 2: 创建客户端证书扩展配置

```bash
cat > efb-client.ext << 'EOF'
authorityKeyIdentifier = keyid, issuer
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = efb-01.c929.internal
DNS.2 = EFB-01.B-929A.CES
IP.1 = 192.168.1.50
EOF
```

#### 步骤 3: 生成 CSR

```bash
# Subject DN 格式: CN=<设备名>.<机尾号>.<ICAO代码>
openssl req -new -key efb-client.key -out efb-client.csr \
    -subj "/C=CN/O=China Eastern Airlines/OU=Flight Operations/CN=EFB-01.B-929A.CES"
```

#### 步骤 4: 签署证书

```bash
# 签发客户端证书 (有效期 1 年)
openssl x509 -req -in efb-client.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out efb-client.crt -days 365 \
    -extfile efb-client.ext -sha256

chmod 644 efb-client.crt
```

#### 步骤 5: 验证证书

```bash
openssl verify -CAfile ca.crt efb-client.crt
```

---

## 4. 证书部署清单

### 4.1 MAGIC Server 部署

部署到机载服务器 (`/etc/magic/certs/`):

| 文件 | 权限 | 说明 |
| :--- | :--- | :--- |
| `ca.crt` | 644 | CA 根证书（用于验证客户端） |
| `magic-server.crt` | 644 | 服务器公钥证书 |
| `magic-server.key` | 600 | 服务器私钥 (**禁止泄露**) |

**freeDiameter 配置**:
```properties
TLS_Cred = "/etc/magic/certs/magic-server.crt", "/etc/magic/certs/magic-server.key";
TLS_CA = "/etc/magic/certs/ca.crt";
```

### 4.2 EFB 客户端部署

部署到 EFB 系统 (`/data/certs/`):

| 文件 | 权限 | 说明 |
| :--- | :--- | :--- |
| `ca.crt` | 644 | CA 根证书（用于验证服务器） |
| `efb-client.crt` | 644 | 客户端公钥证书 |
| `efb-client.key` | 600 | 客户端私钥 (**存储在 HSM**) |

**freeDiameter 配置**:
```properties
TLS_Cred = "/data/certs/efb-client.crt", "/data/certs/efb-client.key";
TLS_CA = "/data/certs/ca.crt";
```

---

## 5. 证书命名规范

### 5.1 Subject DN 命名规则

| 证书类型 | CN 格式 | 示例 |
| :--- | :--- | :--- |
| **CA 根证书** | `<航司名>-MAGIC-Root-CA` | `CES-MAGIC-Root-CA` |
| **MAGIC Server** | `<机尾号>.MAGIC.<ICAO>` | `B-929A.MAGIC.CES` |
| **EFB 客户端** | `EFB-<编号>.<机尾号>.<ICAO>` | `EFB-01.B-929A.CES` |
| **IFE 客户端** | `IFE-<编号>.<机尾号>.<ICAO>` | `IFE-01.B-929A.CES` |
| **OMT 客户端** | `OMT-<编号>.<机尾号>.<ICAO>` | `OMT-01.B-929A.CES` |

### 5.2 SAN 命名规则

**DNS 名称**:
- 机载域名: `<设备>.<机型>.internal` (如 `magic.c929.internal`)
- 外部域名: `<设备>.<机尾号>.<航司域>` (如 `B-929A.aircraft.ces.aero`)

**IP 地址**:
- 遵循 ARINC 664 机载网络规范
- 建议使用 `192.168.x.x` 或 `10.x.x.x` 私有地址段

---

## 6. 证书有效期管理

### 6.1 推荐有效期

| 证书类型 | 有效期 | 更新周期 | 依据 |
| :--- | :--- | :--- | :--- |
| **CA 根证书** | 10 年 | 配合机队更新周期 | ARINC 842 |
| **MAGIC Server** | 3 年 | 配合飞机 C-Check | ARINC 842 AAA Profile |
| **客户端证书** | 1-3 年 | 每次软件升级 | 运营需求 |

### 6.2 过期检查脚本

```bash
#!/bin/bash
# 检查证书有效期

CERT_DIR="/etc/magic/certs"

for cert in $CERT_DIR/*.crt; do
    echo "检查: $cert"
    openssl x509 -in $cert -noout -dates
    
    # 检查是否在 30 天内过期
    if openssl x509 -in $cert -noout -checkend 2592000; then
        echo "✅ 证书有效期充足"
    else
        echo "⚠️  警告: 证书将在 30 天内过期!"
    fi
    echo ""
done
```

---

## 7. 证书吊销管理

### 7.1 生成证书吊销列表 (CRL)

```bash
# 创建 CRL 配置文件
cat > crl.conf << 'EOF'
[ca]
default_ca = CA_default

[CA_default]
database = index.txt
crlnumber = crlnumber
default_crl_days = 30
default_md = sha256
EOF

# 初始化数据库
touch index.txt
echo "01" > crlnumber

# 吊销证书 (以 EFB 客户端为例)
openssl ca -config crl.conf -revoke efb-client.crt \
    -keyfile ca.key -cert ca.crt

# 生成 CRL 文件
openssl ca -config crl.conf -gencrl -out ca.crl \
    -keyfile ca.key -cert ca.crt
```

### 7.2 在 freeDiameter 中启用 CRL

```properties
TLS_CRL = "/etc/magic/certs/ca.crl";
```

### 7.3 定期更新 CRL

```bash
#!/bin/bash
# 自动更新 CRL (建议每天运行)

cd /home/zhuwuhui/freeDiameter/certs
openssl ca -config crl.conf -gencrl -out ca.crl \
    -keyfile ca.key -cert ca.crt

# 部署到机载系统
scp ca.crl root@aircraft:/etc/magic/certs/
```

---

## 8. 安全最佳实践

### 8.1 私钥保护

| 要求 | 说明 |
| :--- | :--- |
| **存储位置** | HSM (硬件安全模块) 或 TPM 芯片 |
| **安全等级** | FIPS 140-2 Level 2+ |
| **文件权限** | 600 (仅所有者可读写) |
| **备份加密** | 使用 AES-256 加密备份 |
| **访问审计** | 记录所有私钥访问日志 |

### 8.2 CA 私钥离线管理

```bash
# CA 私钥应在离线环境中生成
# 1. 准备离线工作站 (断开网络)
# 2. 生成 CA 密钥对
# 3. 签发证书后，立即将 ca.key 转移到安全存储介质
# 4. 在生产环境中只部署 ca.crt (公钥)

# 加密备份 CA 私钥
openssl aes-256-cbc -in ca.key -out ca.key.enc
# 存储到加密 U 盘或 HSM
```

### 8.3 证书透明度日志

```bash
# 记录所有签发的证书
echo "$(date) - 签发证书: EFB-01.B-929A.CES" >> cert-issuance.log
openssl x509 -in efb-client.crt -noout -fingerprint -sha256 >> cert-issuance.log
```

---

## 9. 故障排查

### 9.1 常见错误

| 错误 | 原因 | 解决方案 |
| :--- | :--- | :--- |
| `unable to load certificate` | 文件路径错误或权限不足 | 检查路径和 `chmod 644` |
| `key values mismatch` | 证书和私钥不匹配 | 重新生成证书 |
| `certificate has expired` | 证书过期 | 更新证书 |
| `hostname mismatch` | SAN 不包含连接的域名/IP | 检查 `ConnectPeer` 配置 |

### 9.2 验证证书和私钥匹配

```bash
# 计算证书公钥的 MD5 哈希
openssl x509 -in magic-server.crt -noout -modulus | openssl md5

# 计算私钥的 MD5 哈希
openssl rsa -in magic-server.key -noout -modulus | openssl md5

# 两者必须一致
```

### 9.3 测试 TLS 连接

```bash
# 使用 OpenSSL 测试服务器
openssl s_server -accept 5869 \
    -cert magic-server.crt -key magic-server.key \
    -CAfile ca.crt -Verify 1

# 使用 OpenSSL 测试客户端
openssl s_client -connect 192.168.1.1:5869 \
    -cert efb-client.crt -key efb-client.key \
    -CAfile ca.crt
```

---

## 10. 批量生成脚本示例

```bash
#!/bin/bash
# 批量生成 EFB 客户端证书

AIRCRAFT_REG="B-929A"
AIRLINE_ICAO="CES"
AIRCRAFT_TYPE="C929"

for i in {01..10}; do
    DEVICE_NAME="EFB-${i}"
    DEVICE_CN="${DEVICE_NAME}.${AIRCRAFT_REG}.${AIRLINE_ICAO}"
    
    echo "生成证书: ${DEVICE_CN}"
    
    # 生成私钥
    openssl genrsa -out ${DEVICE_NAME}.key 2048
    
    # 创建扩展配置
    cat > ${DEVICE_NAME}.ext << EOF
authorityKeyIdentifier = keyid, issuer
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = efb-${i}.${AIRCRAFT_TYPE}.internal
DNS.2 = ${DEVICE_CN}
IP.1 = 192.168.1.$((50 + i))
EOF
    
    # 生成 CSR
    openssl req -new -key ${DEVICE_NAME}.key -out ${DEVICE_NAME}.csr \
        -subj "/C=CN/O=China Eastern Airlines/OU=Flight Ops/CN=${DEVICE_CN}"
    
    # 签发证书
    openssl x509 -req -in ${DEVICE_NAME}.csr \
        -CA ca.crt -CAkey ca.key -CAcreateserial \
        -out ${DEVICE_NAME}.crt -days 365 \
        -extfile ${DEVICE_NAME}.ext -sha256
    
    # 清理临时文件
    rm ${DEVICE_NAME}.csr ${DEVICE_NAME}.ext
    
    echo "✅ ${DEVICE_CN} 证书生成完成"
    echo ""
done
```

---

## 11. 参考标准

- **ARINC 839**: MAGIC 协议规范
- **ARINC 842**: 航空 PKI 证书策略
- **ARINC 664**: 机载网络规范 (AFDX)
- **IETF RFC 5280**: X.509 公钥基础设施
- **FIPS 140-2**: 加密模块安全要求
- **ATA Spec 42**: 航空信息安全标准
