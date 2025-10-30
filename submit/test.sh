#!/bin/bash
# build_and_copy_executable.sh - 编译并复制动态链接的可执行文件

echo "=========================================="
echo "       编译部署动态链接可执行文件"
echo "=========================================="

# 1. 显示当前路径
echo "📁 当前工作目录: $(pwd)"
echo "📋 当前目录文件列表:"
ls -la
echo "------------------------------------------"

# 2. 检查必要的文件
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ 错误: 未找到 CMakeLists.txt 文件"
    exit 1
fi
echo "✅ 找到 CMakeLists.txt 文件"

# 3. 检查 bin 目录是否存在
if [ ! -d "bin" ]; then
    echo "⚠️  警告: bin 目录不存在，将创建它"
    mkdir -p bin
fi

# 4. 创建并进入 build 目录
echo "🏗️  创建构建目录..."
if [ -d "build" ]; then
    echo "⚠️  警告: build 目录已存在，将清空重建"
    rm -rf build
fi

mkdir build
cd build

# 5. 使用 cmake 配置项目
echo "🔧 运行 CMake 配置..."
if command -v cmake >/dev/null; then
    cmake ..
    if [ $? -ne 0 ]; then
        echo "❌ CMake 配置失败"
        exit 1
    fi
else
    echo "❌ 错误: 未找到 cmake 命令"
    exit 1
fi

# 6. 编译项目
echo "🔨 编译项目..."
if command -v make >/dev/null; then
    make
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败"
        exit 1
    fi
else
    echo "❌ 错误: 未找到 make 命令"
    exit 1
fi

# 7. 返回项目根目录
cd ..

# 8. 检查编译结果 - 在项目根目录的 bin 目录下查找 trace
echo "🔍 检查编译结果..."
echo "📁 项目根目录内容:"
ls -la

if [ -d "bin" ]; then
    echo "✅ 找到 bin 目录"
    echo "📁 bin 目录内容:"
    ls -la bin/
    
    # 在 bin 目录下查找 trace 可执行文件
    if [ -f "bin/trace" ]; then
        echo "✅ 找到编译生成的可执行文件 bin/trace"
        TRACE_SOURCE="bin/trace"
    else
        echo "❌ 错误: 在 bin 目录下未找到 trace 可执行文件"
        echo "📋 bin 目录中的文件:"
        ls -la bin/ 2>/dev/null || echo "bin 目录为空"
        exit 1
    fi
else
    echo "❌ 错误: 编译后未找到 bin 目录"
    exit 1
fi

# 9. 复制 trace 文件到项目根目录（如果不在根目录）
if [ "$TRACE_SOURCE" != "trace" ]; then
    echo "📋 复制可执行文件到项目根目录..."
    cp "$TRACE_SOURCE" ./trace
fi

# 10. 验证文件
echo "🔍 验证文件..."
if [ ! -f "trace" ]; then
    echo "❌ 错误: trace 文件不存在"
    exit 1
fi

echo "✅ 找到 trace 文件"
echo "📊 trace 文件信息:"
ls -la trace
echo "文件类型: $(if command -v file >/dev/null; then file trace; else echo '无法检测文件类型 (file 命令不存在)'; fi)"

echo "------------------------------------------"

# 11. 创建输出目录
OUTPUT_DIR="/app/output"
echo "创建输出目录: $OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# 12. 复制所有文件到输出目录
echo "📁 复制所有文件到输出目录..."
echo "复制当前目录下的所有文件和文件夹..."
# 使用通配符复制所有可见文件和文件夹
cp -r ./* "$OUTPUT_DIR/" 2>/dev/null || true
# 复制隐藏文件（如果有的话）
cp -r .[^.]* "$OUTPUT_DIR/" 2>/dev/null || true
echo "✅ 所有文件复制完成"

# 13. 设置执行权限
echo "🔒 设置执行权限..."
chmod +x "$OUTPUT_DIR/trace"

# 14. 验证输出文件
echo "🔍 验证输出文件类型:"
if command -v file >/dev/null; then
    file "$OUTPUT_DIR/trace"
else
    echo "⚠️ 警告: file 命令不存在，无法验证文件类型"
fi

# 15. 检查动态链接库依赖
echo "🔗 检查动态链接库依赖:"
if command -v ldd >/dev/null; then
    echo "动态链接库依赖:"
    ldd "$OUTPUT_DIR/trace" 2>&1
    if [ $? -eq 0 ]; then
        echo "✅ 文件是动态链接的可执行文件"
    else
        echo "⚠️ 警告: 无法检查动态链接库依赖"
    fi
else
    echo "⚠️ 警告: ldd 命令不存在，无法检查动态链接库依赖"
fi

# 16. 显示输出目录内容
echo "------------------------------------------"
echo "📁 输出目录内容 ($OUTPUT_DIR):"
ls -la "$OUTPUT_DIR"

echo "------------------------------------------"
echo "🎉 部署完成! 可执行文件: $OUTPUT_DIR/trace"

# 17. 输出系统信息
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

# 18. 清理构建目录（可选）
echo "🧹 清理构建目录..."
if [ -d "build" ]; then
    rm -rf build
    echo "✅ 构建目录已清理"
fi

echo ""
echo "🎉 所有操作已完成!"
echo "📁 可执行文件位置: $OUTPUT_DIR/trace"
echo "=========================================="