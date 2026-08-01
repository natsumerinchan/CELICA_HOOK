#include <windows.h>
#include <string>
#include <iostream>
#include <tlhelp32.h>
#include <shlwapi.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"
#include "author_window.h"
#include "utils.h"
#include "locale_emulator_plus.h"

#pragma comment(lib, "shlwapi.lib")

// 函数声明
DWORD findProcessByPath(const std::wstring& processPath);
bool injectDllToProcess(DWORD processId, const std::wstring& dllPath);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, int nCmdShow)
{
    // 初始化配置
    ConfigManager& configManager = ConfigManager::getInstance();
    std::wstring configFile = L"celica_hook.ini";
    
    if (!configManager.loadConfig(configFile)) {
        MessageBoxW(NULL, L"配置文件加载失败，请确保celica_hook.ini存在", L"错误", MB_ICONERROR);
        return 1;
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
    
    // 获取当前目录
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    std::wstring currentPath = currentDir;
    
    // 构建DLL完整路径
    std::wstring dllPath = currentPath + L"\\CELICA_HOOK.dll";
    
    // 检查DLL是否存在
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
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
    
    // 处理相对路径：如果路径不是绝对路径，则转换为相对于当前目录的绝对路径
    if (PathIsRelativeW(targetPath.c_str())) {
        wchar_t fullPath[MAX_PATH];
        PathCombineW(fullPath, currentDir, targetPath.c_str());
        targetPath = fullPath;
        std::wcout << L"相对路径已转换为绝对路径: " << targetPath << std::endl;
    }
    
    // 检查目标程序是否存在
    if (GetFileAttributesW(targetPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, (L"找不到目标程序: " + targetPath).c_str(), L"错误", MB_ICONERROR);
        return 1;
    }

    std::wcout << L"CELICA_HOOK 启动器" << std::endl;
    std::wcout << L"目标程序: " << targetPath << std::endl;
    std::wcout << L"DLL路径: " << dllPath << std::endl;
    std::wcout << std::endl;

    // 检查是否需要转区
    LocaleEmulatorPlus& localeEmulator = LocaleEmulatorPlus::getInstance();
    if (config.enableLocaleEmulation) {
        std::wcout << L"检测到需要转区，先转区启动目标程序..." << std::endl;
        
        // 使用转区方式启动目标程序
        if (localeEmulator.createProcessWithLocale(targetPath)) {
            std::wcout << L"转区启动成功，等待进程稳定后注入DLL..." << std::endl;
            
            // 等待一小段时间让LEP LoaderDll完成注入初始化，避免与加载器锁竞争
            const DWORD startTick = GetTickCount();
            const DWORD maxWaitMs = 3000;
            Sleep(300);

            // 轮询查找目标进程，找到后立即注入（避免窗口创建后才注入导致错过标题hook）
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
                
                // 注入DLL到已运行的进程
                if (injectDllToProcess(processId, dllPath)) {
                    std::wcout << L"DLL注入成功!" << std::endl;
                    return 0;
                } else {
                    std::wcout << L"DLL注入失败!" << std::endl;
                    return 1;
                }
            } else {
                MessageBoxW(NULL, L"无法找到转区启动的目标进程", L"错误", MB_ICONERROR);
                return 1;
            }
        } else {
            MessageBoxW(NULL, L"转区启动目标程序失败", L"错误", MB_ICONERROR);
            return 1;
        }
    } else {
        // 不需要转区，直接使用DetourCreateProcessWithDllW启动并注入
        std::wcout << L"无需转区，直接启动目标程序并注入DLL..." << std::endl;
        
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);
        
        // 使用DetourCreateProcessWithDllW启动目标程序并注入DLL
        BOOL result = DetourCreateProcessWithDllW(
            targetPath.c_str(),           // 目标 EXE 路径
            NULL,                         // 命令行参数（可为空）
            NULL,                         // 安全属性
            NULL,                         // 线程安全属性
            TRUE,                         // 是否继承句柄
            CREATE_SUSPENDED,             // 创建标志
            NULL,                         // 环境变量
            NULL,                         // 工作目录
            &si,                          // STARTUPINFO
            &pi,                          // PROCESS_INFORMATION
            "CELICA_HOOK.dll",            // DLL 路径（ANSI字符串）
            NULL);                        // 保留字段
        
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
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
        if (hProcess) {
            wchar_t exePath[MAX_PATH];
            DWORD pathSize = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &pathSize)) {
                if (_wcsicmp(exePath, processPath.c_str()) == 0) {
                    CloseHandle(hProcess);
                    CloseHandle(hSnapshot);
                    return pe.th32ProcessID;
                }
            }
            CloseHandle(hProcess);
        }
    } while (Process32NextW(hSnapshot, &pe));

    CloseHandle(hSnapshot);
    return 0;
}

// 注入DLL到指定进程
bool injectDllToProcess(DWORD processId, const std::wstring& dllPath) {
    // 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
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

    // 等待线程完成
    WaitForSingleObject(hThread, INFINITE);

    // 清理资源
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}
