#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <windows.h>

class Logger {
public:
    static Logger& getInstance();
    
    bool initialize(const std::wstring& logFile);
    void log(const std::wstring& message);
    void close();
    
private:
    Logger();
    ~Logger();
    
    std::ofstream m_logFile;
    bool m_initialized = false;
    bool m_useDebugOutput = false;
    
    // 线程安全：hook 回调运行在游戏任意线程上，后台重应用线程也会写日志，
    // 必须串行化对文件流的访问，避免数据竞争导致日志损坏或崩溃
    CRITICAL_SECTION m_lock;
    
    void writeBOM();
    std::string wstringToUTF8(const std::wstring& wstr);
    std::wstring getCurrentTime();
};

#endif // LOGGER_H
