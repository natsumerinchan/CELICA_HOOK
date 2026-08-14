#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "settings.h"
#include "logger.h"
#include "hook_manager.h"
#include "utils.h" 
#include "locale_emulator_plus.h"
#include "detours.h"

// DLL导出函数声明
extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved);

// 全局初始化函数 - 在DllMain之前执行
struct GlobalInitializer {
    GlobalInitializer() {
        // 初始化配置
        ConfigManager& configManager = ConfigManager::getInstance();
        
        // 配置文件与游戏 EXE 同目录（DLL 是注入到游戏进程的，CWD 可能与游戏目录不一致，
        // 因此基于模块目录构建绝对路径，避免依赖当前工作目录）
        std::wstring configFile = Utils::combinePaths(Utils::getModuleDirectory(), L"celica_hook.ini");
        
        // 先加载一次配置，主要是为了决定是否启用日志
        configManager.loadConfig(configFile);
        
        const HookConfig& config = configManager.getConfig();
        
        // 初始化日志系统
        // 相对日志路径基于模块目录解析，避免日志落在与游戏目录不一致的 CWD 下
        if (config.enableLogging) {
            std::wstring logFile = config.logFile;
            if (!Utils::isAbsolutePath(logFile)) {
                logFile = Utils::combinePaths(Utils::getModuleDirectory(), logFile);
            }
            Logger::getInstance().initialize(logFile);
        }
        
        // 初始化转区功能
        LocaleEmulatorPlus::getInstance().initialize();
    }
};

// 全局对象，在程序启动时自动构造
GlobalInitializer g_initializer;

// DLL入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    (void)lpReserved;  // 保留参数以匹配 DllMain 签名，消除 C4100
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            
            // Detours 在 x86 下通过 rundll32 辅助进程完成注入修补，
            // 该辅助进程也会加载本 DLL，此时直接返回，避免执行完整初始化
            if (DetourIsHelperProcess()) {
                return TRUE;
            }
            
            Logger::getInstance().log(L"CELICA_HOOK DLL已加载");
            Logger::getInstance().log(L"进程ID: " + std::to_wstring(GetCurrentProcessId()));
            
            // 执行转区操作
            if (LocaleEmulatorPlus::getInstance().performLocaleEmulation()) {
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
