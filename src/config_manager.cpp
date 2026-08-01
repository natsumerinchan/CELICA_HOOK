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

// 安全的字符串转换函数
int safeStoi(const std::wstring& str, int defaultValue = 0) {
    try {
        return std::stoi(str);
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigManager::loadConfig(const std::wstring& configFile) {
    Logger::getInstance().log(L"正在加载配置文件: " + configFile);
    
    try {
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
        // 重建缓存列表
        rebuildCachedLists();
        
        Logger::getInstance().log(L"配置文件加载完成");
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log(L"配置解析异常: " + Utils::stringToWstring(e.what()));
        return false;
    }
}

void ConfigManager::parseConfigLine(const std::wstring& line) {
    size_t pos = line.find(L'=');
    if (pos == std::wstring::npos) return;
    
    std::wstring key = line.substr(0, pos);
    std::wstring value = line.substr(pos + 1);
    
    if (key == L"General.EnableFileRedirect") {
        m_config.enableFileRedirect = (value == L"1");
    } else if (key == L"General.EnableFileSpoofing") {
        m_config.enableFileSpoofing = (value == L"1");
    } else if (key == L"General.EnableFontHook") {
        m_config.enableFontHook = (value == L"1");
    } else if (key == L"General.EnableWindowTitleHook") {
        m_config.enableWindowTitleHook = (value == L"1");
    } else if (key == L"General.EnableLocaleEmulation") {
        m_config.enableLocaleEmulation = (value == L"1");
    } else if (key == L"General.EnableLogging") {
        m_config.enableLogging = (value == L"1");
    } else if (key == L"File.RedirectFolder") {
        m_config.redirectFolder = value;
    } else if (key == L"File.EnableExtensionCheck") {
        m_config.enableExtensionCheck = (value == L"1");
        Logger::getInstance().log(L"设置扩展名检查: " + value);
    } else if (key == L"File.RedirectExtensions") {
        m_config.redirectExtensions = value;
        Logger::getInstance().log(L"设置重定向扩展名: " + value);
    } else if (key == L"File.SpoofedFiles") {
        m_config.spoofedFiles = value;
    } else if (key == L"File.SpoofedDirectories") {
        m_config.spoofedDirectories = value;
    } else if (key == L"Font.FontName") {
        m_config.fontName = value;
    } else if (key == L"Font.FontFileName") {
        m_config.fontFileName = value;
    } else if (key == L"Font.EnableCreateFontA") {
        m_config.enableCreateFontA = (value == L"1");
    } else if (key == L"Font.EnableCreateFontW") {
        m_config.enableCreateFontW = (value == L"1");
    } else if (key == L"Font.EnableCreateFontIndirectA") {
        m_config.enableCreateFontIndirectA = (value == L"1");
    } else if (key == L"Font.EnableCreateFontIndirectW") {
        m_config.enableCreateFontIndirectW = (value == L"1");
    } else if (key == L"Font.Charset") {
        m_config.localeCharset = Utils::hexStringToInt(value);
    } else if (key == L"Font.FontWeight") {
        m_config.fontWeight = safeStoi(value);
    } else if (key == L"Font.FontHeight") {
        m_config.fontHeight = safeStoi(value);
    } else if (key == L"Font.FontWidth") {
        m_config.fontWidth = safeStoi(value);
    } else if (key == L"WindowTitle.EnableTitleCheck") {
        m_config.enableTitleCheck = (value == L"1");
    } else if (key == L"WindowTitle.OriginalWindowTitle") {
        m_config.originalWindowTitle = value;
    } else if (key == L"WindowTitle.NewWindowTitle") {
        m_config.newWindowTitle = value;
    } else if (key == L"LocaleEmulation.LocaleCodepage") {
        m_config.localeCodepage = safeStoi(value);
    } else if (key == L"LocaleEmulation.LocaleId") {
        m_config.localeId = safeStoi(value);
    } else if (key == L"LocaleEmulation.Timezone") {
        m_config.timezone = value;
    } else if (key == L"Logging.LogFile") {
        m_config.logFile = value;
    } else if (key == L"General.TargetProcess") {
        m_config.targetProcess = value;
    }
}

void ConfigManager::buildRedirectMap() {
    m_redirectMap.clear();
    
    if (!m_config.enableFileRedirect) return;
    
    std::wstring gameDir = getGameDirectory();
    std::wstring redirectPath = Utils::combinePaths(gameDir, m_config.redirectFolder);
    if (!Utils::directoryExists(redirectPath)) {
        Logger::getInstance().log(L"重定向文件夹不存在: " + redirectPath);
        return;
    }
    
    // 递归遍历文件夹并构建映射
    std::vector<std::wstring> files;
    Utils::findFilesRecursive(redirectPath, files, L"*");
    
    for (const auto& file : files) {
        // 构建相对路径映射
        std::wstring relativePath = file.substr(redirectPath.length() + 1);
        // 归一化分隔符并转小写作为 key（Windows 不区分大小写）
        relativePath = Utils::normalizePath(relativePath);
        std::wstring lowerKey = Utils::toLower(relativePath);
        
        // 检查扩展名（如果启用）
        if (m_config.enableExtensionCheck && !Utils::isValidExtension(relativePath, m_config.redirectExtensions)) {
            continue;
        }
        
        m_redirectMap[lowerKey] = file;
        Logger::getInstance().log(L"映射文件: " + relativePath + L" -> " + file);
    }
    
    Logger::getInstance().log(L"文件重定向映射已构建，共映射 " + std::to_wstring(m_redirectMap.size()) + L" 个文件");
}

std::wstring ConfigManager::getGameDirectory() {
    // 统一使用模块目录（游戏可执行文件所在目录）作为基准，
    // 避免与 GetCurrentDirectoryW（工作目录）不一致导致重定向失效
    return Utils::getModuleDirectory();
}

void ConfigManager::rebuildCachedLists() {
    m_redirectExtList.clear();
    m_spoofedFileList.clear();
    m_spoofedDirList.clear();
    
    // 解析扩展名列表（小写）
    for (auto& ext : Utils::splitCommaList(m_config.redirectExtensions)) {
        m_redirectExtList.push_back(Utils::toLower(ext));
    }
    
    // 解析欺骗文件列表（归一化 + 小写）
    for (auto& file : Utils::splitCommaList(m_config.spoofedFiles)) {
        m_spoofedFileList.push_back(Utils::toLower(Utils::normalizePath(file)));
    }
    
    // 解析欺骗目录列表（归一化 + 小写 + 确保末尾反斜杠）
    for (auto& dir : Utils::splitCommaList(m_config.spoofedDirectories)) {
        std::wstring normalized = Utils::toLower(Utils::normalizePath(dir));
        if (!normalized.empty() && normalized.back() != L'\\') {
            normalized += L'\\';
        }
        if (!normalized.empty()) {
            m_spoofedDirList.push_back(normalized);
        }
    }
}

bool ConfigManager::findRedirectedPath(const std::wstring& relativePath, std::wstring& outFullPath) const {
    if (!m_config.enableFileRedirect || m_redirectMap.empty()) return false;
    
    // 归一化 + 小写查找
    std::wstring key = Utils::toLower(Utils::normalizePath(relativePath));
    auto it = m_redirectMap.find(key);
    if (it == m_redirectMap.end()) return false;
    
    outFullPath = it->second;
    return true;
}

bool ConfigManager::isExtensionRedirected(const std::wstring& filename) const {
    // 如果禁用了扩展名检查，重定向所有文件（由调用方保证 enableFileRedirect 已开启）
    if (!m_config.enableExtensionCheck) return true;
    
    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos == std::wstring::npos) return false;
    
    std::wstring ext = Utils::toLower(filename.substr(dotPos));
    for (const auto& allowedExt : m_redirectExtList) {
        if (ext == allowedExt) return true;
    }
    
    return false;
}

bool ConfigManager::isFileSpoofed(const std::wstring& relativePath) const {
    if (!m_config.enableFileSpoofing || m_spoofedFileList.empty()) return false;
    
    std::wstring key = Utils::toLower(Utils::normalizePath(relativePath));
    for (const auto& spoofedFile : m_spoofedFileList) {
        if (key == spoofedFile) return true;
    }
    
    return false;
}

bool ConfigManager::isDirectorySpoofed(const std::wstring& relativePath) const {
    if (!m_config.enableFileSpoofing || m_spoofedDirList.empty()) return false;
    
    std::wstring normalized = Utils::normalizePath(relativePath);
    std::wstring lowerPath = Utils::toLower(normalized);
    
    // 目录匹配：相对路径是某个欺骗目录或在其子目录下
    for (const auto& spoofedDir : m_spoofedDirList) {
        if (lowerPath == spoofedDir || 
            (lowerPath.size() > spoofedDir.size() && lowerPath.compare(0, spoofedDir.size(), spoofedDir) == 0)) {
            return true;
        }
    }
    
    return false;
}

const HookConfig& ConfigManager::getConfig() const {
    return m_config;
}

void ConfigManager::setConfig(const HookConfig& config) {
    m_config = config;
}
