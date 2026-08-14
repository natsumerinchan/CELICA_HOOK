#include <windows.h>
#include <string>
#include <iostream>
#include <cwchar>
#include <tlhelp32.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"
#include "author_window.h"
#include "utils.h"
#include "locale_emulator_plus.h"

// 函数声明
DWORD findProcessByPath(const std::wstring& processPath);
bool injectDllToProcess(DWORD processId, const std::wstring& dllPath);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, int nCmdShow)
{
    // WinMain 固定签名参数无需使用，消除 C4100 未引用参数告警
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    // 初始化配置：配置文件与启动器 EXE 同目录。
    // 通过快捷方式（设置了"起始位置"）启动时 CWD 可能与 EXE 目录不一致，
    // 因此基于模块目录构建绝对路径
    ConfigManager& configManager = ConfigManager::getInstance();
    std::wstring moduleDir = Utils::getModuleDirectory();
    std::wstring configFile = Utils::combinePaths(moduleDir, L"celica_hook.ini");
    
    if (!configManager.loadConfig(configFile)) {
        MessageBoxW(NULL, L"配置文件加载失败，请确保celica_hook.ini存在", L"错误", MB_ICONERROR);
        return 1;
    }

    // 初始化启动器日志：与游戏侧日志（celica_hook.log）分开，
    // 避免游戏加载 DLL 时以 trunc 模式清空启动器日志
    if (configManager.getConfig().enableLogging) {
        Logger::getInstance().initialize(Utils::combinePaths(moduleDir, L"celica_hook_launcher.log"));
    }

    // 显示作者信息窗口
    AuthorWindow::getInstance().show();
    
    // 等待窗口关闭（使用消息循环确保窗口正常更新）
    MSG msg;
    
    while (AuthorWindow::getInstance().isVisible()) {
        // 处理窗口消息，确保窗口正常响应
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(100);
    }
    
    // 检查用户选择：如果点击了"退出"按钮，则中止启动
    if (!AuthorWindow::getInstance().shouldLaunch()) {
        std::wcout << L"用户选择退出，启动流程已中止" << std::endl;
        return 0;
    }
    
    // DLL 必须与启动器同目录（基于模块目录而非 CWD，避免快捷方式起始位置影响）
    std::wstring dllPath = Utils::combinePaths(moduleDir, L"CELICA_HOOK.dll");
    
    // 检查DLL是否存在
    if (!Utils::fileExists(dllPath)) {
        MessageBoxW(NULL, L"找不到CELICA_HOOK.dll，请确保它与启动器在同一目录", L"错误", MB_ICONERROR);
        return 1;
    }

    // 从配置文件中获取目标程序路径
    const HookConfig& config = configManager.getConfig();
    std::wstring targetPath = config.targetProcess;
    
    // 检查目标程序路径是否配置
    if (targetPath.empty()) {
        MessageBoxW(NULL, L"未配置目标程序路径，请在celica_hook.ini中设置TargetProcess", L"错误", MB_ICONERROR);
        return 1;
    }
    
    // 相对路径基于模块目录解析，并规范化为绝对路径（GetFullPathNameW 处理 .. 等段）
    targetPath = Utils::resolveTargetPath(targetPath);
    std::wcout << L"目标程序绝对路径: " << targetPath << std::endl;
    
    // 检查目标程序是否存在
    if (!Utils::fileExists(targetPath)) {
        MessageBoxW(NULL, (L"找不到目标程序: " + targetPath).c_str(), L"错误", MB_ICONERROR);
        return 1;
    }

    std::wcout << L"CELICA_HOOK 启动器" << std::endl;
    std::wcout << L"目标程序: " << targetPath << std::endl;
    std::wcout << L"DLL路径: " << dllPath << std::endl;
    std::wcout << std::endl;

    // 目标程序的工作目录设为其所在目录
    // （对"游戏 exe 位于子目录"的场景比继承启动器 CWD 更可靠）
    std::wstring targetDir = Utils::getDirectory(targetPath);

    // 检查是否需要转区
    LocaleEmulatorPlus& localeEmulator = LocaleEmulatorPlus::getInstance();
    if (config.enableLocaleEmulation) {
        std::wcout << L"检测到需要转区，先转区启动目标程序..." << std::endl;
        
        // 使用转区方式启动目标程序
        if (localeEmulator.createProcessWithLocale(targetPath)) {
            std::wcout << L"转区启动成功，等待目标进程出现..." << std::endl;
            Logger::getInstance().log(L"转区启动成功，开始轮询目标进程: " + targetPath);
            
            // 立即开始轮询（不再先 Sleep）：若游戏启动后立刻崩溃，
            // 延迟 300ms 再轮询会完全错过该进程，导致误报"找不到进程"
            const DWORD startTick = GetTickCount();
            const DWORD maxWaitMs = 10000;  // LEP 加载器注入 + 游戏启动可能较慢，放宽窗口

            DWORD processId = 0;
            while (GetTickCount() - startTick < maxWaitMs) {
                processId = findProcessByPath(targetPath);
                if (processId != 0) {
                    break;
                }
                Sleep(50);
            }

            if (processId != 0) {
                std::wcout << L"找到目标进程ID: " << processId << L"，开始注入DLL..." << std::endl;
                
                // 找到进程后再短暂等待，让 LEP LoaderDll 完成自身的注入初始化，
                // 避免与加载器锁竞争（此时目标进程已确认存在，等待是安全的）
                Sleep(300);
                
                // 注入DLL到已运行的进程
                if (injectDllToProcess(processId, dllPath)) {
                    std::wcout << L"DLL注入成功!" << std::endl;
                    return 0;
                } else {
                    std::wcout << L"DLL注入失败!" << std::endl;
                    return 1;
                }
            } else {
                Logger::getInstance().log(L"未能在 " + std::to_wstring(maxWaitMs) + L"ms 内找到目标进程: " + targetPath);
                MessageBoxW(NULL, (L"无法找到转区启动的目标进程:\n" + targetPath +
                    L"\n\n请检查:\n"
                    L"1. 对应架构的 LoaderDll 与 LocaleEmulatorPlus DLL 是否与启动器同目录\n"
                    L"2. 游戏是否启动后立即崩溃（可先直接运行游戏验证）\n"
                    L"3. 在 celica_hook.ini 中设置 EnableLogging=1 后重试，查看 celica_hook_launcher.log 排查").c_str(),
                    L"错误", MB_ICONERROR);
                return 1;
            }
        } else {
            MessageBoxW(NULL, L"转区启动目标程序失败", L"错误", MB_ICONERROR);
            return 1;
        }
    } else {
        // 不需要转区，直接使用DetourCreateProcessWithDllExW启动并注入
        std::wcout << L"无需转区，直接启动目标程序并注入DLL..." << std::endl;
        
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);
        
        // DLL 名 "CELICA_HOOK.dll" 为纯 ASCII，可通过 Detours 的 ANSI 参数安全传递；
        // 通过 lpCurrentDirectory 把子进程工作目录锚定到游戏目录，
        // 使相对 DLL 名解析不再依赖启动器 CWD（修复快捷方式起始位置问题）
        BOOL result = DetourCreateProcessWithDllExW(
            targetPath.c_str(),           // 目标 EXE 绝对路径
            NULL,                         // 命令行参数（可为空）
            NULL,                         // 安全属性
            NULL,                         // 线程安全属性
            TRUE,                         // 是否继承句柄
            CREATE_SUSPENDED,             // 创建标志
            NULL,                         // 环境变量
            targetDir.c_str(),            // 工作目录 = 目标程序所在目录
            &si,                          // STARTUPINFO
            &pi,                          // PROCESS_INFORMATION
            "CELICA_HOOK.dll",            // DLL 路径（纯 ASCII 文件名）
            NULL);                        // 自定义 CreateProcess 例程
        
        if (!result) {
            DWORD error = GetLastError();
            std::wcout << L"启动目标程序失败，错误代码: " << error << std::endl;
            MessageBoxW(NULL, (L"无法启动目标程序: " + targetPath + L"\n错误代码: " + std::to_wstring(error)).c_str(), L"错误", MB_ICONERROR);
            return 1;
        }

        std::wcout << L"目标程序启动成功，进程ID: " << pi.dwProcessId << std::endl;
        
        // 恢复线程执行
        ResumeThread(pi.hThread);
        
        std::wcout << L"DLL注入完成!" << std::endl;
        
        // 关闭进程和线程句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return 0;
}

// 根据可执行文件路径查找进程ID
DWORD findProcessByPath(const std::wstring& processPath) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe)) {
        CloseHandle(hSnapshot);
        return 0;
    }

    do {
        // 获取进程完整路径
        // 只需 PROCESS_QUERY_INFORMATION 即可查询镜像路径；
        // 不申请 PROCESS_VM_READ，避免对受保护进程因权限过多而失败
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
        if (!hProcess) {
            // 无法打开（系统进程/权限不足），跳过
            continue;
        }

        // 循环增长的缓冲区查询镜像路径，避免 MAX_PATH 截断长路径。
        // 不用 NULL 缓冲区探测大小：不同 Windows 版本对 NULL 入参的行为
        // 不完全一致（可能不返回所需大小），直接以递增缓冲区重试更可靠。
        std::wstring exePath;
        DWORD bufSize = MAX_PATH;
        for (int attempt = 0; attempt < 8; ++attempt) {
            exePath.assign(bufSize, L'\0');
            DWORD needed = bufSize;
            if (QueryFullProcessImageNameW(hProcess, 0, &exePath[0], &needed)) {
                // 成功后按实际字符串长度截断：
                // 不同系统上 lpdwSize 的输出可能包含或不包含终止符，
                // 统一用 wcslen 求实际长度
                exePath.resize(wcslen(exePath.c_str()));
                break;
            }
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed <= bufSize) {
                exePath.clear();
                break;
            }
            bufSize = needed;
        }

        if (!exePath.empty() && _wcsicmp(exePath.c_str(), processPath.c_str()) == 0) {
            CloseHandle(hProcess);
            CloseHandle(hSnapshot);
            return pe.th32ProcessID;
        }

        CloseHandle(hProcess);
    } while (Process32NextW(hSnapshot, &pe));

    CloseHandle(hSnapshot);
    return 0;
}

// 注入DLL到指定进程
bool injectDllToProcess(DWORD processId, const std::wstring& dllPath) {
    // 打开目标进程：仅申请注入所需的最低权限，避免因权限请求过多被拒绝
    // 或触发安全软件的过度告警（PROCESS_ALL_ACCESS 会请求所有权限位）
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, processId);
    if (!hProcess) {
        std::wcout << L"无法打开目标进程，错误代码: " << GetLastError() << std::endl;
        return false;
    }

    // 在目标进程中分配内存用于DLL路径
    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, (dllPath.length() + 1) * sizeof(wchar_t),
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory) {
        std::wcout << L"无法在目标进程中分配内存，错误代码: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    // 将DLL路径写入目标进程
    if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath.c_str(),
                           (dllPath.length() + 1) * sizeof(wchar_t), NULL)) {
        std::wcout << L"无法写入目标进程内存，错误代码: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 获取LoadLibraryW函数地址
    LPTHREAD_START_ROUTINE pLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibrary) {
        std::wcout << L"无法获取LoadLibraryW地址，错误代码: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 在目标进程中创建远程线程执行LoadLibraryW
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibrary, pRemoteMemory, 0, NULL);
    if (!hThread) {
        std::wcout << L"无法创建远程线程，错误代码: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 等待线程完成（LoadLibraryW 通常很快；超时后按失败处理并清理，
    // 避免目标进程卡死时启动器无限阻塞）
    if (WaitForSingleObject(hThread, 10000) != WAIT_OBJECT_0) {
        std::wcout << L"等待LoadLibrary执行超时" << std::endl;
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 检查远程 LoadLibraryW 的返回值：如果为 NULL，说明远程加载失败
    DWORD exitCode = 0;
    BOOL gotExitCode = GetExitCodeThread(hThread, &exitCode);
    if (!gotExitCode || exitCode == 0) {
        std::wcout << L"目标进程内加载DLL失败，错误代码: " << GetLastError() << std::endl;
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 清理资源
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}
