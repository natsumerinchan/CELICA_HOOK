#pragma once
#ifndef WINDOW_TITLE_HOOK_H
#define WINDOW_TITLE_HOOK_H

#include <windows.h>
#include <string>
#include "settings.h"
#include "logger.h"

class WindowTitleHook {
public:
    static WindowTitleHook& getInstance();
    
    bool initialize();
    void shutdown();
    
private:
    WindowTitleHook();
    ~WindowTitleHook();
    
    // 原始函数指针
    static HWND (WINAPI* OriginalCreateWindowExW)(
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
    );
    
    static HWND (WINAPI* OriginalCreateWindowExA)(
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
    );
    
    static BOOL (WINAPI* OriginalSetWindowTextW)(HWND hWnd, LPCWSTR lpString);
    static BOOL (WINAPI* OriginalSetWindowTextA)(HWND hWnd, LPCSTR lpString);
    
    // Hook函数
    static HWND WINAPI HookedCreateWindowExW(
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
    );
    
    static HWND WINAPI HookedCreateWindowExA(
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
    );
    
    static BOOL WINAPI HookedSetWindowTextW(HWND hWnd, LPCWSTR lpString);
    static BOOL WINAPI HookedSetWindowTextA(HWND hWnd, LPCSTR lpString);
    
    // 标题处理函数
    static std::wstring processWindowTitle(const std::wstring& originalTitle);
    static bool isTitleMatch(const std::wstring& originalTitle, const std::wstring& expectedTitle);
    
    // 对当前进程中已经存在的窗口重新应用标题修改
    // （用于DLL在游戏窗口创建后才注入的情况，例如启用了自动转区时）
    static void reapplyExistingWindowTitles();
    
    // 后台线程入口：等待DllMain返回（加载器锁释放）后再重新应用标题，
    // 避免在DllMain持有加载器锁期间跨线程发送窗口消息造成死锁
    static DWORD WINAPI reapplyThreadProc(LPVOID lpParam);
    
    // 枚举当前进程顶层窗口并应用标题修改；返回true表示已找到匹配窗口
    static bool applyTitlesToExistingWindows();
    
    bool m_initialized = false;

    // 后台重应用线程及其退出事件（shutdown 时需等待其退出，避免 DLL 卸载期间
    // 线程仍在调用 Logger/进程代码导致崩溃）
    static HANDLE m_reapplyThread;
    static HANDLE m_reapplyEvent;
};

#endif // WINDOW_TITLE_HOOK_H
