#include "locale_emulator.h"
#include "settings.h"
#include "logger.h"
#include <iostream>

// 基于LELOADER的结构定义
typedef struct ML_PROCESS_INFORMATION : PROCESS_INFORMATION
{
    PVOID FirstCallLdrLoadDll;
} ML_PROCESS_INFORMATION, * PML_PROCESS_INFORMATION;

typedef struct _TIME_FIELDS
{
    SHORT Year;        // range [1601...]
    SHORT Month;       // range [1..12]
    SHORT Day;         // range [1..31]
    SHORT Hour;        // range [0..23]
    SHORT Minute;      // range [0..59]
    SHORT Second;      // range [0..59]
    SHORT Milliseconds;// range [0..999]
    SHORT Weekday;     // range [0..6] == [Sunday..Saturday]
} TIME_FIELDS, * PTIME_FIELDS;

typedef struct _RTL_TIME_ZONE_INFORMATION
{
    LONG        Bias;
    WCHAR       StandardName[32];
    TIME_FIELDS StandardStart;
    LONG        StandardBias;
    WCHAR       DaylightName[32];
    TIME_FIELDS DaylightStart;
    LONG        DaylightBias;
} RTL_TIME_ZONE_INFORMATION, * PRTL_TIME_ZONE_INFORMATION;

typedef struct _REG_TZI_FORMAT
{
    int Bias;
    int StandardBias;
    int DaylightBias;
    _SYSTEMTIME StandardDate;
    _SYSTEMTIME DaylightDate;
} REG_TZI_FORMAT;

typedef struct
{
    USHORT Length;
    USHORT MaximumLength;
    union
    {
        PWSTR  Buffer;
        ULONG64 Dummy;
    };
} UNICODE_STRING3264, * PUNICODE_STRING3264;

typedef UNICODE_STRING3264 UNICODE_STRING64;
typedef PUNICODE_STRING3264 PUNICODE_STRING64;

typedef struct
{
    ULONG64             Root;
    UNICODE_STRING64    SubKey;
    UNICODE_STRING64    ValueName;
    ULONG               DataType;
    PVOID64             Data;
    ULONG64             DataSize;
} REGISTRY_ENTRY64;

typedef struct
{
    REGISTRY_ENTRY64 Original;
    REGISTRY_ENTRY64 Redirected;
} REGISTRY_REDIRECTION_ENTRY64, * PREGISTRY_REDIRECTION_ENTRY64;

typedef struct
{
    ULONG                           AnsiCodePage;
    ULONG                           OemCodePage;
    ULONG                           LocaleID;
    ULONG                           DefaultCharset;
    ULONG                           HookUILanguageApi;
    WCHAR                           DefaultFaceName[LF_FACESIZE];
    RTL_TIME_ZONE_INFORMATION       Timezone;
    ULONG64                         NumberOfRegistryRedirectionEntries;
    REGISTRY_REDIRECTION_ENTRY64    RegistryReplacement[1];
} LOCALE_ENUMLATOR_ENVIRONMENT_BLOCK, * PLOCALE_ENUMLATOR_ENVIRONMENT_BLOCK, LEB, * PLEB;

typedef DWORD(WINAPI* LeCreateProcess_t)(
    PLEB                    leb,
    PCWSTR                  applicationName,
    PCWSTR                  commandLine,
    PCWSTR                  currentDirectory,
    ULONG                   creationFlags,
    LPSTARTUPINFOW          startupInfo,
    PML_PROCESS_INFORMATION processInformation,
    LPSECURITY_ATTRIBUTES   processAttributes,
    LPSECURITY_ATTRIBUTES   threadAttributes,
    PVOID                   environment,
    HANDLE                  token
    );

LocaleEmulator& LocaleEmulator::getInstance() {
    static LocaleEmulator instance;
    return instance;
}

bool LocaleEmulator::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // 获取配置
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    if (!config.enableLocaleEmulation) {
        Logger::getInstance().log(L"转区功能已禁用");
        m_enabled = false;
        m_initialized = true;
        return true;
    }
    
    // 设置转区配置
    // 直接使用[Codepage]中的TargetCodepage和[Font]中的Charset
    m_codepage = config.targetCodepage;
    m_localeId = config.localeId;
    m_charset = config.charset;
    m_timezone = config.timezone;
    
    Logger::getInstance().log(L"转区功能已初始化");
    Logger::getInstance().log(L"代码页: " + std::to_wstring(m_codepage));
    Logger::getInstance().log(L"区域ID: " + std::to_wstring(m_localeId));
    Logger::getInstance().log(L"字符集: " + std::to_wstring(m_charset));
    Logger::getInstance().log(L"时区: " + m_timezone);
    
    m_enabled = true;
    m_initialized = true;
    
    return true;
}

bool LocaleEmulator::needsLocaleEmulation() const {
    if (!m_enabled) {
        return false;
    }
    
    // 检查当前系统代码页是否与目标代码页不同
    UINT currentCodepage = GetACP();
    
    if (currentCodepage == m_codepage) {
        Logger::getInstance().log(L"当前系统代码页与目标代码页相同，无需转区");
        return false;
    }
    
    Logger::getInstance().log(L"当前系统代码页: " + std::to_wstring(currentCodepage) + 
                             L"，目标代码页: " + std::to_wstring(m_codepage) + L"，需要转区");
    return true;
}

bool LocaleEmulator::performLocaleEmulation() {
    if (!m_enabled) {
        Logger::getInstance().log(L"转区功能未启用");
        return false;
    }
    
    if (!needsLocaleEmulation()) {
        Logger::getInstance().log(L"无需转区");
        return false;  // 返回false表示不需要转区，继续正常流程
    }
    
    Logger::getInstance().log(L"开始执行转区操作");
    
    // 尝试重新启动进程
    if (relaunchProcess()) {
        Logger::getInstance().log(L"转区成功，进程已重新启动");
        return true;
    } else {
        Logger::getInstance().log(L"转区失败");
        return false;
    }
}

bool LocaleEmulator::relaunchProcess() {
    Logger::getInstance().log(L"尝试重新启动进程，代码页: " + std::to_wstring(m_codepage));

    // 基于LELOADER的实现
    LEB leb{};
    leb.AnsiCodePage = m_codepage;
    leb.OemCodePage = m_codepage;
    leb.LocaleID = m_localeId;
    leb.DefaultCharset = m_charset;

    // 设置时区信息
    HKEY hTimeZone;
    std::wstring key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\" + m_timezone;
    
    if (RegOpenKeyW(HKEY_LOCAL_MACHINE, key.c_str(), &hTimeZone) == ERROR_SUCCESS) {
        DWORD bufferSize = sizeof leb.Timezone.StandardName;
        RegGetValueW(hTimeZone, nullptr, L"Std", RRF_RT_REG_SZ, nullptr, leb.Timezone.StandardName, &bufferSize);

        bufferSize = sizeof leb.Timezone.DaylightName;
        RegGetValueW(hTimeZone, nullptr, L"Dlt", RRF_RT_REG_SZ, nullptr, leb.Timezone.DaylightName, &bufferSize);

        REG_TZI_FORMAT timeZoneInfo;
        bufferSize = sizeof timeZoneInfo;
        RegGetValueW(hTimeZone, nullptr, L"TZI", RRF_RT_REG_BINARY, nullptr, &timeZoneInfo, &bufferSize);
        leb.Timezone.Bias = timeZoneInfo.Bias;
        leb.Timezone.StandardBias = timeZoneInfo.StandardBias;
        leb.Timezone.DaylightBias = 0;

        RegCloseKey(hTimeZone);
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, std::size(exePath));

    const wchar_t* commandLine = GetCommandLineW();

    wchar_t currentDirectory[MAX_PATH];
    GetCurrentDirectoryW(std::size(currentDirectory), currentDirectory);

    STARTUPINFOW startInfo{};
    startInfo.cb = sizeof(startInfo);
    ML_PROCESS_INFORMATION processInfo{};

    // 尝试加载LoaderDll.dll
    const HMODULE hLoader = LoadLibraryA("LoaderDll.dll");
    if (hLoader == nullptr) {
        Logger::getInstance().log(L"无法加载LoaderDll.dll，转区功能需要此文件");
        return false;
    }

    const auto LeCreateProcess = reinterpret_cast<LeCreateProcess_t>(GetProcAddress(hLoader, "LeCreateProcess"));
    if (LeCreateProcess == nullptr) {
        Logger::getInstance().log(L"无法找到LeCreateProcess函数");
        FreeLibrary(hLoader);
        return false;
    }

    const auto result = LeCreateProcess(
        &leb,
        exePath,
        commandLine,
        currentDirectory,
        0,
        &startInfo,
        &processInfo,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    FreeLibrary(hLoader);

    if (result == ERROR_SUCCESS) {
        // 成功创建新进程，退出当前进程
        Logger::getInstance().log(L"新进程创建成功，退出当前进程");
        ExitProcess(0);
        return true;
    } else {
        Logger::getInstance().log(L"LeCreateProcess失败，错误代码: " + std::to_wstring(result));
        return false;
    }
}

bool LocaleEmulator::setupTimezone() {
    // 时区设置已经在relaunchProcess中处理
    return true;
}
