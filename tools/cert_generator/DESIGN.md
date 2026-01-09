# MAGIC 证书自动生成工具 - 软件设计文档

**版本**: 1.0.0  
**日期**: 2025-12-24  
**作者**: MAGIC 开发团队

---

## 1. 概述

### 1.1 目的

为 MAGIC (Media Independent Aircraft Ground Interface for IP Communications) 系统提供符合 ARINC 839/842 标准的 TLS 证书批量生成工具，简化机载 AAA 服务器、地面基站、客户端设备的证书管理流程。

### 1.2 适用范围

- 航空公司运维人员 (批量生成机队证书)
- MAGIC 系统集成商 (测试环境部署)
- 航电开发团队 (开发测试用证书)

### 1.3 设计原则

1. **声明式配置**: 使用 XML 配置文件定义所有证书参数
2. **批量生成**: 一次运行生成整套证书链 (CA + 服务器 + 客户端)
3. **标准符合**: 严格遵循 ARINC 842、RFC 5280 等航空 PKI 标准
4. **安全优先**: 私钥文件权限 600，建议使用 HSM/TPM 存储
5. **可追溯性**: 自动生成 README 文档记录证书清单和验证方法

---

## 2. 架构设计

### 2.1 系统架构

```
┌────────────────────────────────────────────────────────┐
│                     用户层                              │
│  cert_config.xml (配置文件)                             │
└─────────────────────┬──────────────────────────────────┘
                      │
                      ▼
┌────────────────────────────────────────────────────────┐
│                  应用层                                 │
│  cert_generator.py (主程序)                             │
│  ├─ load_config()       - XML 解析                      │
│  ├─ generate_ca_certificate()   - CA 根证书             │
│  ├─ generate_server_certificates()  - 服务器证书        │
│  ├─ generate_client_certificates()  - 客户端证书        │
│  └─ generate_readme()   - 文档生成                      │
└─────────────────────┬──────────────────────────────────┘
                      │
                      ▼
┌────────────────────────────────────────────────────────┐
│                  加密库层                               │
│  cryptography (Python PKI 库)                           │
│  ├─ RSA 密钥对生成                                      │
│  ├─ X.509 证书构建                                      │
│  ├─ SHA-256 签名                                        │
│  └─ PEM 序列化                                          │
└─────────────────────┬──────────────────────────────────┘
                      │
                      ▼
┌────────────────────────────────────────────────────────┐
│                  输出层                                 │
│  ./output/ (证书输出目录)                               │
│  ├─ ca.crt / ca.key (CA 根证书)                         │
│  ├─ server-*.crt / server-*.key (服务器证书)            │
│  ├─ client-*.crt / client-*.key (客户端证书)            │
│  └─ README.md (证书清单文档)                            │
└────────────────────────────────────────────────────────┘
```

### 2.2 核心类设计

#### 2.2.1 CertificateGenerator 类

```python
class CertificateGenerator:
    """证书生成器核心类"""
    
    # 成员变量
    - config_file: str          # XML 配置文件路径
    - config: ElementTree       # 解析后的配置树
    - output_dir: str           # 证书输出目录
    - ca_cert: x509.Certificate # CA 根证书对象
    - ca_key: RSAPrivateKey     # CA 私钥对象
    
    # 公共方法
    + load_config() -> bool
        """加载并验证 XML 配置文件"""
    
    + generate_ca_certificate() -> None
        """生成 CA 根证书和私钥"""
    
    + generate_server_certificates() -> None
        """批量生成服务器证书"""
    
    + generate_client_certificates() -> None
        """批量生成客户端证书"""
    
    + generate_readme() -> None
        """生成证书清单和验证命令文档"""
    
    + run() -> bool
        """运行完整证书生成流程"""
    
    # 私有方法
    - generate_private_key(key_size: int) -> RSAPrivateKey
        """生成 RSA 私钥"""
    
    - save_private_key(key: RSAPrivateKey, filename: str) -> None
        """保存私钥到文件 (PEM 格式, 权限 600)"""
    
    - save_certificate(cert: x509.Certificate, filename: str) -> None
        """保存证书到文件 (PEM 格式, 权限 644)"""
    
    - build_subject_name(cert_config: Element) -> x509.Name
        """构建证书主题 DN (C, ST, L, O, OU, CN)"""
    
    - build_san_extension(san_config: Element) -> x509.SubjectAlternativeName
        """构建 SAN 扩展 (DNS, IP, OtherName)"""
```

---

## 3. 数据模型

### 3.1 XML 配置文件结构

```xml
<CertificateConfig>
  <OutputDirectory>./output</OutputDirectory>  <!-- 输出目录 -->
  
  <CA>
    <Country>CN</Country>                      <!-- 国家代码 -->
    <State>Shanghai</State>                    <!-- 省/州 -->
    <Locality>Shanghai</Locality>              <!-- 城市 -->
    <Organization>China Eastern Airlines</Organization>
    <OrganizationalUnit>Aviation IT</OrganizationalUnit>
    <CommonName>CES-MAGIC-Root-CA</CommonName> <!-- CN 字段 -->
    <KeySize>4096</KeySize>                    <!-- 密钥长度 -->
    <ValidityDays>3650</ValidityDays>          <!-- 有效期 (天) -->
    <OutputPrefix>ca</OutputPrefix>            <!-- 输出文件前缀 -->
  </CA>
  
  <Servers>
    <Server>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <CommonName>magic-server.example.com</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>1095</ValidityDays>
      <OutputPrefix>server-magic</OutputPrefix>
      
      <SubjectAltNames>
        <DNS>magic-server.example.com</DNS>    <!-- DNS 名称 -->
        <IP>192.168.1.100</IP>                 <!-- IP 地址 -->
        <OtherName>                            <!-- Mode S 地址 -->
          <OID>1.3.6.1.4.1.13712.842.1.1</OID>
          <Value>780ABC</Value>
        </OtherName>
      </SubjectAltNames>
    </Server>
  </Servers>
  
  <Clients>
    <Client>
      <CommonName>magic-client</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>365</ValidityDays>
      <OutputPrefix>client-magic</OutputPrefix>
    </Client>
  </Clients>
</CertificateConfig>
```

### 3.2 证书扩展字段

#### 3.2.1 CA 根证书扩展

| 扩展名称 | OID | Critical | 值 |
|---------|-----|----------|---|
| Basic Constraints | 2.5.29.19 | ✅ | CA:TRUE, pathlen:1 |
| Key Usage | 2.5.29.15 | ✅ | keyCertSign, cRLSign |
| Subject Key Identifier | 2.5.29.14 | ❌ | SHA-1(公钥) |

#### 3.2.2 服务器证书扩展

| 扩展名称 | OID | Critical | 值 |
|---------|-----|----------|---|
| Basic Constraints | 2.5.29.19 | ❌ | CA:FALSE |
| Key Usage | 2.5.29.15 | ✅ | digitalSignature, keyEncipherment |
| Extended Key Usage | 2.5.29.37 | ❌ | serverAuth (1.3.6.1.5.5.7.3.1) |
| Subject Alternative Name | 2.5.29.17 | ❌ | DNS, IP, OtherName (Mode S) |
| Authority Key Identifier | 2.5.29.35 | ❌ | keyid:CA 公钥指纹 |

#### 3.2.3 客户端证书扩展

| 扩展名称 | OID | Critical | 值 |
|---------|-----|----------|---|
| Basic Constraints | 2.5.29.19 | ❌ | CA:FALSE |
| Key Usage | 2.5.29.15 | ✅ | digitalSignature, keyEncipherment |
| Extended Key Usage | 2.5.29.37 | ❌ | clientAuth (1.3.6.1.5.5.7.3.2) |
| Subject Alternative Name | 2.5.29.17 | ❌ | DNS, IP (可选) |
| Authority Key Identifier | 2.5.29.35 | ❌ | keyid:CA 公钥指纹 |

---

## 4. 工作流程

### 4.1 主流程

```
┌─────────────────┐
│ 1. 读取配置文件  │
│   cert_config.xml│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 2. 生成 CA 证书  │
│   - 生成 4096 位  │
│     RSA 密钥     │
│   - 自签名证书   │
│   - 有效期 10 年 │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 3. 生成服务器证书│
│   For each Server│
│   - 生成私钥     │
│   - 添加 SAN     │
│   - CA 签署      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 4. 生成客户端证书│
│   For each Client│
│   - 生成私钥     │
│   - 添加 EKU     │
│   - CA 签署      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 5. 生成 README   │
│   - 证书清单     │
│   - 验证命令     │
│   - 部署说明     │
└─────────────────┘
```

### 4.2 证书签发流程

```python
# 伪代码
def generate_certificate(cert_type):
    # 1. 生成密钥对
    private_key = rsa.generate_private_key(key_size)
    
    # 2. 构建证书主题
    subject = build_subject_from_config(config)
    
    # 3. 创建证书构建器
    builder = x509.CertificateBuilder()
    builder.subject_name(subject)
    builder.issuer_name(ca_cert.subject)  # CA 作为签发者
    builder.public_key(private_key.public_key())
    
    # 4. 添加标准扩展
    builder.add_extension(BasicConstraints(ca=False))
    builder.add_extension(KeyUsage(digitalSignature, keyEncipherment))
    
    # 5. 添加类型特定扩展
    if cert_type == "server":
        builder.add_extension(ExtendedKeyUsage([serverAuth]))
        builder.add_extension(SubjectAlternativeName(san_list))
    elif cert_type == "client":
        builder.add_extension(ExtendedKeyUsage([clientAuth]))
    
    # 6. CA 签署证书
    certificate = builder.sign(ca_private_key, SHA256)
    
    # 7. 保存文件
    save_private_key(private_key, "cert.key", mode=0o600)
    save_certificate(certificate, "cert.crt", mode=0o644)
```

---

## 5. 安全设计

### 5.1 私钥保护

1. **文件权限**:
   - 私钥文件 (*.key): 权限 600 (仅所有者可读写)
   - 证书文件 (*.crt): 权限 644 (公开可读)

2. **加密存储**:
   - 测试环境: 明文存储 (NoEncryption)
   - 生产环境: 建议使用 PKCS#8 加密格式 (AES-256-CBC)

3. **HSM/TPM 集成**:
   - CA 私钥应存储在硬件安全模块 (HSM) 中
   - 机载设备私钥应存储在可信平台模块 (TPM) 中

### 5.2 证书验证

#### 5.2.1 验证命令

```bash
# 验证服务器证书链
openssl verify -CAfile ca.crt server-magic.crt

# 查看证书详情
openssl x509 -in server-magic.crt -text -noout

# 验证 SAN 扩展
openssl x509 -in server-magic.crt -text | grep -A 5 "Subject Alternative Name"
```

#### 5.2.2 自动化验证

生成的 README.md 包含所有证书的验证命令:

```markdown
## 证书验证

验证证书链:
```bash
openssl verify -CAfile ca.crt server-magic.crt
openssl verify -CAfile ca.crt client-magic.crt
openssl verify -CAfile ca.crt client-efb-01.crt
```

### 5.3 已知限制

#### 5.3.1 Mode S 地址扩展

Python `cryptography` 库对 `OtherName` (通用名称) 的支持有限，无法直接添加 ARINC 842 定义的 Mode S 地址扩展 (OID: 1.3.6.1.4.1.13712.842.1.1)。

**解决方案**:

1. **工具警告**: 程序在遇到 `<OtherName>` 配置时输出警告信息
2. **OpenSSL 补救**: 使用 OpenSSL 命令行工具手动添加

```bash
# 创建 OpenSSL 配置文件
cat > server_san.cnf << EOF
[ req ]
distinguished_name = req_distinguished_name
x509_extensions = v3_req

[ v3_req ]
subjectAltName = @alt_names

[ alt_names ]
DNS.1 = magic-server.example.com
IP.1 = 192.168.1.100
otherName.1 = 1.3.6.1.4.1.13712.842.1.1;UTF8:780ABC
EOF

# 重新签署证书
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -out server-magic.crt -days 1095 -sha256 \
  -extfile server_san.cnf -extensions v3_req
```

---

## 6. 依赖管理

### 6.1 Python 依赖

| 库名称 | 版本 | 用途 |
|-------|------|------|
| cryptography | ≥ 42.0.0 | X.509 证书操作 |
| python | ≥ 3.8 | 运行环境 |

### 6.2 安装方法

```bash
# Ubuntu/Debian
sudo apt-get install python3-pip
pip3 install cryptography

# CentOS/RHEL
sudo yum install python3-pip
pip3 install cryptography

# 验证安装
python3 -c "from cryptography import x509; print('OK')"
```

---

## 7. 错误处理

### 7.1 常见错误

| 错误类型 | 原因 | 解决方法 |
|---------|------|---------|
| `FileNotFoundError` | 配置文件不存在 | 检查文件路径 |
| `ParseError` | XML 格式错误 | 验证 XML 语法 |
| `ValueError` | 无效的 IP 地址 | 检查 SAN 中的 IP 格式 |
| `PermissionError` | 输出目录无写权限 | 检查目录权限 |

### 7.2 异常捕获

```python
try:
    generator = CertificateGenerator(config_file)
    generator.run()
except ET.ParseError as e:
    print(f"XML 解析错误: {e}")
except ValueError as e:
    print(f"配置值错误: {e}")
except Exception as e:
    print(f"未知错误: {e}")
    traceback.print_exc()
```

---

## 8. 性能考虑

### 8.1 性能指标

| 操作 | 时间复杂度 | 实测性能 (参考) |
|------|-----------|---------------|
| 生成 2048 位 RSA 密钥 | O(n³) | ~0.5 秒 |
| 生成 4096 位 RSA 密钥 | O(n³) | ~3 秒 |
| 签署证书 (SHA-256) | O(n) | ~0.1 秒 |
| 保存证书文件 (PEM) | O(1) | <0.01 秒 |

**批量生成性能** (1 CA + 10 服务器 + 50 客户端):
- 估算时间: ~40 秒
- 瓶颈: RSA 密钥对生成

### 8.2 优化建议

1. **并行生成**: 使用 Python `multiprocessing` 模块并行生成客户端证书
2. **密钥复用**: 测试环境可复用客户端密钥对 (不推荐生产环境)
3. **预生成密钥**: 提前生成密钥池，签署时直接取用

---

## 9. 扩展性设计

### 9.1 未来功能

- [ ] 支持 CRL (证书吊销列表) 生成
- [ ] OCSP (在线证书状态协议) 响应器集成
- [ ] HSM 接口 (PKCS#11 标准)
- [ ] Web UI 配置界面
- [ ] 证书续期自动化工具

### 9.2 接口设计

```python
class CertificateGenerator:
    # 新增方法
    + generate_crl(revoked_certs: List[str]) -> None
        """生成证书吊销列表"""
    
    + renew_certificate(old_cert: str, new_validity_days: int) -> None
        """续期证书"""
    
    + import_from_hsm(hsm_slot: str, hsm_pin: str) -> None
        """从 HSM 导入 CA 私钥"""
```

---

## 10. 符合标准

### 10.1 航空标准

- ✅ **ARINC 839**: MAGIC 系统架构和协议定义
- ✅ **ARINC 842**: 航空 PKI 证书策略 (Mode S 地址绑定)
- ✅ **ARINC 822A**: Gatelink 安全要求 (TLS 1.2+)
- ✅ **ARINC 830**: AGIE 数据加密要求

### 10.2 通用标准

- ✅ **IETF RFC 5280**: X.509 公钥基础设施证书和 CRL 规范
- ✅ **IETF RFC 8446**: TLS 1.3 协议 (向下兼容 TLS 1.2)
- ✅ **NIST FIPS 186-4**: 数字签名标准 (RSA, SHA-256)

---

## 11. 测试计划

### 11.1 单元测试

```python
# test_cert_generator.py
def test_load_config():
    """测试配置文件加载"""
    
def test_generate_ca():
    """测试 CA 证书生成"""
    
def test_build_san():
    """测试 SAN 扩展构建"""
    
def test_verify_chain():
    """测试证书链验证"""
```

### 11.2 集成测试

1. 生成完整证书链
2. 使用 OpenSSL 验证所有证书
3. 部署到 freeDiameter 并建立 TLS 连接
4. 验证 SAN 扩展中的 DNS/IP 匹配

---

## 12. 文档维护

| 文档名称 | 路径 | 维护频率 |
|---------|------|---------|
| 软件设计文档 | DESIGN.md | 每个版本更新 |
| 用户手册 | USER_GUIDE.md | 每个版本更新 |
| API 文档 | API.md | 代码修改时更新 |
| 变更日志 | CHANGELOG.md | 每次发布时更新 |

---

## 12. 业务配置集成设计 (v1.2.2)

### 12.1 流量安全合规性设计

为了满足 ARINC 839 和 3GPP TS 23.060 的合规性要求，工具在生成证书的同时，会自动生成配套的业务配置文件。

#### 12.1.1 3GPP TFT 模板生成
- **设计目标**: 自动化生成符合 3GPP 规范的流量过滤模板。
- **实现方式**: 在 `Client_Profile.xml` 中生成 `TFTTemplate` 字段，并支持在客户端 `*_magic.conf` 中注入具体的 `_iTFT` 规则。
- **合规性**: 严格遵循 `_iTFT=[cid,[pf_id,precedence[,src_ip.mask[,dst_ip.mask[,proto[,dst_port[,src_port]]]]]]]` 语法。

#### 12.1.2 ARINC 839 NAPT 规则生成
- **设计目标**: 支持机载网关的地址转换配置。
- **实现方式**: 支持 8 字段标准 NAPT 字符串格式。
- **动态性**: 引入 `%LinkIp%` 占位符机制，解决机载环境链路 IP 动态分配的挑战。

### 12.2 自动化集成流程
1. **XML 解析**: 提取 `MagicConfig` 中的业务参数。
2. **模板填充**: 将参数注入到 `Client_Profile.xml` (服务器端) 和 `*_magic.conf` (客户端)。
3. **权限管理**: 自动为生成的业务配置文件设置 600 权限，保护认证凭据。

---

## 13. 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| 1.2.2 | 2025-12-25 | 增加 3GPP TFT 和 ARINC 839 NAPT 标准支持，完善业务配置自动化生成 |
| 1.0.0 | 2025-12-24 | 初始版本，支持 CA/服务器/客户端证书批量生成 |

---

**审核人**: MAGIC 开发团队  
**批准日期**: 2025-12-24
