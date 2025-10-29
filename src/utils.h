#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <string>
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
};

#endif // UTILS_H
