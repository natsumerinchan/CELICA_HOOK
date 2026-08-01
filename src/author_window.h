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
    // 查询用户是否同意声明（true=同意, false=退出）
    bool shouldLaunch() const;
    
private:
    AuthorWindow() = default;
    ~AuthorWindow() = default;
    
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT handleLinkClick(HWND hwnd, int xPos, int yPos);
    static void openLink(const std::wstring& url);
    
    // 图标相关函数
    static HICON getTargetProcessIcon();
    static HICON extractIconFromExecutable(const std::wstring& exePath);
    static HICON loadIconFromFile(const std::wstring& icoPath);
    static HICON findAndLoadGameIcon(const std::wstring& exePath);
    static std::wstring getTargetProcessPath();
    
    // 绘制按钮
    static void drawButton(HDC hdc, const RECT& rect, const std::wstring& text, bool hovered);
    
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
    
    // 按钮相关状态
    static RECT m_confirmBtnRect;   // 确认按钮位置
    static RECT m_cancelBtnRect;    // 退出按钮位置
    static bool m_confirmHovered;   // 确认按钮悬停状态
    static bool m_cancelHovered;    // 退出按钮悬停状态
    static bool m_shouldLaunchFlag; // 用户决策: true=同意, false=退出
    
    static int calculateDisplayWidth(const std::wstring& str);
};

#endif // AUTHOR_WINDOW_H