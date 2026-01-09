#!/bin/bash

# MAGIC链路分配系统 - 测试环境自动化部署脚本
# 版本: 1.0
# 作者: MAGIC Team

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否为root用户
check_root() {
    if [[ $EUID -eq 0 ]]; then
        log_warning "建议不要使用root用户运行此脚本"
        read -p "是否继续? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# 检查操作系统
check_os() {
    log_info "检查操作系统..."
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            OS="ubuntu"
            log_success "检测到Ubuntu/Debian系统"
        elif command -v yum &> /dev/null; then
            OS="centos"
            log_success "检测到CentOS/RHEL系统"
        else
            log_error "不支持的Linux发行版"
            exit 1
        fi
    else
        log_error "不支持的操作系统: $OSTYPE"
        exit 1
    fi
}

# 安装基础依赖
install_dependencies() {
    log_info "安装基础依赖包..."
    
    if [[ "$OS" == "ubuntu" ]]; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            git \
            pkg-config \
            libsctp-dev \
            libgnutls28-dev \
            libgcrypt20-dev \
            flex \
            bison \
            tcpdump \
            wireshark-common \
            net-tools \
            htop \
            iotop \
            iftop \
            sysstat \
            iperf3 \
            iputils-ping \
            traceroute \
            mtr-tiny \
            python3 \
            python3-pip \
            python3-venv \
            jq \
            curl \
            wget
    elif [[ "$OS" == "centos" ]]; then
        sudo yum update -y
        sudo yum groupinstall -y "Development Tools"
        sudo yum install -y \
            cmake \
            git \
            pkgconfig \
            lksctp-tools-devel \
            gnutls-devel \
            libgcrypt-devel \
            flex \
            bison \
            tcpdump \
            wireshark \
            net-tools \
            htop \
            iotop \
            iftop \
            sysstat \
            iperf3 \
            iputils \
            traceroute \
            mtr \
            python3 \
            python3-pip \
            jq \
            curl \
            wget
    fi
    
    log_success "基础依赖安装完成"
}

# 配置网络环境
setup_network() {
    log_info "配置测试网络环境..."
    
    # 检查当前网络配置
    CURRENT_IP=$(ip route get 8.8.8.8 | grep -oP 'src \K\S+')
    log_info "当前主机IP: $CURRENT_IP"
    
    # 创建网络命名空间用于链路模拟
    if ! ip netns list | grep -q "link_sim"; then
        sudo ip netns add link_sim
        log_success "创建网络命名空间: link_sim"
    fi
    
    # 配置虚拟网络接口
    setup_virtual_interfaces
    
    # 配置路由表
    setup_routing_tables
    
    log_success "网络环境配置完成"
}

# 配置虚拟网络接口
setup_virtual_interfaces() {
    log_info "配置虚拟网络接口..."
    
    # 创建虚拟以太网对
    for i in {1..3}; do
        if ! ip link show veth${i} &> /dev/null; then
            sudo ip link add veth${i} type veth peer name veth${i}_sim
            sudo ip link set veth${i}_sim netns link_sim
            sudo ip link set veth${i} up
            sudo ip netns exec link_sim ip link set veth${i}_sim up
            log_success "创建虚拟接口对: veth${i} <-> veth${i}_sim"
        fi
    done
    
    # 配置IP地址
    sudo ip addr add 192.168.2.1/26 dev veth1 2>/dev/null || true      # 卫星链路
    sudo ip addr add 192.168.2.65/26 dev veth2 2>/dev/null || true     # 蜂窝链路  
    sudo ip addr add 192.168.2.129/26 dev veth3 2>/dev/null || true    # WiFi链路
    
    # 在命名空间中配置对端IP
    sudo ip netns exec link_sim ip addr add 192.168.2.2/26 dev veth1_sim 2>/dev/null || true
    sudo ip netns exec link_sim ip addr add 192.168.2.66/26 dev veth2_sim 2>/dev/null || true
    sudo ip netns exec link_sim ip addr add 192.168.2.130/26 dev veth3_sim 2>/dev/null || true
}

# 配置路由表
setup_routing_tables() {
    log_info "配置路由表..."
    
    # 创建自定义路由表
    if ! grep -q "satellite" /etc/iproute2/rt_tables; then
        echo "100 satellite" | sudo tee -a /etc/iproute2/rt_tables
        echo "101 cellular" | sudo tee -a /etc/iproute2/rt_tables  
        echo "102 wifi" | sudo tee -a /etc/iproute2/rt_tables
    fi
    
    # 配置路由规则
    sudo ip route add 192.168.2.0/26 dev veth1 table satellite 2>/dev/null || true
    sudo ip route add 192.168.2.64/26 dev veth2 table cellular 2>/dev/null || true
    sudo ip route add 192.168.2.128/26 dev veth3 table wifi 2>/dev/null || true
}

# 编译freeDiameter
build_freediameter() {
    log_info "编译freeDiameter..."
    
    cd /home/zhuwuhui/freeDiameter
    
    # 创建构建目录
    mkdir -p build
    cd build
    
    # 配置CMake
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTING=ON \
        -DDEFAULT_CONF_PATH=/etc/freeDiameter \
        -DINSTALL_LIBRARY_SUFFIX=""
    
    # 编译
    make -j$(nproc)
    
    # 安装
    sudo make install
    sudo ldconfig
    
    log_success "freeDiameter编译安装完成"
}

# 创建配置文件
create_config_files() {
    log_info "创建配置文件..."
    
    # 创建配置目录
    sudo mkdir -p /etc/freeDiameter
    sudo mkdir -p /var/log/freeDiameter
    sudo chown $USER:$USER /var/log/freeDiameter
    
    # 服务端配置文件
    create_server_config
    
    # 客户端配置文件
    create_client_config
    
    # 链路模拟器配置文件
    create_simulator_config
    
    log_success "配置文件创建完成"
}

# 创建服务端配置
create_server_config() {
    cat > /tmp/freeDiameter_server.conf << 'EOF'
# freeDiameter服务端配置文件
# MAGIC链路分配系统

# 基础配置
Identity = "server.magic.local";
Realm = "magic.local";
Port = 3868;
SecPort = 3869;

# 日志配置
LogLevel = 2;
LogFile = "/var/log/freeDiameter/fd_server.log";

# 监听地址
ListenOn = "192.168.1.10";

# 扩展配置
LoadExtension = "dict_base";
LoadExtension = "dict_eap";

# 对等节点配置
ConnectPeer = "client.magic.local" { ConnectTo = "192.168.1.20"; Port = 3868; };

# MAGIC链路分配扩展
LoadExtension = "magic_link_selection.so" {
    ConfigFile = "/etc/freeDiameter/magic_config.json";
    LogLevel = "DEBUG";
};
EOF

    sudo mv /tmp/freeDiameter_server.conf /etc/freeDiameter/
    log_success "服务端配置文件已创建"
}

# 创建客户端配置
create_client_config() {
    cat > /tmp/freeDiameter_client.conf << 'EOF'
# freeDiameter客户端配置文件
# MAGIC链路分配系统

# 基础配置
Identity = "client.magic.local";
Realm = "magic.local";
Port = 3868;

# 日志配置
LogLevel = 2;
LogFile = "/var/log/freeDiameter/fd_client.log";

# 监听地址
ListenOn = "192.168.1.20";

# 扩展配置
LoadExtension = "dict_base";
LoadExtension = "dict_eap";

# 对等节点配置
ConnectPeer = "server.magic.local" { ConnectTo = "192.168.1.10"; Port = 3868; };

# 客户端应用配置
LoadExtension = "magic_client_app.so" {
    TestMode = "true";
    LoadProfile = "/etc/freeDiameter/test_profiles.json";
};
EOF

    sudo mv /tmp/freeDiameter_client.conf /etc/freeDiameter/
    log_success "客户端配置文件已创建"
}

# 创建链路模拟器配置
create_simulator_config() {
    cat > /tmp/link_simulator.json << 'EOF'
{
    "simulator_config": {
        "listen_port": 8080,
        "log_level": "INFO",
        "log_file": "/var/log/freeDiameter/simulator.log"
    },
    "links": [
        {
            "id": "satellite",
            "name": "卫星链路",
            "interface": "veth1",
            "characteristics": {
                "latency_ms": 500,
                "bandwidth_mbps": 1,
                "packet_loss_rate": 0.001,
                "jitter_ms": 50,
                "cost_per_mb": 0.1
            },
            "variability": {
                "latency_variance": 0.1,
                "bandwidth_variance": 0.05,
                "loss_variance": 0.5
            }
        },
        {
            "id": "cellular",
            "name": "蜂窝链路",
            "interface": "veth2", 
            "characteristics": {
                "latency_ms": 50,
                "bandwidth_mbps": 10,
                "packet_loss_rate": 0.0001,
                "jitter_ms": 10,
                "cost_per_mb": 0.01
            },
            "variability": {
                "latency_variance": 0.2,
                "bandwidth_variance": 0.15,
                "loss_variance": 0.3
            }
        },
        {
            "id": "wifi",
            "name": "WiFi链路",
            "interface": "veth3",
            "characteristics": {
                "latency_ms": 10,
                "bandwidth_mbps": 100,
                "packet_loss_rate": 0.00001,
                "jitter_ms": 2,
                "cost_per_mb": 0.001
            },
            "variability": {
                "latency_variance": 0.3,
                "bandwidth_variance": 0.2,
                "loss_variance": 0.2
            }
        }
    ],
    "fault_injection": {
        "enabled": true,
        "scenarios": [
            {
                "name": "satellite_outage",
                "description": "卫星链路中断",
                "target_link": "satellite",
                "fault_type": "complete_failure",
                "duration_seconds": 60
            },
            {
                "name": "cellular_degradation", 
                "description": "蜂窝链路性能下降",
                "target_link": "cellular",
                "fault_type": "performance_degradation",
                "parameters": {
                    "latency_multiplier": 3,
                    "bandwidth_multiplier": 0.3,
                    "loss_multiplier": 10
                },
                "duration_seconds": 120
            }
        ]
    }
}
EOF

    sudo mv /tmp/link_simulator.json /etc/freeDiameter/
    log_success "链路模拟器配置文件已创建"
}

# 创建MAGIC配置文件
create_magic_config() {
    cat > /tmp/magic_config.json << 'EOF'
{
    "magic_link_selection": {
        "algorithm": {
            "type": "weighted_scoring",
            "update_interval_ms": 1000,
            "decision_timeout_ms": 10
        },
        "scoring_weights": {
            "flight_phases": {
                "takeoff": {
                    "latency": 0.5,
                    "reliability": 0.3,
                    "bandwidth": 0.15,
                    "cost": 0.05
                },
                "cruise": {
                    "latency": 0.2,
                    "reliability": 0.2,
                    "bandwidth": 0.3,
                    "cost": 0.3
                },
                "landing": {
                    "latency": 0.4,
                    "reliability": 0.4,
                    "bandwidth": 0.15,
                    "cost": 0.05
                }
            }
        },
        "constraints": {
            "hard_constraints": {
                "max_latency_ms": 1000,
                "min_bandwidth_mbps": 0.1,
                "max_cost_per_mb": 1.0
            },
            "soft_constraints": {
                "preferred_latency_ms": 100,
                "preferred_bandwidth_mbps": 10,
                "preferred_cost_per_mb": 0.01
            }
        },
        "switching": {
            "threshold_percentage": 15,
            "min_dwell_time_seconds": 30,
            "cooldown_time_seconds": 60,
            "staged_migration": {
                "enabled": true,
                "stages": [30, 70, 100]
            }
        },
        "monitoring": {
            "kpi_collection_interval_ms": 500,
            "metrics_export_interval_ms": 5000,
            "health_check_interval_ms": 2000
        }
    }
}
EOF

    sudo mv /tmp/magic_config.json /etc/freeDiameter/
    log_success "MAGIC配置文件已创建"
}

# 设置Python测试环境
setup_python_env() {
    log_info "设置Python测试环境..."
    
    # 创建虚拟环境
    cd /home/zhuwuhui/freeDiameter
    python3 -m venv venv
    source venv/bin/activate
    
    # 安装Python依赖
    pip install --upgrade pip
    pip install \
        pytest \
        pytest-html \
        pytest-cov \
        requests \
        psutil \
        numpy \
        matplotlib \
        pandas \
        scapy \
        paramiko \
        pexpect \
        jsonschema
    
    # 创建requirements.txt
    pip freeze > requirements.txt
    
    log_success "Python测试环境设置完成"
}

# 创建测试脚本
create_test_scripts() {
    log_info "创建测试脚本..."
    
    mkdir -p tests/{unit,integration,performance,scenario,utils}
    
    # 创建基础测试工具
    create_test_utils
    
    # 创建示例测试
    create_sample_tests
    
    log_success "测试脚本创建完成"
}

# 创建测试工具
create_test_utils() {
    cat > tests/utils/test_env_setup.py << 'EOF'
#!/usr/bin/env python3
"""
测试环境设置工具
"""

import os
import sys
import time
import subprocess
import json
import logging
from pathlib import Path

class TestEnvironment:
    def __init__(self):
        self.base_dir = Path(__file__).parent.parent.parent
        self.config_dir = Path("/etc/freeDiameter")
        self.log_dir = Path("/var/log/freeDiameter")
        
    def setup_logging(self):
        """设置日志"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler(self.log_dir / 'test_setup.log'),
                logging.StreamHandler()
            ]
        )
        return logging.getLogger(__name__)
    
    def check_prerequisites(self):
        """检查测试前提条件"""
        logger = self.setup_logging()
        
        # 检查freeDiameter是否安装
        try:
            result = subprocess.run(['freeDiameterd', '--version'], 
                                  capture_output=True, text=True)
            logger.info(f"freeDiameter版本: {result.stdout.strip()}")
        except FileNotFoundError:
            logger.error("freeDiameter未安装")
            return False
            
        # 检查配置文件
        required_configs = [
            'freeDiameter_server.conf',
            'freeDiameter_client.conf', 
            'link_simulator.json',
            'magic_config.json'
        ]
        
        for config in required_configs:
            config_path = self.config_dir / config
            if not config_path.exists():
                logger.error(f"配置文件不存在: {config_path}")
                return False
            logger.info(f"配置文件检查通过: {config}")
        
        # 检查网络接口
        try:
            result = subprocess.run(['ip', 'link', 'show'], 
                                  capture_output=True, text=True)
            if 'veth1' not in result.stdout:
                logger.error("虚拟网络接口未配置")
                return False
            logger.info("网络接口检查通过")
        except Exception as e:
            logger.error(f"网络检查失败: {e}")
            return False
            
        return True
    
    def start_services(self):
        """启动测试服务"""
        logger = self.setup_logging()
        
        # 启动freeDiameter服务端
        try:
            server_proc = subprocess.Popen([
                'freeDiameterd',
                '-c', str(self.config_dir / 'freeDiameter_server.conf'),
                '-f'
            ])
            logger.info(f"freeDiameter服务端已启动 (PID: {server_proc.pid})")
            time.sleep(2)  # 等待服务启动
        except Exception as e:
            logger.error(f"启动服务端失败: {e}")
            return False
            
        return True
    
    def cleanup(self):
        """清理测试环境"""
        logger = self.setup_logging()
        
        # 停止所有freeDiameter进程
        try:
            subprocess.run(['pkill', '-f', 'freeDiameterd'], check=False)
            logger.info("已停止freeDiameter进程")
        except Exception as e:
            logger.warning(f"停止进程时出错: {e}")

if __name__ == "__main__":
    env = TestEnvironment()
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "check":
            success = env.check_prerequisites()
            sys.exit(0 if success else 1)
        elif sys.argv[1] == "start":
            success = env.start_services()
            sys.exit(0 if success else 1)
        elif sys.argv[1] == "cleanup":
            env.cleanup()
            sys.exit(0)
    else:
        print("用法: python test_env_setup.py [check|start|cleanup]")
        sys.exit(1)
EOF

    chmod +x tests/utils/test_env_setup.py
}

# 创建示例测试
create_sample_tests() {
    # 单元测试示例
    cat > tests/unit/test_basic.py << 'EOF'
#!/usr/bin/env python3
"""
基础功能单元测试
"""

import pytest
import subprocess
import time

class TestBasicFunctionality:
    
    def test_freediameter_version(self):
        """测试freeDiameter版本"""
        result = subprocess.run(['freeDiameterd', '--version'], 
                              capture_output=True, text=True)
        assert result.returncode == 0
        assert 'freeDiameter' in result.stdout
    
    def test_config_file_syntax(self):
        """测试配置文件语法"""
        result = subprocess.run([
            'freeDiameterd', 
            '-c', '/etc/freeDiameter/freeDiameter_server.conf',
            '-C'  # 只检查配置，不启动
        ], capture_output=True, text=True)
        assert result.returncode == 0
    
    def test_network_interfaces(self):
        """测试网络接口"""
        result = subprocess.run(['ip', 'link', 'show'], 
                              capture_output=True, text=True)
        assert 'veth1' in result.stdout
        assert 'veth2' in result.stdout
        assert 'veth3' in result.stdout

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
EOF

    # 集成测试示例
    cat > tests/integration/test_diameter_connection.py << 'EOF'
#!/usr/bin/env python3
"""
Diameter连接集成测试
"""

import pytest
import subprocess
import time
import signal
import os

class TestDiameterConnection:
    
    @pytest.fixture(scope="class")
    def diameter_server(self):
        """启动Diameter服务端"""
        proc = subprocess.Popen([
            'freeDiameterd',
            '-c', '/etc/freeDiameter/freeDiameter_server.conf',
            '-f'
        ])
        time.sleep(3)  # 等待服务启动
        yield proc
        proc.terminate()
        proc.wait()
    
    def test_server_startup(self, diameter_server):
        """测试服务端启动"""
        assert diameter_server.poll() is None  # 进程仍在运行
    
    def test_port_listening(self, diameter_server):
        """测试端口监听"""
        result = subprocess.run(['netstat', '-ln'], 
                              capture_output=True, text=True)
        assert ':3868' in result.stdout
    
    def test_client_connection(self, diameter_server):
        """测试客户端连接"""
        # 启动客户端
        client_proc = subprocess.Popen([
            'freeDiameterd',
            '-c', '/etc/freeDiameter/freeDiameter_client.conf',
            '-f'
        ])
        
        try:
            time.sleep(5)  # 等待连接建立
            
            # 检查连接状态
            # 这里应该检查实际的连接状态，可能需要查看日志或使用专门的工具
            assert client_proc.poll() is None
            
        finally:
            client_proc.terminate()
            client_proc.wait()

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
EOF
}

# 创建启动脚本
create_startup_scripts() {
    log_info "创建启动脚本..."
    
    # 服务端启动脚本
    cat > /tmp/start_server.sh << 'EOF'
#!/bin/bash
# MAGIC链路分配系统 - 服务端启动脚本

cd /home/zhuwuhui/freeDiameter

# 检查配置文件
if [ ! -f /etc/freeDiameter/freeDiameter_server.conf ]; then
    echo "错误: 服务端配置文件不存在"
    exit 1
fi

# 创建日志目录
mkdir -p /var/log/freeDiameter

# 启动服务端
echo "启动freeDiameter服务端..."
freeDiameterd -c /etc/freeDiameter/freeDiameter_server.conf -f
EOF

    # 客户端启动脚本
    cat > /tmp/start_client.sh << 'EOF'
#!/bin/bash
# MAGIC链路分配系统 - 客户端启动脚本

cd /home/zhuwuhui/freeDiameter

# 检查配置文件
if [ ! -f /etc/freeDiameter/freeDiameter_client.conf ]; then
    echo "错误: 客户端配置文件不存在"
    exit 1
fi

# 等待服务端启动
echo "等待服务端启动..."
sleep 5

# 启动客户端
echo "启动freeDiameter客户端..."
freeDiameterd -c /etc/freeDiameter/freeDiameter_client.conf -f
EOF

    # 链路模拟器启动脚本
    cat > /tmp/start_simulator.sh << 'EOF'
#!/bin/bash
# MAGIC链路分配系统 - 链路模拟器启动脚本

cd /home/zhuwuhui/freeDiameter

# 激活Python虚拟环境
source venv/bin/activate

# 启动链路模拟器
echo "启动链路模拟器..."
python3 tools/link_simulator.py -c /etc/freeDiameter/link_simulator.json
EOF

    # 移动脚本到正确位置并设置权限
    sudo mv /tmp/start_server.sh /usr/local/bin/
    sudo mv /tmp/start_client.sh /usr/local/bin/
    sudo mv /tmp/start_simulator.sh /usr/local/bin/
    
    sudo chmod +x /usr/local/bin/start_server.sh
    sudo chmod +x /usr/local/bin/start_client.sh
    sudo chmod +x /usr/local/bin/start_simulator.sh
    
    log_success "启动脚本创建完成"
}

# 运行基础测试
run_basic_tests() {
    log_info "运行基础测试..."
    
    cd /home/zhuwuhui/freeDiameter
    source venv/bin/activate
    
    # 运行环境检查
    python3 tests/utils/test_env_setup.py check
    if [ $? -ne 0 ]; then
        log_error "环境检查失败"
        return 1
    fi
    
    # 运行单元测试
    python3 -m pytest tests/unit/ -v
    if [ $? -ne 0 ]; then
        log_warning "部分单元测试失败，请检查"
    fi
    
    log_success "基础测试完成"
}

# 显示使用说明
show_usage() {
    cat << 'EOF'

=================================================================
           MAGIC链路分配系统 - 测试环境部署完成
=================================================================

环境信息:
- 配置目录: /etc/freeDiameter/
- 日志目录: /var/log/freeDiameter/
- 项目目录: /home/zhuwuhui/freeDiameter/

启动命令:
- 服务端: start_server.sh
- 客户端: start_client.sh  
- 模拟器: start_simulator.sh

测试命令:
- 单元测试: cd /home/zhuwuhui/freeDiameter && source venv/bin/activate && python -m pytest tests/unit/
- 集成测试: cd /home/zhuwuhui/freeDiameter && source venv/bin/activate && python -m pytest tests/integration/
- 环境检查: cd /home/zhuwuhui/freeDiameter && python tests/utils/test_env_setup.py check

网络配置:
- 管理网络: 192.168.1.0/24
- 卫星链路: 192.168.2.0/26 (veth1)
- 蜂窝链路: 192.168.2.64/26 (veth2)
- WiFi链路: 192.168.2.128/26 (veth3)

下一步:
1. 编译MAGIC链路选择扩展模块
2. 运行完整的集成测试
3. 配置性能监控和日志分析

=================================================================
EOF
}

# 主函数
main() {
    log_info "开始部署MAGIC链路分配系统测试环境..."
    
    # 检查权限
    check_root
    
    # 检查操作系统
    check_os
    
    # 安装依赖
    install_dependencies
    
    # 配置网络
    setup_network
    
    # 编译freeDiameter
    build_freediameter
    
    # 创建配置文件
    create_config_files
    create_magic_config
    
    # 设置Python环境
    setup_python_env
    
    # 创建测试脚本
    create_test_scripts
    
    # 创建启动脚本
    create_startup_scripts
    
    # 运行基础测试
    run_basic_tests
    
    # 显示使用说明
    show_usage
    
    log_success "测试环境部署完成！"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi