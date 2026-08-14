#include "locale_emulator_plus.h"
#include "settings.h"
#include "logger.h"
#include "utils.h"

// ============================================================================
// Locale Emulator Plus (LEP) 转区实现
// 基于 https://github.com/julixian/LocaleEmulatorPlus-Core.git 的 LoaderDll
//
// 需要与游戏置于同一目录的文件：
//   x86: LoaderDll_x86.dll          + LocaleEmulatorPlus_x86.dll
//   x64: LoaderDll_x64.dll          + LocaleEmulatorPlus_x64.dll
// LoaderDll 导出函数: LepCreateProcess / LepCreateProcess2
// ============================================================================

// 依据目标架构选择 LEP LoaderDll 与核心 DLL 文件名
#ifdef _WIN64
#define LEP_LOADER_DLL_NAME   L"LoaderDll_x64.dll"
#define LEP_CORE_DLL_NAME     L"LocaleEmulatorPlus_x64.dll"
#else
#define LEP_LOADER_DLL_NAME   L"LoaderDll_x86.dll"
#define LEP_CORE_DLL_NAME     L"LocaleEmulatorPlus_x86.dll"
#endif

// 基于LEP LoaderDll的结构定义
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

// LocaleEmulatorPlus 环境块 (LEPB)
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
} LOCALE_EMULATOR_PLUS_ENVIRONMENT_BLOCK, * PLOCALE_EMULATOR_PLUS_ENVIRONMENT_BLOCK, LEPB, * PLEPB;

LocaleEmulatorPlus& LocaleEmulatorPlus::getInstance() {
    static LocaleEmulatorPlus instance;
    return instance;
}

bool LocaleEmulatorPlus::initialize() {
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
    // 直接使用[Font]中的Charset
    m_codepage = config.localeCodepage;
    m_localeId = config.localeId;
    m_charset = config.localeCharset;
    m_timezone = config.timezone;
    
    Logger::getInstance().log(L"转区功能已初始化 (LocaleEmulatorPlus)");
    Logger::getInstance().log(L"代码页: " + std::to_wstring(m_codepage));
    Logger::getInstance().log(L"区域ID: " + std::to_wstring(m_localeId));
    Logger::getInstance().log(L"字符集: " + std::to_wstring(m_charset));
    Logger::getInstance().log(L"时区: " + m_timezone);
    Logger::getInstance().log(std::wstring(L"LoaderDll: ") + LEP_LOADER_DLL_NAME);
    Logger::getInstance().log(std::wstring(L"核心DLL: ") + LEP_CORE_DLL_NAME);
    
    m_enabled = true;
    m_initialized = true;
    
    return true;
}

bool LocaleEmulatorPlus::needsLocaleEmulation() const {
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

bool LocaleEmulatorPlus::performLocaleEmulation() {
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

// LEP LoaderDll 导出的 LepCreateProcess 函数指针类型 (返回 NTSTATUS)
typedef LONG (WINAPI * LepCreateProcess_t)(
    PLEPB                   leb,
    PCWSTR                  applicationName,
    PWSTR                   commandLine,
    PCWSTR                  currentDirectory,
    ULONG                   creationFlags,
    LPSTARTUPINFOW          startupInfo,
    PML_PROCESS_INFORMATION processInformation,
    LPSECURITY_ATTRIBUTES   processAttributes,
    LPSECURITY_ATTRIBUTES   threadAttributes,
    PVOID                   environment,
    HANDLE                  token
    );

bool LocaleEmulatorPlus::relaunchProcess() {
    Logger::getInstance().log(L"尝试重新启动进程，代码页: " + std::to_wstring(m_codepage));

    // 基于LEP LoaderDll的实现
    LEPB leb{};
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

    // 使用循环缓冲获取完整路径，避免 MAX_PATH 截断长路径
    std::wstring exePath = Utils::getModuleFilePath();

    // LEP 的 LepCreateProcess 需要可修改的命令行缓冲区 (PWSTR)
    PWSTR commandLine = GetCommandLineW();

    // 工作目录统一使用模块目录（游戏 EXE 所在目录），
    // 避免 CWD 与游戏目录不一致（快捷方式起始位置）导致转区重启后工作目录错误
    std::wstring currentDirectory = Utils::getModuleDirectory();

    STARTUPINFOW startInfo{};
    startInfo.cb = sizeof(startInfo);
    ML_PROCESS_INFORMATION processInfo{};

    // 尝试加载 LEP LoaderDll
    const HMODULE hLoader = LoadLibraryW(LEP_LOADER_DLL_NAME);
    if (hLoader == nullptr) {
        Logger::getInstance().log(std::wstring(L"无法加载") + LEP_LOADER_DLL_NAME +
            L"，转区功能需要该文件与" + LEP_CORE_DLL_NAME + L"同时存在于游戏目录");
        return false;
    }

    const auto fnLepCreateProcess = reinterpret_cast<LepCreateProcess_t>(GetProcAddress(hLoader, "LepCreateProcess"));
    if (fnLepCreateProcess == nullptr) {
        Logger::getInstance().log(L"无法找到LepCreateProcess函数");
        FreeLibrary(hLoader);
        return false;
    }

    const auto result = fnLepCreateProcess(
        &leb,
        exePath.c_str(),
        commandLine,
        currentDirectory.c_str(),
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
        // 成功创建新进程，退出当前进程（不再返回）
        Logger::getInstance().log(L"新进程创建成功，退出当前进程");
        ExitProcess(0);
    }

    Logger::getInstance().log(L"LepCreateProcess失败，错误代码: " + std::to_wstring(result));
    return false;
}

bool LocaleEmulatorPlus::createProcessWithLocale(const std::wstring& applicationPath) {
    // 确保转区功能已初始化
    if (!initialize()) {
        return false;
    }

    // 规范化目标程序路径，并以其所在目录作为子进程工作目录
    std::wstring absolutePath = Utils::resolveTargetPath(applicationPath);
    std::wstring currentDirectory = Utils::getDirectory(absolutePath);

    Logger::getInstance().log(L"转区启动目标程序: " + absolutePath);
    Logger::getInstance().log(L"转区启动工作目录: " + currentDirectory);
    
    if (!m_enabled) {
        // 如果转区功能未启用，使用普通方式启动进程
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);
        
        const BOOL created = CreateProcessW(
            absolutePath.c_str(),
            NULL,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            currentDirectory.c_str(),
            &si,
            &pi
        );
        
        // 关闭进程/线程句柄，避免句柄泄漏
        if (created) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        return created;
    }
    
    // 基于LEP LoaderDll的实现
    LEPB leb{};
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

    STARTUPINFOW startInfo{};
    startInfo.cb = sizeof(startInfo);
    ML_PROCESS_INFORMATION processInfo{};

    // 尝试加载 LEP LoaderDll
    const HMODULE hLoader = LoadLibraryW(LEP_LOADER_DLL_NAME);
    if (hLoader == nullptr) {
        Logger::getInstance().log(std::wstring(L"无法加载") + LEP_LOADER_DLL_NAME +
            L"，转区功能需要该文件与" + LEP_CORE_DLL_NAME + L"同时存在于游戏目录");
        return false;
    }

    const auto fnLepCreateProcess = reinterpret_cast<LepCreateProcess_t>(GetProcAddress(hLoader, "LepCreateProcess"));
    if (fnLepCreateProcess == nullptr) {
        Logger::getInstance().log(L"无法找到LepCreateProcess函数");
        FreeLibrary(hLoader);
        return false;
    }

    // 使用转区设置启动目标程序
    const auto result = fnLepCreateProcess(
        &leb,
        absolutePath.c_str(),     // 目标程序绝对路径
        NULL,                     // 命令行参数
        currentDirectory.c_str(), // 工作目录 = 目标程序所在目录
        0,                        // 创建标志
        &startInfo,
        &processInfo,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    FreeLibrary(hLoader);

    if (result == ERROR_SUCCESS) {
        // 成功创建新进程，但不退出当前进程
        Logger::getInstance().log(L"转区启动目标程序成功");
        
        // 关闭进程和线程句柄
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
        
        return true;
    } else {
        Logger::getInstance().log(L"转区启动目标程序失败，错误代码: " + std::to_wstring(result));
        return false;
    }
}

