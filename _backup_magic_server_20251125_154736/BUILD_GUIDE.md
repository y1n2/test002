# MAGIC Server 编译指南

## 📦 编译环境要求

### 必需依赖
```bash
# Ubuntu/Debian
sudo apt-get install -y \
    build-essential \
    cmake \
    libxml2-dev \
    pkg-config

# CentOS/RHEL
sudo yum install -y \
    gcc \
    make \
    cmake \
    libxml2-devel \
    pkgconfig
```

### 可选依赖
- `pthread`: POSIX 线程库（通常已安装）
- `libm`: 数学库（通常已安装）

---

## 🔨 编译步骤

### 方法一：标准编译（推荐）

```bash
cd /home/zhuwuhui/freeDiameter/magic_server

# 1. 创建 build 目录（如果不存在）
mkdir -p build

# 2. 进入 build 目录
cd build

# 3. 运行 CMake 配置
cmake ..

# 4. 编译（使用所有 CPU 核心）
make -j$(nproc)

# 5. 验证生成的可执行文件
ls -lh cm_core_simple magic_core_main test_*
```

### 方法二：清理后重新编译

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 清理旧的编译文件
rm -rf *

# 重新配置和编译
cmake .. && make -j$(nproc)
```

### 方法三：调试模式编译

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 配置为 Debug 模式（包含调试符号，禁用优化）
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 编译
make -j$(nproc)
```

### 方法四：Release 模式编译

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 配置为 Release 模式（启用 -O3 优化）
cmake -DCMAKE_BUILD_TYPE=Release ..

# 编译
make -j$(nproc)
```

---

## 📂 生成的可执行文件

编译成功后，在 `build/` 目录下会生成以下文件：

| 文件名 | 大小 | 用途 |
|--------|------|------|
| **cm_core_simple** | ~49 KB | 简化版 CM Core 服务器，用于 DLM 联调 |
| **magic_core_main** | ~95 KB | 完整的 MAGIC Core 主程序 |
| **test_xml_parser** | ~49 KB | XML 配置解析器测试程序 |
| **test_policy_engine** | ~93 KB | 策略引擎测试程序 |

---

## ✅ 验证编译结果

### 检查可执行文件

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 检查文件是否存在且可执行
file cm_core_simple magic_core_main

# 预期输出:
# cm_core_simple:   ELF 64-bit LSB executable, x86-64, ...
# magic_core_main:  ELF 64-bit LSB executable, x86-64, ...
```

### 检查依赖库

```bash
# 检查动态库依赖
ldd magic_core_main

# 预期输出应包含:
# libxml2.so.2 => /usr/lib/x86_64-linux-gnu/libxml2.so.2
# libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0
# libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6
```

### 运行基础测试

```bash
# 测试 XML 解析器
./test_xml_parser

# 测试策略引擎
./test_policy_engine
```

---

## 🚀 运行编译后的程序

### 启动 CM Core Simple（简单版）

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 前台运行（可以看到输出）
./cm_core_simple

# 后台运行（输出到日志文件）
./cm_core_simple > ../../logs/cm_core.log 2>&1 &
```

**预期输出**:
```
==========================================
  CM Core Server - Simple Version
==========================================
[CM CORE] Unix socket server created: /tmp/magic_core.sock
[CM CORE] Waiting for DLM connections...
```

### 启动 MAGIC Core Main（完整版）

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build

# 前台运行
./magic_core_main

# 后台运行
./magic_core_main > ../../logs/magic_core.log 2>&1 &
```

**预期输出**:
```
╔════════════════════════════════════════╗
║      MAGIC Core System v1.0            ║
║  Multi-link Aggregation Gateway        ║
╚════════════════════════════════════════╝

[MAGIC CORE] Loading configuration files...
[MAGIC CORE] ✓ Loaded 3 datalink profiles
[MAGIC CORE] ✓ Loaded 9 policy rulesets
[MAGIC CORE] ✓ Loaded 4 client profiles
[MAGIC CORE] CM Core server listening on /tmp/magic_core.sock
[MAGIC CORE] Waiting for DLM connections...
```

---

## 🐛 常见编译问题

### 问题 1: CMake 找不到

**错误信息**:
```
bash: cmake: command not found
```

**解决方法**:
```bash
sudo apt-get install cmake
```

---

### 问题 2: 找不到 libxml2

**错误信息**:
```
Package libxml-2.0 was not found in the pkg-config search path
```

**解决方法**:
```bash
sudo apt-get install libxml2-dev pkg-config
```

---

### 问题 3: pthread 相关错误

**错误信息**:
```
undefined reference to `pthread_create'
```

**解决方法**:
CMakeLists.txt 已经包含了 `Threads::Threads` 链接。如果仍有问题：
```bash
# 重新运行 CMake
cd build
rm CMakeCache.txt
cmake ..
make
```

---

### 问题 4: MAX_NAME_LEN 未定义

**错误信息**:
```
error: 'MAX_NAME_LEN' undeclared here
```

**解决方法**:
这个问题已经修复。确保使用 `MAX_IPC_NAME_LEN` 而不是 `MAX_NAME_LEN`。

---

### 问题 5: 编译警告

**警告示例**:
```
warning: cast between incompatible function types
warning: unused parameter 'hb'
warning: '%s' directive output may be truncated
```

**说明**: 这些是非致命警告，不影响程序运行。可以忽略或在代码中修复：
- 函数类型转换警告：可以通过添加中间函数解决
- 未使用参数警告：可以用 `(void)param;` 消除
- 格式化截断警告：可以增大缓冲区大小

---

## 🔧 CMake 配置选项

### 查看所有 CMake 变量

```bash
cd /home/zhuwuhui/freeDiameter/magic_server/build
cmake .. -LAH
```

### 自定义安装路径

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/magic_server ..
make
sudo make install
```

### 指定编译器

```bash
# 使用 GCC
cmake -DCMAKE_C_COMPILER=gcc ..

# 使用 Clang
cmake -DCMAKE_C_COMPILER=clang ..
```

### 启用详细编译输出

```bash
make VERBOSE=1
```

---

## 📊 编译统计

成功编译后：

- **编译时间**: ~10-15 秒（取决于 CPU）
- **生成文件数**: 4 个可执行文件
- **总大小**: ~290 KB
- **依赖库**: libxml2, pthread, libm

---

## 🎯 下一步

编译完成后，您可以：

1. **运行测试**: 参考 `SYSTEM_INTEGRATION_TEST_MANUAL.md`
2. **启动系统**: 使用 `start_all.sh` 脚本
3. **查看日志**: `tail -f ../../logs/*.log`
4. **调试程序**: `gdb ./magic_core_main`

---

## 📚 相关文档

- `SYSTEM_INTEGRATION_TEST_MANUAL.md` - 系统集成测试手册
- `README_CODE_STRUCTURE.md` - 代码结构文档
- `../MAGIC_CLIENT_TEST_GUIDE.md` - 客户端测试指南

---

**文档版本**: 1.0  
**最后更新**: 2025-11-25  
**维护者**: MAGIC 开发组
