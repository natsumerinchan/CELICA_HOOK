#include "logger.h"
#include "utils.h"
#include <chrono>
#include <iomanip>
#include <sstream>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_initialized(false) {
}

Logger::~Logger() {
    close();
}

bool Logger::initialize(const std::wstring& logFile) {
    if (m_initialized) {
        return true;
    }
    
    // 使用trunc模式打开文件，清空之前的内容
    m_logFile.open(logFile, std::ios::out | std::ios::trunc);
    if (!m_logFile.is_open()) {
        // 回退到调试输出
        OutputDebugStringW(L"日志文件打开失败，使用调试输出\n");
        m_useDebugOutput = true;
        m_initialized = true;
        return true;
    }
    
    writeBOM();
    m_initialized = true;
    m_useDebugOutput = false;
    
    log(L"日志系统初始化完成");
    return true;
}

void Logger::writeBOM() {
    // 写入UTF-8 BOM
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    m_logFile.write(reinterpret_cast<char*>(bom), sizeof(bom));
}

void Logger::log(const std::string& message) {
    if (!m_initialized) return;
    
    if (m_useDebugOutput) {
        // 调试输出模式下 m_logFile 未打开，不能写入文件流
        OutputDebugStringA((Utils::wstringToString(getCurrentTime()) + " " + message + "\n").c_str());
        return;
    }
    
    std::string timestamp = Utils::wstringToString(getCurrentTime());
    m_logFile << "[" << timestamp << "] " << message << std::endl;
    m_logFile.flush();
}

void Logger::log(const std::wstring& message) {
    if (!m_initialized) return;
    
    if (m_useDebugOutput) {
        OutputDebugStringW((getCurrentTime() + L" " + message + L"\n").c_str());
    } else {
        std::string utf8Message = wstringToUTF8(message);
        log(utf8Message);
    }
}

void Logger::close() {
    if (m_initialized) {
        log(L"日志系统关闭");
        m_logFile.close();
        m_initialized = false;
    }
}

std::string Logger::wstringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

std::wstring Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // 使用线程安全的 localtime_s 替代 std::localtime（后者使用全局静态缓冲区，多线程下存在数据竞争）
    struct tm localTime {};
    localtime_s(&localTime, &time_t);
    
    std::wstringstream ss;
    ss << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S");
    ss << L"." << std::setfill(L'0') << std::setw(3) << ms.count();
    
    return ss.str();
}
