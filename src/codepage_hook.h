#pragma once
#ifndef CODEPAGE_HOOK_H
#define CODEPAGE_HOOK_H

#include <windows.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"

class CodepageHook {
public:
    static CodepageHook& getInstance();
    
    bool initialize();
    void shutdown();
    
    // Hook函数声明
    static int WINAPI HookedMultiByteToWideChar(
        UINT CodePage,
        DWORD dwFlags,
        LPCSTR lpMultiByteStr,
        int cbMultiByte,
        LPWSTR lpWideCharStr,
        int cchWideChar
    );
    
    static int WINAPI HookedWideCharToMultiByte(
        UINT CodePage,
        DWORD dwFlags,
        LPCWSTR lpWideCharStr,
        int cchWideChar,
        LPSTR lpMultiByteStr,
        int cbMultiByte,
        LPCSTR lpDefaultChar,
        LPBOOL lpUsedDefaultChar
    );
    
private:
    CodepageHook();
    ~CodepageHook();
    
    static UINT getTargetCodepage(UINT originalCodepage);
    static bool isFontRelatedCall();
    
    // 原始函数指针
    static decltype(MultiByteToWideChar)* originalMultiByteToWideChar;
    static decltype(WideCharToMultiByte)* originalWideCharToMultiByte;
};

#endif // CODEPAGE_HOOK_H
