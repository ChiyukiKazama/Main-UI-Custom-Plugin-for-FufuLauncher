// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Engine.h"
#include "Support.h"

namespace
{
    DWORD WINAPI PluginWorker(void*)
    {
        wchar_t executable[32768]{};
        if (!GetModuleFileNameW(nullptr,executable,32768) ||
            _wcsicmp(std::filesystem::path(executable).filename().c_str(),L"YuanShen.exe") != 0) return 0;
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&PluginWorker),&pinned)) return 0;
        try
        {
            HUD::Run();
        }
        catch (...) { OutputDebugStringW(L"MainUICustomizer startup failed.\n"); }
        return 0;
    }
}
BOOL APIENTRY DllMain(HMODULE,DWORD reason,LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // Static CRT: keep thread attach/detach notifications enabled.
        HANDLE thread = CreateThread(nullptr,0,&PluginWorker,nullptr,0,nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
