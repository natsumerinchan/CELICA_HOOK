#include <windows.h>
#include <iostream>
#include "settings.h"
#include "logger.h"
#include "hook_manager.h"
#include "utils.h"
#include "splash_dialog.h"
#include "locale_emulator.h"

// DLL导出函数声明
extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved);

// 全局初始化函数 - 在DllMain之前执行
struct GlobalInitializer {
    GlobalInitializer() {
        // 初始化配置
        ConfigManager& configManager = ConfigManager::getInstance();
        std::wstring configFile = L"celica_hook.ini";
        
        if (!configManager.loadConfig(configFile)) {
            // 如果配置文件不存在，使用默认配置
            // 此时日志系统可能还未初始化，所以不能记录日志
        }
        
        const HookConfig& config = configManager.getConfig();
        
        // 初始化日志系统
        if (config.enableLogging) {
            Logger::getInstance().initialize(config.logFile);
        }
        
        // 初始化转区功能
        LocaleEmulator::getInstance().initialize();
    }
};

// 全局对象，在程序启动时自动构造
GlobalInitializer g_initializer;

// DLL入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            // 禁用线程通知
            DisableThreadLibraryCalls(hModule);
            
            Logger::getInstance().log(L"CELICA_HOOK DLL已加载");
            Logger::getInstance().log(L"进程ID: " + std::to_wstring(GetCurrentProcessId()));
            
            // 执行转区操作（如果需要）
            if (LocaleEmulator::getInstance().performLocaleEmulation()) {
                // 如果转区成功，进程会被重新启动，这里不会继续执行
                Logger::getInstance().log(L"转区操作已执行，进程将重新启动");
                return TRUE;
            }
            
            // 显示作者信息弹窗 - 用户必须确认后才能继续
            if (!SplashDialog::showSplashDialog()) {
                Logger::getInstance().log(L"用户取消弹窗，DLL加载失败");
                return FALSE;
            }
            
            // 初始化hook管理器
            if (!HookManager::getInstance().initialize()) {
                Logger::getInstance().log(L"Hook管理器初始化失败");
                return FALSE;
            }
            
            Logger::getInstance().log(L"CELICA_HOOK 初始化完成");
            break;
        }
        
        case DLL_PROCESS_DETACH: {
            Logger::getInstance().log(L"CELICA_HOOK DLL正在卸载");
            
            // 关闭hook管理器
            HookManager::getInstance().shutdown();
            
            // 关闭日志系统
            Logger::getInstance().close();
            break;
        }
        
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    
    return TRUE;
}

// 控制台测试程序
#ifdef _CONSOLE
int main() {
    // 初始化配置
    ConfigManager& configManager = ConfigManager::getInstance();
    std::wstring configFile = L"celica_hook.ini";
    
    if (!configManager.loadConfig(configFile)) {
        std::wcout << L"配置文件加载失败，使用默认配置" << std::endl;
    }
    
    const HookConfig& config = configManager.getConfig();
    
    // 初始化日志系统
    if (config.enableLogging) {
        if (!Logger::getInstance().initialize(config.logFile)) {
            std::wcout << L"日志系统初始化失败" << std::endl;
        } else {
            Logger::getInstance().log(L"控制台测试程序启动");
        }
    }
    
    std::wcout << L"CELICA_HOOK 控制台测试程序" << std::endl;
    std::wcout << L"这是一个DLL注入工具，请使用注入器将CELICA_HOOK.dll注入到目标进程中" << std::endl;
    std::wcout << std::endl;
    
    // 测试配置加载
    if (configManager.loadConfig(L"celica_hook.ini")) {
        std::wcout << L"配置文件加载成功" << std::endl;
        
        const HookConfig& config = configManager.getConfig();
        std::wcout << L"文件重定向: " << (config.enableFileRedirect ? L"启用" : L"禁用") << std::endl;
        std::wcout << L"字体hook: " << (config.enableFontHook ? L"启用" : L"禁用") << std::endl;
        std::wcout << L"代码页hook: " << (config.enableCodepageHook ? L"启用" : L"禁用") << std::endl;
        std::wcout << L"重定向文件夹: " << config.redirectFolder << std::endl;
        std::wcout << L"字体字符集: " << Utils::intToHexString(config.charset) << std::endl;
        std::wcout << L"原代码页: " << config.sourceCodepage << std::endl;
        std::wcout << L"目标代码页: " << config.targetCodepage << std::endl;
        
        // 记录配置信息到日志
        if (config.enableLogging) {
            Logger::getInstance().log(L"文件重定向: " + std::wstring(config.enableFileRedirect ? L"启用" : L"禁用"));
            Logger::getInstance().log(L"字体hook: " + std::wstring(config.enableFontHook ? L"启用" : L"禁用"));
            Logger::getInstance().log(L"代码页hook: " + std::wstring(config.enableCodepageHook ? L"启用" : L"禁用"));
            Logger::getInstance().log(L"重定向文件夹: " + config.redirectFolder);
            Logger::getInstance().log(L"字体字符集: " + Utils::intToHexString(config.charset));
            Logger::getInstance().log(L"原代码页: " + std::to_wstring(config.sourceCodepage));
            Logger::getInstance().log(L"目标代码页: " + std::to_wstring(config.targetCodepage));
        }
    } else {
        std::wcout << L"配置文件加载失败，使用默认配置" << std::endl;
    }
    
    std::wcout << std::endl;
    std::wcout << L"按任意键退出..." << std::endl;
    std::cin.get();
    
    // 关闭日志系统
    if (config.enableLogging) {
        Logger::getInstance().close();
    }
    
    return 0;
}
#endif
