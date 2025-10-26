#!/bin/bash
# copy_executable.sh - 复制静态链接的可执行文件并输出系统信息

echo "=========================================="
echo "       静态链接可执行文件部署脚本"
echo "=========================================="

# 1. 显示当前路径
echo "📁 当前工作目录: $(pwd)"
echo "📋 当前目录文件列表:"
ls -la
echo "------------------------------------------"

# 2. 检查 trace 可执行文件是否存在
if [ ! -f "trace" ]; then
    echo "❌ 错误: 未找到静态链接的可执行文件 trace"
    echo "当前目录中的文件:"
    ls -la trace 2>/dev/null || echo "未找到 trace 文件"
    exit 1
fi

echo "✅ 找到静态链接的可执行文件 trace"

# 3. 显示 trace 文件信息
echo "📊 trace 文件信息:"
ls -la trace
echo "文件类型: $(if command -v file >/dev/null; then file trace; else echo '无法检测文件类型 (file 命令不存在)'; fi)"

echo "------------------------------------------"

# 4. 创建输出目录
OUTPUT_DIR="/app/output"
echo "创建输出目录: $OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# 5. 复制所有文件到输出目录
echo "📁 复制所有文件到输出目录..."
echo "复制当前目录下的所有文件和文件夹..."
# 使用通配符复制所有可见文件和文件夹
cp -r ./* "$OUTPUT_DIR/" 2>/dev/null || true
# 复制隐藏文件（如果有的话）
cp -r .[^.]* "$OUTPUT_DIR/" 2>/dev/null || true
echo "✅ 所有文件复制完成"

# 6. 设置执行权限
echo "🔒 设置执行权限..."
chmod +x "$OUTPUT_DIR/trace"

# 7. 验证文件类型
echo "🔍 验证输出文件类型:"
if command -v file >/dev/null; then
    file "$OUTPUT_DIR/trace"
else
    echo "⚠️ 警告: file 命令不存在，无法验证文件类型"
fi

# 8. 检查是否为静态链接
echo "🔗 检查链接类型:"
if command -v ldd >/dev/null; then
    ldd "$OUTPUT_DIR/trace" 2>&1 | grep -q "not a dynamic executable"
    if [ $? -eq 0 ]; then
        echo "✅ 文件是静态链接的可执行文件"
    else
        echo "⚠️ 警告: 文件可能是动态链接的"
    fi
else
    echo "⚠️ 警告: ldd 命令不存在，无法检查链接类型"
fi

# 9. 显示输出目录内容
echo "------------------------------------------"
echo "📁 输出目录内容 ($OUTPUT_DIR):"
ls -la "$OUTPUT_DIR"

echo "------------------------------------------"
echo "🎉 部署完成! 可执行文件: $OUTPUT_DIR/trace"

# 10. 输出系统信息（使用基本命令）
echo "=========================================="
echo "           服务器系统信息"
echo "=========================================="

# 获取 CPU 信息
echo "💻 CPU 信息:"
echo "------------------------------------------"
if [ -f "/proc/cpuinfo" ]; then
    echo "CPU 型号: $(grep 'model name' /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | sed -e 's/^[ \t]*//')"
    echo "CPU 核心数: $(grep -c '^processor' /proc/cpuinfo)"
else
    echo "⚠️ 警告: /proc/cpuinfo 文件不存在"
fi

# 获取 CPU 频率
if [ -f "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq" ]; then
    max_freq=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq)
    max_freq_ghz=$(echo "$max_freq" | awk '{printf "%.2f", $1/1000000}')
    echo "CPU 最大频率: ${max_freq_ghz} GHz"
elif [ -f "/proc/cpuinfo" ]; then
    echo "CPU 频率: $(grep 'cpu MHz' /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | sed -e 's/^[ \t]*//') MHz"
else
    echo "⚠️ 警告: 无法获取 CPU 频率信息"
fi

# 获取内存信息
echo "------------------------------------------"
echo "🧠 内存信息:"
if [ -f "/proc/meminfo" ]; then
    total_mem_kb=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    total_mem_mb=$(echo "$total_mem_kb" | awk '{printf "%.2f", $1/1024}')
    echo "总内存: ${total_mem_mb} MB"
    
    if grep -q MemAvailable /proc/meminfo; then
        free_mem_kb=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
        free_mem_mb=$(echo "$free_mem_kb" | awk '{printf "%.2f", $1/1024}')
        echo "可用内存: ${free_mem_mb} MB"
    else
        echo "⚠️ 警告: MemAvailable 信息不可用"
    fi
else
    echo "⚠️ 警告: /proc/meminfo 文件不存在"
fi

# 获取操作系统信息
echo "------------------------------------------"
echo "🖥️ 操作系统信息:"
if [ -f "/etc/os-release" ]; then
    echo "操作系统: $(grep PRETTY_NAME /etc/os-release | cut -d '"' -f 2)"
elif [ -f "/etc/lsb-release" ]; then
    echo "操作系统: $(grep DISTRIB_DESCRIPTION /etc/lsb-release | cut -d '=' -f 2)"
else
    echo "⚠️ 警告: 无法获取操作系统信息"
fi

echo "内核版本: $(uname -r)"
echo "系统架构: $(uname -m)"

# 获取磁盘信息
echo "------------------------------------------"
echo "💾 磁盘信息:"
if command -v df >/dev/null; then
    df -h | grep -v tmpfs | grep -v udev || true
else
    echo "⚠️ 警告: df 命令不存在"
fi

# 获取网络信息
echo "------------------------------------------"
echo "🌐 网络信息:"
echo "主机名: $(hostname)"
if command -v ip >/dev/null; then
    echo "IP 地址: $(ip route get 1 | awk '{print $7; exit}')"
elif command -v hostname >/dev/null; then
    echo "IP 地址: $(hostname -I | awk '{print $1}')"
else
    echo "⚠️ 警告: 无法获取 IP 地址"
fi

# 获取系统负载
echo "------------------------------------------"
echo "📊 系统负载:"
if [ -f "/proc/loadavg" ]; then
    loadavg=$(cat /proc/loadavg | cut -d ' ' -f 1-3)
    echo "1分钟: $(echo $loadavg | awk '{print $1}')"
    echo "5分钟: $(echo $loadavg | awk '{print $2}')"
    echo "15分钟: $(echo $loadavg | awk '{print $3}')"
else
    echo "⚠️ 警告: /proc/loadavg 文件不存在"
fi

echo "=========================================="