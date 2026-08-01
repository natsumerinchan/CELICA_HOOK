#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>


// 链接信息结构
struct LinkInfo {
    std::wstring displayText;
    std::wstring url;
};

// 弹窗显示内容
#define WINDOW_AUTHOR L"作者: " L"natsumerin@ai2.moe==雨宮ゆうこ@moyu.moe"
#define WINDOW_STATEMENT L"声明: " L"本补丁免费发布于御爱和鲲补丁站，允许转载但禁止倒卖或冒充人工汉化发布"

// 链接定义
static const LinkInfo WINDOW_LINKS[] = {
    {L"补丁仓库: ", L"https://github.com/natsumerinchan/MyGalTranslationPatches.git"},
    {L"御爱同萌: ", L"https://www.ai2.moe/profile/13275-natsumerin"},
    {L"鲲Galgame补丁站: ", L"https://www.moyu.moe/user/47/resource"},
    {L"HOOK项目仓库: ", L"https://github.com/natsumerinchan/CELICA_HOOK.git"}
};

static const int WINDOW_LINKS_COUNT = sizeof(WINDOW_LINKS) / sizeof(WINDOW_LINKS[0]);

// 配置结构体
struct HookConfig {
    bool enableFileRedirect = false;
    bool enableFontHook = false;
    bool enableWindowTitleHook = false;
    
    // 文件重定向配置
    bool enableExtensionCheck = true;
    std::wstring redirectFolder = L"CHSFiles";
    std::wstring redirectExtensions = L".txt";
    
    // 文件欺骗配置
    bool enableFileSpoofing = false;
    std::wstring spoofedFiles = L"";  // 逗号分隔的文件路径列表
    std::wstring spoofedDirectories = L"";  // 逗号分隔的目录路径列表
    
    // 字体配置
    std::wstring fontName = L"";
    std::wstring fontFileName = L""; // 自定义字体文件名
    int localeCharset = 0x80; // 默认日文Shift-JIS
    int fontWeight = 0;
    int fontHeight = 0;  // 字体高度 (0表示不修改)
    int fontWidth = 0;   // 字体宽度 (0表示不修改)
    
    // 字体hook细粒度控制
    bool enableCreateFontA = true;           // HookedCreateFontA开关
    bool enableCreateFontW = true;           // HookedCreateFontW开关
    bool enableCreateFontIndirectA = true;   // HookedCreateFontIndirectA开关
    bool enableCreateFontIndirectW = true;   // HookedCreateFontIndirectW开关
    
    // 窗口标题配置
    bool enableTitleCheck = true;        // 是否启用标题检查
    std::wstring originalWindowTitle = L"";  // 原标题
    std::wstring newWindowTitle = L"";       // 新标题
    
    // 转区配置
    bool enableLocaleEmulation = false;      // 是否启用转区功能
    unsigned int localeCodepage = 932;       // 转区代码页 (默认日文932)
    unsigned int localeId = 1041;            // 区域设置ID (默认日文1041)
    std::wstring timezone = L"Tokyo Standard Time"; // 时区 (默认东京时区)
    
    // 日志配置
    bool enableLogging = true;
    std::wstring logFile = L"celica_hook.log";
    
    // 注入器配置
    std::wstring targetProcess = L"";  // 目标进程名称
};

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadConfig(const std::wstring& configFile);
    const HookConfig& getConfig() const;
    void setConfig(const HookConfig& config);
    
    // 文件重定向/欺骗缓存访问接口
    bool findRedirectedPath(const std::wstring& relativePath, std::wstring& outFullPath) const;
    bool isExtensionRedirected(const std::wstring& filename) const;
    bool isFileSpoofed(const std::wstring& relativePath) const;
    bool isDirectorySpoofed(const std::wstring& relativePath) const;
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    HookConfig m_config;
    std::unordered_map<std::wstring, std::wstring> m_redirectMap;
    
    // 缓存解析后的配置列表
    std::vector<std::wstring> m_redirectExtList;   // 小写扩展名列表（含点，如 ".txt"）
    std::vector<std::wstring> m_spoofedFileList;   // 小写相对路径列表
    std::vector<std::wstring> m_spoofedDirList;    // 小写相对目录列表（含末尾反斜杠）
    
    void parseConfigLine(const std::wstring& line);
    void buildRedirectMap();
    void rebuildCachedLists();
    std::wstring getGameDirectory();
};

#endif // SETTINGS_H
