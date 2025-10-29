cmake_minimum_required(VERSION 3.10)
project(CELICA_HOOK)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置目标平台为32位x86
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "Building 32-bit project on 64-bit system")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/detours)
include_directories(${CMAKE_SOURCE_DIR}/src)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 添加源文件
add_library(CELICA_HOOK SHARED
    src/main.cpp
    src/hook_manager.cpp
    src/file_redirect_hook.cpp
    src/font_hook.cpp
    src/codepage_hook.cpp
    src/window_title_hook.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/utils.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK ${CMAKE_SOURCE_DIR}/detours/detours.lib)

# Windows API链接
target_link_libraries(CELICA_HOOK kernel32 user32 gdi32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _X86_)

# 添加控制台测试程序
add_executable(CELICA_HOOK_TEST
    src/main.cpp
    src/hook_manager.cpp
    src/file_redirect_hook.cpp
    src/font_hook.cpp
    src/codepage_hook.cpp
    src/window_title_hook.cpp
    src/logger.cpp
    src/config_manager.cpp
    src/utils.cpp
)

# 链接detours库
target_link_libraries(CELICA_HOOK_TEST ${CMAKE_SOURCE_DIR}/detours/detours.lib)

# Windows API链接
target_link_libraries(CELICA_HOOK_TEST kernel32 user32 gdi32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK_TEST PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _X86_ _CONSOLE)
