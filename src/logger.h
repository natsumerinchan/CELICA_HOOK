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
    void log(const std::string& message);
    void log(const std::wstring& message);
    void close();
    
private:
    Logger();
    ~Logger();
    
    std::ofstream m_logFile;
    bool m_initialized = false;
    
    void writeBOM();
    std::string wstringToUTF8(const std::wstring& wstr);
    std::wstring getCurrentTime();
};

#endif // LOGGER_H
