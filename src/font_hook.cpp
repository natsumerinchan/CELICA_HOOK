#include "font_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"
#include <string>
#include <unordered_map>
#include <mutex>

// 使用Windows SDK中已定义的SHIFTJIS_CHARSET
// #define SHIFTJIS_CHARSET 0x80  // Windows SDK中已定义

// 将 int 字符集安全转换为 BYTE：超出 0-255 范围的配置值会被钳制，
// 避免 C4244 截断告警与意外的数据丢失
static BYTE charsetToByte(int charset) {
    if (charset < 0) return 0;
    if (charset > 0xFF) return 0xFF;
    return static_cast<BYTE>(charset);
}

// 按字体字符集选择对应的 ANSI 代码页。
// 游戏（尤其日文引擎）通常按自身字符集的代码页解码 ANSI 字体名，
// 若一律使用系统 CP_ACP（如中文系统=936），日文字体名会被转成
// GBK 字节，游戏按 CP932 解码后即出现 "! ) $ *" 一类乱码。
static UINT charsetToCodepage(BYTE charset) {
    switch (charset) {
        case SHIFTJIS_CHARSET:    return 932;  // 日文 Shift-JIS
        case HANGEUL_CHARSET:     return 949;  // 韩文
        case GB2312_CHARSET:      return 936;  // 简体中文
        case CHINESEBIG5_CHARSET: return 950;  // 繁体中文
        default:                  return CP_ACP;
    }
}

// 将宽字符字体名按指定字符集转换回 ANSI。
// 目标代码页无法表示时回退到系统 ANSI 代码页，避免返回空名导致空白字体项。
static std::string wideToANSIWithCharset(const std::wstring& wstr, BYTE charset) {
    if (wstr.empty()) return "";

    const UINT codepage = charsetToCodepage(charset);
    int size = WideCharToMultiByte(codepage, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size > 0) {
        std::string result(size, 0);
        if (WideCharToMultiByte(codepage, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, NULL, NULL) > 0) {
            return result;
        }
    }

    // 回退到系统 ANSI 代码页
    return Utils::wstringToANSI(wstr);
}

// 按指定代码页将ANSI字符串转换为UTF-16。
// 说明：本项目不再 hook MultiByteToWideChar/WideCharToMultiByte
// （代码页模拟由 LocaleEmulatorPlus 转区功能实现），
// 因此直接调用系统 API 即可，无需绕过任何代码页 hook。
static std::wstring convertCodePageToUTF16(const std::string& ansiStr, UINT codePage) {
    if (ansiStr.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(codePage, 0, ansiStr.c_str(), (int)ansiStr.size(), NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }
    
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(codePage, 0, ansiStr.c_str(), (int)ansiStr.size(), &result[0], size_needed);
    return result;
}

// 根据字符集将ANSI字体名转换为UTF-16
// 注意：仅对Shift-JIS(日文0x80)做代码页932转换，其他字符集保持默认转换。
// 关键修复：游戏传入的 ANSI 字体名是系统代码页（如中文系统=GBK）编码，
// 不能按 UTF-8 解码（原实现用 Utils::stringToWstring 错误地按 UTF-8 解码，
// 非法 UTF-8 序列会解码失败返回空字符串）。
static std::wstring convertFontNameToUTF16(const std::string& ansiName, BYTE charset) {
    if (charset == SHIFTJIS_CHARSET) {
        std::wstring result = convertCodePageToUTF16(ansiName, 932);
        if (!result.empty()) {
            return result;
        }
    }
    // 其他情况按系统 ANSI 代码页解码
    return Utils::ansiToWstring(ansiName);
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
    
    // 获取游戏根目录：使用模块（EXE）所在目录而非当前工作目录，
    // 与文件重定向的目录基准保持一致（DLL 注入场景下 CWD 可能与游戏目录不同）
    const std::wstring fontPath = Utils::combinePaths(Utils::getModuleDirectory(), fontFileName);
    
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

// 线程安全：字体查询会从游戏任意线程触发（CreateFontA/W/Indirect 的 hook 回调），
// 必须串行化对缓存的读写，避免 unordered_map 并发插入/查询导致崩溃
static std::mutex g_fontCacheMutex;

// 检查字体是否可用
static bool isFontAvailable(const std::wstring& fontName) {
    if (fontName.empty()) return false;
    
    // 查询缓存
    {
        std::lock_guard<std::mutex> lock(g_fontCacheMutex);
        auto it = g_fontAvailableCache.find(fontName);
        if (it != g_fontAvailableCache.end()) {
            return it->second;
        }
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
    {
        std::lock_guard<std::mutex> lock(g_fontCacheMutex);
        g_fontAvailableCache[fontName] = found;
    }
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
    
    // 根据字符集正确转换字体名称（仅转换一次，供日志与后续替换共用）
    std::wstring wFaceName = lpszFace ? convertFontNameToUTF16(lpszFace, (BYTE)fdwCharSet) : L"";
    std::wstring originalFaceNameW = wFaceName.empty() ? L"默认字体" : wFaceName;
    
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
    
    // 记录新字体信息
    std::wstring newFaceNameW;
    if (config.fontName.empty()) {
        newFaceNameW = originalFaceNameW;
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
        lf->lfCharSet = charsetToByte(config.localeCharset);
    }
    
    // 优先使用自定义字体，如果不可用则保持原字体
    if (!config.fontName.empty()) {
        std::wstring customFaceName = config.fontName;
        if (isFontAvailable(customFaceName)) {
            // 按目标字符集转换回 ANSI，确保日文引擎按 CP932 解码时字体名不乱码
            const BYTE targetCharset = (config.localeCharset > 0) ? charsetToByte(config.localeCharset) : DEFAULT_CHARSET;
            std::string fontNameA = wideToANSIWithCharset(customFaceName, targetCharset);
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
        lf->lfCharSet = charsetToByte(config.localeCharset);
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

// EnumFontFamiliesExA 钩子函数
//
// 设计要点：仅当需要修改字符集过滤（config.localeCharset 与请求的
// lfCharSet 不同）时才干预；否则【直接透传原始 API】。
//
// 原因：把 EnumFontFamiliesExA 转发到 W 版、再在回调中把宽字符字体名
// 转回 ANSI（无论按哪个代码页），都会产生字节级转换，导致日文引擎在
// 按 CP932 解码回调返回的字体名时出现 "! ) $ *" 一类乱码。
// 直接透传 A 版时，系统生成的 LOGFONTA.lfFaceName 就是游戏期望的
// ANSI 字节，回调不经任何转换，天然正确。
int WINAPI FontHook::HookedEnumFontFamiliesExA(
    HDC hdc,
    LPLOGFONTA lpLogfont,
    FONTENUMPROCA lpProc,
    LPARAM lParam,
    DWORD dwFlags
) {
    if (!lpLogfont) {
        return originalEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }

    const HookConfig& config = ConfigManager::getInstance().getConfig();
    const BYTE targetCharset = charsetToByte(config.localeCharset);

    // 无需修改字符集过滤（禁用hook、未配置字符集、或目标==请求）：
    // 完整透传，回调零转换
    if (!config.enableFontHook || config.localeCharset <= 0 || lpLogfont->lfCharSet == targetCharset) {
        return originalEnumFontFamiliesExA(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }

    // 需要修改字符集过滤：就地复制 LOGFONTA 并调整 lfCharSet 后调用
    // 原始 A 版，回调原样透传（系统生成的 ANSI 字体名无需转换）
    LOGFONTA modifiedLf = *lpLogfont;
    modifiedLf.lfCharSet = targetCharset;
    Logger::getInstance().log(L"EnumFontFamiliesExA: 字符集过滤 " +
        Utils::intToHexString(lpLogfont->lfCharSet) + L" -> " + Utils::intToHexString(targetCharset));
    return originalEnumFontFamiliesExA(hdc, &modifiedLf, lpProc, lParam, dwFlags);
}

// EnumFontFamiliesExW 钩子函数
// 与 A 版同理：字符集无需修改时透传原始 W 版，回调零转换
int WINAPI FontHook::HookedEnumFontFamiliesExW(
    HDC hdc,
    LPLOGFONTW lpLogfont,
    FONTENUMPROCW lpProc,
    LPARAM lParam,
    DWORD dwFlags
) {
    if (!lpLogfont) {
        return originalEnumFontFamiliesExW(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }

    const HookConfig& config = ConfigManager::getInstance().getConfig();
    const BYTE targetCharset = charsetToByte(config.localeCharset);

    // 无需修改字符集过滤：完整透传
    if (!config.enableFontHook || config.localeCharset <= 0 || lpLogfont->lfCharSet == targetCharset) {
        return originalEnumFontFamiliesExW(hdc, lpLogfont, lpProc, lParam, dwFlags);
    }

    // 需要修改字符集过滤：复制 LOGFONTW 调整 lfCharSet，回调原样透传
    LOGFONTW modifiedLf = *lpLogfont;
    modifiedLf.lfCharSet = targetCharset;
    Logger::getInstance().log(L"EnumFontFamiliesExW: 字符集过滤 " +
        Utils::intToHexString(lpLogfont->lfCharSet) + L" -> " + Utils::intToHexString(targetCharset));
    return originalEnumFontFamiliesExW(hdc, &modifiedLf, lpProc, lParam, dwFlags);
}
