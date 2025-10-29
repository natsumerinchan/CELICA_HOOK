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
};

#endif // FONT_HOOK_H
