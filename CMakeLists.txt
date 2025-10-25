cmake_minimum_required(VERSION 3.10)
project(Trace)

# 设置默认构建类型为 Release（如果未指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Release 模式的编译选项
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# 设置静态编译标志
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # 优先查找静态库

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


# optional 自动收集方式收集所有源文件
# file(GLOB SOURCES "trace/*.cpp")
# file(GLOB HEADERS "trace/*.hpp" "trace/*.h")

# add_executable(trace ${SOURCES} ${HEADERS})


# 链接静态库文件
target_link_libraries(trace
    PRIVATE
    MBSO  # 链接新创建的MBSO库
    ${GMP_LINUX_DIR}/lib/libgmp.a
    ${GMP_LINUX_DIR}/lib/libmpfr.a
)


# 安装目标（可选）
install(TARGETS trace DESTINATION ${CMAKE_SOURCE_DIR}/bin)
