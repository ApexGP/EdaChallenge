cmake_minimum_required(VERSION 3.10)
project(Trace)

# 设置默认构建类型为 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 针对不同编译器设置优化选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC 和 Clang 的优化选项
    add_compile_options(
        -O3                  # 最高级别优化
        -march=native       # 针对当前 CPU 架构优化
        -mtune=native       # 针对当前 CPU 调优
        -flto               # 链接时优化
        -fno-rtti           # 禁用 RTTI（如果不需要）
        -DNDEBUG            # 禁用断言和调试代码
    )
    
    # 链接时优化
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    
    # 额外的性能优化选项
    add_compile_options(
        -ffast-math         # 快速数学运算（精度可能受影响）
        -funroll-loops      # 循环展开
        -fomit-frame-pointer # 省略帧指针（提高性能但影响调试）
    )
    
elseif(MSVC)
    # MSVC 的优化选项
    add_compile_options(
        /O2                 # 最大优化（速度）
        /Ob2                # 内联函数扩展
        /Ot                 # 偏好快速代码
        /GL                 # 全程序优化
        /DNDEBUG            # 禁用调试
        /arch:AVX2          # 使用 AVX2 指令集（如果 CPU 支持）
    )
    
    # 链接时优化
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# 包含OpenMP支持
find_package(OpenMP REQUIRED)

# 包含头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/trace/ManhattanBooleanSetOperation # MBSO头文件
)

# 添加子目录（编译MBSO库）
add_subdirectory(trace/ManhattanBooleanSetOperation)

# 添加可执行文件
add_executable(trace
    trace/trace.cpp
    trace/Graph.hpp
    trace/Input.hpp
    trace/Intersect.hpp
    trace/Output.hpp
    trace/ManhattanIntersectDetector.hpp
    trace/PolygonCutting.hpp
    trace/public.h
    trace/NaiveThreadPool.h
    trace/robin_hood.h
    trace/QuadTree.hpp
    trace/SpaceIndex.hpp
    trace/Trace.hpp
)

# 链接OpenMP和静态库
target_link_libraries(trace PRIVATE
    MBSO
    OpenMP::OpenMP_CXX
)

# 安装目标
install(TARGETS trace DESTINATION ${CMAKE_SOURCE_DIR}/bin)
