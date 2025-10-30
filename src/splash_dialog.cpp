#include "splash_dialog.h"
#include "settings.h"
#include "logger.h"

bool SplashDialog::showSplashDialog() {
    // 获取配置管理器实例
    ConfigManager& configManager = ConfigManager::getInstance();
    const HookConfig& config = configManager.getConfig();
    
    // 构建弹窗标题
    std::wstring dialogTitle = L"CELICA_HOOK";
    if (!config.newWindowTitle.empty()) {
        dialogTitle = config.newWindowTitle;
    }
    
    // 构建消息内容
    std::wstring message = L"作者:\n";
    
    // 添加所有作者ID
    for (int i = 0; i < AuthorInfo::AUTHOR_IDS_COUNT; ++i) {
        message += L"  • " + std::wstring(AuthorInfo::AUTHOR_IDS[i]) + L"\n";
    }
    
    message += L"\n主页:\n";
    
    // 添加所有主页链接
    for (int i = 0; i < AuthorInfo::AUTHOR_HOMEPAGES_COUNT; ++i) {
        message += L"  • " + std::wstring(AuthorInfo::AUTHOR_HOMEPAGES[i]) + L"\n";
    }
    
    message += L"\n声明:\n" + std::wstring(AuthorInfo::ADDITIONAL_NOTES);
    
    // 显示消息框 - 使用MessageBoxW处理宽字符
    int result = MessageBoxW(
        NULL,
        message.c_str(),
        dialogTitle.c_str(),
        MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL
    );
    
    // 记录弹窗显示
    Logger::getInstance().log(L"显示作者信息弹窗，标题: " + dialogTitle);
    
    return (result == IDOK);
}
