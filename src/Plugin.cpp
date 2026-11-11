// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Engine.h"
#include "Native.h"
#include "Support.h"

namespace
{
    DWORD WINAPI PluginWorker(void*)
    {
        wchar_t executable[32768]{};
        const DWORD length = GetModuleFileNameW(nullptr,executable,32768);
        if (!length || length >= 32768) return 0;
        const auto filename = std::filesystem::path(executable).filename();
        const Native::Profile* profile = nullptr;
        for (const auto* candidate : {&Native::CN72, &Native::OS72})
        {
            if (_wcsicmp(filename.c_str(),candidate->executableName) == 0)
            {
                profile = candidate;
                break;
            }
        }
        if (!profile) return 0;
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&PluginWorker),&pinned)) return 0;
        try
        {
            HUD::Run(*profile);
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
