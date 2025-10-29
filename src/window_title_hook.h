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
    static bool shouldCheckTitle(const std::wstring& originalTitle);
    static bool isTitleMatch(const std::wstring& originalTitle, const std::wstring& expectedTitle);
    
    bool m_initialized = false;
};

#endif // WINDOW_TITLE_HOOK_H
