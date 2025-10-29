#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <unordered_map>

// 配置结构体
struct HookConfig {
    bool enableFileRedirect = false;
    bool enableFontHook = false;
    bool enableCodepageHook = false;
    bool enableWindowTitleHook = false;
    
    // 文件重定向配置
    std::wstring redirectFolder = L"CHSFiles";
    
    // 字体配置
    std::wstring fontName = L"";
    int charset = 0x86; // 默认简体中文GB2312
    int fontWeight = 0;
    int fontHeight = 0;  // 字体高度 (0表示不修改)
    int fontWidth = 0;   // 字体宽度 (0表示不修改)
    
    // 代码页配置
    unsigned int sourceCodepage = 932;   // 默认日文Shift-JIS
    unsigned int targetCodepage = 936;   // 默认简体中文GBK
    
    // 窗口标题配置
    bool enableTitleCheck = true;        // 是否启用标题检查
    std::wstring originalWindowTitle = L"";  // 原标题
    std::wstring newWindowTitle = L"";       // 新标题
    
    // 日志配置
    bool enableLogging = true;
    std::wstring logFile = L"celica_hook.log";
};

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    bool loadConfig(const std::wstring& configFile);
    const HookConfig& getConfig() const;
    void setConfig(const HookConfig& config);
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    HookConfig m_config;
    std::unordered_map<std::wstring, std::wstring> m_redirectMap;
    
    void parseConfigLine(const std::wstring& line);
    void buildRedirectMap();
    std::wstring getGameDirectory();
};

#endif // SETTINGS_H
