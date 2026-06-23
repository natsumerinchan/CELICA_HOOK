#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "settings.h"
#include "logger.h"
#include "hook_manager.h"
#include "utils.h" 
#include "locale_emulator.h"

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
            
            Logger::getInstance().log(L"开始执行初始化...");

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

