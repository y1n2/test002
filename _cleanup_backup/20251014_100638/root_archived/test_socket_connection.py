#!/usr/bin/env python3
"""
Socket连接测试脚本
用于验证服务端和链路模拟器之间的socket连接
"""

import socket
import time
import json

def test_link_simulator_connection(host='127.0.0.1', ports=[8001, 8002, 8003, 8004]):
    """
    测试与链路模拟器的连接
    """
    link_types = ['Ethernet', 'WiFi', 'Cellular', 'Satellite']
    results = {}
    
    print("🔍 开始测试链路模拟器连接...")
    print("=" * 50)
    
    for i, port in enumerate(ports):
        link_type = link_types[i]
        print(f"\n📡 测试 {link_type} 链路 (端口 {port})...")
        
        try:
            # 创建socket连接
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)  # 5秒超时
            
            # 连接到链路模拟器
            start_time = time.time()
            sock.connect((host, port))
            connect_time = (time.time() - start_time) * 1000  # 转换为毫秒
            
            # 发送测试数据
            test_data = {
                "type": "test",
                "timestamp": time.time(),
                "link_type": link_type,
                "message": f"Hello from {link_type} link test"
            }
            
            message = json.dumps(test_data) + "\n"
            sock.send(message.encode('utf-8'))
            
            # 接收响应（如果有的话）
            sock.settimeout(2)
            try:
                response = sock.recv(1024)
                response_data = response.decode('utf-8').strip() if response else "No response"
            except socket.timeout:
                response_data = "Timeout - no response"
            
            sock.close()
            
            results[link_type] = {
                "status": "✅ 连接成功",
                "port": port,
                "connect_time": f"{connect_time:.2f}ms",
                "response": response_data
            }
            
            print(f"   ✅ 连接成功 - 延迟: {connect_time:.2f}ms")
            
        except socket.timeout:
            results[link_type] = {
                "status": "❌ 连接超时",
                "port": port,
                "error": "Connection timeout"
            }
            print(f"   ❌ 连接超时")
            
        except ConnectionRefused:
            results[link_type] = {
                "status": "❌ 连接被拒绝",
                "port": port,
                "error": "Connection refused"
            }
            print(f"   ❌ 连接被拒绝")
            
        except Exception as e:
            results[link_type] = {
                "status": "❌ 连接失败",
                "port": port,
                "error": str(e)
            }
            print(f"   ❌ 连接失败: {e}")
    
    return results

def print_summary(results):
    """
    打印测试结果摘要
    """
    print("\n" + "=" * 50)
    print("📊 测试结果摘要")
    print("=" * 50)
    
    successful_connections = 0
    total_connections = len(results)
    
    for link_type, result in results.items():
        print(f"\n🔗 {link_type} 链路:")
        print(f"   状态: {result['status']}")
        print(f"   端口: {result['port']}")
        
        if 'connect_time' in result:
            print(f"   连接时间: {result['connect_time']}")
            successful_connections += 1
        
        if 'error' in result:
            print(f"   错误: {result['error']}")
    
    print(f"\n📈 总体统计:")
    print(f"   成功连接: {successful_connections}/{total_connections}")
    print(f"   成功率: {(successful_connections/total_connections)*100:.1f}%")
    
    if successful_connections == total_connections:
        print("\n🎉 所有链路连接测试通过！")
        return True
    else:
        print(f"\n⚠️  有 {total_connections - successful_connections} 个链路连接失败")
        return False

if __name__ == "__main__":
    print("🚀 Socket连接测试工具")
    print("测试服务端与链路模拟器的socket连接")
    
    # 执行测试
    test_results = test_link_simulator_connection()
    
    # 打印结果
    success = print_summary(test_results)
    
    # 退出码
    exit(0 if success else 1)