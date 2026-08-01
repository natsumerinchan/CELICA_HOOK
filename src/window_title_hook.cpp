#include "window_title_hook.h"
#include "detours.h"
#include <algorithm>
#include <cwctype>

// 定义原始函数指针
HWND (WINAPI* WindowTitleHook::OriginalCreateWindowExW)(
    DWORD dwExStyle,
    LPCWSTR lpClassName,
    LPCWSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
) = CreateWindowExW;

HWND (WINAPI* WindowTitleHook::OriginalCreateWindowExA)(
    DWORD dwExStyle,
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
) = CreateWindowExA;

BOOL (WINAPI* WindowTitleHook::OriginalSetWindowTextW)(HWND hWnd, LPCWSTR lpString) = SetWindowTextW;
BOOL (WINAPI* WindowTitleHook::OriginalSetWindowTextA)(HWND hWnd, LPCSTR lpString) = SetWindowTextA;

WindowTitleHook& WindowTitleHook::getInstance() {
    static WindowTitleHook instance;
    return instance;
}

WindowTitleHook::WindowTitleHook() {
}

WindowTitleHook::~WindowTitleHook() {
    shutdown();
}

bool WindowTitleHook::initialize() {
    if (m_initialized) {
        return true;
    }
    
    Logger::getInstance().log(L"开始初始化窗口标题hook");
    
    // Hook CreateWindowExW
    DetourAttach(&(PVOID&)OriginalCreateWindowExW, HookedCreateWindowExW);
    
    // Hook CreateWindowExA
    DetourAttach(&(PVOID&)OriginalCreateWindowExA, HookedCreateWindowExA);
    
    // Hook SetWindowTextW
    DetourAttach(&(PVOID&)OriginalSetWindowTextW, HookedSetWindowTextW);
    
    // Hook SetWindowTextA
    DetourAttach(&(PVOID&)OriginalSetWindowTextA, HookedSetWindowTextA);
    
    m_initialized = true;
    Logger::getInstance().log(L"窗口标题hook初始化完成");
    
    // 对已经存在的窗口重新应用标题修改
    // （转区模式下DLL是游戏启动后才注入的，主窗口可能已在注入前创建，
    //   此时CreateWindowEx/SetWindowText hook无法捕获窗口创建时的标题设置）
    reapplyExistingWindowTitles();
    
    return true;
}

void WindowTitleHook::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::getInstance().log(L"开始卸载窗口标题hook");
    
    // 卸载hook
    DetourDetach(&(PVOID&)OriginalCreateWindowExW, HookedCreateWindowExW);
    DetourDetach(&(PVOID&)OriginalCreateWindowExA, HookedCreateWindowExA);
    DetourDetach(&(PVOID&)OriginalSetWindowTextW, HookedSetWindowTextW);
    DetourDetach(&(PVOID&)OriginalSetWindowTextA, HookedSetWindowTextA);
    
    m_initialized = false;
    Logger::getInstance().log(L"窗口标题hook卸载完成");
}

HWND WINAPI WindowTitleHook::HookedCreateWindowExW(
    DWORD dwExStyle,
    LPCWSTR lpClassName,
    LPCWSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
) {
    LPCWSTR processedWindowName = lpWindowName;
    // 修复：newTitle必须保持存活到Original调用结束，否则c_str()是悬垂指针
    std::wstring newTitle;
    if (lpWindowName != nullptr) {
        std::wstring originalTitle(lpWindowName);
        newTitle = processWindowTitle(originalTitle);
        
        if (newTitle != originalTitle) {
            Logger::getInstance().log(L"CreateWindowExW: 修改窗口标题");
            Logger::getInstance().log(L"原标题: " + originalTitle);
            Logger::getInstance().log(L"新标题: " + newTitle);
            processedWindowName = newTitle.c_str();
        }
    }
    
    return OriginalCreateWindowExW(
        dwExStyle,
        lpClassName,
        processedWindowName,
        dwStyle,
        X,
        Y,
        nWidth,
        nHeight,
        hWndParent,
        hMenu,
        hInstance,
        lpParam
    );
}

BOOL WINAPI WindowTitleHook::HookedSetWindowTextW(HWND hWnd, LPCWSTR lpString) {
    LPCWSTR processedString = lpString;
    // 修复：newTitle必须保持存活到Original调用结束，否则c_str()是悬垂指针
    std::wstring newTitle;
    if (lpString != nullptr) {
        std::wstring originalTitle(lpString);
        newTitle = processWindowTitle(originalTitle);
        
        if (newTitle != originalTitle) {
            Logger::getInstance().log(L"SetWindowTextW: 修改窗口标题");
            Logger::getInstance().log(L"原标题: " + originalTitle);
            Logger::getInstance().log(L"新标题: " + newTitle);
            processedString = newTitle.c_str();
        }
    }
    
    return OriginalSetWindowTextW(hWnd, processedString);
}

HWND WINAPI WindowTitleHook::HookedCreateWindowExA(
    DWORD dwExStyle,
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
) {
    LPCSTR processedWindowName = lpWindowName;
    
    if (lpWindowName != nullptr) {
        // 将ANSI字符串转换为Unicode
        int len = MultiByteToWideChar(CP_ACP, 0, lpWindowName, -1, nullptr, 0);
        if (len > 0) {
            // 修复1：分配足够空间包含终止符
            std::wstring originalTitle(len, L'\0');
            int result = MultiByteToWideChar(CP_ACP, 0, lpWindowName, -1, &originalTitle[0], len);
            
            if (result > 0) {
                // 修复2：正确设置字符串长度
                originalTitle.resize(result - 1);
                
                std::wstring newTitle = processWindowTitle(originalTitle);
                
                if (newTitle != originalTitle) {
                    Logger::getInstance().log(L"CreateWindowExA: 修改窗口标题");
                    Logger::getInstance().log(L"原标题: " + originalTitle);
                    Logger::getInstance().log(L"新标题: " + newTitle);
                    
                    // 将Unicode字符串转换回ANSI
                    int ansiLen = WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (ansiLen > 0) {
                        // 修复3：确保ANSI字符串正确终止
                        std::vector<char> ansiBuffer(ansiLen);
                        WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, ansiBuffer.data(), ansiLen, nullptr, nullptr);
                        
                        // 使用静态字符串避免生命周期问题
                        static std::string ansiTitle;
                        ansiTitle.assign(ansiBuffer.data());
                        processedWindowName = ansiTitle.c_str();
                    }
                }
            }
        }
    }
    
    return OriginalCreateWindowExA(
        dwExStyle, lpClassName, processedWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam
    );
}

BOOL WINAPI WindowTitleHook::HookedSetWindowTextA(HWND hWnd, LPCSTR lpString) {
    LPCSTR processedString = lpString;
    
    if (lpString != nullptr) {
        int len = MultiByteToWideChar(CP_ACP, 0, lpString, -1, nullptr, 0);
        if (len > 0) {
            std::wstring originalTitle(len, L'\0');
            int result = MultiByteToWideChar(CP_ACP, 0, lpString, -1, &originalTitle[0], len);
            
            if (result > 0) {
                originalTitle.resize(result - 1);
                
                std::wstring newTitle = processWindowTitle(originalTitle);
                
                if (newTitle != originalTitle) {
                    Logger::getInstance().log(L"SetWindowTextA: 修改窗口标题");
                    
                    int ansiLen = WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (ansiLen > 0) {
                        std::vector<char> ansiBuffer(ansiLen);
                        WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, ansiBuffer.data(), ansiLen, nullptr, nullptr);
                        
                        static std::string ansiTitle;
                        ansiTitle.assign(ansiBuffer.data());
                        processedString = ansiTitle.c_str();
                    }
                }
            }
        }
    }
    
    return OriginalSetWindowTextA(hWnd, processedString);
}

std::wstring WindowTitleHook::processWindowTitle(const std::wstring& originalTitle) {
    // 如果标题检查被禁用，直接返回原标题
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableWindowTitleHook) {
        // Logger::getInstance().log(L"窗口标题hook已禁用，返回原标题: " + originalTitle);
        return originalTitle;
    }
    
    Logger::getInstance().log(L"处理窗口标题: " + originalTitle);
    Logger::getInstance().log(L"配置 - 原标题: " + config.originalWindowTitle);
    // Logger::getInstance().log(L"配置 - 新标题: " + config.newWindowTitle);
    Logger::getInstance().log(L"配置 - 启用标题检查: " + std::wstring(config.enableTitleCheck ? L"是" : L"否"));
    
    // 如果不需要检查标题，直接返回原标题
    if (!shouldCheckTitle(originalTitle)) {
        // Logger::getInstance().log(L"跳过标题检查，返回原标题: " + originalTitle);
        return originalTitle;
    }
    
    // 检查原标题是否与配置中的预期标题匹配
    if (isTitleMatch(originalTitle, config.originalWindowTitle)) {
        // 如果匹配，检查新标题是否为空
        if (!config.newWindowTitle.empty()) {
            Logger::getInstance().log(L"标题匹配成功，修改为: " + config.newWindowTitle);
            return config.newWindowTitle;
        }
        Logger::getInstance().log(L"标题匹配成功但新标题为空，返回原标题: " + originalTitle);
    }
    
    // 如果不匹配，返回原标题
    Logger::getInstance().log(L"标题不匹配，返回原标题: " + originalTitle);
    return originalTitle;
}

bool WindowTitleHook::shouldCheckTitle(const std::wstring& originalTitle) {
    // 空标题不需要检查
    if (originalTitle.empty()) {
        return false;
    }
    
    // 如果禁用标题检查，直接返回false
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableTitleCheck) {
        return false;
    }
    
    // 这里可以添加其他逻辑来决定是否需要检查标题
    // 例如：只检查特定窗口类的标题等
    
    return true;
}

bool WindowTitleHook::isTitleMatch(const std::wstring& originalTitle, const std::wstring& expectedTitle) {
    // 如果预期标题为空，则总是匹配
    if (expectedTitle.empty()) {
        return true;
    }
    
    // 简单的大小写不敏感比较
    std::wstring originalLower = originalTitle;
    std::wstring expectedLower = expectedTitle;
    
    std::transform(originalLower.begin(), originalLower.end(), originalLower.begin(), std::towlower);
    std::transform(expectedLower.begin(), expectedLower.end(), expectedLower.begin(), std::towlower);
    
    return originalLower == expectedLower;
}
void WindowTitleHook::reapplyExistingWindowTitles() {
    // 在后台线程执行标题重新应用，避免在DllMain（持有加载器锁）期间
    // 跨线程向游戏主线程发送窗口消息导致死锁
    HANDLE hThread = CreateThread(NULL, 0, reapplyThreadProc, NULL, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
}

// 后台线程：等待DllMain返回（加载器锁释放）且游戏完成初始加载后，
// 枚举已有窗口并重新应用标题修改；窗口稍晚创建时通过多次重试覆盖。
DWORD WINAPI WindowTitleHook::reapplyThreadProc(LPVOID lpParam) {

    Logger::getInstance().log(L"开始对已有窗口重新应用标题修改");

    // 多次重试，覆盖游戏窗口稍晚才创建的情况
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (applyTitlesToExistingWindows()) {
            break;
        }
        Sleep(500);
    }

    Logger::getInstance().log(L"已有窗口标题重新应用完成");
    return 0;
}

// 枚举当前进程的顶层窗口，对匹配的窗口重新应用标题修改。
// 返回true表示本次找到了匹配窗口（后续无需继续重试）。
bool WindowTitleHook::applyTitlesToExistingWindows() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableWindowTitleHook) {
        return true;
    }
    if (config.newWindowTitle.empty()) {
        return true;
    }

    bool foundAny = false;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        // 只处理当前进程的顶层窗口
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != GetCurrentProcessId()) {
            return TRUE;
        }

        wchar_t buffer[1024] = { 0 };
        const int len = GetWindowTextW(hwnd, buffer, 1024);
        if (len <= 0) {
            return TRUE;
        }

        const std::wstring currentTitle(buffer, len);
        const std::wstring newTitle = processWindowTitle(currentTitle);

        if (newTitle != currentTitle) {
            *reinterpret_cast<bool*>(lParam) = true;

            // 检查新标题能否在当前ANSI代码页下无损表示；不能则跳过，
            // 避免在转区（如日文CP932）下把中文标题设置成"??????"
            int aLen = WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string ansiTitle(aLen > 0 ? aLen : 1, L"\0"[0]);
            if (aLen > 0) { WideCharToMultiByte(CP_ACP, 0, newTitle.c_str(), -1, &ansiTitle[0], aLen, nullptr, nullptr); }
            int wLen = MultiByteToWideChar(CP_ACP, 0, ansiTitle.c_str(), -1, nullptr, 0);
            std::wstring roundTrip(wLen > 0 ? wLen : 1, L"\0"[0]);
            if (wLen > 0) { MultiByteToWideChar(CP_ACP, 0, ansiTitle.c_str(), -1, &roundTrip[0], wLen); }
            if (wLen > 0) { roundTrip.resize(wLen - 1); }
            if (roundTrip == newTitle) {
                Logger::getInstance().log(L"重新应用窗口标题: " + currentTitle + L" -> " + newTitle);
                // 使用带超时的消息发送，避免游戏主线程忙碌时无限阻塞后台线程
                SendMessageTimeoutW(hwnd, WM_SETTEXT, 0, (LPARAM)newTitle.c_str(),
                    SMTO_ABORTIFHUNG, 1000, nullptr);
            } else {
                Logger::getInstance().log(L"新标题无法在当前ANSI代码页下表示，跳过: " + newTitle);
            }
        }

        return TRUE;
    }, (LPARAM)&foundAny);

    return foundAny;
}
