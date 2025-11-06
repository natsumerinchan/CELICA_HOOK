#include "file_hook.h"
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
    
    // 如果文件重定向和文件欺骗都禁用，则不需要hook
    if (!config.enableFileRedirect && !config.enableFileSpoofing) {
        Logger::getInstance().log(L"文件重定向和文件欺骗功能都已禁用");
        return true;
    }
    
    Logger::getInstance().log(L"初始化文件hook");
    
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
    
    Logger::getInstance().log(L"文件hook初始化完成");
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
    
    // 首先检查文件重定向（优先级最高）
    if (config.enableFileRedirect) {
        std::wstring wFileName = Utils::stringToWstring(lpFileName);
        std::wstring redirectedPath = getRedirectedPath(wFileName);
        
        if (redirectedPath != wFileName) {
            Logger::getInstance().log(L"文件重定向(A): " + wFileName + L" -> " + redirectedPath);
            std::string redirectedPathA = Utils::wstringToString(redirectedPath);
            return originalCreateFileW(Utils::stringToWstring(redirectedPathA).c_str(),
                                     dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                     dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        }
    }
    
    // 然后检查文件欺骗（优先级低于文件重定向）
    if (config.enableFileSpoofing) {
        std::wstring wFileName = Utils::stringToWstring(lpFileName);
        if (shouldSpoofFile(wFileName)) {
            Logger::getInstance().log(L"文件欺骗(A): " + wFileName + L" -> 文件不存在");
            return INVALID_HANDLE_VALUE;
        }
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
    
    // 首先检查文件重定向（优先级最高）
    if (config.enableFileRedirect) {
        std::wstring fileName(lpFileName);
        std::wstring redirectedPath = getRedirectedPath(fileName);
        
        if (redirectedPath != fileName) {
            Logger::getInstance().log(L"文件重定向(W): " + fileName + L" -> " + redirectedPath);
            return originalCreateFileW(redirectedPath.c_str(), dwDesiredAccess, dwShareMode,
                                     lpSecurityAttributes, dwCreationDisposition,
                                     dwFlagsAndAttributes, hTemplateFile);
        }
    }
    
    // 然后检查文件欺骗（优先级低于文件重定向）
    if (config.enableFileSpoofing) {
        std::wstring fileName(lpFileName);
        if (shouldSpoofFile(fileName)) {
            Logger::getInstance().log(L"文件欺骗(W): " + fileName + L" -> 文件不存在");
            return INVALID_HANDLE_VALUE;
        }
    }
    
    return originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSecurityAttributes, dwCreationDisposition,
                             dwFlagsAndAttributes, hTemplateFile);
}

std::wstring FileRedirectHook::getRedirectedPath(const std::wstring& originalPath) {
    try {
        if (!shouldRedirect(originalPath)) {
            return originalPath;
        }

        // 检查原始路径是否有效
        if (originalPath.empty() || originalPath.find(L"\\\\?\\") == 0) {
            Logger::getInstance().log(L"无效的原始路径: " + originalPath);
            return originalPath;
        }

        const HookConfig& config = ConfigManager::getInstance().getConfig();
        std::wstring gameDir = Utils::getModuleDirectory();
        
        // 检查游戏目录是否有效
        if (gameDir.empty()) {
            Logger::getInstance().log(L"无法获取游戏目录");
            return originalPath;
        }

        std::wstring redirectDir = Utils::combinePaths(gameDir, config.redirectFolder);
        
        // 获取相对路径
        std::wstring relativePath;
        if (originalPath.find(gameDir) == 0) {
            relativePath = originalPath.substr(gameDir.length());
            if (!relativePath.empty() && (relativePath.front() == L'\\' || relativePath.front() == L'/')) {
                relativePath = relativePath.substr(1);
            }
        } else {
            // 如果不是游戏目录下的文件，尝试获取文件名
            relativePath = Utils::getFileName(originalPath);
        }

        // 检查相对路径是否有效
        if (relativePath.empty()) {
            Logger::getInstance().log(L"无法获取相对路径: " + originalPath);
            return originalPath;
        }

        // 构建重定向路径
        std::wstring redirectedPath = Utils::combinePaths(redirectDir, relativePath);
        
        // 检查重定向路径是否有效
        if (redirectedPath.empty()) {
            Logger::getInstance().log(L"生成的重定向路径无效");
            return originalPath;
        }

        // 检查重定向文件是否存在
        if (Utils::fileExists(redirectedPath)) {
            // Logger::getInstance().log(L"成功重定向: " + originalPath + L" -> " + redirectedPath);
            return redirectedPath;
        } else {
            // Logger::getInstance().log(L"重定向文件不存在: " + redirectedPath);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().log(L"路径重定向异常: " + Utils::stringToWstring(e.what()));
    } catch (...) {
        Logger::getInstance().log(L"未知路径重定向异常");
    }
    
    return originalPath;
}

bool FileRedirectHook::shouldRedirect(const std::wstring& path) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    // 如果禁用扩展名检查，则重定向所有文件
    if (!config.enableExtensionCheck) {
        // Logger::getInstance().log(L"扩展名检查已禁用，重定向所有文件: " + path);
        return true;
    }

    // 检查路径是否有效
    if (path.empty()) {
        return false;
    }

    // 获取文件扩展名
    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos) {
        return false;
    }
    std::wstring ext = path.substr(dotPos);

    // 获取配置中的扩展名列表
    std::wstring extensionsConfig = config.redirectExtensions;
    
    // 解析逗号分隔的扩展名
    std::vector<std::wstring> allowedExtensions;
    size_t start = 0;
    size_t end = extensionsConfig.find(L',');
    while (end != std::wstring::npos) {
        std::wstring ext = extensionsConfig.substr(start, end - start);
        if (!ext.empty()) {
            allowedExtensions.push_back(ext);
        }
        start = end + 1;
        end = extensionsConfig.find(L',', start);
    }
    // 添加最后一个扩展名
    std::wstring lastExt = extensionsConfig.substr(start);
    if (!lastExt.empty()) {
        allowedExtensions.push_back(lastExt);
    }

    // 如果没有配置则使用默认值
    if (allowedExtensions.empty()) {
        allowedExtensions = { L".txt" };
    }

    // 检查扩展名是否在允许列表中
    for (const auto& allowedExt : allowedExtensions) {
        if (_wcsicmp(ext.c_str(), allowedExt.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

bool FileRedirectHook::shouldSpoofFile(const std::wstring& path) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    if (!config.enableFileSpoofing) {
        return false;
    }

    // 检查路径是否有效
    if (path.empty()) {
        return false;
    }

    // 获取游戏目录
    std::wstring gameDir = Utils::getModuleDirectory();
    if (gameDir.empty()) {
        return false;
    }

    // 获取相对于游戏目录的路径
    std::wstring relativePath;
    if (path.find(gameDir) == 0) {
        relativePath = path.substr(gameDir.length());
        if (!relativePath.empty() && (relativePath.front() == L'\\' || relativePath.front() == L'/')) {
            relativePath = relativePath.substr(1);
        }
    } else {
        // 如果不是游戏目录下的文件，则直接使用路径
        relativePath = path;
    }

    // 解析逗号分隔的文件路径列表
    std::vector<std::wstring> spoofedFiles;
    size_t start = 0;
    size_t end = config.spoofedFiles.find(L',');
    while (end != std::wstring::npos) {
        std::wstring file = config.spoofedFiles.substr(start, end - start);
        if (!file.empty()) {
            spoofedFiles.push_back(file);
        }
        start = end + 1;
        end = config.spoofedFiles.find(L',', start);
    }
    // 添加最后一个文件
    std::wstring lastFile = config.spoofedFiles.substr(start);
    if (!lastFile.empty()) {
        spoofedFiles.push_back(lastFile);
    }

    // 检查文件是否在欺骗列表中
    for (const auto& spoofedFile : spoofedFiles) {
        if (_wcsicmp(relativePath.c_str(), spoofedFile.c_str()) == 0) {
            return true;
        }
    }

    // 解析逗号分隔的目录路径列表
    std::vector<std::wstring> spoofedDirectories;
    start = 0;
    end = config.spoofedDirectories.find(L',');
    while (end != std::wstring::npos) {
        std::wstring dir = config.spoofedDirectories.substr(start, end - start);
        if (!dir.empty()) {
            spoofedDirectories.push_back(dir);
        }
        start = end + 1;
        end = config.spoofedDirectories.find(L',', start);
    }
    // 添加最后一个目录
    std::wstring lastDir = config.spoofedDirectories.substr(start);
    if (!lastDir.empty()) {
        spoofedDirectories.push_back(lastDir);
    }

    // 检查文件是否在欺骗目录中
    for (const auto& spoofedDir : spoofedDirectories) {
        // 确保目录路径以反斜杠结尾
        std::wstring dirWithSlash = spoofedDir;
        if (!dirWithSlash.empty() && dirWithSlash.back() != L'\\' && dirWithSlash.back() != L'/') {
            dirWithSlash += L'\\';
        }
        
        if (relativePath.find(dirWithSlash) == 0) {
            return true;
        }
    }

    return false;
}
