#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "settings.h"
#include "logger.h"
#include "hook_manager.h"
#include "utils.h" 
#include "locale_emulator.h"
#include "author_window.h"

// DLL导出函数声明
extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved);

// 全局初始化函数 - 在DllMain之前执行
struct GlobalInitializer {
    GlobalInitializer() {
        // 初始化配置
        ConfigManager& configManager = ConfigManager::getInstance();
        std::wstring configFile = L"celica_hook.ini";
        
        // 先加载一次配置，主要是为了决定是否启用日志
        configManager.loadConfig(configFile);
        
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
            DisableThreadLibraryCalls(hModule);
            
            Logger::getInstance().log(L"CELICA_HOOK DLL已加载");
            Logger::getInstance().log(L"进程ID: " + std::to_wstring(GetCurrentProcessId()));
            
            // 执行转区操作
            if (LocaleEmulator::getInstance().performLocaleEmulation()) {
                Logger::getInstance().log(L"转区操作已执行，进程将重新启动");
                return TRUE;
            }
            
            // 显示作者署名弹窗
            AuthorWindow::getInstance().show();
            
            // 运行标准消息循环，它会阻塞直到弹窗关闭
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            Logger::getInstance().log(L"作者窗口已关闭，继续执行初始化...");

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
            
            HookManager::getInstance().shutdown();
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
    
    // 调用 loadConfig 并将其返回值用于判断，而不是调用不存在的 isConfigLoaded
    bool configLoaded = configManager.loadConfig(configFile);
    
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
    
    // 使用布尔变量 configLoaded 来判断
    if (configLoaded) {
        std::wcout << L"配置文件加载成功" << std::endl;
        
        // 这里的 config 变量是上面已经获取过的，可以直接使用
        std::wcout << L"文件重定向: " << (config.enableFileRedirect ? L"启用" : L"禁用") << std::endl;
        std::wcout << L"字体hook: " << (config.enableFontHook ? L"启用" : L"禁用") << std::endl;
        std::wcout << L"重定向文件夹: " << config.redirectFolder << std::endl;
        std::wcout << L"字体字符集: " << Utils::intToHexString(config.localeCharset) << std::endl;
        std::wcout << L"代码页: " << config.localeCodepage << std::endl;
        
        if (config.enableLogging) {
            Logger::getInstance().log(L"文件重定向: " + std::wstring(config.enableFileRedirect ? L"启用" : L"禁用"));
            Logger::getInstance().log(L"字体hook: " + std::wstring(config.enableFontHook ? L"启用" : L"禁用"));
            Logger::getInstance().log(L"重定向文件夹: " + config.redirectFolder);
            Logger::getInstance().log(L"字体字符集: " + Utils::intToHexString(config.localeCharset));
            Logger::getInstance().log(L"代码页: " + std::to_wstring(config.localeCodepage));
        }
    } else {
        std::wcout << L"配置文件加载失败，使用默认配置" << std::endl;
    }
    
    std::wcout << std::endl;
    std::wcout << L"按任意键退出..." << std::endl;
    std::cin.get();
    
    if (config.enableLogging) {
        Logger::getInstance().close();
    }
    
    return 0;
}
#endif