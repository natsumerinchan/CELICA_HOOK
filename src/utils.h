#pragma once
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h>

class Utils {
public:
    static std::wstring stringToWstring(const std::string& str);
    static std::wstring ansiToWstring(const std::string& str);
    static std::wstring utf8ToWstring(const std::string& str);
    static std::string wstringToString(const std::wstring& wstr);
    static std::string wstringToANSI(const std::wstring& wstr);
    static std::wstring getModuleDirectory();
    static std::wstring getModuleFilePath();
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

    // 新增工具函数
    static std::vector<std::wstring> splitCommaList(const std::wstring& str);
    static std::wstring stripLongPathPrefix(const std::wstring& path);
    static bool ensureDirectoryExists(const std::wstring& path);
    static bool startsWithIgnoreCase(const std::wstring& str, const std::wstring& prefix);
    static std::wstring toLower(const std::wstring& str);

    // 路径解析工具
    static bool isAbsolutePath(const std::wstring& path);
    static std::wstring getFullPath(const std::wstring& path);
    // 将配置中的目标程序路径解析为绝对路径（相对路径基于模块目录）
    static std::wstring resolveTargetPath(const std::wstring& targetPath);
};

#endif // UTILS_H