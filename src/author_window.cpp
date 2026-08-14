#include "author_window.h"
#include "settings.h"
#include "logger.h"
#include "utils.h"
#include <shellapi.h>
#include <algorithm>
#include <sstream>
#include <psapi.h>

// 静态成员初始化
std::vector<AuthorWindow::LinkInfo> AuthorWindow::m_links;
bool AuthorWindow::m_linksInitialized = false;
HFONT AuthorWindow::m_linkFont = nullptr;
RECT AuthorWindow::m_confirmBtnRect = {};
RECT AuthorWindow::m_cancelBtnRect = {};
bool AuthorWindow::m_confirmHovered = false;
bool AuthorWindow::m_cancelHovered = false;
bool AuthorWindow::m_shouldLaunchFlag = true; // 默认同意，防止意外退出

AuthorWindow& AuthorWindow::getInstance() {
    static AuthorWindow instance;
    return instance;
}

void AuthorWindow::show() {
    if (m_visible) {
        Logger::getInstance().log(L"AuthorWindow::show() - 窗口已显示，跳过");
        return;
    }
    
    Logger::getInstance().log(L"AuthorWindow::show() - 开始显示窗口");
    
    // 获取目标程序图标
    m_hIcon = getTargetProcessIcon();
    
    // 创建链接文本测量/绘制共用的字体（创建一次，避免每条消息都重建字体）
    if (m_linkFont == nullptr) {
        m_linkFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"黑体");
    }
    
    // 注册窗口类
    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"AuthorInfoWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    // 设置图标
    if (m_hIcon) {
        wc.hIcon = m_hIcon;
        Logger::getInstance().log(L"AuthorWindow::show() - 已设置目标程序图标");
    } else {
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        Logger::getInstance().log(L"AuthorWindow::show() - 使用默认应用程序图标");
    }
    
    ATOM classAtom = RegisterClassW(&wc);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        DWORD error = GetLastError();
        Logger::getInstance().log(L"AuthorWindow::show() - 注册窗口类失败，错误代码: " + std::to_wstring(error));
        return;
    }
    Logger::getInstance().log(L"AuthorWindow::show() - 窗口类注册成功");
    
    // 重置用户决策标志，每次显示窗口时都重新等待用户选择
    m_shouldLaunchFlag = false;
    
    // 获取配置管理器实例
    ConfigManager& configManager = ConfigManager::getInstance();
    const HookConfig& config = configManager.getConfig();
    
    // 确定窗口标题
    std::wstring windowTitle = L"CELICA HOOK LAUNCHER";
    if (!config.newWindowTitle.empty()) {
        windowTitle = config.newWindowTitle;
    } else if (!config.originalWindowTitle.empty()) {
        windowTitle = config.originalWindowTitle;
    }
    
    // 计算窗口大小 - 根据标题长度动态调整宽度
    int titleWidth = calculateDisplayWidth(windowTitle);
    int windowWidth = (std::max)(600, (std::min)(1200, titleWidth * 8 + 200));
    int windowHeight = 400;
    
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算窗口位置（居中）
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;
    
    // 创建窗口
    Logger::getInstance().log(L"AuthorWindow::show() - 开始创建窗口...");
    
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"AuthorInfoWindow",
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW, // 移除 WS_VISIBLE, 由 ShowWindow 控制
        windowX, windowY, windowWidth, windowHeight,
        NULL, NULL, GetModuleHandle(NULL), this // 最后一个参数可以传递this指针
    );
    
    if (m_hwnd) {
        Logger::getInstance().log(L"AuthorWindow::show() - 窗口创建成功，句柄: " + std::to_wstring(reinterpret_cast<uintptr_t>(m_hwnd)));
        m_visible = true;
        
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        
    } else {
        DWORD error = GetLastError();
        Logger::getInstance().log(L"AuthorWindow::show() - 窗口创建失败，错误代码: " + std::to_wstring(error));
    }
}

void AuthorWindow::close() {
    if (m_hwnd) {
        Logger::getInstance().log(L"AuthorWindow::close() - 开始关闭窗口");
        DestroyWindow(m_hwnd);
        
        // 清理图标资源
        if (m_hIcon) {
            DestroyIcon(m_hIcon);
            m_hIcon = nullptr;
            Logger::getInstance().log(L"AuthorWindow::close() - 已清理图标资源");
        }
        
        // 清理链接字体（下次 show 时重建）
        if (m_linkFont) {
            DeleteObject(m_linkFont);
            m_linkFont = nullptr;
        }
    } else {
        Logger::getInstance().log(L"AuthorWindow::close() - 窗口句柄为空，无需关闭");
    }
}

bool AuthorWindow::isVisible() const {
    return m_visible;
}

bool AuthorWindow::shouldLaunch() const {
    return m_shouldLaunchFlag;
}

void AuthorWindow::drawButton(HDC hdc, const RECT& rect, const std::wstring& text, bool hovered) {
    // 设置背景填充
    HBRUSH bgBrush = CreateSolidBrush(hovered ? RGB(220, 235, 255) : RGB(245, 245, 245));
    HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
    
    // 设置边框
    HPEN borderPen = CreatePen(PS_SOLID, 2, hovered ? RGB(0, 120, 215) : RGB(160, 160, 160));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    
    // 绘制圆角矩形按钮
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
    
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bgBrush);
    
    // 绘制按钮文字
    SetTextColor(hdc, RGB(0, 0, 0));
    RECT textRect = rect;
    DrawTextW(hdc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 命中测试：鼠标是否位于链接行中 URL 文本上。
// 与 WM_PAINT 使用同一字体测量，保证命中区域与绘制区域一致；
// 将原先在 WM_MOUSEMOVE / WM_SETCURSOR / handleLinkClick 中重复三遍的
// 文本测量逻辑集中于此。
bool AuthorWindow::hitTestLinkUrl(HWND hwnd, const LinkInfo& link, POINT pt) {
    if (pt.y < link.rect.top || pt.y > link.rect.bottom || link.url.empty()) {
        return false;
    }
    if (m_linkFont == nullptr) {
        return false;
    }

    std::wstring displayText = link.displayText;
    size_t colonPos = displayText.find(L':');
    std::wstring description = (colonPos != std::wstring::npos) ? displayText.substr(0, colonPos + 1) : displayText;
    std::wstring urlPart = (colonPos != std::wstring::npos) ? displayText.substr(colonPos + 1) : L"";
    if (urlPart.empty()) {
        return false;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        return false;
    }
    HGDIOBJ hOldFont = SelectObject(hdc, m_linkFont);

    SIZE descSize, urlSize;
    GetTextExtentPoint32W(hdc, description.c_str(), (int)description.length(), &descSize);
    GetTextExtentPoint32W(hdc, urlPart.c_str(), (int)urlPart.length(), &urlSize);

    SelectObject(hdc, hOldFont);
    ReleaseDC(hwnd, hdc);

    int totalWidth = descSize.cx + urlSize.cx;
    int startX = (link.rect.right - totalWidth) / 2;
    int urlStartX = startX + descSize.cx;
    int urlEndX = urlStartX + urlSize.cx;

    return pt.x >= urlStartX && pt.x <= urlEndX;
}

LRESULT AuthorWindow::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // 为了避免日志过于冗长，可以过滤掉一些高频消息
    if (uMsg != WM_MOUSEMOVE && uMsg != WM_SETCURSOR && uMsg != WM_NCHITTEST) {
        std::wstringstream msgStream;
        msgStream << L"AuthorWindow::windowProc() - 收到消息: " << uMsg << L" (0x" << std::hex << uMsg << L")";
        Logger::getInstance().log(msgStream.str());
    }
    
    switch (uMsg) {
        case WM_CREATE: {
            Logger::getInstance().log(L"AuthorWindow::windowProc() - WM_CREATE 消息");
            // 初始化链接信息
            if (!m_linksInitialized) {
                for (int i = 0; i < WINDOW_LINKS_COUNT; i++) {
                    LinkInfo linkInfo;
                    linkInfo.displayText = WINDOW_LINKS[i].displayText + WINDOW_LINKS[i].url;
                    linkInfo.url = WINDOW_LINKS[i].url;
                    linkInfo.rect = {};
                    linkInfo.hovered = false;
                    m_links.push_back(linkInfo);
                }
                m_linksInitialized = true;
            }
            
            Logger::getInstance().log(L"AuthorWindow::windowProc() - WM_CREATE 完成");
            return 0;
        }
            
        case WM_CLOSE:
            Logger::getInstance().log(L"AuthorWindow::windowProc() - WM_CLOSE 消息");
            // DestroyWindow 会触发 WM_DESTROY
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            Logger::getInstance().log(L"AuthorWindow::windowProc() - WM_DESTROY 消息");
            
            // 清理资源
            AuthorWindow::getInstance().m_hwnd = nullptr;
            AuthorWindow::getInstance().m_visible = false;
            
            // 【关键改动】发送退出消息，以终止在 DllMain 中的消息循环
            PostQuitMessage(0);
            
            return 0;
            
        case WM_LBUTTONDOWN: {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            
            // 检查是否点击了"同意"按钮
            POINT pt = {xPos, yPos};
            if (PtInRect(&m_confirmBtnRect, pt)) {
                Logger::getInstance().log(L"AuthorWindow::windowProc() - 点击同意按钮");
                m_shouldLaunchFlag = true;
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            
            // 检查是否点击了"退出"按钮
            if (PtInRect(&m_cancelBtnRect, pt)) {
                Logger::getInstance().log(L"AuthorWindow::windowProc() - 点击退出按钮");
                m_shouldLaunchFlag = false;
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            
            // 否则检查链接点击
            return handleLinkClick(hwnd, xPos, yPos);
        }
            
        case WM_MOUSEMOVE: {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            POINT pt = {xPos, yPos};
            bool needRepaint = false;
            
            for (auto& link : m_links) {
                bool wasHovered = link.hovered;
                link.hovered = hitTestLinkUrl(hwnd, link, pt);
                
                if (wasHovered != link.hovered) {
                    needRepaint = true;
                }
            }
            
            // 检查确认按钮悬停状态
            bool confirmHovered = PtInRect(&m_confirmBtnRect, pt);
            if (confirmHovered != m_confirmHovered) {
                m_confirmHovered = confirmHovered;
                needRepaint = true;
            }
            
            // 检查退出按钮悬停状态
            bool cancelHovered = PtInRect(&m_cancelBtnRect, pt);
            if (cancelHovered != m_cancelHovered) {
                m_cancelHovered = cancelHovered;
                needRepaint = true;
            }
            
            if (needRepaint) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
            
        case WM_SETCURSOR: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            
            for (const auto& link : m_links) {
                if (hitTestLinkUrl(hwnd, link, pt)) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HFONT hOldFont = nullptr;
            if (m_linkFont != nullptr) {
                hOldFont = (HFONT)SelectObject(hdc, m_linkFont);
            }
            
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            int yPos = 20;
            int lineHeight = 20;
            
            RECT authorRect = {0, yPos, rect.right, yPos + lineHeight};
            DrawTextW(hdc, WINDOW_AUTHOR, -1, &authorRect, DT_CENTER);
            yPos += lineHeight * 2;
            
            RECT stmtRect = {0, yPos, rect.right, yPos + lineHeight * 2};
            DrawTextW(hdc, WINDOW_STATEMENT, -1, &stmtRect, DT_CENTER | DT_WORDBREAK);
            yPos += lineHeight * 3;
            
            for (auto& link : m_links) {
                RECT linkRect = {0, yPos, rect.right, yPos + lineHeight};
                link.rect = linkRect;
                
                std::wstring displayText = link.displayText;
                size_t colonPos = displayText.find(L':');
                std::wstring description, urlPart;
                
                if (colonPos != std::wstring::npos) {
                    description = displayText.substr(0, colonPos + 1);
                    urlPart = displayText.substr(colonPos + 1);
                } else {
                    description = displayText;
                    urlPart = L"";
                }
                
                SIZE descSize, urlSize;
                GetTextExtentPoint32W(hdc, description.c_str(), (int)description.length(), &descSize);
                GetTextExtentPoint32W(hdc, urlPart.c_str(), (int)urlPart.length(), &urlSize);
                
                int totalWidth = descSize.cx + urlSize.cx;
                int startX = (rect.right - totalWidth) / 2;
                
                SetTextColor(hdc, RGB(0, 0, 0));
                TextOutW(hdc, startX, yPos, description.c_str(), (int)description.length());
                
                SetTextColor(hdc, link.hovered ? RGB(0, 0, 255) : RGB(0, 0, 200));
                TextOutW(hdc, startX + descSize.cx, yPos, urlPart.c_str(), (int)urlPart.length());
                
                if (link.hovered && !urlPart.empty()) {
                    int underlineY = yPos + lineHeight - 2;
                    int underlineX = startX + descSize.cx;
                    
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 200));
                    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                    MoveToEx(hdc, underlineX, underlineY, NULL);
                    LineTo(hdc, underlineX + urlSize.cx, underlineY);
                    SelectObject(hdc, hOldPen);
                    DeleteObject(hPen);
                }
                
                yPos += lineHeight;
            }
            
            // 绘制确认/退出按钮
            yPos += lineHeight * 2;
            
            int buttonY = yPos;
            int buttonHeight = 32;
            int buttonWidth = 120;
            int spacing = 20;
            int totalBtnWidth = buttonWidth * 2 + spacing;
            int startX = (rect.right - totalBtnWidth) / 2;
            
            // 同意按钮
            m_confirmBtnRect = {startX, buttonY, startX + buttonWidth, buttonY + buttonHeight};
            drawButton(hdc, m_confirmBtnRect, L"同意", m_confirmHovered);
            
            // 退出按钮
            m_cancelBtnRect = {startX + buttonWidth + spacing, buttonY, startX + buttonWidth * 2 + spacing, buttonY + buttonHeight};
            drawButton(hdc, m_cancelBtnRect, L"退出", m_cancelHovered);
            
            if (hOldFont != nullptr) {
                SelectObject(hdc, hOldFont);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT AuthorWindow::handleLinkClick(HWND hwnd, int xPos, int yPos) {
    POINT pt = {xPos, yPos};
    for (const auto& link : m_links) {
        if (hitTestLinkUrl(hwnd, link, pt)) {
            // 同步调用打开链接：ShellExecuteW 本身很快，
            // 避免原 detach 线程在启动器退出后未执行完导致链接打不开
            openLink(link.url);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, WM_LBUTTONDOWN, 0, 0);
}

void AuthorWindow::openLink(const std::wstring& url) {
    ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

int AuthorWindow::calculateDisplayWidth(const std::wstring& str) {
    int width = 0;
    for (wchar_t c : str) {
        if (c >= 0x1100 && (c <= 0x115f || (c >= 0x2e80 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe6f) || (c >= 0xff00 && c <= 0xffef))) {
            width += 2;
        } else {
            width += 1;
        }
    }
    return width;
}

// 获取目标程序路径
std::wstring AuthorWindow::getTargetProcessPath() {
    // 从配置文件中获取目标程序路径
    ConfigManager& configManager = ConfigManager::getInstance();
    const HookConfig& config = configManager.getConfig();
    std::wstring targetPath = config.targetProcess;
    
    if (targetPath.empty()) {
        Logger::getInstance().log(L"AuthorWindow::getTargetProcessPath() - 配置文件中未设置目标程序路径");
        return L"";
    }
    
    // 相对路径基于模块目录解析并规范化为绝对路径
    targetPath = Utils::resolveTargetPath(targetPath);
    
    // 检查目标程序是否存在
    if (!Utils::fileExists(targetPath)) {
        Logger::getInstance().log(L"AuthorWindow::getTargetProcessPath() - 目标程序不存在: " + targetPath);
        return L"";
    }
    
    Logger::getInstance().log(L"AuthorWindow::getTargetProcessPath() - 目标程序路径: " + targetPath);
    return targetPath;
}

// 从ICO文件加载图标
HICON AuthorWindow::loadIconFromFile(const std::wstring& icoPath) {
    if (icoPath.empty()) {
        Logger::getInstance().log(L"AuthorWindow::loadIconFromFile() - ICO文件路径为空");
        return nullptr;
    }
    
    // 检查ICO文件是否存在
    if (GetFileAttributesW(icoPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Logger::getInstance().log(L"AuthorWindow::loadIconFromFile() - ICO文件不存在: " + icoPath);
        return nullptr;
    }
    
    // 从ICO文件加载图标
    HICON hIcon = (HICON)LoadImageW(
        NULL, 
        icoPath.c_str(), 
        IMAGE_ICON, 
        0, 0, 
        LR_LOADFROMFILE | LR_DEFAULTSIZE
    );
    
    if (hIcon == nullptr) {
        DWORD error = GetLastError();
        Logger::getInstance().log(L"AuthorWindow::loadIconFromFile() - 加载ICO文件失败，错误代码: " + std::to_wstring(error));
        return nullptr;
    }
    
    Logger::getInstance().log(L"AuthorWindow::loadIconFromFile() - 成功从ICO文件加载图标: " + icoPath);
    return hIcon;
}

// 扫描游戏目录中的所有ICO文件并选择最大的
HICON AuthorWindow::findAndLoadGameIcon(const std::wstring& exePath) {
    if (exePath.empty()) {
        return nullptr;
    }
    
    // 获取游戏目录
    std::wstring gameDir = exePath;
    size_t lastSlash = gameDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        gameDir = gameDir.substr(0, lastSlash + 1);
    } else {
        gameDir = L"";
    }
    
    Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 开始扫描游戏目录: " + gameDir);
    
    // 查找所有ICO文件
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((gameDir + L"*.ico").c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 未找到任何ICO文件");
        return nullptr;
    }
    
    std::vector<std::pair<std::wstring, ULONGLONG>> icoFiles;
    
    do {
        // 跳过目录
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        
        std::wstring fileName = findData.cFileName;
        std::wstring fullPath = gameDir + fileName;
        
        // 计算文件大小
        ULONGLONG fileSize = (static_cast<ULONGLONG>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
        
        icoFiles.push_back(std::make_pair(fullPath, fileSize));
        
        Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 找到ICO文件: " + fileName + L", 大小: " + std::to_wstring(fileSize));
        
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    if (icoFiles.empty()) {
        Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 未找到有效的ICO文件");
        return nullptr;
    }
    
    // 按文件大小排序（从大到小）
    std::sort(icoFiles.begin(), icoFiles.end(), 
        [](const std::pair<std::wstring, ULONGLONG>& a, const std::pair<std::wstring, ULONGLONG>& b) {
            return a.second > b.second;
        });
    
    // 尝试加载最大的ICO文件
    for (const auto& icoFile : icoFiles) {
        Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 尝试加载ICO文件: " + icoFile.first + L", 大小: " + std::to_wstring(icoFile.second));
        
        HICON hIcon = loadIconFromFile(icoFile.first);
        if (hIcon != nullptr) {
            Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 成功加载最大的ICO文件: " + icoFile.first);
            return hIcon;
        } else {
            Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 无法加载ICO文件: " + icoFile.first);
        }
    }
    
    Logger::getInstance().log(L"AuthorWindow::findAndLoadGameIcon() - 所有ICO文件都无法加载");
    return nullptr;
}

// 从可执行文件提取图标
HICON AuthorWindow::extractIconFromExecutable(const std::wstring& exePath) {
    if (exePath.empty()) {
        Logger::getInstance().log(L"AuthorWindow::extractIconFromExecutable() - 可执行文件路径为空");
        return nullptr;
    }
    
    // 尝试提取大图标
    HICON hIcon = nullptr;
    UINT result = ExtractIconExW(exePath.c_str(), 0, &hIcon, nullptr, 1);
    
    if (result == 0 || hIcon == nullptr) {
        Logger::getInstance().log(L"AuthorWindow::extractIconFromExecutable() - 提取图标失败，尝试其他方法");
        
        // 如果ExtractIconEx失败，尝试使用ExtractIcon
        hIcon = ExtractIconW(GetModuleHandle(NULL), exePath.c_str(), 0);
        
        if (hIcon == nullptr) {
            Logger::getInstance().log(L"AuthorWindow::extractIconFromExecutable() - 所有图标提取方法都失败");
            return nullptr;
        }
    }
    
    Logger::getInstance().log(L"AuthorWindow::extractIconFromExecutable() - 成功提取图标");
    return hIcon;
}

// 获取目标程序图标
HICON AuthorWindow::getTargetProcessIcon() {
    std::wstring processPath = getTargetProcessPath();
    
    if (processPath.empty()) {
        Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 无法获取进程路径");
        return nullptr;
    }
    
    Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 开始获取图标，目标程序路径: " + processPath);
    
    // 优先从游戏目录查找ICO文件
    HICON hIcon = findAndLoadGameIcon(processPath);
    if (hIcon != nullptr) {
        Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 已使用游戏目录中的ICO文件");
        return hIcon;
    }
    
    Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 未找到ICO文件，尝试从可执行文件提取图标");
    
    // 如果找不到ICO文件，则从目标程序提取图标
    hIcon = extractIconFromExecutable(processPath);
    
    if (hIcon == nullptr) {
        Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 无法提取目标程序图标");
        return nullptr;
    }
    
    Logger::getInstance().log(L"AuthorWindow::getTargetProcessIcon() - 成功从可执行文件提取图标");
    return hIcon;
}
