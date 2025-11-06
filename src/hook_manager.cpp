#include "hook_manager.h"
#include "file_hook.h"
#include "font_hook.h"
#include "window_title_hook.h"
#include "detours.h"

HookManager& HookManager::getInstance() {
    static HookManager instance;
    return instance;
}

HookManager::HookManager() {
}

HookManager::~HookManager() {
    shutdown();
}

bool HookManager::initialize() {
    Logger::getInstance().log(L"开始初始化hook管理器");
    
    // 开始事务
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    bool success = true;
    
    // 安装文件重定向hook
    if (!installFileRedirectHooks()) {
        success = false;
    }
    
    // 安装字体hook
    if (!installFontHooks()) {
        success = false;
    }
    
    // 安装窗口标题hook
    if (!installWindowTitleHooks()) {
        success = false;
    }
    
    // 提交事务
    if (success) {
        LONG result = DetourTransactionCommit();
        if (result != NO_ERROR) {
            Logger::getInstance().log(L"Hook事务提交失败: " + std::to_wstring(result));
            success = false;
        }
    } else {
        DetourTransactionAbort();
        Logger::getInstance().log(L"Hook事务已中止");
    }
    
    if (success) {
        Logger::getInstance().log(L"hook管理器初始化完成");
    } else {
        Logger::getInstance().log(L"hook管理器初始化失败");
    }
    
    return success;
}

void HookManager::shutdown() {
    Logger::getInstance().log(L"开始卸载hook管理器");
    
    // 开始事务
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    // 卸载所有hook
    FileRedirectHook::getInstance().shutdown();
    FontHook::getInstance().shutdown();
    WindowTitleHook::getInstance().shutdown();
    
    // 提交事务
    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        Logger::getInstance().log(L"Hook卸载事务提交失败: " + std::to_wstring(result));
    } else {
        Logger::getInstance().log(L"hook管理器卸载完成");
    }
}

bool HookManager::installFileRedirectHooks() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFileRedirect && !config.enableFileSpoofing) {
        Logger::getInstance().log(L"文件重定向和文件欺骗hook已禁用");
        return true;
    }
    
    bool success = FileRedirectHook::getInstance().initialize();
    logHookStatus("文件hook", success);
    return success;
}

bool HookManager::installFontHooks() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableFontHook) {
        Logger::getInstance().log(L"字体hook已禁用");
        return true;
    }
    
    bool success = FontHook::getInstance().initialize();
    logHookStatus("字体hook", success);
    return success;
}

bool HookManager::installWindowTitleHooks() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableWindowTitleHook) {
        Logger::getInstance().log(L"窗口标题hook已禁用");
        return true;
    }
    
    bool success = WindowTitleHook::getInstance().initialize();
    logHookStatus("窗口标题hook", success);
    return success;
}

void HookManager::logHookStatus(const char* hookName, bool success) {
    if (success) {
        Logger::getInstance().log(std::string(hookName) + " 安装成功");
    } else {
        Logger::getInstance().log(std::string(hookName) + " 安装失败");
    }
}
