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

    // 文件欺骗：目录枚举 hook（仅启用文件欺骗时安装）
    static HANDLE WINAPI HookedFindFirstFileW(
        LPCWSTR lpFileName,
        LPWIN32_FIND_DATAW lpFindFileData
    );
    static HANDLE WINAPI HookedFindFirstFileA(
        LPCSTR lpFileName,
        LPWIN32_FIND_DATAA lpFindFileData
    );
    static BOOL WINAPI HookedFindNextFileW(
        HANDLE hFindFile,
        LPWIN32_FIND_DATAW lpFindFileData
    );
    static BOOL WINAPI HookedFindNextFileA(
        HANDLE hFindFile,
        LPWIN32_FIND_DATAA lpFindFileData
    );
    static BOOL WINAPI HookedFindClose(HANDLE hFindFile);
    
private:
    FileRedirectHook();
    ~FileRedirectHook();
    
    static std::wstring getRedirectedPath(const std::wstring& originalPath, DWORD dwCreationDisposition);
    static std::wstring getRelativePath(const std::wstring& path);
    static bool shouldSpoofFile(const std::wstring& path);
    
    // 文件欺骗枚举辅助
    static bool spoofingActive();
    static bool isSearchDirSpoofed(const std::wstring& searchDir);
    static bool isEntrySpoofed(const std::wstring& searchDir, const std::wstring& entryName);
    static void recordFindSearchDir(HANDLE hFind, const std::wstring& searchDir);
    static bool getFindSearchDir(HANDLE hFind, std::wstring& outDir);
    static void removeFindSearchDir(HANDLE hFind);
    
    // 重定向实时查找的 TTL 缓存辅助（仅缓存非写场景，减少每次调用的磁盘查询）
    static bool checkRedirectCache(const std::wstring& key, bool& outRedirected);
    static void storeRedirectCache(const std::wstring& key, bool redirected);
    
    // 原始函数指针
    static decltype(CreateFileA)* originalCreateFileA;
    static decltype(CreateFileW)* originalCreateFileW;
    static decltype(CreateFile2)* originalCreateFile2;
    static decltype(FindFirstFileW)* originalFindFirstFileW;
    static decltype(FindFirstFileA)* originalFindFirstFileA;
    static decltype(FindNextFileW)* originalFindNextFileW;
    static decltype(FindNextFileA)* originalFindNextFileA;
    static decltype(FindClose)* originalFindClose;
    
    // 缓存目录（模块目录、重定向目录），初始化时计算一次
    static std::wstring m_gameDir;
    static std::wstring m_redirectDir;
    static bool m_initialized;
    
    // 判断是否为写类 disposition，需要时创建目标父目录
    static bool isWriteDisposition(DWORD dwCreationDisposition);
};

#endif // FILE_HOOK_H
