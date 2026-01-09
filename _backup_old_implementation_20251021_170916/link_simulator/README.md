# 链路模拟器使用说明

## 简介

链路模拟器是为freeDiameter服务器提供的一个模拟链路连接工具，可以模拟多个链路连接，并处理基本的数据交换。

## 功能特点

- 支持最多10个并发链路连接
- 自动分配链路ID
- 实时显示链路状态
- 支持数据内容的十六进制显示
- 智能响应处理

## 编译与安装

```bash
cd /home/zhuwuhui/freeDiameter/link_simulator
make
make install  # 将可执行文件安装到/home/zhuwuhui/freeDiameter/exe目录
```

## 使用方法

```bash
./link_simulator [选项]
```

### 命令行选项

- `-p PORT`: 指定监听端口（默认：30000）
- `-h`: 显示帮助信息
- `-v`: 启用详细日志模式

### 示例

1. 使用默认配置启动链路模拟器：
   ```bash
   ./link_simulator
   ```

2. 指定端口启动链路模拟器：
   ```bash
   ./link_simulator -p 30001
   ```

3. 启用详细日志模式：
   ```bash
   ./link_simulator -v
   ```

## 与freeDiameter服务器连接

freeDiameter服务器的magic_server扩展模块会尝试连接到链路模拟器。确保链路模拟器在freeDiameter服务器启动前已经运行。

## 链路状态监控

链路模拟器每5秒会显示一次当前所有链路的状态，包括链路ID和连接状态。