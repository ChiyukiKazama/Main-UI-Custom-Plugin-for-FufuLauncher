// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Engine.h"
#include "Support.h"
#include "NativeProfile.h"

namespace
{
    DWORD WINAPI PluginWorker(void*)
    {
        wchar_t executable[32768]{};
        const DWORD length = GetModuleFileNameW(nullptr,executable,32768);
        if (!length || length >= 32768 ||
            !Native::IsGameExecutable(std::filesystem::path(executable).filename().wstring())) return 0;
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
