#!/bin/bash
# freeDiameter Windows 源代码打包脚本
# 将客户端源代码打包，供Windows下编译使用

echo "=========================================="
echo "freeDiameter Windows 源代码打包工具"
echo "=========================================="

# 设置变量
SOURCE_DIR="/home/zhuwuhui/freeDiameter"
PACKAGE_DIR="/mnt/d/freeDiameter_Windows_Source"
CLIENT_DIR="$SOURCE_DIR/diameter_client"

# 创建打包目录
echo "创建打包目录..."
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

# 创建目录结构
mkdir -p "$PACKAGE_DIR/src/client"
mkdir -p "$PACKAGE_DIR/src/libfdcore"
mkdir -p "$PACKAGE_DIR/src/libfdproto"
mkdir -p "$PACKAGE_DIR/include"
mkdir -p "$PACKAGE_DIR/config"
mkdir -p "$PACKAGE_DIR/certs"
mkdir -p "$PACKAGE_DIR/build"
mkdir -p "$PACKAGE_DIR/docs"

echo "复制客户端源代码..."
# 复制客户端源代码
cp "$CLIENT_DIR"/*.c "$PACKAGE_DIR/src/client/"
cp "$CLIENT_DIR"/*.h "$PACKAGE_DIR/src/client/"
cp "$CLIENT_DIR/CMakeLists.txt" "$PACKAGE_DIR/src/client/"
cp "$CLIENT_DIR/Makefile" "$PACKAGE_DIR/src/client/"

echo "复制核心库源代码..."
# 复制核心库源代码
cp "$SOURCE_DIR/libfdcore"/*.c "$PACKAGE_DIR/src/libfdcore/"
cp "$SOURCE_DIR/libfdcore"/*.h "$PACKAGE_DIR/src/libfdcore/"
cp "$SOURCE_DIR/libfdproto"/*.c "$PACKAGE_DIR/src/libfdproto/"
cp "$SOURCE_DIR/libfdproto"/*.h "$PACKAGE_DIR/src/libfdproto/"

echo "复制头文件..."
# 复制头文件
if [ -d "$SOURCE_DIR/include" ]; then
    cp -r "$SOURCE_DIR/include"/* "$PACKAGE_DIR/include/"
fi

echo "复制配置文件..."
# 复制配置文件
cp "$CLIENT_DIR"/*.conf "$PACKAGE_DIR/config/"
cp "$SOURCE_DIR/exe/conf"/*.conf "$PACKAGE_DIR/config/" 2>/dev/null || true

echo "复制证书文件..."
# 复制证书文件
cp "$CLIENT_DIR"/*.crt "$PACKAGE_DIR/certs/" 2>/dev/null || true
cp "$CLIENT_DIR"/*.key "$PACKAGE_DIR/certs/" 2>/dev/null || true
cp "$SOURCE_DIR/exe/certs"/* "$PACKAGE_DIR/certs/" 2>/dev/null || true

echo "复制文档..."
# 复制文档
cp "$CLIENT_DIR/README_客户端使用说明.md" "$PACKAGE_DIR/docs/"
cp "$CLIENT_DIR/客户端命令文档.txt" "$PACKAGE_DIR/docs/"
cp "$SOURCE_DIR/docs"/*.md "$PACKAGE_DIR/docs/" 2>/dev/null || true

echo "复制主要配置文件..."
# 复制主要配置文件
cp "$SOURCE_DIR/CMakeLists.txt" "$PACKAGE_DIR/"
cp "$SOURCE_DIR/Makefile" "$PACKAGE_DIR/"

echo "创建文件清单..."
# 创建文件清单
find "$PACKAGE_DIR" -type f > "$PACKAGE_DIR/FILE_LIST.txt"

echo "=========================================="
echo "打包完成！"
echo "Windows 源代码包位置: $PACKAGE_DIR"
echo "=========================================="

# 显示目录结构
echo "目录结构:"
tree "$PACKAGE_DIR" 2>/dev/null || find "$PACKAGE_DIR" -type d | sed 's|[^/]*/|  |g'

echo ""
echo "文件统计:"
echo "源代码文件: $(find "$PACKAGE_DIR/src" -name "*.c" | wc -l) 个"
echo "头文件: $(find "$PACKAGE_DIR" -name "*.h" | wc -l) 个"
echo "配置文件: $(find "$PACKAGE_DIR/config" -name "*.conf" | wc -l) 个"
echo "证书文件: $(find "$PACKAGE_DIR/certs" -type f | wc -l) 个"

echo ""
echo "下一步："
echo "1. 将 D:\\freeDiameter_Windows_Source 目录复制到 Windows 系统"
echo "2. 在 Windows 上安装 Visual Studio 或 MinGW"
echo "3. 使用提供的编译脚本进行编译"