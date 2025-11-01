#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h>

class Utils {
public:
    static std::wstring stringToWstring(const std::string& str);
    static std::string wstringToString(const std::wstring& wstr);
    static std::string wstringToANSI(const std::wstring& wstr);
    static std::wstring getModuleDirectory();
    static bool fileExists(const std::wstring& path);
    static bool directoryExists(const std::wstring& path);
    static std::wstring getFileName(const std::wstring& path);
    static std::wstring getDirectory(const std::wstring& path);
    static std::wstring combinePaths(const std::wstring& path1, const std::wstring& path2);
    static int hexStringToInt(const std::wstring& hexStr);
    static std::wstring intToHexString(int value);
    
    // 安全验证函数
    static std::wstring normalizePath(const std::wstring& path);
    static bool isValidExtension(const std::wstring& filename, const std::wstring& allowedExtensions);
    static void findFilesRecursive(const std::wstring& directory, std::vector<std::wstring>& files, const std::wstring& pattern = L"*");
};

#endif // UTILS_H
