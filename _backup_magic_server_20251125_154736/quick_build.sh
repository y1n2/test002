#!/bin/bash
# MAGIC Server 快速编译脚本

set -e  # 遇到错误立即退出

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$BASE_DIR/build"

echo "=========================================="
echo "  MAGIC Server 快速编译"
echo "=========================================="
echo ""

# 1. 检查依赖
echo "[1/4] 检查编译依赖..."
if ! command -v cmake &> /dev/null; then
    echo "✗ 错误: 未找到 cmake"
    echo "  请运行: sudo apt-get install cmake"
    exit 1
fi

if ! pkg-config --exists libxml-2.0; then
    echo "✗ 错误: 未找到 libxml2"
    echo "  请运行: sudo apt-get install libxml2-dev pkg-config"
    exit 1
fi

echo "  ✓ cmake 已安装"
echo "  ✓ libxml2 已安装"
echo ""

# 2. 创建 build 目录
echo "[2/4] 准备 build 目录..."
mkdir -p "$BUILD_DIR"
echo "  ✓ build 目录已准备"
echo ""

# 3. 运行 CMake
echo "[3/4] 配置 CMake..."
cd "$BUILD_DIR"
if cmake .. > cmake_output.log 2>&1; then
    echo "  ✓ CMake 配置成功"
else
    echo "  ✗ CMake 配置失败"
    echo "  查看日志: $BUILD_DIR/cmake_output.log"
    exit 1
fi
echo ""

# 4. 编译
echo "[4/4] 编译中..."
CPU_COUNT=$(nproc)
echo "  使用 $CPU_COUNT 个 CPU 核心并行编译..."

if make -j$CPU_COUNT > make_output.log 2>&1; then
    echo "  ✓ 编译成功！"
else
    echo "  ✗ 编译失败"
    echo "  查看日志: $BUILD_DIR/make_output.log"
    tail -20 make_output.log
    exit 1
fi
echo ""

# 5. 显示结果
echo "=========================================="
echo "  编译完成！"
echo "=========================================="
echo ""
echo "生成的可执行文件:"
ls -lh "$BUILD_DIR" | grep -E "cm_core_simple|magic_core_main|test_" | awk '{printf "  %-25s %6s\n", $9, $5}'
echo ""
echo "下一步:"
echo "  1. 运行 CM Core:       cd build && ./cm_core_simple"
echo "  2. 运行 MAGIC Core:    cd build && ./magic_core_main"
echo "  3. 运行测试:           cd build && ./test_xml_parser"
echo "  4. 查看测试手册:       cat ../SYSTEM_INTEGRATION_TEST_MANUAL.md"
echo ""
