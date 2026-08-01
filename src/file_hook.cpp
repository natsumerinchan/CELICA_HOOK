#include "file_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include "logger.h"
#include <string>

// 初始化静态成员变量
decltype(CreateFileA)* FileRedirectHook::originalCreateFileA = nullptr;
decltype(CreateFileW)* FileRedirectHook::originalCreateFileW = nullptr;
decltype(CreateFile2)* FileRedirectHook::originalCreateFile2 = nullptr;

std::wstring FileRedirectHook::m_gameDir;
std::wstring FileRedirectHook::m_redirectDir;
bool FileRedirectHook::m_initialized = false;

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

    // 缓存目录基准（DLL 注入后模块目录固定，只需计算一次）
    m_gameDir = Utils::getModuleDirectory();
    m_redirectDir = Utils::combinePaths(m_gameDir, config.redirectFolder);
    m_initialized = true;

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

    // Hook CreateFile2（Windows 8+ 引入，较新的游戏可能使用）
    // 注意：不能直接写 originalCreateFile2 = CreateFile2;，
    // 那会在导入表中生成对 kernel32!CreateFile2 的静态引用，
    // Win7 的 kernel32 没有该导出，会导致 DLL 加载失败。
    // 因此用 GetProcAddress 动态获取，Win7 上获取失败则跳过。
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        originalCreateFile2 = reinterpret_cast<decltype(CreateFile2)*>(
            GetProcAddress(hKernel32, "CreateFile2"));
    }

    if (originalCreateFile2) {
        if (DetourAttach(&(PVOID&)originalCreateFile2, HookedCreateFile2) != NO_ERROR) {
            Logger::getInstance().log(L"Hook CreateFile2 失败");
            DetourDetach(&(PVOID&)originalCreateFileW, HookedCreateFileW);
            DetourDetach(&(PVOID&)originalCreateFileA, HookedCreateFileA);
            return false;
        }
        Logger::getInstance().log(L"Hook CreateFile2 成功");
    } else {
        Logger::getInstance().log(L"CreateFile2 不可用（Windows 8 以下），跳过该 Hook");
    }

    Logger::getInstance().log(L"文件hook初始化完成");
    return true;
}

void FileRedirectHook::shutdown() {
    if (m_initialized) {
        if (originalCreateFileA) {
            DetourDetach(&(PVOID&)originalCreateFileA, HookedCreateFileA);
            originalCreateFileA = nullptr;
        }

        if (originalCreateFileW) {
            DetourDetach(&(PVOID&)originalCreateFileW, HookedCreateFileW);
            originalCreateFileW = nullptr;
        }

        if (originalCreateFile2) {
            DetourDetach(&(PVOID&)originalCreateFile2, HookedCreateFile2);
            originalCreateFile2 = nullptr;
        }

        m_gameDir.clear();
        m_redirectDir.clear();
        m_initialized = false;
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

    if (lpFileName) {
        // CreateFileA 的路径是 ANSI（系统代码页）编码，不能按 UTF-8 处理
        std::wstring wFileName = Utils::ansiToWstring(lpFileName);

        // 首先检查文件重定向（优先级最高）
        if (config.enableFileRedirect) {
            std::wstring redirectedPath = getRedirectedPath(wFileName, dwCreationDisposition);

            if (redirectedPath != wFileName) {
                Logger::getInstance().log(L"文件重定向(A): " + wFileName + L" -> " + redirectedPath);
                // 直接调用 W 版本，避免 ANSI 往返转换
                return originalCreateFileW(redirectedPath.c_str(), dwDesiredAccess, dwShareMode,
                                         lpSecurityAttributes, dwCreationDisposition,
                                         dwFlagsAndAttributes, hTemplateFile);
            }
        }

        // 然后检查文件欺骗（优先级低于文件重定向）
        if (config.enableFileSpoofing) {
            if (shouldSpoofFile(wFileName)) {
                Logger::getInstance().log(L"文件欺骗(A): " + wFileName + L" -> 文件不存在");
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
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

    if (lpFileName) {
        std::wstring fileName(lpFileName);

        // 首先检查文件重定向（优先级最高）
        if (config.enableFileRedirect) {
            std::wstring redirectedPath = getRedirectedPath(fileName, dwCreationDisposition);

            if (redirectedPath != fileName) {
                Logger::getInstance().log(L"文件重定向(W): " + fileName + L" -> " + redirectedPath);
                return originalCreateFileW(redirectedPath.c_str(), dwDesiredAccess, dwShareMode,
                                         lpSecurityAttributes, dwCreationDisposition,
                                         dwFlagsAndAttributes, hTemplateFile);
            }
        }

        // 然后检查文件欺骗（优先级低于文件重定向）
        if (config.enableFileSpoofing) {
            if (shouldSpoofFile(fileName)) {
                Logger::getInstance().log(L"文件欺骗(W): " + fileName + L" -> 文件不存在");
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
        }
    }

    return originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSecurityAttributes, dwCreationDisposition,
                             dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI FileRedirectHook::HookedCreateFile2(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    DWORD dwCreationDisposition,
    LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();

    if (lpFileName) {
        std::wstring fileName(lpFileName);

        // 首先检查文件重定向（优先级最高）
        if (config.enableFileRedirect) {
            std::wstring redirectedPath = getRedirectedPath(fileName, dwCreationDisposition);

            if (redirectedPath != fileName) {
                Logger::getInstance().log(L"文件重定向(2): " + fileName + L" -> " + redirectedPath);
                return originalCreateFile2(redirectedPath.c_str(), dwDesiredAccess, dwShareMode,
                                         dwCreationDisposition, pCreateExParams);
            }
        }

        // 然后检查文件欺骗（优先级低于文件重定向）
        if (config.enableFileSpoofing) {
            if (shouldSpoofFile(fileName)) {
                Logger::getInstance().log(L"文件欺骗(2): " + fileName + L" -> 文件不存在");
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
        }
    }

    return originalCreateFile2(lpFileName, dwDesiredAccess, dwShareMode,
                             dwCreationDisposition, pCreateExParams);
}

// 提取相对于游戏目录的路径（含 \\?\ 前缀处理）
std::wstring FileRedirectHook::getRelativePath(const std::wstring& path) {
    // 去掉长路径前缀 \\?\（如 \\?\C:\game\data.txt -> C:\game\data.txt）
    std::wstring normalizedPath = Utils::stripLongPathPrefix(path);
    if (normalizedPath.empty()) {
        return L"";
    }

    // 统一分隔符
    std::wstring gameDir = Utils::normalizePath(m_gameDir);
    normalizedPath = Utils::normalizePath(normalizedPath);

    // 游戏目录下的文件：提取相对路径
    if (Utils::startsWithIgnoreCase(normalizedPath, gameDir)) {
        std::wstring relativePath = normalizedPath.substr(gameDir.length());
        if (!relativePath.empty() && (relativePath.front() == L'\\')) {
            relativePath = relativePath.substr(1);
        }
        return relativePath;
    }

    // 不在游戏目录下：尝试仅用文件名匹配
    return Utils::getFileName(normalizedPath);
}

std::wstring FileRedirectHook::getRedirectedPath(const std::wstring& originalPath, DWORD dwCreationDisposition) {
    try {
        if (originalPath.empty() || !m_initialized) {
            return originalPath;
        }

        std::wstring relativePath = getRelativePath(originalPath);
        if (relativePath.empty()) {
            return originalPath;
        }

        const HookConfig& config = ConfigManager::getInstance().getConfig();

        // 扩展名检查（如果启用）
        if (config.enableExtensionCheck && !ConfigManager::getInstance().isExtensionRedirected(relativePath)) {
            return originalPath;
        }

        ConfigManager& cm = ConfigManager::getInstance();

        // 1) 优先使用预构建映射表（O(1) 查找，大小写不敏感）
        std::wstring mappedPath;
        if (cm.findRedirectedPath(relativePath, mappedPath)) {
            return mappedPath;
        }

        // 2) 兜底：实时检查重定向目录下的文件是否存在
        // 防止运行时新增的文件未包含在构建映射中
        std::wstring redirectedPath = Utils::combinePaths(m_redirectDir, relativePath);
        if (Utils::fileExists(redirectedPath)) {
            return redirectedPath;
        }

        // 3) 写场景：目标是重定向目录下的新文件（如存档/日志），
        //    即使文件不存在也允许重定向，并预先递归创建父目录
        if (isWriteDisposition(dwCreationDisposition)) {
            std::wstring parentDir = Utils::getDirectory(redirectedPath);
            if (!parentDir.empty() && Utils::ensureDirectoryExists(parentDir)) {
                return redirectedPath;
            }
            Logger::getInstance().log(L"创建重定向目录失败: " + parentDir);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().log(L"路径重定向异常: " + Utils::stringToWstring(e.what()));
    } catch (...) {
        Logger::getInstance().log(L"未知路径重定向异常");
    }

    return originalPath;
}

bool FileRedirectHook::shouldSpoofFile(const std::wstring& path) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();

    if (!config.enableFileSpoofing) {
        return false;
    }

    if (path.empty() || !m_initialized) {
        return false;
    }

    std::wstring relativePath = getRelativePath(path);
    if (relativePath.empty()) {
        return false;
    }

    ConfigManager& cm = ConfigManager::getInstance();

    // 文件精确匹配（大小写不敏感）
    if (cm.isFileSpoofed(relativePath)) {
        return true;
    }

    // 目录前缀匹配（含子目录下的所有文件）
    if (cm.isDirectorySpoofed(relativePath)) {
        return true;
    }

    return false;
}

bool FileRedirectHook::isWriteDisposition(DWORD dwCreationDisposition) {
    switch (dwCreationDisposition) {
        case CREATE_ALWAYS:
        case CREATE_NEW:
        case TRUNCATE_EXISTING:
            return true;
        default:
            return false;
    }
}