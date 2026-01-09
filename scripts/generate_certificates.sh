#!/bin/bash
# generate_certificates.sh - 为 freeDiameter 分布式部署生成证书

set -e  # 遇到错误立即退出

echo "=========================================="
echo "freeDiameter 分布式部署证书生成工具"
echo "=========================================="

# 配置参数
CERT_DIR="/home/zhuwuhui/freeDiameter/exe/certs"
COUNTRY="CN"
STATE="Beijing"
CITY="Beijing"
ORGANIZATION="freeDiameter"
ORG_UNIT="Aviation"
CA_DAYS=3650
CERT_DAYS=365

# 主机配置
UBUNTU_SERVER="192.168.1.10"
WINDOWS_CLIENT="192.168.1.20"
WINDOWS_SIMULATOR="192.168.1.30"

# 创建证书目录
echo "创建证书目录: $CERT_DIR"
mkdir -p $CERT_DIR
cd $CERT_DIR

# 清理旧证书
echo "清理旧证书文件..."
rm -f *.crt *.key *.csr *.pem *.srl

echo "开始生成证书..."

# 1. 生成CA私钥
echo "1. 生成CA私钥..."
openssl genrsa -out ca.key 4096
chmod 600 ca.key

# 2. 生成CA证书
echo "2. 生成CA证书..."
openssl req -new -x509 -days $CA_DAYS -key ca.key -out ca.crt \
    -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORGANIZATION/OU=$ORG_UNIT/CN=freeDiameter CA"
chmod 644 ca.crt

# 3. 生成服务端私钥和证书
echo "3. 生成服务端证书..."
openssl genrsa -out server.key 2048
chmod 600 server.key

# 创建服务端证书扩展配置
cat > server.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = server.freediameter.local
DNS.2 = ubuntu-server
DNS.3 = localhost
IP.1 = $UBUNTU_SERVER
IP.2 = 127.0.0.1
EOF

openssl req -new -key server.key -out server.csr \
    -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORGANIZATION/OU=Server/CN=server.freediameter.local"

openssl x509 -req -days $CERT_DAYS -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt -extensions v3_req -extfile server.ext
chmod 644 server.crt

# 4. 生成客户端私钥和证书
echo "4. 生成客户端证书..."
openssl genrsa -out client.key 2048
chmod 600 client.key

# 创建客户端证书扩展配置
cat > client.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = client.freediameter.local
DNS.2 = windows-client
IP.1 = $WINDOWS_CLIENT
EOF

openssl req -new -key client.key -out client.csr \
    -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORGANIZATION/OU=Client/CN=client.freediameter.local"

openssl x509 -req -days $CERT_DAYS -in client.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out client.crt -extensions v3_req -extfile client.ext
chmod 644 client.crt

# 5. 生成模拟器私钥和证书
echo "5. 生成模拟器证书..."
openssl genrsa -out simulator.key 2048
chmod 600 simulator.key

# 创建模拟器证书扩展配置
cat > simulator.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names

[alt_names]
DNS.1 = simulator.freediameter.local
DNS.2 = windows-simulator
IP.1 = $WINDOWS_SIMULATOR
EOF

openssl req -new -key simulator.key -out simulator.csr \
    -subj "/C=$COUNTRY/ST=$STATE/L=$CITY/O=$ORGANIZATION/OU=Simulator/CN=simulator.freediameter.local"

openssl x509 -req -days $CERT_DAYS -in simulator.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out simulator.crt -extensions v3_req -extfile simulator.ext
chmod 644 simulator.crt

# 6. 生成DH参数文件
echo "6. 生成DH参数文件..."
openssl dhparam -out dh.pem 2048
chmod 644 dh.pem

# 7. 创建证书包
echo "7. 创建证书包..."

# 为服务端创建证书包
mkdir -p server_certs
cp ca.crt server.crt server.key dh.pem server_certs/

# 为客户端创建证书包
mkdir -p client_certs
cp ca.crt client.crt client.key client_certs/

# 为模拟器创建证书包
mkdir -p simulator_certs
cp ca.crt simulator.crt simulator.key simulator_certs/

# 8. 验证证书
echo "8. 验证证书..."

echo "验证服务端证书:"
openssl verify -CAfile ca.crt server.crt

echo "验证客户端证书:"
openssl verify -CAfile ca.crt client.crt

echo "验证模拟器证书:"
openssl verify -CAfile ca.crt simulator.crt

# 9. 显示证书信息
echo "9. 证书信息摘要:"
echo "CA证书:"
openssl x509 -in ca.crt -noout -subject -dates

echo "服务端证书:"
openssl x509 -in server.crt -noout -subject -dates

echo "客户端证书:"
openssl x509 -in client.crt -noout -subject -dates

echo "模拟器证书:"
openssl x509 -in simulator.crt -noout -subject -dates

# 10. 清理临时文件
echo "10. 清理临时文件..."
rm -f *.csr *.ext

# 11. 创建证书部署脚本
echo "11. 创建证书部署脚本..."

cat > deploy_certificates.sh << 'EOF'
#!/bin/bash
# deploy_certificates.sh - 部署证书到各个节点

CERT_DIR="/home/zhuwuhui/freeDiameter/exe/certs"
UBUNTU_SERVER="192.168.1.10"
WINDOWS_CLIENT="192.168.1.20"
WINDOWS_SIMULATOR="192.168.1.30"

echo "部署证书到各个节点..."

# 部署到Windows客户端 (需要手动复制或使用scp)
echo "客户端证书文件位置: $CERT_DIR/client_certs/"
echo "请将以下文件复制到Windows客户端:"
echo "  - ca.crt"
echo "  - client.crt"
echo "  - client.key"

# 部署到Windows模拟器 (需要手动复制或使用scp)
echo "模拟器证书文件位置: $CERT_DIR/simulator_certs/"
echo "请将以下文件复制到Windows模拟器:"
echo "  - ca.crt"
echo "  - simulator.crt"
echo "  - simulator.key"

echo "证书部署说明已生成"
EOF

chmod +x deploy_certificates.sh

# 12. 生成证书使用说明
cat > certificate_usage.md << EOF
# freeDiameter 分布式部署证书使用说明

## 证书文件说明

### CA证书
- **文件**: ca.crt
- **用途**: 根证书，用于验证其他证书
- **部署**: 所有节点都需要

### 服务端证书
- **文件**: server.crt, server.key
- **用途**: Ubuntu服务端SSL/TLS通信
- **部署**: Ubuntu服务端 (192.168.1.10)

### 客户端证书
- **文件**: client.crt, client.key
- **用途**: Windows客户端SSL/TLS通信
- **部署**: Windows客户端 (192.168.1.20)

### 模拟器证书
- **文件**: simulator.crt, simulator.key
- **用途**: Windows模拟器SSL/TLS通信
- **部署**: Windows模拟器 (192.168.1.30)

### DH参数文件
- **文件**: dh.pem
- **用途**: Diffie-Hellman密钥交换
- **部署**: 服务端

## 证书有效期
- CA证书: $CA_DAYS 天
- 其他证书: $CERT_DAYS 天

## 证书更新
证书到期前需要重新生成，建议在到期前30天更新。

## 安全注意事项
1. 私钥文件(.key)权限设置为600
2. 证书文件(.crt)权限设置为644
3. 定期备份证书文件
4. 不要将私钥文件提交到版本控制系统
EOF

echo "=========================================="
echo "证书生成完成!"
echo "=========================================="
echo "证书目录: $CERT_DIR"
echo "证书文件列表:"
ls -la $CERT_DIR/*.crt $CERT_DIR/*.key $CERT_DIR/*.pem
echo ""
echo "证书包目录:"
ls -la $CERT_DIR/*_certs/
echo ""
echo "请查看 certificate_usage.md 了解证书使用说明"
echo "运行 ./deploy_certificates.sh 获取部署指导"
echo "=========================================="