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

std::wstring Utils::getModuleDirectory() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    
    std::wstring path(buffer);
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
    
    // 移除0x前缀
    if (str.length() > 2 && str.substr(0, 2) == L"0x") {
        str = str.substr(2);
    }
    
    int value;
    std::wstringstream ss;
    ss << std::hex << str;
    ss >> value;
    
    return value;
}

std::wstring Utils::intToHexString(int value) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << std::uppercase << value;
    return ss.str();
}

// 安全验证函数实现

std::wstring Utils::normalizePath(const std::wstring& path) {
    std::wstring normalized = path;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    
    // 移除路径遍历攻击
    size_t pos;
    while ((pos = normalized.find(L"..\\")) != std::wstring::npos) {
        normalized.erase(pos, 3);
    }
    
    return normalized;
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
