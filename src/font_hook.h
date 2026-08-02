#pragma once
#ifndef FONT_HOOK_H
#define FONT_HOOK_H

#include <windows.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"

class FontHook {
public:
    static FontHook& getInstance();
    
    bool initialize();
    void shutdown();
    
    // 获取原始EnumFontFamiliesExW函数指针（供isFontAvailable等内部检测使用，绕过hook）
    static decltype(EnumFontFamiliesExW)* getRawEnumFontFamiliesExW();
    
    // Hook函数声明
    static HFONT WINAPI HookedCreateFontA(
        int nHeight,
        int nWidth,
        int nEscapement,
        int nOrientation,
        int fnWeight,
        DWORD fdwItalic,
        DWORD fdwUnderline,
        DWORD fdwStrikeOut,
        DWORD fdwCharSet,
        DWORD fdwOutputPrecision,
        DWORD fdwClipPrecision,
        DWORD fdwQuality,
        DWORD fdwPitchAndFamily,
        LPCSTR lpszFace
    );
    
    static HFONT WINAPI HookedCreateFontW(
        int nHeight,
        int nWidth,
        int nEscapement,
        int nOrientation,
        int fnWeight,
        DWORD fdwItalic,
        DWORD fdwUnderline,
        DWORD fdwStrikeOut,
        DWORD fdwCharSet,
        DWORD fdwOutputPrecision,
        DWORD fdwClipPrecision,
        DWORD fdwQuality,
        DWORD fdwPitchAndFamily,
        LPCWSTR lpszFace
    );
    
    static HFONT WINAPI HookedCreateFontIndirectA(const LOGFONTA* lplf);
    static HFONT WINAPI HookedCreateFontIndirectW(const LOGFONTW* lplf);
    
    // EnumFontFamiliesEx 钩子函数声明
    static int WINAPI HookedEnumFontFamiliesExA(
        HDC hdc,
        LPLOGFONTA lpLogfont,
        FONTENUMPROCA lpProc,
        LPARAM lParam,
        DWORD dwFlags
    );
    
    static int WINAPI HookedEnumFontFamiliesExW(
        HDC hdc,
        LPLOGFONTW lpLogfont,
        FONTENUMPROCW lpProc,
        LPARAM lParam,
        DWORD dwFlags
    );
    
private:
    FontHook();
    ~FontHook();
    
    static void modifyFontParams(LOGFONTA* lf);
    static void modifyFontParams(LOGFONTW* lf);
    
    // 原始函数指针
    static decltype(CreateFontA)* originalCreateFontA;
    static decltype(CreateFontW)* originalCreateFontW;
    static decltype(CreateFontIndirectA)* originalCreateFontIndirectA;
    static decltype(CreateFontIndirectW)* originalCreateFontIndirectW;
    static decltype(EnumFontFamiliesExA)* originalEnumFontFamiliesExA;
    static decltype(EnumFontFamiliesExW)* originalEnumFontFamiliesExW;
    
    // hook安装标志：区分"有效原始地址"与"已实际attach"，
    // 避免shutdown对未attach的指针执行无效DetourDetach
    static bool m_createFontAHooked;
    static bool m_createFontWHooked;
    static bool m_createFontIndirectAHooked;
    static bool m_createFontIndirectWHooked;
    static bool m_enumFontFamiliesExAHooked;
    static bool m_enumFontFamiliesExWHooked;
};

#endif // FONT_HOOK_H