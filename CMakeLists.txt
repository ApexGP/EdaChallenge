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

# 设置完全静态编译标志
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # 强制使用静态库

# 选项
########################## 兼容性优化标志 - 在服务器和本地都能高效运行 ##########################
set(COMPAT_OPT_FLAGS "-O3 -flto -fno-semantic-interposition -mavx2 -mfma -mbmi2 -mpopcnt")
# 针对两种CPU的微优化
set(MICRO_OPT_FLAGS "-mtune=generic -march=x86-64-v3")
# 设置Release模式编译选项
set(CMAKE_CXX_FLAGS_RELEASE "${COMPAT_OPT_FLAGS} ${MICRO_OPT_FLAGS} -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${COMPAT_OPT_FLAGS} ${MICRO_OPT_FLAGS} -DNDEBUG")
########################## 兼容性优化标志 - 在服务器和本地都能高效运行 ##########################


########################## 设置针对服务器CPU的优化标志 ##########################
# set(SERVER_CPU_OPT_FLAGS "-march=cascadelake -mtune=cascadelake -O3 -flto -fno-semantic-interposition")
# # 设置Release模式编译选项
# set(CMAKE_CXX_FLAGS_RELEASE "${SERVER_CPU_OPT_FLAGS} -DNDEBUG")
# set(CMAKE_C_FLAGS_RELEASE "${SERVER_CPU_OPT_FLAGS} -DNDEBUG")
########################## end #########################

# 包含OpenMP支持
find_package(OpenMP REQUIRED)

# 设置第三方库路径
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/trace/third_party)
set(GMP_LINUX_DIR ${THIRD_PARTY_DIR}/gmp-linux)

# 包含头文件目录
include_directories(
    ${THIRD_PARTY_DIR} # CGAL的和Boost的header
    ${GMP_LINUX_DIR}/include
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
    trace/QuadTree.hpp
    trace/SpaceIndex.hpp
    trace/Trace.hpp
)

# 解决dlopen警告
target_link_options(trace PRIVATE "-Wl,--no-as-needed")

# 链接OpenMP和静态库
target_link_libraries(trace PRIVATE
    MBSO
    ${GMP_LINUX_DIR}/lib/libgmp.a
    ${GMP_LINUX_DIR}/lib/libmpfr.a
    OpenMP::OpenMP_CXX
)

# 安装目标
install(TARGETS trace DESTINATION ${CMAKE_SOURCE_DIR}/bin)
