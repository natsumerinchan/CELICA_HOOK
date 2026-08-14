cmake_minimum_required(VERSION 3.15)
project(CELICA_HOOK)

# 可选：使用静态 MSVC 运行时(/MT)，使注入 DLL 不依赖 VC 运行库
# 注意：需确认 detours.lib 的运行时链接方式与之匹配
option(CELICA_HOOK_STATIC_CRT "使用静态MSVC运行时(/MT)，产物不依赖VC运行库" OFF)

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

if(MSVC AND CELICA_HOOK_STATIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # x86_64
    set(CELICA_HOOK_ARCH "x64")
    # Detours 要求显式定义 _X86_ 或 _AMD64_ 来选择 x86/x64 代码路径
    set(CELICA_HOOK_ARCH_DEFINES WIN32_LEAN_AND_MEAN _AMD64_)
    set(CELICA_HOOK_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/x64)
else()
    # 32Bit x86
    set(CELICA_HOOK_ARCH "x86")
    set(CELICA_HOOK_ARCH_DEFINES WIN32_LEAN_AND_MEAN _X86_)
    set(CELICA_HOOK_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/x86)
endif()

message(STATUS "CELICA_HOOK: 当前目标架构 = ${CELICA_HOOK_ARCH}")

# 输出目录：Debug/Release 分开，避免互相覆盖产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CELICA_HOOK_OUTPUT_DIR}/$<CONFIG>)

# 架构对应的Detours静态库
set(CELICA_HOOK_DETOURS_LIB ${CMAKE_SOURCE_DIR}/src/detours/${CELICA_HOOK_ARCH}/detours.lib)
set(CELICA_HOOK_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/detours/${CELICA_HOOK_ARCH}
    ${CMAKE_SOURCE_DIR}/src
)

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

target_include_directories(CELICA_HOOK PRIVATE ${CELICA_HOOK_INCLUDE_DIRS})

# 链接detours库与Windows API（advapi32: 注册表时区查询）
target_link_libraries(CELICA_HOOK ${CELICA_HOOK_DETOURS_LIB} kernel32 user32 gdi32 advapi32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK PRIVATE ${CELICA_HOOK_ARCH_DEFINES})

# 添加DLL注入器（使用DetourCreateProcessWithDllExW）- Windows子系统
add_executable(CELICA_HOOK_LAUNCHER WIN32
    src/launcher.cpp
    src/config_manager.cpp
    src/logger.cpp
    src/utils.cpp
    src/author_window.cpp
    src/locale_emulator_plus.cpp
)

target_include_directories(CELICA_HOOK_LAUNCHER PRIVATE ${CELICA_HOOK_INCLUDE_DIRS})

# 链接detours库与Windows API
target_link_libraries(CELICA_HOOK_LAUNCHER ${CELICA_HOOK_DETOURS_LIB} kernel32 user32 gdi32 advapi32 shell32)

# 设置预处理器定义
target_compile_definitions(CELICA_HOOK_LAUNCHER PRIVATE ${CELICA_HOOK_ARCH_DEFINES})

# 单元测试（纯逻辑工具函数）
enable_testing()
add_executable(CELICA_HOOK_TESTS
    src/tests/test_utils.cpp
    src/utils.cpp
)
target_include_directories(CELICA_HOOK_TESTS PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(CELICA_HOOK_TESTS PRIVATE ${CELICA_HOOK_ARCH_DEFINES})
target_link_libraries(CELICA_HOOK_TESTS kernel32)
add_test(NAME CELICA_HOOK_utils COMMAND CELICA_HOOK_TESTS)
