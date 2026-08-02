#include "font_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include <string>
#include <unordered_map>

// 使用Windows SDK中已定义的SHIFTJIS_CHARSET
// #define SHIFTJIS_CHARSET 0x80  // Windows SDK中已定义

// 按指定代码页将ANSI字符串转换为UTF-16 - 绕过代码页hook直接处理
static std::wstring convertCodePageToUTF16(const std::string& ansiStr, UINT codePage) {
    if (ansiStr.empty()) return L"";
    
    // 直接调用原始MultiByteToWideChar函数，绕过代码页hook
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return L"";
    }
    
    typedef int (WINAPI *MultiByteToWideCharFunc)(UINT, DWORD, LPCSTR, int, LPWSTR, int);
    MultiByteToWideCharFunc originalMultiByteToWideChar = 
        (MultiByteToWideCharFunc)GetProcAddress(kernel32, "MultiByteToWideChar");
    
    if (!originalMultiByteToWideChar) {
        return L"";
    }
    
    int size_needed = originalMultiByteToWideChar(codePage, 0, ansiStr.c_str(), (int)ansiStr.size(), NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }
    
    std::wstring result(size_needed, 0);
    originalMultiByteToWideChar(codePage, 0, ansiStr.c_str(), (int)ansiStr.size(), &result[0], size_needed);
    return result;
}

// 根据字符集将ANSI字体名转换为UTF-16
// 注意：仅对Shift-JIS(日文0x80)做代码页932转换，其他字符集保持默认转换，
// 与原始实现行为一致
static std::wstring convertFontNameToUTF16(const std::string& ansiName, BYTE charset) {
    if (charset == SHIFTJIS_CHARSET) {
        std::wstring result = convertCodePageToUTF16(ansiName, 932);
        if (!result.empty()) {
            return result;
        }
    }
    // 其他情况使用默认转换
    return Utils::stringToWstring(ansiName);
}

// 初始化静态成员变量
decltype(CreateFontA)* FontHook::originalCreateFontA = nullptr;
decltype(CreateFontW)* FontHook::originalCreateFontW = nullptr;
decltype(CreateFontIndirectA)* FontHook::originalCreateFontIndirectA = nullptr;
decltype(CreateFontIndirectW)* FontHook::originalCreateFontIndirectW = nullptr;
decltype(EnumFontFamiliesExA)* FontHook::originalEnumFontFamiliesExA = nullptr;
decltype(EnumFontFamiliesExW)* FontHook::originalEnumFontFamiliesExW = nullptr;

bool FontHook::m_createFontAHooked = false;
bool FontHook::m_createFontWHooked = false;
bool FontHook::m_createFontIndirectAHooked = false;
bool FontHook::m_createFontIndirectWHooked = false;
bool FontHook::m_enumFontFamiliesExAHooked = false;
bool FontHook::m_enumFontFamiliesExWHooked = false;

FontHook& FontHook::getInstance() {
    static FontHook instance;
    return instance;
}

FontHook::FontHook() {
}

FontHook::~FontHook() {
    shutdown();
}

// 自定义字体加载函数
static bool loadCustomFont(const std::wstring& fontFileName) {
    if (fontFileName.empty()) {
        Logger::getInstance().log(L"未指定自定义字体文件名");
        return false;
    }
    
    // 获取游戏根目录
    wchar_t gameDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, gameDir);
    std::wstring fontPath = std::wstring(gameDir) + L"\\" + fontFileName;
    
    // 检查字体文件是否存在 (使用Windows API确保Windows 7兼容性)
    DWORD attrib = GetFileAttributesW(fontPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES || (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        Logger::getInstance().log(L"自定义字体文件不存在: " + fontPath);
        return false;
    }
    
    // 加载字体
    if (AddFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0) != 0) {
        Logger::getInstance().log(L"成功加载自定义字体: " + fontPath);
        return true;
    } else {
        Logger::getInstance().log(L"加载自定义字体失败: " + fontPath);
        return false;
    }
}

// 字体可用性结果缓存（自定义字体在初始化时加载，运行期间系统字体不变，缓存是安全的）
static std::unordered_map<std::wstring, bool> g_fontAvailableCache;

// 检查字体是否可用
static bool isFontAvailable(const std::wstring& fontName) {
    if (fontName.empty()) return false;
    
    // 查询缓存
    auto it = g_fontAvailableCache.find(fontName);
    if (it != g_fontAvailableCache.end()) {
        return it->second;
    }
    
    HDC hdc = GetDC(NULL);
    if (!hdc) return false;
    
    LOGFONTW lf = {0};
    wcsncpy_s(lf.lfFaceName, LF_FACESIZE, fontName.c_str(), _TRUNCATE);
    lf.lfCharSet = DEFAULT_CHARSET;
    
    bool found = false;
    // 通过trampoline调用原始EnumFontFamiliesExW，避免hook递归包装
    FontHook::getRawEnumFontFamiliesExW()(hdc, &lf, [](const LOGFONTW* /*lplf*/, const TEXTMETRICW* /*lptm*/, DWORD /*dwType*/, LPARAM lParam) -> int {
        *reinterpret_cast<bool*>(lParam) = true;
        return 0; // 停止枚举
    }, reinterpret_cast<LPARAM>(&found), 0);
    
    ReleaseDC(NULL, hdc);
    
    // 缓存结果
    g_fontAvailableCache[fontName] = found;
    return found;
}

// 获取原始EnumFontFamiliesExW函数指针（供isFontAvailable等内部检测使用，绕过hook）
decltype(EnumFontFamiliesExW)* FontHook::getRawEnumFontFamiliesExW() {
    return originalEnumFontFamiliesExW;
}

bool FontHook::initialize() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        Logger::getInstance().log(L"字体hook功能已禁用");
        return true;
    }
    
    Logger::getInstance().log(L"初始化字体hook");
    
    // 加载自定义字体
    if (!config.fontFileName.empty()) {
        if (loadCustomFont(config.fontFileName)) {
            Logger::getInstance().log(L"自定义字体加载成功");
        } else {
            Logger::getInstance().log(L"自定义字体加载失败，将使用系统字体");
        }
    }
    
    // 先将所有原始函数指针初始化为真实API地址。
    // 这样即使某个函数未启用hook（如EnableCreateFontW=0），
    // HookedCreateFontA 也能通过 originalCreateFontW 直接调用trampoline，
    // 避免二次hook或空指针崩溃。
    originalCreateFontA = CreateFontA;
    originalCreateFontW = CreateFontW;
    originalCreateFontIndirectA = CreateFontIndirectA;
    originalCreateFontIndirectW = CreateFontIndirectW;
    originalEnumFontFamiliesExA = EnumFontFamiliesExA;
    originalEnumFontFamiliesExW = EnumFontFamiliesExW;
    
    // 根据细粒度开关决定是否安装各个hook
    bool anyHookInstalled = false;
    
    // Hook CreateFontA (如果启用)
    if (config.enableCreateFontA) {
        if (DetourAttach(&(PVOID&)originalCreateFontA, HookedCreateFontA) != NO_ERROR) {
            Logger::getInstance().log(L"Hook CreateFontA 失败");
            shutdown();
            return false;
        }
        m_createFontAHooked = true;
        anyHookInstalled = true;
        Logger::getInstance().log(L"Hook CreateFontA 已安装");
    }
    
    // Hook CreateFontW (如果启用)
    if (config.enableCreateFontW) {
        if (DetourAttach(&(PVOID&)originalCreateFontW, HookedCreateFontW) != NO_ERROR) {
            Logger::getInstance().log(L"Hook CreateFontW 失败");
            shutdown();
            return false;
        }
        m_createFontWHooked = true;
        anyHookInstalled = true;
        Logger::getInstance().log(L"Hook CreateFontW 已安装");
    }
    
    // Hook CreateFontIndirectA (如果启用)
    if (config.enableCreateFontIndirectA) {
        if (DetourAttach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA) != NO_ERROR) {
            Logger::getInstance().log(L"Hook CreateFontIndirectA 失败");
            shutdown();
            return false;
        }
        m_createFontIndirectAHooked = true;
        anyHookInstalled = true;
        Logger::getInstance().log(L"Hook CreateFontIndirectA 已安装");
    }
    
    // Hook CreateFontIndirectW (如果启用)
    if (config.enableCreateFontIndirectW) {
        if (DetourAttach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW) != NO_ERROR) {
            Logger::getInstance().log(L"Hook CreateFontIndirectW 失败");
            shutdown();
            return false;
        }
        m_createFontIndirectWHooked = true;
        anyHookInstalled = true;
        Logger::getInstance().log(L"Hook CreateFontIndirectW 已安装");
    }
    
    // Hook EnumFontFamiliesExA (总是安装，因为它是字体枚举功能)
    if (DetourAttach(&(PVOID&)originalEnumFontFamiliesExA, HookedEnumFontFamiliesExA) != NO_ERROR) {
        Logger::getInstance().log(L"Hook EnumFontFamiliesExA 失败");
        shutdown();
        return false;
    }
    m_enumFontFamiliesExAHooked = true;
    anyHookInstalled = true;
    Logger::getInstance().log(L"Hook EnumFontFamiliesExA 已安装");
    
    // Hook EnumFontFamiliesExW (总是安装，因为它是字体枚举功能)
    if (DetourAttach(&(PVOID&)originalEnumFontFamiliesExW, HookedEnumFontFamiliesExW) != NO_ERROR) {
        Logger::getInstance().log(L"Hook EnumFontFamiliesExW 失败");
        shutdown();
        return false;
    }
    m_enumFontFamiliesExWHooked = true;
    anyHookInstalled = true;
    Logger::getInstance().log(L"Hook EnumFontFamiliesExW 已安装");
    
    if (anyHookInstalled) {
        Logger::getInstance().log(L"字体hook初始化完成");
    } else {
        Logger::getInstance().log(L"所有字体hook功能都已禁用，未安装任何hook");
    }
    
    return true;
}

void FontHook::shutdown() {
    // 仅对实际attach过的函数执行DetourDetach，避免对未安装hook的
    // 原始地址执行无效的detach操作
    if (m_createFontAHooked) {
        DetourDetach(&(PVOID&)originalCreateFontA, HookedCreateFontA);
        m_createFontAHooked = false;
    }
    originalCreateFontA = nullptr;
    
    if (m_createFontWHooked) {
        DetourDetach(&(PVOID&)originalCreateFontW, HookedCreateFontW);
        m_createFontWHooked = false;
    }
    originalCreateFontW = nullptr;
    
    if (m_createFontIndirectAHooked) {
        DetourDetach(&(PVOID&)originalCreateFontIndirectA, HookedCreateFontIndirectA);
        m_createFontIndirectAHooked = false;
    }
    originalCreateFontIndirectA = nullptr;
    
    if (m_createFontIndirectWHooked) {
        DetourDetach(&(PVOID&)originalCreateFontIndirectW, HookedCreateFontIndirectW);
        m_createFontIndirectWHooked = false;
    }
    originalCreateFontIndirectW = nullptr;
    
    if (m_enumFontFamiliesExAHooked) {
        DetourDetach(&(PVOID&)originalEnumFontFamiliesExA, HookedEnumFontFamiliesExA);
        m_enumFontFamiliesExAHooked = false;
    }
    originalEnumFontFamiliesExA = nullptr;
    
    if (m_enumFontFamiliesExWHooked) {
        DetourDetach(&(PVOID&)originalEnumFontFamiliesExW, HookedEnumFontFamiliesExW);
        m_enumFontFamiliesExWHooked = false;
    }
    originalEnumFontFamiliesExW = nullptr;
    
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
    
    // 记录原始字体信息
    std::string originalFaceName = lpszFace ? std::string(lpszFace) : "默认字体";
    std::wstring originalFaceNameW;
    
    // 根据字符集正确转换字体名称
    originalFaceNameW = convertFontNameToUTF16(originalFaceName, (BYTE)fdwCharSet);
    
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
        wFaceName = convertFontNameToUTF16(lpszFace, (BYTE)fdwCharSet);
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
    
    // 独立处理，不依赖 HookedCreateFontW
    LPCWSTR newFaceName = wFaceName.c_str();
    std::wstring customFaceName;
    
    // 优先使用自定义字体，如果不可用则回退到系统字体
    if (!config.fontName.empty()) {
        customFaceName = config.fontName;
        // 检查自定义字体是否可用
        if (isFontAvailable(customFaceName)) {
            newFaceName = customFaceName.c_str();
            Logger::getInstance().log(L"使用自定义字体: " + customFaceName);
        } else {
            Logger::getInstance().log(L"自定义字体不可用，使用系统字体: " + wFaceName);
        }
    }
    
    // 通过trampoline调用系统 CreateFontW 函数，传递修改后的参数
    // 注意：必须使用originalCreateFontW而非CreateFontW，避免在启用CreateFontW hook时被二次处理
    return originalCreateFontW(newHeight, newWidth, nEscapement, nOrientation,
                             newWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                             newCharSet, fdwOutputPrecision, fdwClipPrecision,
                             fdwQuality, fdwPitchAndFamily, newFaceName);
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
    
    // 优先使用自定义字体，如果不可用则回退到系统字体
    if (!config.fontName.empty()) {
        customFaceName = config.fontName;
        // 检查自定义字体是否可用
        if (isFontAvailable(customFaceName)) {
            newFaceName = customFaceName.c_str();
            Logger::getInstance().log(L"使用自定义字体: " + customFaceName);
        } else {
            Logger::getInstance().log(L"自定义字体不可用，使用系统字体: " + std::wstring(lpszFace ? lpszFace : L"默认字体"));
        }
    }
    
    Logger::getInstance().log(L"新字体(W): " + std::wstring(newFaceName) +
                            L", Charset=" + Utils::intToHexString(newCharSet) +
                            L", Height=" + std::to_wstring(newHeight) +
                            L", Width=" + std::to_wstring(newWidth) +
                            L", Weight=" + std::to_wstring(newWeight));
    
    return originalCreateFontW(newHeight, newWidth, nEscapement, nOrientation,
                             newWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
                             newCharSet, fdwOutputPrecision, fdwClipPrecision,
                             fdwQuality, fdwPitchAndFamily, newFaceName);
}

HFONT WINAPI FontHook::HookedCreateFontIndirectA(const LOGFONTA* lplf) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!lplf) {
        return originalCreateFontIndirectA(lplf);
    }
    
    // 记录原始字体信息
    std::string originalFaceName = lplf->lfFaceName;
    std::wstring originalFaceNameW;
    
    // 根据字符集正确转换字体名称
    originalFaceNameW = convertFontNameToUTF16(originalFaceName, lplf->lfCharSet);
    
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
    if (!lplf) {
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
    
    // 优先使用自定义字体，如果不可用则保持原字体
    if (!config.fontName.empty()) {
        std::wstring customFaceName = config.fontName;
        if (isFontAvailable(customFaceName)) {
            std::string fontNameA = Utils::wstringToANSI(customFaceName);
            strncpy_s(lf->lfFaceName, LF_FACESIZE, fontNameA.c_str(), _TRUNCATE);
            Logger::getInstance().log(L"使用自定义字体: " + customFaceName);
        } else {
            Logger::getInstance().log(L"自定义字体不可用，保持原字体");
        }
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
    
    // 与LOGFONTA版本保持一致：自定义字体不可用时保持原字体
    if (!config.fontName.empty()) {
        if (isFontAvailable(config.fontName)) {
            wcsncpy_s(lf->lfFaceName, LF_FACESIZE, config.fontName.c_str(), _TRUNCATE);
            Logger::getInstance().log(L"使用自定义字体: " + config.fontName);
        } else {
            Logger::getInstance().log(L"自定义字体不可用，保持原字体");
        }
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
    originalFaceNameW = convertFontNameToUTF16(originalFaceName, lpLogfont->lfCharSet);
    
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
    
    // 设置字体名称 - 不修改字体名称，保持原始字体枚举
    wcsncpy_s(logfontW.lfFaceName, LF_FACESIZE, originalFaceNameW.c_str(), _TRUNCATE);
    
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
            
            // TEXTMETRICW 参数在枚举TrueType字体时为NULL，必须做空指针检查
            // 否则直接解引用会导致游戏崩溃
            TEXTMETRICA textmetricA = {0};
            TEXTMETRICA* pTextmetricA = nullptr;
            if (lptm) {
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
                pTextmetricA = &textmetricA;
            }
            
            return wrapper->originalProc(&logfontA, pTextmetricA, dwType, wrapper->originalParam);
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
    
    // 修改字体参数 - 不修改字体名称，保持原始字体枚举
    // 字体名称保持不变，避免影响游戏正常的字体枚举功能
    
    if (config.localeCharset > 0) {
        modifiedLogfont.lfCharSet = config.localeCharset;
    }
    
    // 创建回调包装器，仅应用字符集过滤，不修改字体名称
    struct CallbackWrapperW {
        FONTENUMPROCW originalProc;
        LPARAM originalParam;
        
        static int CALLBACK EnumProcWrapper(const LOGFONTW* lplf, const TEXTMETRICW* lptm, DWORD dwType, LPARAM lParam) {
            CallbackWrapperW* wrapper = reinterpret_cast<CallbackWrapperW*>(lParam);
            
            // 仅修改字符集，保持字体名称不变
            LOGFONTW modifiedLf = *lplf;
            const HookConfig& config = ConfigManager::getInstance().getConfig();
            
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