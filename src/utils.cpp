#include "utils.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

std::wstring Utils::stringToWstring(const std::string& str) {
    if (str.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

std::string Utils::wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::string Utils::wstringToANSI(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
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
