cmake_minimum_required(VERSION 3.15)
project(CELICA_HOOK)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置目标平台为32位x86
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 对于MSVC，使用平台工具集而不是-m32标志
    set(CMAKE_GENERATOR_PLATFORM "Win32")
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src/detours)
include_directories(${CMAKE_SOURCE_DIR}/src)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

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
    src/locale_emulator.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK ${CMAKE_SOURCE_DIR}/src/detours/detours.lib)

# Windows API链接
target_link_libraries(CELICA_HOOK kernel32 user32 gdi32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _X86_)

# 添加DLL启动器（使用DetourCreateProcessWithDllW）- Windows子系统
add_executable(CELICA_HOOK_LAUNCHER WIN32
    src/launcher.cpp
    src/config_manager.cpp
    src/logger.cpp
    src/utils.cpp
    src/author_window.cpp
    src/locale_emulator.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK_LAUNCHER ${CMAKE_SOURCE_DIR}/src/detours/detours.lib)

# Windows API链接
target_link_libraries(CELICA_HOOK_LAUNCHER kernel32 user32 gdi32 shlwapi)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK_LAUNCHER PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _X86_)
