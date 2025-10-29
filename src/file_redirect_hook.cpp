#include "file_redirect_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include <string>

// 初始化静态成员变量
decltype(CreateFileA)* FileRedirectHook::originalCreateFileA = nullptr;
decltype(CreateFileW)* FileRedirectHook::originalCreateFileW = nullptr;

FileRedirectHook& FileRedirectHook::getInstance() {
    static FileRedirectHook instance;
    return instance;
}

FileRedirectHook::FileRedirectHook() {
}

FileRedirectHook::~FileRedirectHook() {
    shutdown();
}

bool FileRedirectHook::initialize() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFileRedirect) {
        Logger::getInstance().log(L"文件重定向功能已禁用");
        return true;
    }
    
    Logger::getInstance().log(L"初始化文件重定向hook");
    
    // Hook CreateFileA
    originalCreateFileA = CreateFileA;
    if (DetourAttach(&(PVOID&)originalCreateFileA, HookedCreateFileA) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFileA 失败");
        return false;
    }
    
    // Hook CreateFileW
    originalCreateFileW = CreateFileW;
    if (DetourAttach(&(PVOID&)originalCreateFileW, HookedCreateFileW) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFileW 失败");
        DetourDetach(&(PVOID&)originalCreateFileA, HookedCreateFileA);
        return false;
    }
    
    Logger::getInstance().log(L"文件重定向hook初始化完成");
    return true;
}

void FileRedirectHook::shutdown() {
    if (originalCreateFileA) {
        DetourDetach(&(PVOID&)originalCreateFileA, HookedCreateFileA);
        originalCreateFileA = nullptr;
    }
    
    if (originalCreateFileW) {
        DetourDetach(&(PVOID&)originalCreateFileW, HookedCreateFileW);
        originalCreateFileW = nullptr;
    }
    
    Logger::getInstance().log(L"文件重定向hook已卸载");
}

HANDLE WINAPI FileRedirectHook::HookedCreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFileRedirect) {
        return originalCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
    }
    
    std::wstring wFileName = Utils::stringToWstring(lpFileName);
    std::wstring redirectedPath = getRedirectedPath(wFileName);
    
    if (redirectedPath != wFileName) {
        Logger::getInstance().log(L"文件重定向: " + wFileName + L" -> " + redirectedPath);
        std::string redirectedPathA = Utils::wstringToString(redirectedPath);
        return originalCreateFileW(Utils::stringToWstring(redirectedPathA).c_str(),
                                 dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    
    return originalCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSecurityAttributes, dwCreationDisposition,
                             dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI FileRedirectHook::HookedCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFileRedirect) {
        return originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
    }
    
    std::wstring fileName(lpFileName);
    std::wstring redirectedPath = getRedirectedPath(fileName);
    
    if (redirectedPath != fileName) {
        Logger::getInstance().log(L"文件重定向: " + fileName + L" -> " + redirectedPath);
        return originalCreateFileW(redirectedPath.c_str(), dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
    }
    
    return originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSecurityAttributes, dwCreationDisposition,
                             dwFlagsAndAttributes, hTemplateFile);
}

std::wstring FileRedirectHook::getRedirectedPath(const std::wstring& originalPath) {
    if (!shouldRedirect(originalPath)) {
        return originalPath;
    }
    
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    std::wstring gameDir = Utils::getModuleDirectory();
    std::wstring redirectDir = Utils::combinePaths(gameDir, config.redirectFolder);
    
    // 获取相对路径
    std::wstring relativePath;
    if (originalPath.find(gameDir) == 0) {
        relativePath = originalPath.substr(gameDir.length());
        if (relativePath.front() == L'\\' || relativePath.front() == L'/') {
            relativePath = relativePath.substr(1);
        }
    } else {
        // 如果不是游戏目录下的文件，尝试获取文件名
        relativePath = Utils::getFileName(originalPath);
    }
    
    // 构建重定向路径
    std::wstring redirectedPath = Utils::combinePaths(redirectDir, relativePath);
    
    // 检查重定向文件是否存在
    if (Utils::fileExists(redirectedPath)) {
        return redirectedPath;
    }
    
    return originalPath;
}

bool FileRedirectHook::shouldRedirect(const std::wstring& path) {
    // 这里可以添加过滤逻辑，比如只重定向特定类型的文件
    // 目前重定向所有文件访问
    return true;
}
