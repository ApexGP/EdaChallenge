# /ManhattanBooleanSetOperation/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(ManhattanBooleanSetOperation)

# 设置源文件
set(SOURCES
    model/MVertex.cpp
    model/MEdge.cpp
    model/MPolygon.cpp
    solve/MBSOCore.cpp
)

# 设置头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/const
    ${CMAKE_CURRENT_SOURCE_DIR}/model
    ${CMAKE_CURRENT_SOURCE_DIR}/solve
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# 创建静态库
add_library(MBSO STATIC ${SOURCES})

# 设置目标属性
set_target_properties(MBSO PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 安装规则（可选）
install(TARGETS MBSO
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY const model solve utils
    DESTINATION include/ManhattanBooleanSetOperation
    FILES_MATCHING PATTERN "*.h"
    PATTERN "robin_hood.h" EXCLUDE
)