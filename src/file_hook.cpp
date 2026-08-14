#include "file_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include "logger.h"
#include <string>
#include <unordered_map>

// 初始化静态成员变量
decltype(CreateFileA)* FileRedirectHook::originalCreateFileA = nullptr;
decltype(CreateFileW)* FileRedirectHook::originalCreateFileW = nullptr;
decltype(CreateFile2)* FileRedirectHook::originalCreateFile2 = nullptr;
decltype(FindFirstFileW)* FileRedirectHook::originalFindFirstFileW = nullptr;
decltype(FindFirstFileA)* FileRedirectHook::originalFindFirstFileA = nullptr;
decltype(FindNextFileW)* FileRedirectHook::originalFindNextFileW = nullptr;
decltype(FindNextFileA)* FileRedirectHook::originalFindNextFileA = nullptr;
decltype(FindClose)* FileRedirectHook::originalFindClose = nullptr;

std::wstring FileRedirectHook::m_gameDir;
std::wstring FileRedirectHook::m_redirectDir;
bool FileRedirectHook::m_initialized = false;

// ---------------------------------------------------------------------------
// 查找缓存（带锁）
// ---------------------------------------------------------------------------

// 重定向实时查找 TTL 缓存：仅缓存非写场景的"文件是否存在"结果，
// 减少每次 CreateFile 调用对重定向目录的磁盘查询；
// TTL 过期后重新检测，保证运行时新增文件仍能被发现
static std::unordered_map<std::wstring, std::pair<ULONGLONG, bool>> g_redirectLookupCache;
static CRITICAL_SECTION g_redirectLookupLock;
static bool g_redirectLookupLockInit = false;

// 查找句柄 -> 搜索目录映射：FindNextFile 无法获知搜索目录，
// 在 HookedFindFirstFile 中记录，供 FindNextFile 过滤欺骗条目时使用
static std::unordered_map<HANDLE, std::wstring> g_findSearchDirs;
static CRITICAL_SECTION g_findSearchDirsLock;
static bool g_findSearchDirsLockInit = false;

static const ULONGLONG REDIRECT_CACHE_TTL_MS = 2000;
static const size_t REDIRECT_CACHE_MAX_ENTRIES = 8192;
static const size_t FIND_SEARCH_DIRS_MAX = 4096;

// 防止递归：日志写入会触发日志文件自身被打开（ofstream -> CreateFileW），
// 若日志文件恰好命中重定向/欺骗规则会导致无限递归。
// 使用线程局部标志在 hook 处理期间屏蔽重入。
static thread_local bool g_inCreateFileHook = false;

struct CreateFileHookScope {
    CreateFileHookScope() { g_inCreateFileHook = true; }
    ~CreateFileHookScope() { g_inCreateFileHook = false; }
};

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

    // 初始化查找缓存锁与缓存（锁在进程结束前不删除：
    // 卸载时可能有线程仍在 hook 内执行，删除临界区会导致未定义行为，
    // 进程退出时由系统统一回收）
    if (!g_redirectLookupLockInit) {
        InitializeCriticalSection(&g_redirectLookupLock);
        g_redirectLookupLockInit = true;
    }
    if (!g_findSearchDirsLockInit) {
        InitializeCriticalSection(&g_findSearchDirsLock);
        g_findSearchDirsLockInit = true;
    }
    g_redirectLookupCache.clear();
    g_findSearchDirs.clear();

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

    // 文件欺骗：安装目录枚举 hook，使被欺骗的文件在目录枚举中同样不可见
    if (config.enableFileSpoofing) {
        bool enumHooksOk = true;

        originalFindFirstFileW = FindFirstFileW;
        if (DetourAttach(&(PVOID&)originalFindFirstFileW, HookedFindFirstFileW) != NO_ERROR) {
            Logger::getInstance().log(L"Hook FindFirstFileW 失败");
            enumHooksOk = false;
        }

        if (enumHooksOk) {
            originalFindFirstFileA = FindFirstFileA;
            if (DetourAttach(&(PVOID&)originalFindFirstFileA, HookedFindFirstFileA) != NO_ERROR) {
                Logger::getInstance().log(L"Hook FindFirstFileA 失败");
                enumHooksOk = false;
            }
        }

        if (enumHooksOk) {
            originalFindNextFileW = FindNextFileW;
            if (DetourAttach(&(PVOID&)originalFindNextFileW, HookedFindNextFileW) != NO_ERROR) {
                Logger::getInstance().log(L"Hook FindNextFileW 失败");
                enumHooksOk = false;
            }
        }

        if (enumHooksOk) {
            originalFindNextFileA = FindNextFileA;
            if (DetourAttach(&(PVOID&)originalFindNextFileA, HookedFindNextFileA) != NO_ERROR) {
                Logger::getInstance().log(L"Hook FindNextFileA 失败");
                enumHooksOk = false;
            }
        }

        if (enumHooksOk) {
            originalFindClose = FindClose;
            if (DetourAttach(&(PVOID&)originalFindClose, HookedFindClose) != NO_ERROR) {
                Logger::getInstance().log(L"Hook FindClose 失败");
                enumHooksOk = false;
            }
        }

        if (!enumHooksOk) {
            // 回滚已安装的枚举 hook 与文件 hook（事务由 HookManager 统一提交/中止）
            if (originalFindClose) {
                DetourDetach(&(PVOID&)originalFindClose, HookedFindClose);
                originalFindClose = nullptr;
            }
            if (originalFindNextFileA) {
                DetourDetach(&(PVOID&)originalFindNextFileA, HookedFindNextFileA);
                originalFindNextFileA = nullptr;
            }
            if (originalFindNextFileW) {
                DetourDetach(&(PVOID&)originalFindNextFileW, HookedFindNextFileW);
                originalFindNextFileW = nullptr;
            }
            if (originalFindFirstFileA) {
                DetourDetach(&(PVOID&)originalFindFirstFileA, HookedFindFirstFileA);
                originalFindFirstFileA = nullptr;
            }
            if (originalFindFirstFileW) {
                DetourDetach(&(PVOID&)originalFindFirstFileW, HookedFindFirstFileW);
                originalFindFirstFileW = nullptr;
            }
            if (originalCreateFile2) {
                DetourDetach(&(PVOID&)originalCreateFile2, HookedCreateFile2);
                originalCreateFile2 = nullptr;
            }
            DetourDetach(&(PVOID&)originalCreateFileW, HookedCreateFileW);
            DetourDetach(&(PVOID&)originalCreateFileA, HookedCreateFileA);
            return false;
        }

        Logger::getInstance().log(L"文件欺骗目录枚举hook已安装");
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

        if (originalFindFirstFileW) {
            DetourDetach(&(PVOID&)originalFindFirstFileW, HookedFindFirstFileW);
            originalFindFirstFileW = nullptr;
        }

        if (originalFindFirstFileA) {
            DetourDetach(&(PVOID&)originalFindFirstFileA, HookedFindFirstFileA);
            originalFindFirstFileA = nullptr;
        }

        if (originalFindNextFileW) {
            DetourDetach(&(PVOID&)originalFindNextFileW, HookedFindNextFileW);
            originalFindNextFileW = nullptr;
        }

        if (originalFindNextFileA) {
            DetourDetach(&(PVOID&)originalFindNextFileA, HookedFindNextFileA);
            originalFindNextFileA = nullptr;
        }

        if (originalFindClose) {
            DetourDetach(&(PVOID&)originalFindClose, HookedFindClose);
            originalFindClose = nullptr;
        }

        m_gameDir.clear();
        m_redirectDir.clear();
        m_initialized = false;

        // 清空查找缓存与句柄映射（卸载时点游戏线程已不再调用 hook）
        if (g_redirectLookupLockInit) {
            EnterCriticalSection(&g_redirectLookupLock);
            g_redirectLookupCache.clear();
            LeaveCriticalSection(&g_redirectLookupLock);
        }
        if (g_findSearchDirsLockInit) {
            EnterCriticalSection(&g_findSearchDirsLock);
            g_findSearchDirs.clear();
            LeaveCriticalSection(&g_findSearchDirsLock);
        }
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
    if (!lpFileName || g_inCreateFileHook) {
        return originalCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
    }
    CreateFileHookScope hookScope;

    const HookConfig& config = ConfigManager::getInstance().getConfig();

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
    if (!lpFileName || g_inCreateFileHook) {
        return originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
    }
    CreateFileHookScope hookScope;

    const HookConfig& config = ConfigManager::getInstance().getConfig();
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
    if (!lpFileName || g_inCreateFileHook) {
        return originalCreateFile2(lpFileName, dwDesiredAccess, dwShareMode,
                                 dwCreationDisposition, pCreateExParams);
    }
    CreateFileHookScope hookScope;

    const HookConfig& config = ConfigManager::getInstance().getConfig();
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

    return originalCreateFile2(lpFileName, dwDesiredAccess, dwShareMode,
                             dwCreationDisposition, pCreateExParams);
}

// ---------------------------------------------------------------------------
// 目录枚举 hook（文件欺骗）
// ---------------------------------------------------------------------------

bool FileRedirectHook::spoofingActive() {
    return m_initialized && ConfigManager::getInstance().getConfig().enableFileSpoofing;
}

bool FileRedirectHook::isSearchDirSpoofed(const std::wstring& searchDir) {
    if (searchDir.empty()) return false;

    std::wstring rel;
    if (!Utils::isAbsolutePath(searchDir)) {
        // 相对搜索目录：直接按相对路径判断（游戏通常以自身目录为基准枚举）
        rel = Utils::normalizePath(searchDir);
    } else {
        rel = getRelativePath(searchDir);
    }
    if (rel.empty()) return false;

    return ConfigManager::getInstance().isDirectorySpoofed(rel);
}

bool FileRedirectHook::isEntrySpoofed(const std::wstring& searchDir, const std::wstring& entryName) {
    std::wstring rel;
    if (!Utils::isAbsolutePath(searchDir)) {
        rel = Utils::normalizePath(Utils::combinePaths(searchDir, entryName));
    } else {
        rel = getRelativePath(Utils::combinePaths(searchDir, entryName));
    }
    if (rel.empty()) return false;

    ConfigManager& cm = ConfigManager::getInstance();
    if (cm.isFileSpoofed(rel)) return true;
    if (cm.isDirectorySpoofed(rel)) return true;
    return false;
}

void FileRedirectHook::recordFindSearchDir(HANDLE hFind, const std::wstring& searchDir) {
    if (!g_findSearchDirsLockInit) return;
    EnterCriticalSection(&g_findSearchDirsLock);
    if (g_findSearchDirs.size() >= FIND_SEARCH_DIRS_MAX) {
        // 超出上限整体清空（存在未调用 FindClose 的句柄泄漏），避免无界增长
        g_findSearchDirs.clear();
        Logger::getInstance().log(L"警告: 查找句柄映射达到上限，已清空");
    }
    g_findSearchDirs[hFind] = searchDir;
    LeaveCriticalSection(&g_findSearchDirsLock);
}

bool FileRedirectHook::getFindSearchDir(HANDLE hFind, std::wstring& outDir) {
    if (!g_findSearchDirsLockInit) return false;
    EnterCriticalSection(&g_findSearchDirsLock);
    auto it = g_findSearchDirs.find(hFind);
    bool found = (it != g_findSearchDirs.end());
    if (found) {
        outDir = it->second;
    }
    LeaveCriticalSection(&g_findSearchDirsLock);
    return found;
}

void FileRedirectHook::removeFindSearchDir(HANDLE hFind) {
    if (!g_findSearchDirsLockInit) return;
    EnterCriticalSection(&g_findSearchDirsLock);
    g_findSearchDirs.erase(hFind);
    LeaveCriticalSection(&g_findSearchDirsLock);
}

HANDLE WINAPI FileRedirectHook::HookedFindFirstFileW(
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData
) {
    if (!spoofingActive()) {
        return originalFindFirstFileW(lpFileName, lpFindFileData);
    }

    std::wstring searchDir = lpFileName ? Utils::getDirectory(lpFileName) : L"";

    // 搜索目录本身被欺骗：整个枚举按"不存在"处理
    if (isSearchDirSpoofed(searchDir)) {
        Logger::getInstance().log(L"文件欺骗(FindFirstFileW): 隐藏目录枚举 " +
            (searchDir.empty() ? L"(当前目录)" : searchDir));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hFind = originalFindFirstFileW(lpFileName, lpFindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return hFind;
    }
    recordFindSearchDir(hFind, searchDir);

    // 跳过被欺骗的条目
    while (isEntrySpoofed(searchDir, lpFindFileData->cFileName)) {
        if (!originalFindNextFileW(hFind, lpFindFileData)) {
            originalFindClose(hFind);
            removeFindSearchDir(hFind);
            // 所有可见条目都被欺骗：按"不存在"处理
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }
    return hFind;
}

HANDLE WINAPI FileRedirectHook::HookedFindFirstFileA(
    LPCSTR lpFileName,
    LPWIN32_FIND_DATAA lpFindFileData
) {
    if (!spoofingActive()) {
        return originalFindFirstFileA(lpFileName, lpFindFileData);
    }

    std::wstring searchDir = lpFileName ? Utils::getDirectory(Utils::ansiToWstring(lpFileName)) : L"";

    if (isSearchDirSpoofed(searchDir)) {
        Logger::getInstance().log(L"文件欺骗(FindFirstFileA): 隐藏目录枚举 " +
            (searchDir.empty() ? L"(当前目录)" : searchDir));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hFind = originalFindFirstFileA(lpFileName, lpFindFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return hFind;
    }
    recordFindSearchDir(hFind, searchDir);

    while (isEntrySpoofed(searchDir, Utils::ansiToWstring(lpFindFileData->cFileName))) {
        if (!originalFindNextFileA(hFind, lpFindFileData)) {
            originalFindClose(hFind);
            removeFindSearchDir(hFind);
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }
    return hFind;
}

BOOL WINAPI FileRedirectHook::HookedFindNextFileW(
    HANDLE hFindFile,
    LPWIN32_FIND_DATAW lpFindFileData
) {
    if (!spoofingActive()) {
        return originalFindNextFileW(hFindFile, lpFindFileData);
    }

    std::wstring searchDir;
    bool known = getFindSearchDir(hFindFile, searchDir);
    if (!known) {
        // 未知句柄（如 hook 安装前创建的枚举）：无法判断目录，直接透传
        return originalFindNextFileW(hFindFile, lpFindFileData);
    }

    while (true) {
        if (!originalFindNextFileW(hFindFile, lpFindFileData)) {
            return FALSE;
        }
        if (!isEntrySpoofed(searchDir, lpFindFileData->cFileName)) {
            return TRUE;
        }
    }
}

BOOL WINAPI FileRedirectHook::HookedFindNextFileA(
    HANDLE hFindFile,
    LPWIN32_FIND_DATAA lpFindFileData
) {
    if (!spoofingActive()) {
        return originalFindNextFileA(hFindFile, lpFindFileData);
    }

    std::wstring searchDir;
    bool known = getFindSearchDir(hFindFile, searchDir);
    if (!known) {
        return originalFindNextFileA(hFindFile, lpFindFileData);
    }

    while (true) {
        if (!originalFindNextFileA(hFindFile, lpFindFileData)) {
            return FALSE;
        }
        if (!isEntrySpoofed(searchDir, Utils::ansiToWstring(lpFindFileData->cFileName))) {
            return TRUE;
        }
    }
}

BOOL WINAPI FileRedirectHook::HookedFindClose(HANDLE hFindFile) {
    removeFindSearchDir(hFindFile);
    return originalFindClose(hFindFile);
}

// ---------------------------------------------------------------------------
// 重定向路径处理
// ---------------------------------------------------------------------------

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
        // 组件边界检查：避免 D:\game2\... 命中 D:\game 前缀
        bool boundary = normalizedPath.size() == gameDir.size() ||
                        normalizedPath[gameDir.size()] == L'\\';
        if (boundary) {
            std::wstring relativePath = normalizedPath.substr(gameDir.size());
            if (!relativePath.empty() && (relativePath.front() == L'\\')) {
                relativePath = relativePath.substr(1);
            }
            return relativePath;
        }
    }

    // 不在游戏目录下：仅当配置允许时才退化为"仅文件名"匹配。
    // 默认关闭，避免系统目录/外部目录中的同名文件被误重定向或误欺骗。
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFilenameOnlyMatch) {
        return L"";
    }
    return Utils::getFileName(normalizedPath);
}

bool FileRedirectHook::checkRedirectCache(const std::wstring& key, bool& outRedirected) {
    if (!g_redirectLookupLockInit) return false;
    EnterCriticalSection(&g_redirectLookupLock);
    auto it = g_redirectLookupCache.find(key);
    bool hit = false;
    if (it != g_redirectLookupCache.end()) {
        if (GetTickCount64() - it->second.first < REDIRECT_CACHE_TTL_MS) {
            outRedirected = it->second.second;
            hit = true;
        } else {
            g_redirectLookupCache.erase(it);
        }
    }
    LeaveCriticalSection(&g_redirectLookupLock);
    return hit;
}

void FileRedirectHook::storeRedirectCache(const std::wstring& key, bool redirected) {
    if (!g_redirectLookupLockInit) return;
    EnterCriticalSection(&g_redirectLookupLock);
    if (g_redirectLookupCache.size() >= REDIRECT_CACHE_MAX_ENTRIES) {
        // 超过上限整体清空（游戏访问的文件数量极大），避免无界增长
        g_redirectLookupCache.clear();
    }
    g_redirectLookupCache[key] = { GetTickCount64(), redirected };
    LeaveCriticalSection(&g_redirectLookupLock);
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

        std::wstring redirectedPath = Utils::combinePaths(m_redirectDir, relativePath);
        const bool isWrite = isWriteDisposition(dwCreationDisposition);
        std::wstring cacheKey = Utils::toLower(Utils::normalizePath(relativePath));

        // 2) 非写场景先查 TTL 缓存，避免每次调用都打磁盘；
        //    TTL 过期后重新检测，运行时新增的重定向文件仍会被发现
        if (!isWrite) {
            bool cachedRedirected = false;
            if (checkRedirectCache(cacheKey, cachedRedirected)) {
                return cachedRedirected ? redirectedPath : originalPath;
            }
        }

        // 3) 兜底：实时检查重定向目录下的文件是否存在
        bool exists = Utils::fileExists(redirectedPath);
        if (!isWrite) {
            storeRedirectCache(cacheKey, exists);
        }
        if (exists) {
            return redirectedPath;
        }

        // 4) 写场景：目标是重定向目录下的新文件（如存档/日志），
        //    即使文件不存在也允许重定向，并预先递归创建父目录
        if (isWrite) {
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
