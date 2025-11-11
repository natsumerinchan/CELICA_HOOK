#pragma once
#ifndef AUTHOR_WINDOW_H
#define AUTHOR_WINDOW_H

#include <windows.h>
#include <string>
#include <vector>
#include "settings.h"

class AuthorWindow {
public:
    static AuthorWindow& getInstance();
    
    void show();
    void close();
    bool isVisible() const;
    
private:
    AuthorWindow() = default;
    ~AuthorWindow() = default;
    
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT handleLinkClick(HWND hwnd, int xPos, int yPos);
    static void openLink(const std::wstring& url);
    
    // 图标相关函数
    static HICON getTargetProcessIcon();
    static HICON extractIconFromExecutable(const std::wstring& exePath);
    static std::wstring getTargetProcessPath();
    
    HWND m_hwnd = nullptr;
    bool m_visible = false;
    HICON m_hIcon = nullptr; // 存储目标程序图标
    
    // 链接信息结构
    struct LinkInfo {
        std::wstring displayText;
        std::wstring url;
        RECT rect;
        bool hovered;
    };
    
    static std::vector<LinkInfo> m_links;
    static bool m_linksInitialized;
    static int m_countdown; // 新增：用于倒计时的静态变量
    
    static int calculateDisplayWidth(const std::wstring& str);
};

#endif // AUTHOR_WINDOW_H
