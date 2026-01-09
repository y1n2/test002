#!/bin/bash

# 保守的文件清理脚本
# 只删除确定安全的临时文件和空目录
# 作者: 系统清理工具
# 日期: $(date '+%Y-%m-%d')

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 日志函数
log_info() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] INFO: $1${NC}"
}

log_success() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')] SUCCESS: $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] WARNING: $1${NC}"
}

log_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $1${NC}"
}

# 报告文件
REPORT_FILE="/home/zhuwuhui/freeDiameter/conservative_cleanup_report_$(date '+%Y%m%d_%H%M%S').txt"

# 初始化报告
cat > "$REPORT_FILE" << EOF
保守文件清理报告
================
清理时间: $(date '+%Y-%m-%d %H:%M:%S')
清理目录: /home/zhuwuhui/freeDiameter
清理策略: 只删除确定安全的临时文件和空目录

删除的文件列表:
EOF

# 统计变量
deleted_files=0
deleted_dirs=0
total_size=0

log_info "开始保守清理操作..."

# 1. 删除空的CMake临时目录
log_info "清理空的CMake临时目录..."
find /home/zhuwuhui/freeDiameter -type d -name "CMakeTmp" -empty | while read -r dir; do
    if [ -d "$dir" ]; then
        log_info "删除空目录: $dir"
        echo "空目录: $dir" >> "$REPORT_FILE"
        rmdir "$dir" 2>/dev/null || true
        deleted_dirs=$((deleted_dirs + 1))
    fi
done

# 2. 删除空的编译器ID目录
log_info "清理空的编译器ID目录..."
find /home/zhuwuhui/freeDiameter -type d -path "*/CMakeFiles/*/CompilerIdC/tmp" -empty | while read -r dir; do
    if [ -d "$dir" ]; then
        log_info "删除空目录: $dir"
        echo "空目录: $dir" >> "$REPORT_FILE"
        rmdir "$dir" 2>/dev/null || true
        deleted_dirs=$((deleted_dirs + 1))
    fi
done

# 3. 删除可能的编辑器临时文件（如果存在）
log_info "查找编辑器临时文件..."
temp_files_found=false
for pattern in "*~" "*.swp" "*.swo" ".DS_Store" "Thumbs.db"; do
    find /home/zhuwuhui/freeDiameter -type f -name "$pattern" | while read -r file; do
        if [ -f "$file" ]; then
            temp_files_found=true
            size=$(stat -c%s "$file" 2>/dev/null || echo 0)
            log_info "删除临时文件: $file (大小: $size 字节)"
            echo "临时文件: $file (大小: $size 字节)" >> "$REPORT_FILE"
            rm -f "$file"
            deleted_files=$((deleted_files + 1))
            total_size=$((total_size + size))
        fi
    done
done

if [ "$temp_files_found" = false ]; then
    log_info "未发现编辑器临时文件"
    echo "未发现编辑器临时文件" >> "$REPORT_FILE"
fi

# 4. 删除可能的系统临时文件
log_info "查找系统临时文件..."
system_temp_found=false
for pattern in "*.tmp" "*.temp" "*.cache" "core" "*.core"; do
    find /home/zhuwuhui/freeDiameter -type f -name "$pattern" | while read -r file; do
        if [ -f "$file" ]; then
            system_temp_found=true
            size=$(stat -c%s "$file" 2>/dev/null || echo 0)
            log_info "删除系统临时文件: $file (大小: $size 字节)"
            echo "系统临时文件: $file (大小: $size 字节)" >> "$REPORT_FILE"
            rm -f "$file"
            deleted_files=$((deleted_files + 1))
            total_size=$((total_size + size))
        fi
    done
done

if [ "$system_temp_found" = false ]; then
    log_info "未发现系统临时文件"
    echo "未发现系统临时文件" >> "$REPORT_FILE"
fi

# 生成报告摘要
cat >> "$REPORT_FILE" << EOF

清理摘要:
========
删除文件数量: $deleted_files
删除目录数量: $deleted_dirs
释放空间: $total_size 字节 ($(echo "scale=2; $total_size/1024/1024" | bc -l 2>/dev/null || echo "计算失败") MB)

注意事项:
========
- 本次清理采用极其保守的策略
- 只删除了确定安全的临时文件和空目录
- 所有重要的文档、测试脚本和源代码文件均已保留
- 如需删除其他文件，请手动确认后操作

清理完成时间: $(date '+%Y-%m-%d %H:%M:%S')
EOF

log_success "保守清理完成！"
log_info "删除了 $deleted_files 个文件和 $deleted_dirs 个目录"
log_info "释放空间: $total_size 字节"
log_info "详细报告已保存到: $REPORT_FILE"

echo
log_warning "重要提醒: 本次清理非常保守，只删除了确定安全的文件"
log_warning "如果您需要删除更多文件，请手动检查并确认"