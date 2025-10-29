#include "codepage_hook.h"
#include "settings.h"
#include "utils.h"
#include "detours.h"

// 初始化静态成员变量
decltype(MultiByteToWideChar)* CodepageHook::originalMultiByteToWideChar = nullptr;
decltype(WideCharToMultiByte)* CodepageHook::originalWideCharToMultiByte = nullptr;

CodepageHook& CodepageHook::getInstance() {
    static CodepageHook instance;
    return instance;
}

CodepageHook::CodepageHook() {
}

CodepageHook::~CodepageHook() {
    shutdown();
}

bool CodepageHook::initialize() {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableCodepageHook) {
        Logger::getInstance().log(L"代码页hook功能已禁用");
        return true;
    }
    
    Logger::getInstance().log(L"初始化代码页hook");
    
    // Hook MultiByteToWideChar
    originalMultiByteToWideChar = MultiByteToWideChar;
    if (DetourAttach(&(PVOID&)originalMultiByteToWideChar, HookedMultiByteToWideChar) != NO_ERROR) {
        Logger::getInstance().log(L"Hook MultiByteToWideChar 失败");
        return false;
    }
    
    // Hook WideCharToMultiByte
    originalWideCharToMultiByte = WideCharToMultiByte;
    if (DetourAttach(&(PVOID&)originalWideCharToMultiByte, HookedWideCharToMultiByte) != NO_ERROR) {
        Logger::getInstance().log(L"Hook WideCharToMultiByte 失败");
        DetourDetach(&(PVOID&)originalMultiByteToWideChar, HookedMultiByteToWideChar);
        return false;
    }
    
    Logger::getInstance().log(L"代码页hook初始化完成");
    return true;
}

void CodepageHook::shutdown() {
    if (originalMultiByteToWideChar) {
        DetourDetach(&(PVOID&)originalMultiByteToWideChar, HookedMultiByteToWideChar);
        originalMultiByteToWideChar = nullptr;
    }
    
    if (originalWideCharToMultiByte) {
        DetourDetach(&(PVOID&)originalWideCharToMultiByte, HookedWideCharToMultiByte);
        originalWideCharToMultiByte = nullptr;
    }
    
    Logger::getInstance().log(L"代码页hook已卸载");
}

int WINAPI CodepageHook::HookedMultiByteToWideChar(
    UINT CodePage,
    DWORD dwFlags,
    LPCSTR lpMultiByteStr,
    int cbMultiByte,
    LPWSTR lpWideCharStr,
    int cchWideChar
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableCodepageHook) {
        return originalMultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte,
                                         lpWideCharStr, cchWideChar);
    }
    
    // 检查是否是字体相关的调用
    // 如果是字体相关的调用，不进行代码页转换，避免字体名称乱码
    if (isFontRelatedCall()) {
        return originalMultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte,
                                         lpWideCharStr, cchWideChar);
    }
    
    UINT targetCodepage = getTargetCodepage(CodePage);
    
    if (targetCodepage != CodePage) {
        Logger::getInstance().log(L"MultiByteToWideChar 代码页转换: " + 
                                std::to_wstring(CodePage) + L" -> " + 
                                std::to_wstring(targetCodepage));
    }
    
    return originalMultiByteToWideChar(targetCodepage, dwFlags, lpMultiByteStr, cbMultiByte,
                                     lpWideCharStr, cchWideChar);
}

int WINAPI CodepageHook::HookedWideCharToMultiByte(
    UINT CodePage,
    DWORD dwFlags,
    LPCWSTR lpWideCharStr,
    int cchWideChar,
    LPSTR lpMultiByteStr,
    int cbMultiByte,
    LPCSTR lpDefaultChar,
    LPBOOL lpUsedDefaultChar
) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    if (!config.enableCodepageHook) {
        return originalWideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar,
                                         lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);
    }
    
    UINT targetCodepage = getTargetCodepage(CodePage);
    
    if (targetCodepage != CodePage) {
        Logger::getInstance().log(L"WideCharToMultiByte 代码页转换: " + 
                                std::to_wstring(CodePage) + L" -> " + 
                                std::to_wstring(targetCodepage));
    }
    
    return originalWideCharToMultiByte(targetCodepage, dwFlags, lpWideCharStr, cchWideChar,
                                     lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);
}

// 检查是否是字体相关的调用
bool CodepageHook::isFontRelatedCall() {
    // 简单实现：检查调用堆栈中是否有字体相关的函数
    // 这里使用线程局部存储来标记字体hook的调用
    
    // 临时解决方案：对于Shift-JIS编码的字体名称，不进行代码页转换
    // 这样可以避免字体名称乱码
    return true; // 暂时让所有调用都绕过代码页hook，测试效果
}

UINT CodepageHook::getTargetCodepage(UINT originalCodepage) {
    const HookConfig& config = ConfigManager::getInstance().getConfig();
    
    // 如果原始代码页与配置的原代码页匹配，则转换为目标代码页
    if (originalCodepage == config.sourceCodepage) {
        return config.targetCodepage;
    }
    
    // 特殊处理：如果原始代码页是CP_ACP (0)，则根据系统区域设置判断
    if (originalCodepage == CP_ACP) {
        // 这里可以根据需要添加更多逻辑
        // 目前简单返回目标代码页
        return config.targetCodepage;
    }
    
    // 其他情况保持原样
    return originalCodepage;
}
