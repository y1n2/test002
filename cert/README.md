# MAGIC 系统证书清单

## 生成时间
2025-12-24

## 证书体系结构

```
CES-MAGIC-Root-CA (根CA)
├── magic-server.example.com (服务器证书)
└── magic-client.example.com (客户端证书)
```

## 证书列表

| 文件 | 类型 | Subject CN | 有效期 | 用途 |
|------|------|-----------|--------|------|
| ca.crt | 根CA证书 | CES-MAGIC-Root-CA | 10年 (2025-2035) | 签发所有证书 |
| ca.key | 根CA私钥 | - | - | **严格保密** |
| server.crt | 服务器证书 | magic-server.example.com | 3年 (2025-2028) | MAGIC Server TLS |
| server.key | 服务器私钥 | - | - | **严格保密** |
| client.crt | 客户端证书 | magic-client.example.com | 1年 (2025-2026) | MAGIC Client TLS |
| client.key | 客户端私钥 | - | - | **严格保密** |

## 证书特性

### CA 根证书
- **组织**: China Eastern Airlines
- **部门**: Aviation PKI
- **密钥长度**: 4096 位 RSA
- **签名算法**: SHA-256

### MAGIC Server 证书
- **Extended Key Usage**: serverAuth
- **Subject Alternative Names**:
  - Mode S 地址: 780ABC (OID: 1.3.6.1.4.1.13712.842.1.1)
  - DNS: magic-server.example.com
  - DNS: magic.server.example.com
  - DNS: localhost
  - IP: 127.0.0.1, 192.168.126.1, 192.168.1.1
- **密钥长度**: 2048 位 RSA

### MAGIC Client 证书
- **Extended Key Usage**: clientAuth
- **Subject Alternative Names**:
  - DNS: magic-client.example.com
  - DNS: magic-client-pc3.arinc839.local
  - DNS: localhost
  - IP: 127.0.0.1, 192.168.10.5
- **密钥长度**: 2048 位 RSA

## 使用说明

### 部署到 MAGIC Server
```bash
cp ca.crt server.crt server.key /path/to/magic/server/certs/
chmod 644 ca.crt server.crt
chmod 600 server.key
```

### 部署到 MAGIC Client
```bash
cp ca.crt client.crt client.key /path/to/magic/client/certs/
chmod 644 ca.crt client.crt
chmod 600 client.key
```

### freeDiameter 配置

**服务器配置**:
```properties
TLS_Cred = "/path/to/certs/server.crt", "/path/to/certs/server.key";
TLS_CA = "/path/to/certs/ca.crt";
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
TLS_DH_Bits = 2048;
```

**客户端配置**:
```properties
TLS_Cred = "/path/to/certs/client.crt", "/path/to/certs/client.key";
TLS_CA = "/path/to/certs/ca.crt";
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
TLS_DH_Bits = 2048;
```

## 证书验证

验证证书链:
```bash
openssl verify -CAfile ca.crt server.crt
openssl verify -CAfile ca.crt client.crt
```

查看证书详情:
```bash
openssl x509 -in server.crt -noout -text
openssl x509 -in client.crt -noout -text
```

## 安全注意事项

1. ✅ 所有私钥文件 (*.key) 权限必须为 600
2. ✅ CA 私钥 (ca.key) 应在离线环境生成并妥善保管
3. ✅ 生产环境应使用 HSM 或 TPM 存储私钥
4. ⚠️ 客户端证书将在 2026-12-24 过期，需提前更新
5. ⚠️ 建议启用 CRL 检查 (TLS_CRL 配置)

## 符合标准

- ✅ ARINC 839 (MAGIC)
- ✅ ARINC 842 (Aviation PKI)
- ✅ ARINC 822A (Gatelink)
- ✅ IETF RFC 5280 (X.509 PKI)
- ✅ FIPS 140-2 (加密要求)
