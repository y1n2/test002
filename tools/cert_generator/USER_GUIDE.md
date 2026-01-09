# MAGIC 证书自动生成工具 - 用户手册

**版本**: 1.0.0  
**适用人群**: 航空公司运维人员、MAGIC 系统集成商、航电开发团队

---

## 目录

1. [快速开始](#1-快速开始)
2. [安装配置](#2-安装配置)
3. [配置文件详解](#3-配置文件详解)
    - [3.1 全局配置](#31-全局配置)
    - [3.2 CA 配置](#32-ca-配置)
    - [3.3 服务器配置](#33-服务器配置)
    - [3.4 客户端配置](#34-客户端配置)
    - [3.5 TFT 与 NAPT 高级配置](#35-tft-与-napt-高级配置)
4. [使用方法](#4-使用方法)
5. [证书部署](#5-证书部署)
6. [常见问题](#6-常见问题)
7. [故障排查](#7-故障排查)
8. [安全最佳实践](#8-安全最佳实践)

---

## 1. 快速开始

### 1.1 最小化示例

```bash
# 1. 进入工具目录
cd /home/zhuwuhui/freeDiameter/tools/cert_generator

# 2. 编辑配置文件 (使用示例配置)
cp cert_config.xml my_config.xml
vim my_config.xml  # 修改 CommonName、Organization 等字段

# 3. 运行生成工具
python3 cert_generator.py my_config.xml

# 4. 查看生成的证书
ls -lh ./output/
```

**预期输出**:

```
============================================================
MAGIC 证书自动生成工具 v1.0.0
============================================================
✓ 配置加载成功
✓ 输出目录: ./output

[1/3] 生成 CA 根证书...
  └─ 私钥: ca.key (权限 600)
  └─ 证书: ca.crt
  ✓ CA 证书生成完成 (有效期: 3650 天)

[2/3] 生成服务器证书 (共 1 个)...

  [1] magic-server.example.com
  ⚠️  警告: Mode S 地址 (OID: 1.3.6.1.4.1.13712.842.1.1) 需要使用 OpenSSL 手动添加
  └─ 私钥: server-magic.key (权限 600)
  └─ 证书: server-magic.crt
      ✓ 完成 (有效期: 1095 天)

[3/3] 生成客户端证书 (共 2 个)...

  [1] magic-client
  └─ 私钥: client-magic.key (权限 600)
  └─ 证书: client-magic.crt
      ✓ 完成 (有效期: 365 天)

  [2] EFB-01
  └─ 私钥: client-efb-01.key (权限 600)
  └─ 证书: client-efb-01.crt
      ✓ 完成 (有效期: 365 天)

✓ README 文档生成: ./output/README.md

============================================================
✓ 所有证书生成完成！
============================================================

证书输出目录: /home/zhuwuhui/freeDiameter/tools/cert_generator/output
```

---

## 2. 安装配置

### 2.1 系统要求

- **操作系统**: Linux (Ubuntu 20.04+, CentOS 8+, Debian 11+)
- **Python 版本**: Python 3.8 或更高
- **依赖库**: `cryptography` ≥ 42.0.0

### 2.2 安装 Python 依赖

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install python3 python3-pip -y
pip3 install cryptography
```

#### CentOS/RHEL

```bash
sudo yum install python3 python3-pip -y
pip3 install cryptography
```

#### 验证安装

```bash
python3 --version        # 应显示 Python 3.8+
python3 -c "from cryptography import x509; print('✓ cryptography 库已安装')"
```

### 2.3 工具安装

```bash
# 克隆或下载工具
cd /path/to/freeDiameter/tools/cert_generator

# 赋予执行权限
chmod +x cert_generator.py

# (可选) 创建符号链接到 /usr/local/bin
sudo ln -s $(pwd)/cert_generator.py /usr/local/bin/magic-cert-gen
```

---

## 3. 配置文件详解

### 3.1 XML 结构

配置文件 `cert_config.xml` 采用分层结构:

```
CertificateConfig
├─ OutputDirectory (输出目录)
├─ CA (CA 根证书配置)
├─ Servers (服务器证书数组)
│  └─ Server (单个服务器)
└─ Clients (客户端证书数组)
   └─ Client (单个客户端)
```

### 3.2 字段说明

#### 3.2.1 基础字段 (所有证书通用)

| 字段名 | 必填 | 说明 | 示例 |
|-------|------|------|------|
| `Country` | ✅ | 国家代码 (2 字母) | `CN`, `US` |
| `State` | ❌ | 省/州 | `Shanghai`, `California` |
| `Locality` | ❌ | 城市 | `Shanghai`, `San Francisco` |
| `Organization` | ✅ | 组织名称 | `China Eastern Airlines` |
| `OrganizationalUnit` | ❌ | 部门 | `Aviation IT`, `Fleet Operations` |
| `CommonName` | ✅ | 通用名称 | `magic-server.example.com` |
| `KeySize` | ✅ | 密钥长度 (位) | `2048`, `4096` |
| `ValidityDays` | ✅ | 有效期 (天) | `365`, `1095`, `3650` |
| `OutputPrefix` | ✅ | 输出文件前缀 | `ca`, `server-magic` |

#### 3.2.2 CA 根证书特殊配置

- **KeySize**: 建议 `4096` (更高安全性)
- **ValidityDays**: 建议 `3650` (10 年)

#### 3.2.3 服务器证书特殊配置

- **SubjectAltNames**: 主题备用名称 (必须配置)
  - `<DNS>`: DNS 域名
  - `<IP>`: IP 地址
  - `<OtherName>`: Mode S 地址 (需手动添加，见 5.3 节)

#### 3.2.4 客户端证书特殊配置

- **SubjectAltNames**: 可选 (通常仅用于设备证书)
- **ValidityDays**: 建议 `365` (1 年，便于定期更新)

### 3.3 配置示例

#### 3.3.1 东方航空 C929 机队

```xml
<CertificateConfig>
  <OutputDirectory>./output/ces-c929</OutputDirectory>
  
  <CA>
    <Country>CN</Country>
    <State>Shanghai</State>
    <Locality>Shanghai</Locality>
    <Organization>China Eastern Airlines</Organization>
    <OrganizationalUnit>Aviation IT Department</OrganizationalUnit>
    <CommonName>CES-MAGIC-Root-CA</CommonName>
    <KeySize>4096</KeySize>
    <ValidityDays>3650</ValidityDays>
    <OutputPrefix>ca</OutputPrefix>
  </CA>
  
  <Servers>
    <!-- C929-001 号机 MAGIC 服务器 -->
    <Server>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <OrganizationalUnit>C929 Fleet</OrganizationalUnit>
      <CommonName>magic-c929-001.ces.com</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>1095</ValidityDays>
      <OutputPrefix>server-c929-001</OutputPrefix>
      
      <SubjectAltNames>
        <DNS>magic-c929-001.ces.com</DNS>
        <DNS>magic-server.c929-001.local</DNS>
        <IP>192.168.100.10</IP>
        <OtherName>
          <OID>1.3.6.1.4.1.13712.842.1.1</OID>
          <Value>780ABC</Value>  <!-- Mode S 地址 -->
        </OtherName>
      </SubjectAltNames>
    </Server>
    
    <!-- C929-002 号机 -->
    <Server>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <CommonName>magic-c929-002.ces.com</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>1095</ValidityDays>
      <OutputPrefix>server-c929-002</OutputPrefix>
      
      <SubjectAltNames>
        <DNS>magic-c929-002.ces.com</DNS>
        <IP>192.168.100.20</IP>
        <OtherName>
          <OID>1.3.6.1.4.1.13712.842.1.1</OID>
          <Value>780ABD</Value>
        </OtherName>
      </SubjectAltNames>
    </Server>
  </Servers>
  
  <Clients>
    <!-- EFB 客户端 -->
    <Client>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <CommonName>EFB-Pilot-01</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>365</ValidityDays>
      <OutputPrefix>client-efb-pilot-01</OutputPrefix>
    </Client>
    
    <!-- IFE 客户端 -->
    <Client>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <CommonName>IFE-Passenger-System</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>365</ValidityDays>
      <OutputPrefix>client-ife-passenger</OutputPrefix>
    </Client>
    
    <!-- OMT 维护终端 -->
    <Client>
      <Country>CN</Country>
      <Organization>China Eastern Airlines</Organization>
      <CommonName>OMT-Maintenance-01</CommonName>
      <KeySize>2048</KeySize>
      <ValidityDays>365</ValidityDays>
      <OutputPrefix>client-omt-maint-01</OutputPrefix>
    </Client>
  </Clients>
</CertificateConfig>
```

### 3.5 TFT 与 NAPT 高级配置

为了符合 ARINC 839 和 3GPP 标准，工具支持在 `MagicConfig` 节点下配置精确的流量过滤和地址转换规则。

#### 3.5.1 TFT (Traffic Flow Template) 配置

TFT 用于定义具体的业务流过滤规则。工具生成的格式符合 3GPP TS 23.060 标准。

**配置示例**:
```xml
<TFTs>
    <!-- 格式: _iTFT=[cid,[pf_id,precedence[,src_ip.mask[,dst_ip.mask[,proto[,dst_port[,src_port]]]]]]] -->
    <TFT direction="GROUND">_iTFT=,1,10,192.168.126.5.255.255.255.255,10.2.2.8.255.255.255.255,6,80.80,0.65535</TFT>
</TFTs>
```

**参数说明**:
- `direction`: 流量方向，可选 `GROUND` (下行) 或 `AIR` (上行)。
- `pf_id`: 过滤器标识符。
- `precedence`: 优先级，数值越小优先级越高。
- `src_ip.mask`: 源 IP 及其掩码（点分十进制）。
- `dst_ip.mask`: 目的 IP 及其掩码。
- `proto`: 协议号（6 为 TCP，17 为 UDP）。
- `dst_port`: 目的端口范围（如 `80.80` 表示仅 80 端口）。

#### 3.5.2 NAPT (Network Address and Port Translation) 配置

NAPT 用于在机载网关执行地址转换。工具生成的格式符合 ARINC 839 标准。

**配置示例**:
```xml
<NAPTs>
    <!-- 格式: <NAT-Type>,<Source-IP>,<Destination-IP>,<IP-Protocol>,<Destination-Port>,<Source-Port>,<to-IP>,<to-Port> -->
    <NAPT>SNAT,192.168.126.5.255.255.255.255,0.0.0.0.0.0.0.0,6,80,0.65535,%LinkIp%,8080</NAPT>
</NAPTs>
```

**关键特性**:
- **%LinkIp% 占位符**: 在规则中使用 `%LinkIp%`，MAGIC 系统会在运行时自动将其替换为当前链路分配的真实公网 IP。
- **端口范围**: 使用点分隔（如 `2000.2099`）。

---

## 4. 使用方法

### 4.1 基本用法

```bash
python3 cert_generator.py <配置文件.xml>
```

**示例**:

```bash
# 生成东方航空 C929 机队证书
python3 cert_generator.py ces_c929_config.xml

# 生成测试环境证书
python3 cert_generator.py test_config.xml
```

### 4.2 批量生成

#### 4.2.1 生成多个机队证书

```bash
# 创建配置文件目录
mkdir -p configs/

# 为每个机队创建配置文件
vim configs/c929_fleet.xml
vim configs/a320_fleet.xml
vim configs/b787_fleet.xml

# 批量生成
for config in configs/*.xml; do
    echo "生成 $config ..."
    python3 cert_generator.py $config
done
```

#### 4.2.2 脚本封装

```bash
#!/bin/bash
# generate_all_certs.sh

CONFIGS=(
    "configs/ces_c929_fleet.xml"
    "configs/ces_a320_fleet.xml"
    "configs/emu_b787_fleet.xml"
)

for config in "${CONFIGS[@]}"; do
    echo "===== 生成 $config ====="
    python3 cert_generator.py "$config"
    
    if [ $? -eq 0 ]; then
        echo "✓ 成功"
    else
        echo "✗ 失败"
        exit 1
    fi
done

echo "===== 所有证书生成完成 ====="
```

### 4.3 验证生成的证书

```bash
# 进入输出目录
cd ./output/

# 验证 CA 证书
openssl x509 -in ca.crt -text -noout

# 验证服务器证书链
openssl verify -CAfile ca.crt server-magic.crt

# 验证客户端证书链
openssl verify -CAfile ca.crt client-efb-01.crt

# 查看 SAN 扩展
openssl x509 -in server-magic.crt -text | grep -A 5 "Subject Alternative Name"
```

**预期输出**:

```
server-magic.crt: OK
client-efb-01.crt: OK

X509v3 Subject Alternative Name:
    DNS:magic-server.example.com
    DNS:magic-server.local
    IP Address:192.168.1.100
```

---

## 5. 证书部署

### 5.1 部署到 MAGIC Server (freeDiameter)

#### 5.1.1 复制证书到配置目录

```bash
# 假设 freeDiameter 配置目录为 /etc/freeDiameter
sudo mkdir -p /etc/freeDiameter/certs
sudo cp output/ca.crt /etc/freeDiameter/certs/
sudo cp output/server-magic.crt /etc/freeDiameter/certs/
sudo cp output/server-magic.key /etc/freeDiameter/certs/

# 设置私钥权限
sudo chmod 600 /etc/freeDiameter/certs/server-magic.key
sudo chown freediameter:freediameter /etc/freeDiameter/certs/*
```

#### 5.1.2 修改 freeDiameter 配置文件

编辑 `/etc/freeDiameter/magic_server.conf`:

```
# TLS 配置
TLS_Cred = "/etc/freeDiameter/certs/server-magic.crt" , "/etc/freeDiameter/certs/server-magic.key";
TLS_CA = "/etc/freeDiameter/certs/ca.crt";
TLS_DH_Bits = 2048;
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
```

#### 5.1.3 重启服务

```bash
sudo systemctl restart freeDiameter
sudo systemctl status freeDiameter
```

### 5.2 部署到客户端 (EFB/IFE/OMT)

#### 5.2.1 复制客户端证书

```bash
# 示例: 复制到 EFB 设备
scp output/ca.crt pilot@efb-device:/opt/magic-client/certs/
scp output/client-efb-01.crt pilot@efb-device:/opt/magic-client/certs/
scp output/client-efb-01.key pilot@efb-device:/opt/magic-client/certs/

# 设置权限
ssh pilot@efb-device "chmod 600 /opt/magic-client/certs/client-efb-01.key"
```

#### 5.2.2 修改客户端配置

编辑 `/opt/magic-client/magic_client.conf`:

```
TLS_Cred = "/opt/magic-client/certs/client-efb-01.crt" , "/opt/magic-client/certs/client-efb-01.key";
TLS_CA = "/opt/magic-client/certs/ca.crt";
TLS_DH_Bits = 2048;
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
```

### 5.3 手动添加 Mode S 地址扩展

⚠️ **重要**: Python `cryptography` 库不支持 `OtherName` 扩展，需使用 OpenSSL 补救。

#### 5.3.1 准备 OpenSSL 配置文件

```bash
cat > server_san_modes.cnf << 'EOF'
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
```

#### 5.3.2 重新签署服务器证书

```bash
# 1. 生成 CSR (证书签名请求)
openssl req -new -key output/server-magic.key \
  -out server-magic.csr \
  -subj "/C=CN/O=China Eastern Airlines/CN=magic-server.example.com"

# 2. 使用 CA 签署证书并添加 SAN
openssl x509 -req -in server-magic.csr \
  -CA output/ca.crt -CAkey output/ca.key -CAcreateserial \
  -out output/server-magic-modes.crt \
  -days 1095 -sha256 \
  -extfile server_san_modes.cnf -extensions v3_req

# 3. 验证 Mode S 扩展
openssl x509 -in output/server-magic-modes.crt -text | grep -A 10 "Subject Alternative Name"
```

**预期输出**:

```
X509v3 Subject Alternative Name:
    DNS:magic-server.example.com
    IP Address:192.168.1.100
    othername: 1.3.6.1.4.1.13712.842.1.1::UTF8:780ABC
```

---

## 6. 常见问题

### Q1: 提示 "ModuleNotFoundError: No module named 'cryptography'"

**原因**: 未安装 `cryptography` 库

**解决**:

```bash
pip3 install cryptography
```

### Q2: 提示 "PermissionError: [Errno 13] Permission denied: './output/ca.key'"

**原因**: 输出目录无写权限

**解决**:

```bash
sudo mkdir -p ./output
sudo chown $USER:$USER ./output
```

### Q3: 生成的证书验证失败 "error 20 at 0 depth lookup:unable to get local issuer certificate"

**原因**: CA 证书路径错误或未指定

**解决**:

```bash
# 确保使用 -CAfile 参数
openssl verify -CAfile ca.crt server-magic.crt
```

### Q4: Mode S 地址未出现在 SAN 扩展中

**原因**: Python `cryptography` 库限制

**解决**: 参考 [5.3 手动添加 Mode S 地址扩展](#53-手动添加-mode-s-地址扩展)

### Q5: 证书有效期过短

**原因**: `ValidityDays` 配置错误

**解决**:

- CA 证书: `3650` (10 年)
- 服务器证书: `1095` (3 年)
- 客户端证书: `365` (1 年)

### Q6: CommonName 不匹配警告

**原因**: 证书 CN 与实际域名/IP 不匹配

**解决**:

1. 修改 XML 配置中的 `<CommonName>` 字段
2. 确保 SAN 扩展中包含正确的 DNS/IP
3. 重新生成证书

---

## 7. 故障排查

### 7.1 调试模式

```bash
# 启用 Python 调试输出
python3 -v cert_generator.py config.xml

# 查看详细证书信息
openssl x509 -in output/server-magic.crt -text -noout
```

### 7.2 日志分析

工具输出的日志分为三个阶段:

```
[1/3] 生成 CA 根证书...       ← CA 证书生成阶段
[2/3] 生成服务器证书...       ← 服务器证书批量生成
[3/3] 生成客户端证书...       ← 客户端证书批量生成
```

每个阶段的错误信息会明确指出失败的步骤。

### 7.3 常见错误代码

| 错误信息 | 原因 | 解决方法 |
|---------|------|---------|
| `ParseError` | XML 格式错误 | 使用 `xmllint config.xml` 验证语法 |
| `ValueError: Invalid IP address` | IP 格式错误 | 检查 `<IP>` 字段格式 (如 `192.168.1.100`) |
| `FileNotFoundError` | 配置文件不存在 | 检查文件路径是否正确 |
| `KeyError: 'CommonName'` | 必填字段缺失 | 补充 `<CommonName>` 字段 |

---

## 8. 安全最佳实践

### 8.1 私钥保护

1. **权限控制**:
   ```bash
   chmod 600 *.key        # 仅所有者可读写
   chmod 644 *.crt        # 公开可读
   ```

2. **加密存储** (生产环境):
   ```bash
   # 使用 AES-256-CBC 加密私钥
   openssl rsa -aes256 -in server-magic.key -out server-magic-encrypted.key
   ```

3. **HSM 存储**:
   - CA 私钥应存储在 HSM (如 Thales Luna, Gemalto SafeNet)
   - 机载设备私钥应存储在 TPM 2.0 芯片

### 8.2 证书有效期管理

| 证书类型 | 推荐有效期 | 更新频率 |
|---------|-----------|---------|
| CA 根证书 | 10 年 | 每 8 年更新 |
| 服务器证书 | 3 年 | 每 2 年更新 |
| 客户端证书 | 1 年 | 每 9 个月更新 |

### 8.3 证书吊销

虽然工具暂不支持 CRL 生成,但建议:

1. 记录所有已签发证书的序列号
2. 建立证书吊销数据库
3. 定期发布 CRL 文件

### 8.4 审计与合规

1. **日志记录**: 保存每次证书生成的日志
2. **变更管理**: 使用 Git 版本控制配置文件
3. **定期审核**: 每季度检查证书有效期

```bash
# 批量检查证书有效期
for cert in output/*.crt; do
    echo "=== $cert ==="
    openssl x509 -in "$cert" -noout -dates
done
```

---

## 9. 参考资料

### 9.1 标准文档

- **ARINC 839**: Media Independent Aircraft Ground Interface for IP Communications
- **ARINC 842**: Aircraft Data Network Part 2 - Security
- **IETF RFC 5280**: Internet X.509 Public Key Infrastructure Certificate and CRL Profile

### 9.2 工具文档

- OpenSSL 官方文档: https://www.openssl.org/docs/
- Python Cryptography 库文档: https://cryptography.io/

### 9.3 freeDiameter 配置

- freeDiameter 官方 Wiki: http://www.freediameter.net/
- MAGIC TLS 配置指南: `/mddoc/MAGIC_TLS_CERT_GUIDE.md`

---

## 10. 技术支持

### 10.1 问题反馈

- **邮箱**: magic-support@example.com
- **GitHub**: https://github.com/example/freeDiameter/issues

### 10.2 紧急联系

- **电话**: +86-21-1234-5678 (7×24 小时)
- **值班工程师**: duty-engineer@example.com

---

**文档版本**: 1.0.0  
**最后更新**: 2025-12-24  
**维护团队**: MAGIC 开发团队
