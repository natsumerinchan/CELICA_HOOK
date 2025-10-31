#include "font_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include <string>

// 使用Windows SDK中已定义的SHIFTJIS_CHARSET
// #define SHIFTJIS_CHARSET 0x80  // Windows SDK中已定义

// Shift-JIS到UTF-16转换函数 - 绕过代码页hook直接处理
static std::wstring convertShiftJISToUTF16(const std::string& sjisStr) {
    if (sjisStr.empty()) return L"";
    
    // 直接调用原始MultiByteToWideChar函数，绕过代码页hook
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return L"获取kernel32模块失败";
    }
    
    typedef int (WINAPI *MultiByteToWideCharFunc)(UINT, DWORD, LPCSTR, int, LPWSTR, int);
    MultiByteToWideCharFunc originalMultiByteToWideChar = 
        (MultiByteToWideCharFunc)GetProcAddress(kernel32, "MultiByteToWideChar");
    
    if (!originalMultiByteToWideChar) {
        return L"获取MultiByteToWideChar函数失败";
    }
    
    int size_needed = originalMultiByteToWideChar(932, 0, sjisStr.c_str(), (int)sjisStr.size(), NULL, 0);
    if (size_needed == 0) {
        return L"转换失败";
    }
    
    std::wstring result(size_needed, 0);
    originalMultiByteToWideChar(932, 0, sjisStr.c_str(), (int)sjisStr.size(), &result[0], size_needed);
    return result;
}

// 初始化静态成员变量
decltype(CreateFontA)* FontHook::originalCreateFontA = nullptr;
decltype(CreateFontW)* FontHook::originalCreateFontW = nullptr;
decltype(CreateFontIndirectA)* FontHook::originalCreateFontIndirectA = nullptr;
decltype(CreateFontIndirectW)* FontHook::originalCreateFontIndirectW = nullptr;
decltype(EnumFontFamiliesExA)* FontHook::originalEnumFontFamiliesExA = nullptr;
decltype(EnumFontFamiliesExW)* FontHook::originalEnumFontFamiliesExW = nullptr;

FontHook& FontHook::getInstance() {
    static FontHook instance;
    return instance;
}

FontHook::FontHook() {
}

FontHook::~FontHook() {
    shutdown();
}

bool FontHook::initialize() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        Logger::getInstance().log(L"字体hook功能已禁用");
        return true;
    }
    
    Logger::getInstance().log(L"初始化字体hook");
    
    // Hook CreateFontA
    originalCreateFontA = CreateFontA;
    if (DetourAttach(&(PVOID&)originalCreateFontA, HookedCreateFontA) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFontA 失败");
        return false;
    }
    
    // Hook CreateFontW
    originalCreateFontW = CreateFontW;
    if (DetourAttach(&(PVOID&)originalCreateFontW, HookedCreateFontW) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFontW 失败");
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        return false;
    }
    
    // Hook CreateFontIndirectA
    originalCreateFontIndirectA = CreateFontIndirectA;
    if (DetourAttach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFontIndirectA 失败");
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        return false;
    }
    
    // Hook CreateFontIndirectW
    originalCreateFontIndirectW = CreateFontIndirectW;
    if (DetourAttach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW) != NO_ERROR) {
        Logger::getInstance().log(L"Hook CreateFontIndirectW 失败");
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        DetourDetach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA);
        return false;
    }
    
    // Hook EnumFontFamiliesExA
    originalEnumFontFamiliesExA = EnumFontFamiliesExA;
    if (DetourAttach(&(PVOID&)originalEnumFontFamiliesExA, HookedEnumFontFamiliesExA) != NO_ERROR) {
        Logger::getInstance().log(L"Hook EnumFontFamiliesExA 失败");
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        DetourDetach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA);
        DetourDetach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW);
        return false;
    }
    
    // Hook EnumFontFamiliesExW
    originalEnumFontFamiliesExW = EnumFontFamiliesExW;
    if (DetourAttach(&(PVOID&)originalEnumFontFamiliesExW, HookedEnumFontFamiliesExW) != NO_ERROR) {
        Logger::getInstance().log(L"Hook EnumFontFamiliesExW 失败");
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        DetourDetach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA);
        DetourDetach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW);
        DetourDetach(&(PVOID&)originalEnumFontFamiliesExA, HookedEnumFontFamiliesExA);
        return false;
    }
    
    Logger::getInstance().log(L"字体hook初始化完成");
    return true;
}

void FontHook::shutdown() {
    if (originalCreateFontA) {
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        originalCreateFontA = nullptr;
    }
    
    if (originalCreateFontW) {
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        originalCreateFontW = nullptr;
    }
    
    if (originalCreateFontIndirectA) {
        DetourDetach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA);
        originalCreateFontIndirectA = nullptr;
    }
    
    if (originalCreateFontIndirectW) {
        DetourDetach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW);
        originalCreateFontIndirectW = nullptr;
    }
    
    if (originalEnumFontFamiliesExA) {
        DetourDetach(&(PVOID&)originalEnumFontFamiliesExA, HookedEnumFontFamiliesExA);
        originalEnumFontFamiliesExA = nullptr;
    }
    
    if (originalEnumFontFamiliesExW) {
        DetourDetach(&(PVOID&)originalEnumFontFamiliesExW, HookedEnumFontFamiliesExW);
        originalEnumFontFamiliesExW = nullptr;
    }
    
    Logger::getInstance().log(L"字体hook已卸载");
}

HFONT WINAPI FontHook::HookedCreateFontA(
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
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        return originalCreateFontA(nHeight, nWidth, nEscapement, nOrientation,
                                 fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                                 fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
                                 fdwQuality, fdwPitchAndFamily, lpszFace);
    }
    
    // 记录原始字体信息
    std::string originalFaceName = lpszFace ? std::string(lpszFace) : "默认字体";
    std::wstring originalFaceNameW;
    
    // 根据字符集正确转换字体名称
    if (fdwCharSet == SHIFTJIS_CHARSET) {
        // 日文游戏使用Shift-JIS编码
        originalFaceNameW = convertShiftJISToUTF16(originalFaceName);
    } else {
        // 其他情况使用默认转换
        originalFaceNameW = Utils::stringToWstring(originalFaceName);
    }
    
    Logger::getInstance().log(L"原字体(A): " + originalFaceNameW + 
                            L", Charset=" + Utils::intToHexString(fdwCharSet) +
                            L", Height=" + std::to_wstring(nHeight) +
                            L", Width=" + std::to_wstring(nWidth) +
                            L", Weight=" + std::to_wstring(fnWeight));
    
    // 修改字体参数
    int newHeight = (config.fontHeight > 0) ? config.fontHeight : nHeight;
    int newWidth = (config.fontWidth > 0) ? config.fontWidth : nWidth;
    int newWeight = (config.fontWeight > 0) ? config.fontWeight : fnWeight;
    DWORD newCharSet = (config.localeCharset > 0) ? config.localeCharset : fdwCharSet;
    
    // 转换为宽字符版本处理
    std::wstring wFaceName;
    if (lpszFace) {
        // 根据字符集正确转换字体名称
        if (fdwCharSet == SHIFTJIS_CHARSET) {
            // 日文游戏使用Shift-JIS编码
            wFaceName = convertShiftJISToUTF16(lpszFace);
        } else {
            // 其他情况使用默认转换
            wFaceName = Utils::stringToWstring(lpszFace);
        }
    }
    
    // 记录新字体信息
    std::wstring newFaceNameW;
    if (config.fontName.empty()) {
        newFaceNameW = wFaceName.empty() ? L"默认字体" : wFaceName;
    } else {
        newFaceNameW = config.fontName;
    }
    
    Logger::getInstance().log(L"新字体(A): " + newFaceNameW +
                            L", Charset=" + Utils::intToHexString(newCharSet) +
                            L", Height=" + std::to_wstring(newHeight) +
                            L", Width=" + std::to_wstring(newWidth) +
                            L", Weight=" + std::to_wstring(newWeight));
    
    return HookedCreateFontW(nHeight, nWidth, nEscapement, nOrientation,
                           fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                           fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
                           fdwQuality, fdwPitchAndFamily, wFaceName.c_str());
}

HFONT WINAPI FontHook::HookedCreateFontW(
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
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        return originalCreateFontW(nHeight, nWidth, nEscapement, nOrientation,
                                 fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                                 fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
                                 fdwQuality, fdwPitchAndFamily, lpszFace);
    }
    
    // 记录原始字体信息
    std::wstring originalFaceName = lpszFace ? std::wstring(lpszFace) : L"默认字体";
    Logger::getInstance().log(L"原字体(W): " + originalFaceName + 
                            L", Charset=" + Utils::intToHexString(fdwCharSet) +
                            L", Height=" + std::to_wstring(nHeight) +
                            L", Width=" + std::to_wstring(nWidth) +
                            L", Weight=" + std::to_wstring(fnWeight));
    
    // 修改字体参数
    int newHeight = (config.fontHeight > 0) ? config.fontHeight : nHeight;
    int newWidth = (config.fontWidth > 0) ? config.fontWidth : nWidth;
    int newWeight = (config.fontWeight > 0) ? config.fontWeight : fnWeight;
    DWORD newCharSet = (config.localeCharset > 0) ? config.localeCharset : fdwCharSet;
    
    LPCWSTR newFaceName = lpszFace;
    std::wstring customFaceName;
    if (!config.fontName.empty()) {
        customFaceName = config.fontName;
        newFaceName = customFaceName.c_str();
    }
    
    Logger::getInstance().log(L"新字体(W): " + std::wstring(newFaceName) +
                            L", Charset=" + Utils::intToHexString(newCharSet) +
                            L", Height=" + std::to_wstring(newHeight) +
                            L", Width=" + std::to_wstring(newWidth) +
                            L", Weight=" + std::to_wstring(newWeight));
    
    return originalCreateFontW(newHeight, nWidth, nEscapement, nOrientation,
                             newWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                             newCharSet, fdwOutputPrecision, fdwClipPrecision,
                             fdwQuality, fdwPitchAndFamily, newFaceName);
}

HFONT WINAPI FontHook::HookedCreateFontIndirectA(const LOGFONTA* lplf) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook || !lplf) {
        return originalCreateFontIndirectA(lplf);
    }
    
    // 记录原始字体信息
    std::string originalFaceName = lplf->lfFaceName;
    std::wstring originalFaceNameW;
    
    // 根据字符集正确转换字体名称
    if (lplf->lfCharSet == SHIFTJIS_CHARSET) {
        // 日文游戏使用Shift-JIS编码
        // 直接读取原始内存数据，避免代码页hook的影响
        originalFaceNameW = convertShiftJISToUTF16(originalFaceName);
    } else {
        // 其他情况使用默认转换
        originalFaceNameW = Utils::stringToWstring(originalFaceName);
    }
    
    Logger::getInstance().log(L"原字体(IA): " + originalFaceNameW + 
                            L", Charset=" + Utils::intToHexString(lplf->lfCharSet) +
                            L", Height=" + std::to_wstring(lplf->lfHeight) +
                            L", Width=" + std::to_wstring(lplf->lfWidth) +
                            L", Weight=" + std::to_wstring(lplf->lfWeight));
    
    LOGFONTA modifiedLf = *lplf;
    modifyFontParams(&modifiedLf);
    
    // 记录新字体信息
    std::wstring newFaceNameW;
    if (config.fontName.empty()) {
        newFaceNameW = originalFaceNameW;
    } else {
        newFaceNameW = config.fontName;
    }
    
    Logger::getInstance().log(L"新字体(IA): " + newFaceNameW +
                            L", Charset=" + Utils::intToHexString(modifiedLf.lfCharSet) +
                            L", Height=" + std::to_wstring(modifiedLf.lfHeight) +
                            L", Width=" + std::to_wstring(modifiedLf.lfWidth) +
                            L", Weight=" + std::to_wstring(modifiedLf.lfWeight));
    
    return originalCreateFontIndirectA(&modifiedLf);
}

HFONT WINAPI FontHook::HookedCreateFontIndirectW(const LOGFONTW* lplf) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook || !lplf) {
        return originalCreateFontIndirectW(lplf);
    }
    
    // 记录原始字体信息
    std::wstring originalFaceName = lplf->lfFaceName;
    Logger::getInstance().log(L"原字体(IW): " + originalFaceName + 
                            L", Charset=" + Utils::intToHexString(lplf->lfCharSet) +
                            L", Height=" + std::to_wstring(lplf->lfHeight) +
                            L", Width=" + std::to_wstring(lplf->lfWidth) +
                            L", Weight=" + std::to_wstring(lplf->lfWeight));
    
    LOGFONTW modifiedLf = *lplf;
    modifyFontParams(&modifiedLf);
    
    // 记录新字体信息
    std::wstring newFaceNameW;
    if (config.fontName.empty()) {
        newFaceNameW = originalFaceName;
    } else {
        newFaceNameW = config.fontName;
    }
    
    Logger::getInstance().log(L"新字体(IW): " + newFaceNameW +
                            L", Charset=" + Utils::intToHexString(modifiedLf.lfCharSet) +
                            L", Height=" + std::to_wstring(modifiedLf.lfHeight) +
                            L", Width=" + std::to_wstring(modifiedLf.lfWidth) +
                            L", Weight=" + std::to_wstring(modifiedLf.lfWeight));
    
    return originalCreateFontIndirectW(&modifiedLf);
}

void FontHook::modifyFontParams(LOGFONTA* lf) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    if (config.fontHeight > 0) {
        lf->lfHeight = config.fontHeight;
    }
    
    if (config.fontWidth > 0) {
        lf->lfWidth = config.fontWidth;
    }
    
    if (config.fontWeight > 0) {
        lf->lfWeight = config.fontWeight;
    }
    
    if (config.localeCharset > 0) {
        lf->lfCharSet = config.localeCharset;
    }
    
    if (!config.fontName.empty()) {
        std::string fontNameA = Utils::wstringToANSI(config.fontName);
        strncpy_s(lf->lfFaceName, LF_FACESIZE, fontNameA.c_str(), _TRUNCATE);
    }
    
    Logger::getInstance().log(L"字体修改: Charset=" + Utils::intToHexString(lf->lfCharSet) +
                            L", Height=" + std::to_wstring(lf->lfHeight) +
                            L", Width=" + std::to_wstring(lf->lfWidth) +
                            L", Weight=" + std::to_wstring(lf->lfWeight));
}

void FontHook::modifyFontParams(LOGFONTW* lf) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    if (config.fontHeight > 0) {
        lf->lfHeight = config.fontHeight;
    }
    
    if (config.fontWidth > 0) {
        lf->lfWidth = config.fontWidth;
    }
    
    if (config.fontWeight > 0) {
        lf->lfWeight = config.fontWeight;
    }
    
    if (config.localeCharset > 0) {
        lf->lfCharSet = config.localeCharset;
    }
    
    if (!config.fontName.empty()) {
        wcsncpy_s(lf->lfFaceName, LF_FACESIZE, config.fontName.c_str(), _TRUNCATE);
    }
    
    Logger::getInstance().log(L"字体修改: Charset=" + Utils::intToHexString(lf->lfCharSet) +
                            L", Height=" + std::to_wstring(lf->lfHeight) +
                            L", Width=" + std::to_wstring(lf->lfWidth) +
                            L", Weight=" + std::to_wstring(lf->lfWeight));
}

// EnumFontFamiliesExA 钩子函数 - 重定向到 EnumFontFamiliesExW
int WINAPI FontHook::HookedEnumFontFamiliesExA(
    HDC hdc,
    LPLOGFONTA lpLogfont,
    FONTENUMPROCA lpProc,
    LPARAM lParam,
    DWORD dwFlags
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        return originalEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }
    
    Logger::getInstance().log(L"EnumFontFamiliesExA 被调用");
    
    // 如果 lpLogfont 为 NULL，直接调用原始函数
    if (!lpLogfont) {
        return originalEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }
    
    // 记录原始字体信息
    std::string originalFaceName = lpLogfont->lfFaceName;
    std::wstring originalFaceNameW;
    
    // 根据字符集正确转换字体名称
    if (lpLogfont->lfCharSet == SHIFTJIS_CHARSET) {
        // 日文游戏使用Shift-JIS编码
        originalFaceNameW = convertShiftJISToUTF16(originalFaceName);
    } else {
        // 其他情况使用默认转换
        originalFaceNameW = Utils::stringToWstring(originalFaceName);
    }
    
    Logger::getInstance().log(L"原始字体枚举(A): " + originalFaceNameW + 
                            L", Charset=" + Utils::intToHexString(lpLogfont->lfCharSet));
    
    // 创建修改后的 LOGFONTW 结构
    LOGFONTW logfontW = {0};
    logfontW.lfHeight = lpLogfont->lfHeight;
    logfontW.lfWidth = lpLogfont->lfWidth;
    logfontW.lfEscapement = lpLogfont->lfEscapement;
    logfontW.lfOrientation = lpLogfont->lfOrientation;
    logfontW.lfWeight = lpLogfont->lfWeight;
    logfontW.lfItalic = lpLogfont->lfItalic;
    logfontW.lfUnderline = lpLogfont->lfUnderline;
    logfontW.lfStrikeOut = lpLogfont->lfStrikeOut;
    logfontW.lfCharSet = lpLogfont->lfCharSet;
    logfontW.lfOutPrecision = lpLogfont->lfOutPrecision;
    logfontW.lfClipPrecision = lpLogfont->lfClipPrecision;
    logfontW.lfQuality = lpLogfont->lfQuality;
    logfontW.lfPitchAndFamily = lpLogfont->lfPitchAndFamily;
    
    // 设置字体名称
    if (!config.fontName.empty()) {
        // 使用配置的字体名称
        wcsncpy_s(logfontW.lfFaceName, LF_FACESIZE, config.fontName.c_str(), _TRUNCATE);
    } else {
        // 使用原始字体名称（转换为宽字符）
        wcsncpy_s(logfontW.lfFaceName, LF_FACESIZE, originalFaceNameW.c_str(), _TRUNCATE);
    }
    
    // 修改字符集
    if (config.localeCharset > 0) {
        logfontW.lfCharSet = config.localeCharset;
    }
    
    // 创建回调包装器，将宽字符字体信息转换为ANSI
    struct CallbackWrapper {
        FONTENUMPROCA originalProc;
        LPARAM originalParam;
        
        static int CALLBACK EnumProcWrapper(const LOGFONTW* lplf, const TEXTMETRICW* lptm, DWORD dwType, LPARAM lParam) {
            CallbackWrapper* wrapper = reinterpret_cast<CallbackWrapper*>(lParam);
            
            // 将 LOGFONTW 转换为 LOGFONTA
            LOGFONTA logfontA = {0};
            logfontA.lfHeight = lplf->lfHeight;
            logfontA.lfWidth = lplf->lfWidth;
            logfontA.lfEscapement = lplf->lfEscapement;
            logfontA.lfOrientation = lplf->lfOrientation;
            logfontA.lfWeight = lplf->lfWeight;
            logfontA.lfItalic = lplf->lfItalic;
            logfontA.lfUnderline = lplf->lfUnderline;
            logfontA.lfStrikeOut = lplf->lfStrikeOut;
            logfontA.lfCharSet = lplf->lfCharSet;
            logfontA.lfOutPrecision = lplf->lfOutPrecision;
            logfontA.lfClipPrecision = lplf->lfClipPrecision;
            logfontA.lfQuality = lplf->lfQuality;
            logfontA.lfPitchAndFamily = lplf->lfPitchAndFamily;
            
            // 转换字体名称
            std::string faceNameA = Utils::wstringToANSI(lplf->lfFaceName);
            strncpy_s(logfontA.lfFaceName, LF_FACESIZE, faceNameA.c_str(), _TRUNCATE);
            
            // 将 TEXTMETRICW 转换为 TEXTMETRICA
            TEXTMETRICA textmetricA = {0};
            textmetricA.tmHeight = lptm->tmHeight;
            textmetricA.tmAscent = lptm->tmAscent;
            textmetricA.tmDescent = lptm->tmDescent;
            textmetricA.tmInternalLeading = lptm->tmInternalLeading;
            textmetricA.tmExternalLeading = lptm->tmExternalLeading;
            textmetricA.tmAveCharWidth = lptm->tmAveCharWidth;
            textmetricA.tmMaxCharWidth = lptm->tmMaxCharWidth;
            textmetricA.tmWeight = lptm->tmWeight;
            textmetricA.tmOverhang = lptm->tmOverhang;
            textmetricA.tmDigitizedAspectX = lptm->tmDigitizedAspectX;
            textmetricA.tmDigitizedAspectY = lptm->tmDigitizedAspectY;
            textmetricA.tmFirstChar = static_cast<BYTE>(lptm->tmFirstChar);
            textmetricA.tmLastChar = static_cast<BYTE>(lptm->tmLastChar);
            textmetricA.tmDefaultChar = static_cast<BYTE>(lptm->tmDefaultChar);
            textmetricA.tmBreakChar = static_cast<BYTE>(lptm->tmBreakChar);
            textmetricA.tmItalic = lptm->tmItalic;
            textmetricA.tmUnderlined = lptm->tmUnderlined;
            textmetricA.tmStruckOut = lptm->tmStruckOut;
            textmetricA.tmPitchAndFamily = lptm->tmPitchAndFamily;
            textmetricA.tmCharSet = lptm->tmCharSet;
            
            return wrapper->originalProc(&logfontA, &textmetricA, dwType, wrapper->originalParam);
        }
    };
    
    CallbackWrapper wrapper = {lpProc, lParam};
    
    // 调用宽字符版本
    int result = originalEnumFontFamiliesExW(hdc, &logfontW, CallbackWrapper::EnumProcWrapper, reinterpret_cast<LPARAM>(&wrapper), dwFlags);
    
    Logger::getInstance().log(L"EnumFontFamiliesExA 完成，结果=" + std::to_wstring(result));
    return result;
}

// EnumFontFamiliesExW 钩子函数
int WINAPI FontHook::HookedEnumFontFamiliesExW(
    HDC hdc,
    LPLOGFONTW lpLogfont,
    FONTENUMPROCW lpProc,
    LPARAM lParam,
    DWORD dwFlags
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        return originalEnumFontFamiliesExW(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }
    
    Logger::getInstance().log(L"EnumFontFamiliesExW 被调用");
    
    // 如果 lpLogfont 为 NULL，直接调用原始函数
    if (!lpLogfont) {
        return originalEnumFontFamiliesExW(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }
    
    // 记录原始字体信息
    std::wstring originalFaceName = lpLogfont->lfFaceName;
    Logger::getInstance().log(L"原始字体枚举(W): " + originalFaceName + 
                            L", 字符集=" + Utils::intToHexString(lpLogfont->lfCharSet));
    
    // 创建修改后的 LOGFONTW 结构
    LOGFONTW modifiedLogfont = *lpLogfont;
    
    // 修改字体参数
    if (!config.fontName.empty()) {
        wcsncpy_s(modifiedLogfont.lfFaceName, LF_FACESIZE, config.fontName.c_str(), _TRUNCATE);
    }
    
    if (config.localeCharset > 0) {
        modifiedLogfont.lfCharSet = config.localeCharset;
    }
    
    // 创建回调包装器，应用字体修改
    struct CallbackWrapperW {
        FONTENUMPROCW originalProc;
        LPARAM originalParam;
        
        static int CALLBACK EnumProcWrapper(const LOGFONTW* lplf, const TEXTMETRICW* lptm, DWORD dwType, LPARAM lParam) {
            CallbackWrapperW* wrapper = reinterpret_cast<CallbackWrapperW*>(lParam);
            
            // 创建修改后的字体信息
            LOGFONTW modifiedLf = *lplf;
            const HookConfig& config = ConfigManager::getInstance().getConfig();
            
            if (!config.fontName.empty()) {
                wcsncpy_s(modifiedLf.lfFaceName, LF_FACESIZE, config.fontName.c_str(), _TRUNCATE);
            }
            
            if (config.localeCharset > 0) {
                modifiedLf.lfCharSet = config.localeCharset;
            }
            
            return wrapper->originalProc(&modifiedLf, lptm, dwType, wrapper->originalParam);
        }
    };
    
    CallbackWrapperW wrapper = {lpProc, lParam};
    
    // 调用原始函数
    int result = originalEnumFontFamiliesExW(hdc, &modifiedLogfont, CallbackWrapperW::EnumProcWrapper, reinterpret_cast<LPARAM>(&wrapper), dwFlags);
    
    Logger::getInstance().log(L"EnumFontFamiliesExW 完成，结果=" + std::to_wstring(result));
    return result;
}
