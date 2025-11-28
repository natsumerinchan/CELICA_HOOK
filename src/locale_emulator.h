#pragma once
#ifndef LOCALE_EMULATOR_H
#define LOCALE_EMULATOR_H

#include <windows.h>
#include <string>

// 基于LELOADER的转区功能实现
class LocaleEmulator {
public:
    static LocaleEmulator& getInstance();
    
    // 初始化转区功能
    bool initialize();
    
    // 检查是否需要转区
    bool needsLocaleEmulation() const;
    
    // 执行转区操作
    bool performLocaleEmulation();
    
    // 使用转区设置创建进程
    bool createProcessWithLocale(const std::wstring& applicationPath);
    
    // 获取转区状态
    bool isLocaleEmulationEnabled() const { return m_enabled; }
    
private:
    LocaleEmulator() = default;
    ~LocaleEmulator() = default;
    
    // 禁用拷贝和赋值
    LocaleEmulator(const LocaleEmulator&) = delete;
    LocaleEmulator& operator=(const LocaleEmulator&) = delete;
    
    // 重新启动进程（基于LELOADER的实现）
    bool relaunchProcess();
    
    // 设置时区信息
    bool setupTimezone();
    
    bool m_enabled = false;
    bool m_initialized = false;
    
    // 转区配置
    unsigned int m_codepage = 932;
    unsigned int m_localeId = 1041;
    unsigned int m_charset = SHIFTJIS_CHARSET;
    std::wstring m_timezone = L"Tokyo Standard Time";
};

#endif // LOCALE_EMULATOR_H
