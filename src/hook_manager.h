#pragma once
#ifndef HOOK_MANAGER_H
#define HOOK_MANAGER_H

#include <windows.h>
#include "detours.h"
#include "settings.h"
#include "logger.h"

class HookManager {
public:
    static HookManager& getInstance();
    
    bool initialize();
    void shutdown();
    
private:
    HookManager();
    ~HookManager();
    
    bool installFileRedirectHooks();
    bool installFontHooks();
    bool installCodepageHooks();
    bool installWindowTitleHooks();
    
    void logHookStatus(const char* hookName, bool success);
};

#endif // HOOK_MANAGER_H
