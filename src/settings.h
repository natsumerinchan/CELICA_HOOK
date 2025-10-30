#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <unordered_map>
#include <windows.h>

// 作者信息 - 用户不可修改
namespace AuthorInfo {
    // 作者在不同论坛的ID
    constexpr const wchar_t* AUTHOR_IDS[] = {
        L"natsumerinchan (GitHub)",
        L"natsumerin@ai2.moe (御爱同萌)",
        L"雨宮ゆうこ@moyu.moe (鲲补丁站)"
    };
    constexpr int AUTHOR_IDS_COUNT = 3;
    
    // 多个主页链接
    constexpr const wchar_t* AUTHOR_HOMEPAGES[] = {
        L"https://github.com/natsumerinchan",
        L"https://github.com/natsumerinchan/MyGalTranslationPatches",
        L"https://www.ai2.moe/profile/13275-natsumerin/",
        L"https://www.moyu.moe/user/47/resource"
    };
    constexpr int AUTHOR_HOMEPAGES_COUNT = 4;
    
    constexpr const wchar_t* ADDITIONAL_NOTES = L"本补丁免费发布在Github、御爱以及鲲补丁站，\n允许转载但严禁倒卖和冒充人工汉化发布，\n如果你是花钱获得本补丁的说明你被骗了。";
}

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
    
    // 转区配置
    bool enableLocaleEmulation = false;      // 是否启用转区功能
    unsigned int localeCodepage = 932;       // 转区代码页 (默认日文932)
    unsigned int localeId = 1041;            // 区域设置ID (默认日文1041)
    unsigned int localeCharset = SHIFTJIS_CHARSET; // 字符集 (默认Shift-JIS)
    std::wstring timezone = L"Tokyo Standard Time"; // 时区 (默认东京时区)
    
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
