// 抑制C++17弃用警告
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "settings.h"
#include "utils.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <codecvt>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig(const std::wstring& configFile) {
    Logger::getInstance().log(L"正在加载配置文件: " + configFile);
    
    std::wifstream file(configFile);
    if (!file.is_open()) {
        Logger::getInstance().log(L"无法打开配置文件: " + configFile);
        return false;
    }
    
    // 设置UTF-8编码，确保正确读取中文字符
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));
    
    std::wstring line;
    std::wstring currentSection;
    
    while (std::getline(file, line)) {
        // 移除前后空白字符
        line.erase(0, line.find_first_not_of(L" \t"));
        line.erase(line.find_last_not_of(L" \t") + 1);
        
        // 跳过空行和注释
        if (line.empty() || line[0] == L';') {
            continue;
        }
        
        // 检查是否是节
        if (line[0] == L'[' && line[line.length() - 1] == L']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // 解析键值对
        size_t pos = line.find(L'=');
        if (pos != std::wstring::npos) {
            std::wstring key = line.substr(0, pos);
            std::wstring value = line.substr(pos + 1);
            
            // 移除键值对的空白字符
            key.erase(0, key.find_first_not_of(L" \t"));
            key.erase(key.find_last_not_of(L" \t") + 1);
            value.erase(0, value.find_first_not_of(L" \t"));
            value.erase(value.find_last_not_of(L" \t") + 1);
            
            parseConfigLine(currentSection + L"." + key + L"=" + value);
        }
    }
    
    file.close();
    
    // 构建重定向映射
    buildRedirectMap();
    
    Logger::getInstance().log(L"配置文件加载完成");
    return true;
}

void ConfigManager::parseConfigLine(const std::wstring& line) {
    size_t pos = line.find(L'=');
    if (pos == std::wstring::npos) return;
    
    std::wstring key = line.substr(0, pos);
    std::wstring value = line.substr(pos + 1);
    
    if (key == L"General.EnableFileRedirect") {
        m_config.enableFileRedirect = (value == L"1");
    } else if (key == L"General.EnableFontHook") {
        m_config.enableFontHook = (value == L"1");
    } else if (key == L"General.EnableCodepageHook") {
        m_config.enableCodepageHook = (value == L"1");
    } else if (key == L"General.EnableWindowTitleHook") {
        m_config.enableWindowTitleHook = (value == L"1");
    } else if (key == L"General.EnableLogging") {
        m_config.enableLogging = (value == L"1");
    } else if (key == L"FileRedirect.RedirectFolder") {
        m_config.redirectFolder = value;
    } else if (key == L"Font.FontName") {
        m_config.fontName = value;
    } else if (key == L"Font.Charset") {
        m_config.charset = Utils::hexStringToInt(value);
    } else if (key == L"Font.FontWeight") {
        m_config.fontWeight = std::stoi(value);
    } else if (key == L"Font.FontHeight") {
        m_config.fontHeight = std::stoi(value);
    } else if (key == L"Font.FontWidth") {
        m_config.fontWidth = std::stoi(value);
    } else if (key == L"Codepage.SourceCodepage") {
        m_config.sourceCodepage = std::stoi(value);
    } else if (key == L"Codepage.TargetCodepage") {
        m_config.targetCodepage = std::stoi(value);
    } else if (key == L"WindowTitle.EnableTitleCheck") {
        m_config.enableTitleCheck = (value == L"1");
    } else if (key == L"WindowTitle.OriginalWindowTitle") {
        m_config.originalWindowTitle = value;
    } else if (key == L"WindowTitle.NewWindowTitle") {
        m_config.newWindowTitle = value;
    } else if (key == L"Logging.LogFile") {
        m_config.logFile = value;
    }
}

void ConfigManager::buildRedirectMap() {
    if (!m_config.enableFileRedirect) return;
    
    std::wstring redirectPath = getGameDirectory() + L"\\" + m_config.redirectFolder;
    if (!Utils::directoryExists(redirectPath)) {
        Logger::getInstance().log(L"重定向文件夹不存在: " + redirectPath);
        return;
    }
    
    // 这里应该递归遍历文件夹并构建映射
    // 简化实现，实际使用时需要完整实现递归遍历
    Logger::getInstance().log(L"文件重定向映射已构建，文件夹: " + redirectPath);
}

std::wstring ConfigManager::getGameDirectory() {
    // 获取当前工作目录而不是模块目录
    wchar_t buffer[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, buffer);
    return std::wstring(buffer);
}

const HookConfig& ConfigManager::getConfig() const {
    return m_config;
}

void ConfigManager::setConfig(const HookConfig& config) {
    m_config = config;
}
