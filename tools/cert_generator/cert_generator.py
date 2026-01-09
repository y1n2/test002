#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MAGIC 证书自动生成工具 (集成版)
============================
符合 ARINC 839/842 标准的 TLS 证书批量生成工具
集成功能：
1. 批量生成 CA/Server/Client 证书
2. 自动更新 freeDiameter ACL 白名单 (acl_wl.conf)
3. 自动更新 MAGIC 业务策略 (Client_Profile.xml)
4. 自动生成客户端 freeDiameter 配置文件 (.conf)

作者: MAGIC 开发团队
版本: 1.2.2
日期: 2025-12-24
"""

import os
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
from cryptography import x509
from cryptography.x509.oid import NameOID, ExtensionOID, ExtendedKeyUsageOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.backends import default_backend
import ipaddress


class CertificateGenerator:
    """证书生成器类"""
    
    def __init__(self, config_file):
        """
        初始化证书生成器
        
        Args:
            config_file: XML 配置文件路径
        """
        self.config_file = config_file
        self.config = None
        self.output_dir = None
        self.client_fd_dir = None
        self.client_magic_dir = None
        self.server_profile_dir = None
        self.ca_cert = None
        self.ca_key = None
        # 项目根目录路径 (假设工具在 tools/cert_generator/)
        self.root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../"))
        
    def load_config(self):
        """加载 XML 配置文件"""
        try:
            tree = ET.parse(self.config_file)
            self.config = tree.getroot()
            self.output_dir = self.config.find('OutputDirectory').text
            
            # 创建输出目录
            os.makedirs(self.output_dir, exist_ok=True)
            
            # 创建子目录
            self.client_fd_dir = os.path.join(self.output_dir, "client_fd_config")
            self.client_magic_dir = os.path.join(self.output_dir, "client_magic_config")
            self.server_profile_dir = os.path.join(self.output_dir, "server_profile")
            os.makedirs(self.client_fd_dir, exist_ok=True)
            os.makedirs(self.client_magic_dir, exist_ok=True)
            os.makedirs(self.server_profile_dir, exist_ok=True)

            print(f"✓ 配置加载成功")
            print(f"✓ 输出目录: {self.output_dir}")
            print(f"  └─ 客户端FD配置: {self.client_fd_dir}")
            print(f"  └─ 客户端业务配置: {self.client_magic_dir}")
            print(f"  └─ 服务器端策略: {self.server_profile_dir}")
            return True
        except Exception as e:
            print(f"✗ 配置加载失败: {e}")
            return False
    
    def generate_private_key(self, key_size):
        """生成 RSA 私钥"""
        return rsa.generate_private_key(
            public_exponent=65537,
            key_size=key_size,
            backend=default_backend()
        )
    
    def save_private_key(self, key, filename):
        """保存私钥到文件 (PEM 格式, 权限 600)"""
        key_path = os.path.join(self.output_dir, filename)
        with open(key_path, 'wb') as f:
            f.write(key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.TraditionalOpenSSL,
                encryption_algorithm=serialization.NoEncryption()
            ))
        os.chmod(key_path, 0o600)
        print(f"  └─ 私钥: {filename} (权限 600)")
    
    def save_certificate(self, cert, filename):
        """保存证书到文件 (PEM 格式, 权限 644)"""
        cert_path = os.path.join(self.output_dir, filename)
        with open(cert_path, 'wb') as f:
            f.write(cert.public_bytes(serialization.Encoding.PEM))
        os.chmod(cert_path, 0o644)
        print(f"  └─ 证书: {filename}")
    
    def build_subject_name(self, cert_config):
        """构建证书主题名 (Subject DN)"""
        name_attrs = []
        
        fields = {
            'Country': NameOID.COUNTRY_NAME,
            'State': NameOID.STATE_OR_PROVINCE_NAME,
            'Locality': NameOID.LOCALITY_NAME,
            'Organization': NameOID.ORGANIZATION_NAME,
            'OrganizationalUnit': NameOID.ORGANIZATIONAL_UNIT_NAME,
            'CommonName': NameOID.COMMON_NAME
        }
        
        for xml_tag, oid in fields.items():
            node = cert_config.find(xml_tag)
            if node is not None:
                name_attrs.append(x509.NameAttribute(oid, node.text))
        
        return x509.Name(name_attrs)
    
    def build_san_extension(self, san_config):
        """构建 Subject Alternative Name 扩展"""
        san_list = []
        if san_config is None:
            return None
        
        for dns in san_config.findall('DNS'):
            san_list.append(x509.DNSName(dns.text))
        
        for ip in san_config.findall('IP'):
            san_list.append(x509.IPAddress(ipaddress.ip_address(ip.text)))
        
        othername = san_config.find('OtherName')
        if othername is not None:
            oid = othername.find('OID').text
            val = othername.find('Value').text
            # 将字符串包装为 DER 编码的 UTF8String (Tag: 0x0C)
            # 格式: [0x0C] [Length] [Value]
            # 注意：这里仅支持长度小于 128 的字符串，符合 Mode S 地址需求
            encoded_val = b'\x0c' + len(val).to_bytes(1, 'big') + val.encode('utf-8')
            san_list.append(x509.OtherName(x509.ObjectIdentifier(oid), encoded_val))
            print(f"  ✓ 已添加 Mode S 地址 (OID: {oid}, Value: {val})")
        
        if san_list:
            return x509.SubjectAlternativeName(san_list)
        return None
    
    def generate_ca_certificate(self):
        """生成 CA 根证书"""
        print("\n[1/7] 生成 CA 根证书...")
        ca_config = self.config.find('CA')
        key_size = int(ca_config.find('KeySize').text)
        validity_days = int(ca_config.find('ValidityDays').text)
        prefix = ca_config.find('OutputPrefix').text
        
        self.ca_key = self.generate_private_key(key_size)
        subject = issuer = self.build_subject_name(ca_config)
        
        cert = (
            x509.CertificateBuilder()
            .subject_name(subject)
            .issuer_name(issuer)
            .public_key(self.ca_key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(datetime.utcnow())
            .not_valid_after(datetime.utcnow() + timedelta(days=validity_days))
            .add_extension(x509.BasicConstraints(ca=True, path_length=1), critical=True)
            .add_extension(x509.KeyUsage(
                digital_signature=False, content_commitment=False, key_encipherment=False,
                data_encipherment=False, key_agreement=False, key_cert_sign=True,
                crl_sign=True, encipher_only=False, decipher_only=False), critical=True)
            .add_extension(x509.SubjectKeyIdentifier.from_public_key(self.ca_key.public_key()), critical=False)
            .sign(self.ca_key, hashes.SHA256(), default_backend())
        )
        
        self.ca_cert = cert
        self.save_private_key(self.ca_key, f"{prefix}.key")
        self.save_certificate(cert, f"{prefix}.crt")
        print(f"  ✓ CA 证书生成完成")
    
    def generate_server_certificates(self):
        """生成服务器证书"""
        servers = self.config.find('Servers')
        if servers is None or len(servers) == 0:
            print("\n[2/7] 跳过服务器证书生成")
            return
        
        print(f"\n[2/7] 生成服务器证书...")
        for server in servers.findall('Server'):
            cn = server.find('CommonName').text
            print(f"  - {cn}")
            key_size = int(server.find('KeySize').text)
            validity_days = int(server.find('ValidityDays').text)
            prefix = server.find('OutputPrefix').text
            
            key = self.generate_private_key(key_size)
            builder = (
                x509.CertificateBuilder()
                .subject_name(self.build_subject_name(server))
                .issuer_name(self.ca_cert.subject)
                .public_key(key.public_key())
                .serial_number(x509.random_serial_number())
                .not_valid_before(datetime.utcnow())
                .not_valid_after(datetime.utcnow() + timedelta(days=validity_days))
                .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=False)
                .add_extension(x509.KeyUsage(
                    digital_signature=True, content_commitment=False, key_encipherment=True,
                    data_encipherment=False, key_agreement=False, key_cert_sign=False,
                    crl_sign=False, encipher_only=False, decipher_only=False), critical=True)
                .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
                .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(self.ca_key.public_key()), critical=False)
            )
            
            san = self.build_san_extension(server.find('SubjectAltNames'))
            if san: builder = builder.add_extension(san, critical=False)
            
            cert = builder.sign(self.ca_key, hashes.SHA256(), default_backend())
            self.save_private_key(key, f"{prefix}.key")
            self.save_certificate(cert, f"{prefix}.crt")
    
    def generate_client_certificates(self):
        """生成客户端证书"""
        clients = self.config.find('Clients')
        if clients is None or len(clients) == 0:
            print("\n[3/7] 跳过客户端证书生成")
            return
        
        print(f"\n[3/7] 生成客户端证书...")
        for client in clients.findall('Client'):
            cn = client.find('CommonName').text
            print(f"  - {cn}")
            key_size = int(client.find('KeySize').text)
            validity_days = int(client.find('ValidityDays').text)
            prefix = client.find('OutputPrefix').text
            
            key = self.generate_private_key(key_size)
            builder = (
                x509.CertificateBuilder()
                .subject_name(self.build_subject_name(client))
                .issuer_name(self.ca_cert.subject)
                .public_key(key.public_key())
                .serial_number(x509.random_serial_number())
                .not_valid_before(datetime.utcnow())
                .not_valid_after(datetime.utcnow() + timedelta(days=validity_days))
                .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=False)
                .add_extension(x509.KeyUsage(
                    digital_signature=True, content_commitment=False, key_encipherment=True,
                    data_encipherment=False, key_agreement=False, key_cert_sign=False,
                    crl_sign=False, encipher_only=False, decipher_only=False), critical=True)
                .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH]), critical=False)
                .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(self.ca_key.public_key()), critical=False)
            )
            
            san = self.build_san_extension(client.find('SubjectAltNames'))
            if san: builder = builder.add_extension(san, critical=False)
            
            cert = builder.sign(self.ca_key, hashes.SHA256(), default_backend())
            self.save_private_key(key, f"{prefix}.key")
            self.save_certificate(cert, f"{prefix}.crt")

    def update_acl_whitelist(self):
        """更新 freeDiameter ACL 白名单 (conf/acl_wl.conf)"""
        print("\n[4/7] 集成：更新 ACL 白名单...")
        acl_path = os.path.join(self.root_dir, "conf/acl_wl.conf")
        
        if not os.path.exists(acl_path):
            print(f"  ⚠️  警告: 未找到 ACL 文件 {acl_path}")
            return

        with open(acl_path, 'r') as f:
            content = f.read()

        new_entries = []
        clients = self.config.find('Clients')
        if clients:
            for client in clients.findall('Client'):
                identity = client.find('CommonName').text
                if identity not in content:
                    new_entries.append(identity)

        if new_entries:
            with open(acl_path, 'a') as f:
                if not content.endswith('\n'):
                    f.write('\n')
                f.write("# 自动添加的新客户端身份\n")
                for entry in new_entries:
                    f.write(f"{entry}\n")
                    print(f"  ✓ 已添加: {entry}")
        else:
            print("  ✓ ACL 已是最新的")

    def update_client_profile(self):
        """生成服务器端 MAGIC 业务策略 (Client_Profile.xml)"""
        print("\n[5/7] 集成：生成服务器端 MAGIC 业务策略...")
        
        # 输出到 output/server_profile/ 目录
        profile_path = os.path.join(self.server_profile_dir, "Client_Profile.xml")

        try:
            # 创建新的 XML 根节点
            root = ET.Element('ClientProfiles')
            
            # 添加 XML 声明的注释
            comment = ET.Comment(f' Auto-generated by cert_generator.py v1.2.2 at {datetime.now().strftime("%Y-%m-%d %H:%M:%S")} ')
            
            clients = self.config.find('Clients')
            if clients:
                for client in clients.findall('Client'):
                    identity = client.find('CommonName').text
                    ip_node = client.find('SubjectAltNames/IP')
                    source_ip = ip_node.text if ip_node is not None else ""

                    # 创建新的完整 Profile 节点
                    new_profile = ET.SubElement(root, 'ClientProfile')
                    ET.SubElement(new_profile, 'ProfileName').text = f"auto_profile_{identity.split('.')[0]}"
                    ET.SubElement(new_profile, 'ClientID').text = identity
                    ET.SubElement(new_profile, 'Description').text = f"自动生成的客户端策略: {identity}"
                    ET.SubElement(new_profile, 'Enabled').text = "true"
                    
                    # 认证配置
                    auth = ET.SubElement(new_profile, 'Auth')
                    ET.SubElement(auth, 'Username').text = identity.split('.')[0]
                    ET.SubElement(auth, 'ClientPassword').text = "magic123"
                    ET.SubElement(auth, 'ServerPassword').text = "1111"
                    if source_ip:
                        ET.SubElement(auth, 'SourceIP').text = source_ip
                    
                    # 带宽配置
                    bandwidth = ET.SubElement(new_profile, 'Bandwidth')
                    ET.SubElement(bandwidth, 'MaxForward').text = "20000000"
                    ET.SubElement(bandwidth, 'MaxReturn').text = "5000000"
                    ET.SubElement(bandwidth, 'GuaranteedForward').text = "2000000"
                    ET.SubElement(bandwidth, 'GuaranteedReturn').text = "1000000"
                    ET.SubElement(bandwidth, 'DefaultRequest').text = "5000000"
                    
                    # QoS 配置
                    qos = ET.SubElement(new_profile, 'QoS')
                    ET.SubElement(qos, 'PriorityType').text = "1"
                    ET.SubElement(qos, 'PriorityClass').text = "3"
                    ET.SubElement(qos, 'DefaultLevel').text = "2"
                    allowed_levels = ET.SubElement(qos, 'AllowedLevels')
                    ET.SubElement(allowed_levels, 'Level').text = "0"
                    ET.SubElement(allowed_levels, 'Level').text = "1"
                    ET.SubElement(allowed_levels, 'Level').text = "2"
                    ET.SubElement(qos, 'AllowedQoSCount').text = "3"
                    
                    # 链路策略
                    link_policy = ET.SubElement(new_profile, 'LinkPolicy')
                    allowed_dlms = ET.SubElement(link_policy, 'AllowedDLMs')
                    ET.SubElement(allowed_dlms, 'DLM').text = "LINK_SATCOM"
                    ET.SubElement(allowed_dlms, 'DLM').text = "LINK_WIFI"
                    ET.SubElement(allowed_dlms, 'DLM').text = "LINK_CELLULAR"
                    ET.SubElement(link_policy, 'AllowedDLMCount').text = "3"
                    ET.SubElement(link_policy, 'PreferredDLM').text = "LINK_WIFI"
                    ET.SubElement(link_policy, 'AllowMultiLink').text = "false"
                    ET.SubElement(link_policy, 'MaxConcurrentLinks').text = "1"
                    
                    # 会话配置
                    session = ET.SubElement(new_profile, 'Session')
                    ET.SubElement(session, 'Timeout').text = "3600"
                    ET.SubElement(session, 'IdleTimeout').text = "600"
                    ET.SubElement(session, 'MaxSessions').text = "5"
                    ET.SubElement(session, 'AutoReconnect').text = "true"
                    ET.SubElement(session, 'KeepRequestAllowed').text = "true"
                    ET.SubElement(session, 'AllowRegisteredClients').text = "true"
                    
                    # 流量配置
                    traffic = ET.SubElement(new_profile, 'Traffic')
                    
                    # 尝试从客户端配置中读取覆盖值
                    magic_cfg = client.find('MagicConfig')
                    def get_mcfg(tag, default):
                        if magic_cfg is not None and magic_cfg.find(tag) is not None:
                            return magic_cfg.find(tag).text
                        return default

                    ET.SubElement(traffic, 'AccountingEnabled').text = get_mcfg('AccountingEnabled', 'true')
                    ET.SubElement(traffic, 'MonthlyQuota').text = "0"
                    ET.SubElement(traffic, 'DailyQuota').text = "0"
                    
                    # TFT 白名单 - 从 MagicConfig 中读取允许的 TFT 规则
                    # 服务器会验证 MCCR 中的 TFT 是否在此列表中
                    tfts_whitelist = ET.SubElement(traffic, 'TFTs')
                    if magic_cfg is not None:
                        tfts_node = magic_cfg.find('TFTs')
                        if tfts_node is not None:
                            for tft_elem in tfts_node.findall('TFT'):
                                direction = tft_elem.get('direction', 'GROUND')
                                tft_text = tft_elem.text.strip()
                                whitelist_entry = ET.SubElement(tfts_whitelist, 'TFT')
                                whitelist_entry.set('direction', direction)
                                whitelist_entry.text = tft_text
                        else:
                            # 如果没有配置 TFT，添加默认白名单
                            default_tft_ground = ET.SubElement(tfts_whitelist, 'TFT')
                            default_tft_ground.set('direction', 'GROUND')
                            default_tft_ground.text = "_iTFT=,1,10,{client_ip}.255.255.255.255,10.2.2.8.255.255.255.255,6,80.80,0.65535"
                            default_tft_air = ET.SubElement(tfts_whitelist, 'TFT')
                            default_tft_air.set('direction', 'AIR')
                            default_tft_air.text = "_iTFT=,2,10,10.2.2.8.255.255.255.255,{client_ip}.255.255.255.255,6,0.65535,80.80"
                    
                    # 位置配置
                    location = ET.SubElement(new_profile, 'Location')
                    ET.SubElement(location, 'Enabled').text = "true"
                    flight_phases = ET.SubElement(location, 'AllowedFlightPhases')
                    for phase in ['GATE', 'TAXI', 'TAKE_OFF', 'CLIMB', 'CRUISE', 'DESCENT', 'APPROACH', 'LANDING']:
                        ET.SubElement(flight_phases, 'Phase').text = phase
                    ET.SubElement(location, 'FlightPhaseCount').text = "8"
                    ET.SubElement(location, 'MinAltitude').text = "0"
                    ET.SubElement(location, 'MaxAltitude').text = "60000"
                    ET.SubElement(location, 'AllowedAirports')
                    ET.SubElement(location, 'AirportCount').text = "0"
                    
                    print(f"  ✓ 已为 {identity} 创建完整业务策略")
            
            # 保存生成的 XML (带缩进)
            from xml.dom import minidom
            xml_str = minidom.parseString(ET.tostring(root)).toprettyxml(indent="    ")
            # 移除多余空行
            xml_str = "\n".join([line for line in xml_str.split('\n') if line.strip()])
            
            with open(profile_path, 'w', encoding='utf-8') as f:
                f.write(xml_str)
            
            print(f"  ✓ 已生成: server_profile/Client_Profile.xml")
            
            # 复制另外两个必需的 XML 配置文件
            import shutil
            source_config_dir = os.path.join(self.root_dir, "extensions/app_magic/config")
            for xml_file in ['Datalink_Profile.xml', 'Central_Policy_Profile.xml']:
                src = os.path.join(source_config_dir, xml_file)
                dst = os.path.join(self.server_profile_dir, xml_file)
                if os.path.exists(src):
                    shutil.copy2(src, dst)
                    print(f"  ✓ 已复制: server_profile/{xml_file}")
                else:
                    print(f"  ⚠ 未找到: {src}")
                
        except Exception as e:
            print(f"  ✗ 生成策略文件失败: {e}")
            import traceback
            traceback.print_exc()

    def generate_client_fd_configs(self):
        """为每个客户端生成 freeDiameter 配置文件 (.conf)"""
        print("\n[6/7] 集成：生成客户端 freeDiameter 配置文件...")
        clients = self.config.find('Clients')
        if not clients: return

        server_identity = "magic-server.example.com"
        server_ip = "192.168.126.1"
        
        servers = self.config.find('Servers')
        if servers and len(servers) > 0:
            s = servers.find('Server')
            server_identity = s.find('CommonName').text
            ip_node = s.find('SubjectAltNames/IP')
            if ip_node is not None: server_ip = ip_node.text

        for client in clients.findall('Client'):
            identity = client.find('CommonName').text
            prefix = client.find('OutputPrefix').text
            conf_path = os.path.join(self.client_fd_dir, f"fd_{prefix}.conf")
            
            # 读取端口配置（可选）
            client_port = 0
            client_secport = 0
            listen_on = ""  # 监听地址（空=所有接口）
            connect_to = server_ip  # 默认使用服务器IP
            server_port = 5870      # 默认服务器端口
            
            magic_cfg = client.find('MagicConfig')
            if magic_cfg is not None:
                port_node = magic_cfg.find('Port')
                if port_node is not None and port_node.text:
                    client_port = int(port_node.text.strip())
                secport_node = magic_cfg.find('SecPort')
                if secport_node is not None and secport_node.text:
                    client_secport = int(secport_node.text.strip())
                # 读取监听地址
                listen_node = magic_cfg.find('ListenOn')
                if listen_node is not None and listen_node.text and listen_node.text.strip():
                    listen_on = listen_node.text.strip()
                # 读取服务器连接地址和端口
                connect_node = magic_cfg.find('ServerConnectTo')
                if connect_node is not None and connect_node.text and connect_node.text.strip():
                    connect_to = connect_node.text.strip()
                server_port_node = magic_cfg.find('ServerPort')
                if server_port_node is not None and server_port_node.text:
                    server_port = int(server_port_node.text.strip())

            # 构建ListenOn配置行(避免f-string中使用反斜杠)
            if listen_on:
                listen_on_line = f'ListenOn = "{listen_on}";'
            else:
                listen_on_line = '# ListenOn = "0.0.0.0";  # 未配置，监听所有接口'

            conf_content = f"""# freeDiameter 配置文件 - {identity}
# 自动生成于 {datetime.now().strftime('%Y-%m-%d')}

Identity = "{identity}";
Realm = "example.com";

# 网络配置
No_SCTP;
No_IPv6;
{listen_on_line}
Port = {client_port};
SecPort = {client_secport};

# TLS 配置
TLS_Cred = "{os.path.abspath(os.path.join(self.output_dir, prefix + '.crt'))}", "{os.path.abspath(os.path.join(self.output_dir, prefix + '.key'))}";
TLS_CA = "{os.path.abspath(os.path.join(self.output_dir, 'ca.crt'))}";
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
TLS_DH_Bits = 2048;

# 加载字典
LoadExtension = "{os.path.join(self.root_dir, 'build/extensions/dict_magic_839.fdx')}";

# 2. 调试消息转储
LoadExtension = "{os.path.join(self.root_dir, 'build/extensions/dbg_msg_dumps.fdx')}" : "0x0080";

# 连接到服务器
ConnectPeer = "{server_identity}" {{ ConnectTo = "{connect_to}"; Port = {server_port}; }};
"""
            with open(conf_path, 'w') as f:
                f.write(conf_content)
            print(f"  ✓ 已生成: client_fd_config/fd_{prefix}.conf")

    def generate_server_fd_config(self):
        """生成服务器端的 freeDiameter 配置文件 (fd_server.conf)"""
        print("\n[7/7] 集成：生成服务器端 freeDiameter 配置文件...")
        
        server_identity = "magic-server.example.com"
        server_prefix = "magic-server"
        
        servers = self.config.find('Servers')
        if servers and len(servers) > 0:
            s = servers.find('Server')
            server_identity = s.find('CommonName').text
            server_prefix = s.find('OutputPrefix').text

        # 输出到 server_profile 目录
        conf_path = os.path.join(self.server_profile_dir, "fd_server.conf")
        
        # 获取绝对路径（用于证书和扩展）
        cert_path = os.path.abspath(os.path.join(self.output_dir, server_prefix + ".crt"))
        key_path = os.path.abspath(os.path.join(self.output_dir, server_prefix + ".key"))
        ca_path = os.path.abspath(os.path.join(self.output_dir, "ca.crt"))
        acl_path = os.path.abspath(os.path.join(self.root_dir, "conf/acl_wl.conf"))
        dict_path = os.path.abspath(os.path.join(self.root_dir, "build/extensions/dict_magic_839.fdx"))
        app_path = os.path.abspath(os.path.join(self.root_dir, "build/extensions/app_magic.fdx"))
        acl_ext_path = os.path.abspath(os.path.join(self.root_dir, "build/extensions/acl_wl.fdx"))
        
        # app_magic.fdx 需要的是目录路径（它会自动拼接 /Client_Profile.xml）
        profile_dir = os.path.abspath(self.server_profile_dir)

        conf_content = f"""# freeDiameter 服务器配置文件 - {server_identity}
# 自动生成于 {datetime.now().strftime('%Y-%m-%d')}

Identity = "{server_identity}";
Realm = "example.com";

# 网络配置
Port = 3869;
SecPort = 5870;
No_SCTP;
No_IPv6;
ListenOn = "0.0.0.0";

# TLS 配置
TLS_Cred = "{cert_path}", "{key_path}";
TLS_CA = "{ca_path}";
TLS_Prio = "SECURE256:+SECURE128:-VERS-TLS1.0:-VERS-TLS1.1:-MD5:-SHA1:-3DES-CBC:-ARCFOUR-128:%SAFE_RENEGOTIATION";
TLS_DH_Bits = 2048;

# 加载扩展
LoadExtension = "{dict_path}";
LoadExtension = "{app_path}" : "{profile_dir}";
LoadExtension = "{acl_ext_path}" : "{acl_path}";

# 日志配置
# 1=Fatal, 2=Error, 3=Warning, 4=Notice, 5=Info, 6=Debug
# LogLevel = 4;
"""
        with open(conf_path, 'w') as f:
            f.write(conf_content)
        print(f"  ✓ 已生成: server_profile/fd_server.conf")

    def generate_client_magic_configs(self):
        """生成客户端 MAGIC 业务配置文件 (*_magic.conf)"""
        print("\n[6] 生成客户端 MAGIC 业务配置文件...")
        
        clients = self.config.find('Clients')
        if clients is None:
            print("  ⚠ 未定义客户端，跳过")
            return
        
        # 获取服务器 Realm (从 Server 配置中提取)
        server = self.config.find('.//Servers/Server')
        if server is not None:
            server_cn = server.find('CommonName').text
            server_realm = '.'.join(server_cn.split('.')[1:]) if '.' in server_cn else 'example.com'
        else:
            server_realm = 'example.com'
        
        for client in clients.findall('Client'):
            cn = client.find('CommonName').text
            org = client.find('Organization').text
            prefix = client.find('OutputPrefix').text
            
            # 获取客户端 IP (从 SAN 中提取)
            client_ip = "192.168.126.5"  # 默认值
            san_element = client.find('SubjectAltNames')
            if san_element is not None:
                ip_element = san_element.find('IP')
                if ip_element is not None:
                    client_ip = ip_element.text
            
            # 从 XML 读取 MAGIC 业务配置参数（如果有的话）
            magic_cfg = client.find('MagicConfig')
            
            # 使用辅助函数读取配置，支持默认值
            def get_config(tag, default):
                if magic_cfg is not None and magic_cfg.find(tag) is not None:
                    return magic_cfg.find(tag).text
                return default
            
            # 读取所有业务参数
            tail_number = get_config('TailNumber', 'B-929A')
            aircraft_type = get_config('AircraftType', 'C929')
            profile_name = get_config('ProfileName', 'IP_DATA')
            
            requested_bw = get_config('RequestedBW', '12000000')
            requested_return_bw = get_config('RequestedReturnBW', '3000000')
            required_bw = get_config('RequiredBW', '4000000')
            required_return_bw = get_config('RequiredReturnBW', '1000000')
            max_bw = get_config('MaxBW', '20000000')
            
            qos_level = get_config('QoSLevel', '2')
            priority_class = get_config('PriorityClass', '6')
            priority = get_config('Priority', '8')
            cost_tolerance = get_config('CostTolerance', '2.0')
            
            username = get_config('Username', cn.split('.')[0] if '.' in cn else cn)
            client_password = get_config('ClientPassword', 'magic123')
            server_password = get_config('ServerPassword', '1111')
            
            timeout = get_config('Timeout', '900')
            keep_request = get_config('KeepRequest', '1')
            auto_detect = get_config('AutoDetect', '1')
            accounting_enabled = get_config('AccountingEnabled', '1')
            
            # 生成业务配置文件
            magic_conf_path = os.path.join(self.client_magic_dir, f"{prefix}_magic.conf")
            
            conf_content = f"""# ========================================================
# {prefix}_magic.conf
# ARINC 839-2014 MAGIC Diameter 客户端业务配置文件
# Auto-generated by cert_generator.py v1.2.2 at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
# ========================================================

# --------------------- 基本身份信息 ---------------------
CLIENT_ID           = {cn}                          # 设备唯一ID，必须与 freeDiameter Identity 一致
TAIL_NUMBER         = {tail_number}                 # 飞机注册号
AIRCRAFT_TYPE       = {aircraft_type}               # 机型

# --------------------- Diameter 路由信息 ---------------------
DESTINATION_REALM   = {server_realm}                # 地面服务器 Realm

# --------------------- 会话业务核心参数 ---------------------
PROFILE_NAME        = {profile_name}                # 会话类型：IP_DATA / VOICE / VIDEO_STREAM / SAFETY

REQUESTED_BW        = {requested_bw}                # 请求下行带宽
REQUESTED_RETURN_BW = {requested_return_bw}         # 请求上行带宽
REQUIRED_BW         = {required_bw}                 # 最低保障下行
REQUIRED_RETURN_BW  = {required_return_bw}          # 最低保障上行

QOS_LEVEL           = {qos_level}                   # QoS等级：0=BestEffort 1=Silver 2=Gold 3=Platinum
PRIORITY_CLASS      = {priority_class}              # 协议标准优先级：1~8

# --------------------- 历史兼容字段 ---------------------
MAX_BW              = {max_bw}                      # 最大可申请带宽
PRIORITY            = {priority}                    # 旧版优先级字段（1-10）
COST_TOLERANCE      = {cost_tolerance}              # 费用容忍度

# --------------------- 认证信息 ---------------------
USERNAME            = {username}                    # 用户名
CLIENT_PASSWORD     = {client_password}             # 客户端密码

SERVER_PASSWORD     = {server_password}             # 双向认证密码

# --------------------- 高级功能开关 ---------------------
TIMEOUT             = {timeout}                     # 会话超时时间（秒）
KEEP_REQUEST        = {keep_request}                # 保持会话（长连接）
AUTO_DETECT         = {auto_detect}                 # 自动探测可用链路
ACCOUNTING_ENABLED  = {accounting_enabled}          # 启用计费

# --------------------- 流量过滤规则（TFT） ---------------------
# 格式: _iTFT=[cid,[pf_id,precedence[,src_ip.mask[,dst_ip.mask[,proto[,dst_port[,src_port]]]]]]]
"""
            
            # 处理 TFT 规则
            tft_section = "# --------------------- 流量过滤规则（TFT） ---------------------\n"
            tft_section += "# 格式: _iTFT=[cid,[pf_id,precedence[,src_ip.mask[,dst_ip.mask[,proto[,dst_port[,src_port]]]]]]]\n"
            
            # 解析自定义 TFT 规则
            custom_tfts_ground = []
            custom_tfts_air = []
            if magic_cfg is not None:
                tfts_node = magic_cfg.find('TFTs')
                if tfts_node is not None:
                    for tft in tfts_node.findall('TFT'):
                        direction = tft.get('direction', 'GROUND')
                        tft_text = tft.text.strip()
                        
                        if direction == 'GROUND':
                            custom_tfts_ground.append(tft_text)
                        else:
                            custom_tfts_air.append(tft_text)
            
            # 生成 TFT 配置（使用自定义规则或默认规则）
            if not custom_tfts_ground:
                # 默认下行规则：TCP 80，任意源端口
                tft_section += f"# 地→机（下行）规则 (TCP Port 80，任意源端口）\nTFT_GROUND.1        = _iTFT=,1,10,{client_ip}.255.255.255.255,10.2.2.8.255.255.255.255,6,80.80,0.65535\n"
            else:
                for i, tft in enumerate(custom_tfts_ground):
                    tft_section += f"TFT_GROUND.{i+1}        = {tft}\n"
            
            tft_section += "\n"
            
            if not custom_tfts_air:
                # 默认上行规则：TCP 80，任意源端口
                tft_section += f"# 机→地（上行）规则 (TCP Port 80，任意源端口）\nTFT_AIR.1           = _iTFT=,2,10,10.2.2.8.255.255.255.255,{client_ip}.255.255.255.255,6,0.65535,80.80\n"
            else:
                for i, tft in enumerate(custom_tfts_air):
                    tft_section += f"TFT_AIR.{i+1}           = {tft}\n"

            # 处理 NAPT 规则 (ARINC 839 标准格式)
            napt_section = "\n# --------------------- NAPT 端口映射 (ARINC 839) ---------------------\n"
            napt_section += "# 格式: <NAT-Type>,<Source-IP>,<Destination-IP>,<IP-Protocol>,<Destination-Port>,<Source-Port>,<to-IP>,<to-Port>\n"
            
            custom_napts = []
            if magic_cfg is not None:
                napts_node = magic_cfg.find('NAPTs')
                if napts_node is not None:
                    for napt in napts_node.findall('NAPT'):
                        custom_napts.append(napt.text)
            
            if not custom_napts:
                # 默认生成一个 SNAT 规则示例
                napt_section += f"NAPT.1             = SNAT,{client_ip}.255.255.255.255,0.0.0.0.0.0.0.0,6,80,0.65535,%LinkIp%,8080\n"
            else:
                for i, napt in enumerate(custom_napts):
                    napt_section += f"NAPT.{i+1}             = {napt}\n"

            conf_content += tft_section + napt_section + f"""
# --------------------- 部署说明 ---------------------
# 1. 将本文件与 {prefix}.crt、{prefix}.key、ca.crt 一起部署到客户端
# 2. 使用命令启动客户端：
#    ./magic_client -c fd_{prefix}.conf -m {prefix}_magic.conf
# 3. 如需修改带宽、QoS 等参数，请编辑上述字段后重启客户端
"""
            
            with open(magic_conf_path, 'w') as f:
                f.write(conf_content)
            
            os.chmod(magic_conf_path, 0o600)
            print(f"  ✓ 已生成: client_magic_config/{prefix}_magic.conf (权限 600)")

    def run(self):
        """运行完整流程"""
        print("=" * 60)
        print("MAGIC 证书与配置集成生成工具 v1.2.2")
        print("=" * 60)
        
        if not self.load_config(): return False
        
        try:
            self.generate_ca_certificate()
            self.generate_server_certificates()
            self.generate_client_certificates()
            self.update_acl_whitelist()
            self.update_client_profile()
            self.generate_client_fd_configs()
            self.generate_client_magic_configs()
            self.generate_server_fd_config()
            
            print("\n" + "=" * 60)
            print("✓ 所有任务完成！")
            print("=" * 60)
            return True
        except Exception as e:
            print(f"\n✗ 运行失败: {e}")
            import traceback
            traceback.print_exc()
            return False


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python3 cert_generator.py <配置文件.xml>")
        sys.exit(1)
    
    generator = CertificateGenerator(sys.argv[1])
    sys.exit(0 if generator.run() else 1)
