#include "settings.h"
#include "utils.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// 安全的字符串转换函数
// 注意：必须声明为 static/内部链接，避免该自由函数泄漏到全局命名空间，
// 与其他翻译单元（或第三方库）的同名函数产生 ODR 冲突
static int safeStoi(const std::wstring& str, int defaultValue = 0) {
    try {
        return std::stoi(str);
    } catch (...) {
        return defaultValue;
    }
}

// 布尔值解析：兼容 1/0 与 true/false/yes/no/on/off（大小写不敏感）
static bool parseBool(const std::wstring& value) {
    std::wstring v = Utils::toLower(value);
    return v == L"1" || v == L"true" || v == L"yes" || v == L"on";
}

bool ConfigManager::loadConfig(const std::wstring& configFile) {
    Logger::getInstance().log(L"正在加载配置文件: " + configFile);
    
    try {
        std::ifstream file(configFile, std::ios::binary);
        if (!file.is_open()) {
            Logger::getInstance().log(L"无法打开配置文件: " + configFile);
            return false;
        }
    
        // 以字节流读入后按 UTF-8 解码（与日志/字体等模块的编码约定一致），
        // 替代已弃用的 std::codecvt_utf8，并显式剥离 BOM，
        // 避免首行注释因 BOM 前缀而解析失败
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        std::wstring wcontent = Utils::utf8ToWstring(content);
        std::wstringstream ss(wcontent);

        std::wstring line;
        std::wstring currentSection;
        
        while (std::getline(ss, line)) {
            // 去掉可能的 CR（Windows 换行）
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }

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
        m_config.enableFileRedirect = parseBool(value);
    } else if (key == L"General.EnableFileSpoofing") {
        m_config.enableFileSpoofing = parseBool(value);
    } else if (key == L"General.EnableFontHook") {
        m_config.enableFontHook = parseBool(value);
    } else if (key == L"General.EnableWindowTitleHook") {
        m_config.enableWindowTitleHook = parseBool(value);
    } else if (key == L"General.EnableLocaleEmulation") {
        m_config.enableLocaleEmulation = parseBool(value);
    } else if (key == L"General.EnableLogging") {
        m_config.enableLogging = parseBool(value);
    } else if (key == L"File.RedirectFolder") {
        m_config.redirectFolder = value;
    } else if (key == L"File.EnableExtensionCheck") {
        m_config.enableExtensionCheck = parseBool(value);
        Logger::getInstance().log(L"设置扩展名检查: " + value);
    } else if (key == L"File.EnableFilenameOnlyMatch") {
        m_config.enableFilenameOnlyMatch = parseBool(value);
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
        m_config.enableCreateFontA = parseBool(value);
    } else if (key == L"Font.EnableCreateFontW") {
        m_config.enableCreateFontW = parseBool(value);
    } else if (key == L"Font.EnableCreateFontIndirectA") {
        m_config.enableCreateFontIndirectA = parseBool(value);
    } else if (key == L"Font.EnableCreateFontIndirectW") {
        m_config.enableCreateFontIndirectW = parseBool(value);
    } else if (key == L"Font.Charset") {
        m_config.localeCharset = Utils::hexStringToInt(value);
    } else if (key == L"Font.FontWeight") {
        m_config.fontWeight = safeStoi(value);
    } else if (key == L"Font.FontHeight") {
        m_config.fontHeight = safeStoi(value);
    } else if (key == L"Font.FontWidth") {
        m_config.fontWidth = safeStoi(value);
    } else if (key == L"WindowTitle.EnableTitleCheck") {
        m_config.enableTitleCheck = parseBool(value);
    } else if (key == L"WindowTitle.OriginalWindowTitle") {
        m_config.originalWindowTitle = value;
    } else if (key == L"WindowTitle.NewWindowTitle") {
        m_config.newWindowTitle = value;
    } else if (key == L"LocaleEmulation.LocaleCodepage") {
        int cp = safeStoi(value);
        m_config.localeCodepage = cp < 0 ? 0u : static_cast<unsigned int>(cp);
    } else if (key == L"LocaleEmulation.LocaleId") {
        int id = safeStoi(value);
        m_config.localeId = id < 0 ? 0u : static_cast<unsigned int>(id);
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

    // 游戏目录（归一化），用于把配置中的绝对路径转换为相对路径
    std::wstring gameDir = Utils::normalizePath(getGameDirectory());

    // 解析欺骗文件列表（归一化 + 小写）
    // 修复：配置注释声称"绝对路径亦可"，但此前实现只做相对比较导致绝对路径
    // 永远无法命中。现在位于游戏目录内的绝对路径会被转换为相对路径。
    for (auto& file : Utils::splitCommaList(m_config.spoofedFiles)) {
        std::wstring f = Utils::normalizePath(file);
        if (Utils::isAbsolutePath(f)) {
            std::wstring full = Utils::normalizePath(Utils::getFullPath(f));
            if (Utils::startsWithIgnoreCase(full, gameDir) &&
                (full.size() == gameDir.size() || full[gameDir.size()] == L'\\')) {
                f = full.substr(gameDir.size() + 1);
            } else {
                f = full;  // 游戏目录外的绝对路径保留（仅文件名兜底模式下可能匹配）
            }
        }
        std::wstring lower = Utils::toLower(f);
        if (!lower.empty()) {
            m_spoofedFileList.push_back(lower);
        }
    }
    
    // 解析欺骗目录列表（归一化 + 小写；不再强制末尾反斜杠，
    // 由 isDirectorySpoofed 用组件边界判断）
    for (auto& dir : Utils::splitCommaList(m_config.spoofedDirectories)) {
        std::wstring d = Utils::normalizePath(dir);
        if (Utils::isAbsolutePath(d)) {
            std::wstring full = Utils::normalizePath(Utils::getFullPath(d));
            if (Utils::startsWithIgnoreCase(full, gameDir) &&
                (full.size() == gameDir.size() || full[gameDir.size()] == L'\\')) {
                d = full.substr(gameDir.size() + 1);
            } else {
                d = full;
            }
        }
        std::wstring lower = Utils::toLower(d);
        while (!lower.empty() && lower.back() == L'\\') {
            lower.pop_back();
        }
        if (!lower.empty()) {
            m_spoofedDirList.push_back(lower);
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
    
    std::wstring lowerPath = Utils::toLower(Utils::normalizePath(relativePath));
    while (!lowerPath.empty() && lowerPath.back() == L'\\') {
        lowerPath.pop_back();
    }
    
    // 目录匹配：相对路径是某个欺骗目录，或在其子目录下。
    // 使用组件边界判断，避免 "temp\logs" 误匹配 "temp\logs2\..."
    for (const auto& spoofedDir : m_spoofedDirList) {
        if (lowerPath == spoofedDir) return true;
        if (lowerPath.size() > spoofedDir.size() &&
            lowerPath.compare(0, spoofedDir.size(), spoofedDir) == 0 &&
            lowerPath[spoofedDir.size()] == L'\\') {
            return true;
        }
    }
    
    return false;
}

const HookConfig& ConfigManager::getConfig() const {
    return m_config;
}
