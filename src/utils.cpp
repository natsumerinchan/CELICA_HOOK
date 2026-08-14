#include "utils.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

std::wstring Utils::stringToWstring(const std::string& str) {
    // 别名：保持与原有调用兼容，UTF-8 语义
    return utf8ToWstring(str);
}

std::wstring Utils::ansiToWstring(const std::string& str) {
    if (str.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed <= 0) return L"";
    
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::wstring Utils::utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed <= 0) return L"";
    
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::string Utils::wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return "";
    
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::string Utils::wstringToANSI(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return "";
    
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::wstring Utils::getModuleFilePath() {
    // 用循环增长的缓冲区获取完整模块路径，避免 MAX_PATH 截断长路径
    DWORD size = MAX_PATH;
    std::wstring buffer;
    for (;;) {
        buffer.resize(size);
        DWORD len = GetModuleFileNameW(NULL, &buffer[0], size);
        if (len == 0) {
            return L"";
        }
        if (len < size) {
            buffer.resize(len);
            return buffer;
        }
        // 缓冲区不足：翻倍重试
        size *= 2;
    }
}

std::wstring Utils::getModuleDirectory() {
    std::wstring path = getModuleFilePath();
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos);
    }
    return L"";
}

bool Utils::fileExists(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Utils::directoryExists(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring Utils::getFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::wstring Utils::getDirectory(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos);
    }
    return L"";
}

std::wstring Utils::combinePaths(const std::wstring& path1, const std::wstring& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    
    if (path1.back() == L'\\' || path1.back() == L'/') {
        if (path2.front() == L'\\' || path2.front() == L'/') {
            return path1 + path2.substr(1);
        }
        return path1 + path2;
    } else {
        if (path2.front() == L'\\' || path2.front() == L'/') {
            return path1 + path2;
        }
        return path1 + L"\\" + path2;
    }
}

int Utils::hexStringToInt(const std::wstring& hexStr) {
    std::wstring str = hexStr;
    
    // 移除0x前缀（大小写均可）
    if (str.length() > 2 && (str.substr(0, 2) == L"0x" || str.substr(0, 2) == L"0X")) {
        str = str.substr(2);
    }
    
    if (str.empty()) return 0;
    
    // 修复：value 必须初始化，且解析失败（如 "zz"）时返回 0，避免未初始化变量 UB
    int value = 0;
    std::wstringstream ss;
    ss << std::hex << str;
    if (!(ss >> value)) {
        return 0;
    }
    
    return value;
}

std::wstring Utils::intToHexString(int value) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << std::uppercase << value;
    return ss.str();
}

// 安全验证函数实现

std::wstring Utils::normalizePath(const std::wstring& path) {
    // 纯词法路径归一化（不访问文件系统）：
    // 1. 统一分隔符为反斜杠
    // 2. 折叠连续分隔符
    // 3. 忽略 "." 段
    // 4. 解析 ".." 段（弹出上一级；相对路径不越过根，防止路径遍历）
    // 5. 保留盘符/UNC 前缀
    //
    // 修复原实现直接删除 "..\" 子串的问题：既无法真正归一化，
    // 又会破坏形如 "v1..2\file.txt" 的合法文件名。
    std::wstring p = path;
    std::replace(p.begin(), p.end(), L'/', L'\\');

    std::wstring prefix;
    size_t i = 0;

    if (p.size() >= 2 && p[1] == L':' &&
        ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z'))) {
        // 盘符，如 "C:"
        prefix = p.substr(0, 2);
        i = 2;
    } else if (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\') {
        // UNC 前缀，如 \\server\share
        size_t third = p.find(L'\\', 2);
        if (third != std::wstring::npos) {
            size_t fourth = p.find(L'\\', third + 1);
            prefix = p.substr(0, (fourth == std::wstring::npos) ? p.size() : fourth);
            i = prefix.size();
        } else {
            prefix = p;
            i = p.size();
        }
    }

    std::vector<std::wstring> parts;
    while (i < p.size()) {
        size_t next = p.find(L'\\', i);
        std::wstring seg = (next == std::wstring::npos) ? p.substr(i) : p.substr(i, next - i);

        if (seg.empty() || seg == L".") {
            // 空段（连续分隔符）或当前目录段：忽略
        } else if (seg == L"..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
            // 相对路径在根处遇到 ".."：直接丢弃（不越过根）
        } else {
            parts.push_back(seg);
        }

        if (next == std::wstring::npos) break;
        i = next + 1;
    }

    std::wstring result = prefix;
    for (const auto& seg : parts) {
        if (!result.empty() && result.back() != L'\\') {
            result += L'\\';
        }
        result += seg;
    }
    return result;
}

bool Utils::isValidExtension(const std::wstring& filename, const std::wstring& allowedExtensions) {
    if (allowedExtensions.empty()) return true;
    
    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos == std::wstring::npos) return false;
    
    std::wstring ext = toLower(filename.substr(dotPos));
    std::vector<std::wstring> allowedExts = splitCommaList(allowedExtensions);
    
    for (const auto& allowedExt : allowedExts) {
        if (ext == toLower(allowedExt)) return true;
    }
    
    return false;
}

void Utils::findFilesRecursive(const std::wstring& directory, std::vector<std::wstring>& files, const std::wstring& pattern) {
    std::wstring searchPath = combinePaths(directory, pattern);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                std::wstring fullPath = combinePaths(directory, findData.cFileName);
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // 递归遍历子目录
                    findFilesRecursive(fullPath, files, pattern);
                } else {
                    files.push_back(fullPath);
                }
            }
        } while (FindNextFileW(hFind, &findData) != 0);
        
        FindClose(hFind);
    }
}

// 新增工具函数实现

std::vector<std::wstring> Utils::splitCommaList(const std::wstring& str) {
    std::vector<std::wstring> result;
    if (str.empty()) return result;
    
    size_t start = 0;
    size_t end = str.find(L',');
    while (end != std::wstring::npos) {
        std::wstring item = str.substr(start, end - start);
        // 移除前后空白
        item.erase(0, item.find_first_not_of(L" \t"));
        if (!item.empty()) {
            size_t last = item.find_last_not_of(L" \t");
            if (last != std::wstring::npos) {
                item.erase(last + 1);
            }
        }
        if (!item.empty()) {
            result.push_back(item);
        }
        start = end + 1;
        end = str.find(L',', start);
    }
    
    std::wstring lastItem = str.substr(start);
    lastItem.erase(0, lastItem.find_first_not_of(L" \t"));
    if (!lastItem.empty()) {
        size_t last = lastItem.find_last_not_of(L" \t");
        if (last != std::wstring::npos) {
            lastItem.erase(last + 1);
        }
    }
    if (!lastItem.empty()) {
        result.push_back(lastItem);
    }
    
    return result;
}

std::wstring Utils::stripLongPathPrefix(const std::wstring& path) {
    // 去掉 \\?\ 前缀，保留盘符（如 \\?\C:\foo -> C:\foo）
    if (path.rfind(L"\\\\?\\", 0) == 0) {
        if (path.size() > 4) {
            return path.substr(4);
        }
        return L"";
    }
    // 设备路径 \\?\GLOBALROOT\... 不在此处理，由调用方决定
    return path;
}

bool Utils::ensureDirectoryExists(const std::wstring& path) {
    if (path.empty()) return false;
    if (directoryExists(path)) return true;
    
    // 递归创建父目录
    std::wstring parent = getDirectory(path);
    if (!parent.empty() && !directoryExists(parent)) {
        ensureDirectoryExists(parent);
    }
    
    return CreateDirectoryW(path.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool Utils::startsWithIgnoreCase(const std::wstring& str, const std::wstring& prefix) {
    if (str.size() < prefix.size()) return false;
    return _wcsnicmp(str.c_str(), prefix.c_str(), prefix.size()) == 0;
}

std::wstring Utils::toLower(const std::wstring& str) {
    std::wstring lower = str;
    for (auto& ch : lower) {
        ch = towlower(ch);
    }
    return lower;
}

bool Utils::isAbsolutePath(const std::wstring& path) {
    if (path.empty()) return false;
    // UNC / 以根为基准的路径
    if (path[0] == L'\\') return true;
    // 盘符路径，如 C:\...
    if (path.size() >= 2 && path[1] == L':') return true;
    return false;
}

std::wstring Utils::getFullPath(const std::wstring& path) {
    // GetFullPathNameW 会解析相对段与 ".."，返回规范化的绝对路径；
    // 失败时原样返回，由调用方决定后续处理
    if (path.empty()) return L"";

    DWORD size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (size == 0) return path;

    std::wstring buffer(size, L'\0');
    DWORD len = GetFullPathNameW(path.c_str(), size, &buffer[0], nullptr);
    if (len == 0 || len >= size) {
        // 罕见：路径在计算期间变化导致缓冲区不足，用大缓冲重试一次
        size = 32768;
        buffer.assign(size, L'\0');
        len = GetFullPathNameW(path.c_str(), size, &buffer[0], nullptr);
        if (len == 0 || len >= size) {
            return path;
        }
    }
    buffer.resize(len);
    return buffer;
}

std::wstring Utils::resolveTargetPath(const std::wstring& targetPath) {
    if (targetPath.empty()) return L"";
    std::wstring combined = isAbsolutePath(targetPath)
        ? targetPath
        : combinePaths(getModuleDirectory(), targetPath);
    return getFullPath(combined);
}
