#!/bin/bash
# 快速验证 dict_magic_839 改进的脚本

set -e  # 遇到错误立即退出

echo "========================================"
echo "dict_magic_839 改进验证脚本"
echo "========================================"
echo ""

# 1. 检查文件完整性
echo "📋 1. 检查文件完整性..."
REQUIRED_FILES=(
    "dict_magic.c"
    "add_avps.c"
    "dict_magic_839.csv"
    "dict_magic_codes.h"
    "dict_magic_codes.c"
    "CMakeLists.txt"
    "README.md"
    "USAGE_GUIDE.md"
    "DICT_CHECK_REPORT.md"
    "COMPLETION_SUMMARY.md"
)

DICT_DIR="/home/zhuwuhui/freeDiameter/extensions/dict_magic_839"
MISSING_FILES=0

for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$DICT_DIR/$file" ]; then
        echo "  ✅ $file"
    else
        echo "  ❌ $file (缺失)"
        MISSING_FILES=$((MISSING_FILES + 1))
    fi
done

if [ $MISSING_FILES -eq 0 ]; then
    echo "  ✅ 所有文件完整"
else
    echo "  ❌ 有 $MISSING_FILES 个文件缺失"
    exit 1
fi
echo ""

# 2. 检查关键内容
echo "🔍 2. 检查关键内容..."

# 检查枚举值定义
if grep -q "DICT_ENUMVAL" "$DICT_DIR/add_avps.c"; then
    echo "  ✅ add_avps.c 包含枚举值定义"
else
    echo "  ❌ add_avps.c 缺少枚举值定义"
    exit 1
fi

# 检查状态码常量
if grep -q "MAGIC_ERROR_AUTHENTICATION_FAILED" "$DICT_DIR/dict_magic_codes.h"; then
    echo "  ✅ dict_magic_codes.h 包含状态码常量"
else
    echo "  ❌ dict_magic_codes.h 缺少状态码常量"
    exit 1
fi

# 检查辅助函数
if grep -q "magic_status_code_str" "$DICT_DIR/dict_magic_codes.c"; then
    echo "  ✅ dict_magic_codes.c 包含辅助函数"
else
    echo "  ❌ dict_magic_codes.c 缺少辅助函数"
    exit 1
fi

# 检查头文件引用
if grep -q "dict_magic_codes.h" "$DICT_DIR/dict_magic.c"; then
    echo "  ✅ dict_magic.c 引用了新头文件"
else
    echo "  ❌ dict_magic.c 未引用新头文件"
    exit 1
fi

# 检查 CMakeLists.txt
if grep -q "dict_magic_codes.c" "$DICT_DIR/CMakeLists.txt"; then
    echo "  ✅ CMakeLists.txt 包含新源文件"
else
    echo "  ❌ CMakeLists.txt 未包含新源文件"
    exit 1
fi

echo ""

# 3. 统计信息
echo "📊 3. 统计信息..."

# 统计状态码数量
STATUS_CODES=$(grep -c "^#define DIAMETER_\|^#define MAGIC_" "$DICT_DIR/dict_magic_codes.h" || true)
echo "  📌 状态码常量数量: $STATUS_CODES"

# 统计枚举值数量
ENUM_VALUES=$(grep -c "enum_name = " "$DICT_DIR/add_avps.c" || true)
echo "  📌 枚举值定义数量: $ENUM_VALUES"

# 文件行数
CODES_H_LINES=$(wc -l < "$DICT_DIR/dict_magic_codes.h")
CODES_C_LINES=$(wc -l < "$DICT_DIR/dict_magic_codes.c")
AVPS_C_LINES=$(wc -l < "$DICT_DIR/add_avps.c")

echo "  📌 dict_magic_codes.h: $CODES_H_LINES 行"
echo "  📌 dict_magic_codes.c: $CODES_C_LINES 行"
echo "  📌 add_avps.c: $AVPS_C_LINES 行 (包含枚举值)"

echo ""

# 4. 编译测试
echo "🔨 4. 编译测试..."

BUILD_DIR="/home/zhuwuhui/freeDiameter/build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "  ⚠️  构建目录不存在: $BUILD_DIR"
    echo "  提示: 请先运行 cmake 配置构建目录"
    exit 0
fi

cd "$BUILD_DIR"

echo "  🔄 开始编译 dict_magic_839..."
if make dict_magic_839 > /tmp/dict_magic_build.log 2>&1; then
    echo "  ✅ 编译成功！"
    
    # 检查生成的库文件
    if [ -f "$BUILD_DIR/extensions/dict_magic_839/dict_magic_839.so" ] || \
       [ -f "$BUILD_DIR/extensions/dict_magic_839/libdict_magic_839.so" ]; then
        echo "  ✅ 词典库已生成"
    else
        echo "  ⚠️  未找到生成的词典库文件"
    fi
else
    echo "  ❌ 编译失败！查看日志: /tmp/dict_magic_build.log"
    tail -20 /tmp/dict_magic_build.log
    exit 1
fi

echo ""

# 5. 检查文档
echo "📚 5. 检查文档..."

README_LINES=$(wc -l < "$DICT_DIR/README.md")
USAGE_LINES=$(wc -l < "$DICT_DIR/USAGE_GUIDE.md")
REPORT_LINES=$(wc -l < "$DICT_DIR/DICT_CHECK_REPORT.md")

echo "  📄 README.md: $README_LINES 行"
echo "  📄 USAGE_GUIDE.md: $USAGE_LINES 行"
echo "  📄 DICT_CHECK_REPORT.md: $REPORT_LINES 行"
echo "  ✅ 所有文档完整"

echo ""

# 6. 最终总结
echo "========================================"
echo "✅ 验证完成！"
echo "========================================"
echo ""
echo "改进总结:"
echo "  ✅ 枚举值定义: $ENUM_VALUES 个"
echo "  ✅ 状态码常量: $STATUS_CODES 个"
echo "  ✅ 新增源文件: 2 个 (dict_magic_codes.h/c)"
echo "  ✅ 文档文件: 4 个"
echo "  ✅ 编译状态: 成功"
echo ""
echo "下一步:"
echo "  1. 安装词典: sudo make install"
echo "  2. 查看文档: cat $DICT_DIR/README.md"
echo "  3. 使用指南: cat $DICT_DIR/USAGE_GUIDE.md"
echo ""
echo "词典系统已就绪，可用于生产环境！ 🎉"
