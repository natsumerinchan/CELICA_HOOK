cmake_minimum_required(VERSION 3.15)
project(CELICA_HOOK)

if(MSVC)
    add_compile_options(/utf-8)
    # 提升警告级别，帮助尽早发现潜在问题
    add_compile_options(/W4)
else()
    # 非 MSVC 编译器（如 MinGW）启用常见警告
    add_compile_options(-Wall -Wextra)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # x86_64
    set(CELICA_HOOK_ARCH "x64")
    # Detours 要求显式定义 _X86_ 或 _AMD64_ 来选择 x86/x64 代码路径
    set(CELICA_HOOK_ARCH_DEFINES WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _AMD64_)
    set(CELICA_HOOK_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/x64)
else()
    # 32Bit x86
    set(CELICA_HOOK_ARCH "x86")
    set(CELICA_HOOK_ARCH_DEFINES WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _X86_)
    set(CELICA_HOOK_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/x86)
endif()

message(STATUS "CELICA_HOOK: 当前目标架构 = ${CELICA_HOOK_ARCH}")

# 输出目录：Debug/Release 分开，避免互相覆盖产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)

# 包含目录（架构对应的Detours头文件目录）
include_directories(${CMAKE_SOURCE_DIR}/src/detours/${CELICA_HOOK_ARCH})
include_directories(${CMAKE_SOURCE_DIR}/src)

# 架构对应的Detours静态库
set(CELICA_HOOK_DETOURS_LIB ${CMAKE_SOURCE_DIR}/src/detours/${CELICA_HOOK_ARCH}/detours.lib)

# 添加源文件
add_library(CELICA_HOOK SHARED
    src/main.cpp
    src/hook_manager.cpp
    src/file_hook.cpp
    src/font_hook.cpp
    src/window_title_hook.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/utils.cpp
    src/locale_emulator_plus.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK ${CELICA_HOOK_DETOURS_LIB})

# Windows API链接
target_link_libraries(CELICA_HOOK kernel32 user32 gdi32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK PRIVATE ${CELICA_HOOK_ARCH_DEFINES})

# 添加DLL注入器（使用DetourCreateProcessWithDllW）- Windows子系统
add_executable(CELICA_HOOK_LAUNCHER WIN32
    src/launcher.cpp
    src/config_manager.cpp
    src/logger.cpp
    src/utils.cpp
    src/author_window.cpp
    src/locale_emulator_plus.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK_LAUNCHER ${CELICA_HOOK_DETOURS_LIB})

# Windows API链接
target_link_libraries(CELICA_HOOK_LAUNCHER kernel32 user32 gdi32 shlwapi)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK_LAUNCHER PRIVATE ${CELICA_HOOK_ARCH_DEFINES})