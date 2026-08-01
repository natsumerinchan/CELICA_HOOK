#pragma once
#ifndef FILE_HOOK_H
#define FILE_HOOK_H

#include <windows.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"

class FileRedirectHook {
public:
    static FileRedirectHook& getInstance();
    
    bool initialize();
    void shutdown();
    
    // Hook函数声明
    static HANDLE WINAPI HookedCreateFileA(
        LPCSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile
    );
    
    static HANDLE WINAPI HookedCreateFileW(
        LPCWSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile
    );
    
    static HANDLE WINAPI HookedCreateFile2(
        LPCWSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        DWORD dwCreationDisposition,
        LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams
    );
    
private:
    FileRedirectHook();
    ~FileRedirectHook();
    
    static std::wstring getRedirectedPath(const std::wstring& originalPath, DWORD dwCreationDisposition);
    static std::wstring getRelativePath(const std::wstring& path);
    static bool shouldSpoofFile(const std::wstring& path);
    
    // 原始函数指针
    static decltype(CreateFileA)* originalCreateFileA;
    static decltype(CreateFileW)* originalCreateFileW;
    static decltype(CreateFile2)* originalCreateFile2;
    
    // 缓存目录（模块目录、重定向目录），初始化时计算一次
    static std::wstring m_gameDir;
    static std::wstring m_redirectDir;
    static bool m_initialized;
    
    // 判断是否为写类 disposition，需要时创建目标父目录
    static bool isWriteDisposition(DWORD dwCreationDisposition);
};

#endif // FILE_HOOK_H
