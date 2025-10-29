#pragma once
#ifndef FILE_REDIRECT_HOOK_H
#define FILE_REDIRECT_HOOK_H

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
    
private:
    FileRedirectHook();
    ~FileRedirectHook();
    
    static std::wstring getRedirectedPath(const std::wstring& originalPath);
    static bool shouldRedirect(const std::wstring& path);
    
    // 原始函数指针
    static decltype(CreateFileA)* originalCreateFileA;
    static decltype(CreateFileW)* originalCreateFileW;
};

#endif // FILE_REDIRECT_HOOK_H
