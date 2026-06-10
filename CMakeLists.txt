cmake_minimum_required(VERSION 3.21)
project(Trace VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# 编译器检测
# ============================================================================
if(MSVC)
    set(COMPILER_ID "msvc")
elseif(MINGW)
    set(COMPILER_ID "mingw")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_ID "gcc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_ID "clang")
else()
    message(WARNING "未知编译器: ${CMAKE_CXX_COMPILER_ID}，将使用通用GCC兼容配置")
    set(COMPILER_ID "gcc")
endif()
message(STATUS "检测到编译器: ${CMAKE_CXX_COMPILER_ID} (模式: ${COMPILER_ID})")

# ============================================================================
# C++ 标准
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compile Commands 导出（供 clangd / IDE 使用）
# ============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 默认构建类型
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

# ============================================================================
# 可执行文件输出目录
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ============================================================================
# 第三方库路径（相对于 trace/third_party/）
# ============================================================================
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/trace/third_party)
set(THIRD_PARTY_BOOST_DIR  ${THIRD_PARTY_DIR}/boost)
set(THIRD_PARTY_CGAL_DIR   ${THIRD_PARTY_DIR}/CGAL)
set(THIRD_PARTY_GMP_WIN    ${THIRD_PARTY_DIR}/gmp-win)
set(THIRD_PARTY_GMP_LINUX  ${THIRD_PARTY_DIR}/gmp-linux)

# ============================================================================
# 平台相关编译选项
# ============================================================================
if(COMPILER_ID STREQUAL "msvc")
    # ----- MSVC -----
    message(STATUS "使用 MSVC 编译选项")

    # 静态链接 CRT（/MT），避免运行时依赖
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Release 优化
    set(MSVC_RELEASE_FLAGS "/O2 /Oi /Ot /Gy /GL /arch:AVX2 /fp:fast /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${MSVC_RELEASE_FLAGS}" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE   "${MSVC_RELEASE_FLAGS}" CACHE STRING "" FORCE)

    # Debug 选项
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1" CACHE STRING "" FORCE)

    # MSVC 不需要 -static 标志
    set(STATIC_LINK_FLAGS "")

    # GMP/MPFR 路径（MSVC 可以用 .lib 直接链接）
    set(GMP_INCLUDE_DIR ${THIRD_PARTY_GMP_WIN}/include)
    set(GMP_LIBRARY_DIR ${THIRD_PARTY_GMP_WIN}/lib)

elseif(COMPILER_ID STREQUAL "mingw")
    # ----- MinGW -----
    message(STATUS "使用 MinGW 编译选项")

    # Release 优化
    set(GNU_RELEASE_FLAGS "-O3 -flto -fno-semantic-interposition -mavx2 -mfma -mbmi2 -mpopcnt -mtune=generic -march=x86-64-v3")
    set(CMAKE_CXX_FLAGS_RELEASE "${GNU_RELEASE_FLAGS} -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE   "${GNU_RELEASE_FLAGS} -DNDEBUG" CACHE STRING "" FORCE)

    # 静态链接（MinGW 支持 -static，LTO 链接需要 -flto -fuse-linker-plugin）
    set(STATIC_LINK_FLAGS "-static -flto")

    # GMP/MPFR：优先找 .dll.a 导入库，找不到则尝试 MSYS2 路径
    set(GMP_INCLUDE_DIR ${THIRD_PARTY_GMP_WIN}/include)
    set(GMP_LIBRARY_DIR ${THIRD_PARTY_GMP_WIN}/lib)

else()  # GCC / Clang (Linux)
    # ----- GCC / Clang (Linux) -----
    message(STATUS "使用 GCC/Clang(Linux) 编译选项")

    # Release 优化
    set(GNU_RELEASE_FLAGS "-O3 -flto -fno-semantic-interposition -mavx2 -mfma -mbmi2 -mpopcnt -mtune=generic -march=x86-64-v3")
    set(CMAKE_CXX_FLAGS_RELEASE "${GNU_RELEASE_FLAGS} -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE   "${GNU_RELEASE_FLAGS} -DNDEBUG" CACHE STRING "" FORCE)

    # 静态链接（LTO 链接需要 -flto）
    set(STATIC_LINK_FLAGS "-static -flto")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    # GMP/MPFR（Linux 预编译静态库）
    set(GMP_INCLUDE_DIR ${THIRD_PARTY_GMP_LINUX}/include)
    set(GMP_LIBRARY_DIR ${THIRD_PARTY_GMP_LINUX}/lib)
endif()

# 应用静态链接标志
if(STATIC_LINK_FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_LINK_FLAGS}")
endif()

# ============================================================================
# OpenMP 检测
# ============================================================================
find_package(OpenMP QUIET)
if(NOT OPENMP_FOUND)
    message(FATAL_ERROR
        "❌ 未找到 OpenMP 库！\n"
        "  OpenMP 是本项目的必需依赖。请根据您的平台安装：\n"
        "  • Ubuntu/Debian:  sudo apt install libomp-dev\n"
        "  • CentOS/RHEL:    sudo yum install libomp-devel\n"
        "  • macOS:          brew install libomp\n"
        "  • MSVC:           OpenMP 随 Visual Studio 一起安装，请在 VS Installer 中确认勾选\n"
        "  • MinGW:          pacman -S mingw-w64-x86_64-openmp\n"
        "  安装后请重新运行 cmake。")
endif()
message(STATUS "✅ OpenMP 已找到 (版本: ${OpenMP_CXX_VERSION})")

# ============================================================================
# GMP/MPFR — 可选依赖（仅 CGAL 路径需要，当前 MBSO 路径不依赖）
# ============================================================================
set(GMP_ENABLED OFF)
if(COMPILER_ID STREQUAL "mingw")
    # MinGW 需要在 gmp-win 目录下有 .dll.a 导入库
    if(EXISTS "${GMP_LIBRARY_DIR}/libgmp.dll.a" AND EXISTS "${GMP_LIBRARY_DIR}/libmpfr.dll.a")
        set(GMP_ENABLED ON)
        set(GMP_LIBRARIES gmp mpfr)
        message(STATUS "✅ GMP/MPFR 导入库已找到")
    else()
        # 尝试查找系统安装的 GMP/MPFR（如 MSYS2）
        find_library(GMP_FOUND_LIB NAMES gmp mpfr PATHS ENV PATH NO_DEFAULT_PATH)
        if(GMP_FOUND_LIB)
            set(GMP_ENABLED ON)
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(GMP gmp)
                pkg_check_modules(MPFR mpfr)
            endif()
            message(STATUS "✅ GMP/MPFR 已从系统路径找到")
        else()
            message(STATUS "ℹ️  GMP/MPFR 未找到（MinGW 需要 .dll.a 导入库）。当前 MBSO 路径不需要它们，跳过。")
            message(STATUS "   如需 CGAL 支持，请执行：")
            message(STATUS "     gendef ${GMP_LIBRARY_DIR}/gmp-10.dll")
            message(STATUS "     dlltool -d gmp-10.def -l ${GMP_LIBRARY_DIR}/libgmp.dll.a -D ${GMP_LIBRARY_DIR}/gmp-10.dll")
            message(STATUS "   或: pacman -S mingw-w64-x86_64-gmp mingw-w64-x86_64-mpfr")
        endif()
    endif()
else()
    # MSVC / GCC：检查库文件是否存在
    if(EXISTS "${GMP_LIBRARY_DIR}" AND EXISTS "${GMP_INCLUDE_DIR}")
        set(GMP_ENABLED ON)
        set(GMP_LIBRARIES gmp mpfr)
        message(STATUS "✅ GMP/MPFR 已找到 (${GMP_LIBRARY_DIR})")
    else()
        message(STATUS "ℹ️  GMP/MPFR 未找到，跳过。当前 MBSO 路径不需要它们。")
    endif()
endif()

# ============================================================================
# 头文件路径
# ============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/trace/ManhattanBooleanSetOperation  # MBSO
)

# Boost 和 CGAL（header-only，始终加入路径）
if(EXISTS ${THIRD_PARTY_BOOST_DIR})
    include_directories(${THIRD_PARTY_BOOST_DIR})
endif()
if(EXISTS ${THIRD_PARTY_CGAL_DIR}/include)
    include_directories(${THIRD_PARTY_CGAL_DIR}/include)
endif()

if(GMP_ENABLED)
    include_directories(${GMP_INCLUDE_DIR})
    link_directories(${GMP_LIBRARY_DIR})
endif()

# ============================================================================
# MBSO 子库
# ============================================================================
add_subdirectory(trace/ManhattanBooleanSetOperation)

# MBSO 静态库在 MinGW/GCC 下禁用 LTO（避免 ar 不支持 LTO 目标文件导致链接失败）
if(COMPILER_ID STREQUAL "mingw" OR COMPILER_ID STREQUAL "gcc")
    target_compile_options(MBSO PRIVATE -fno-lto)
endif()

# ============================================================================
# 可执行文件
# ============================================================================
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

# 链接库
target_link_libraries(trace PRIVATE
    MBSO
    OpenMP::OpenMP_CXX
)

if(GMP_ENABLED AND GMP_LIBRARIES)
    target_link_libraries(trace PRIVATE ${GMP_LIBRARIES})
endif()

# 目标编译特性
set_target_properties(trace PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# MSVC 额外定义
if(COMPILER_ID STREQUAL "msvc")
    target_compile_definitions(trace PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ============================================================================
# 安装
# ============================================================================
install(TARGETS trace DESTINATION ${CMAKE_SOURCE_DIR}/bin)

# ============================================================================
# 编译环境汇总
# ============================================================================
message(STATUS "")
message(STATUS "╔════════════════════════════════════════╗")
message(STATUS "║  编译环境配置汇总                      ║")
message(STATUS "╠════════════════════════════════════════╣")
message(STATUS "║  编译器:    ${COMPILER_ID} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "║  构建类型:  ${CMAKE_BUILD_TYPE}")
message(STATUS "║  C++标准:   C++${CMAKE_CXX_STANDARD}")
message(STATUS "║  OpenMP:    ${OpenMP_CXX_VERSION}")
if(GMP_ENABLED)
    message(STATUS "║  GMP/MPFR:  已启用")
else()
    message(STATUS "║  GMP/MPFR:  未启用 (MBSO模式不需要)")
endif()
message(STATUS "╚════════════════════════════════════════╝")
message(STATUS "")
