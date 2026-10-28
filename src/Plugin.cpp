// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Engine.h"
#include "Support.h"
#include "HookSetupGuard.h"
#include <fstream>
#include <iterator>

namespace
{
    bool ReloadSettings()
    {
        std::ifstream input(Support::PluginDirectory() / L"config.ini", std::ios::binary);
        if (!input) return false;
        std::string text((std::istreambuf_iterator<char>(input)), {});
        if (input.bad() || text.size() > 256 * 1024) return false;
        UI::Settings settings; std::string error;
        if (!UI::ParseSettings(text, settings, error)) return false;
        Engine::PublishSettings(std::make_shared<const UI::Settings>(std::move(settings)));
        return true;
    }
    void WatchConfigurationChanges()
    {
        const auto directory = Support::PluginDirectory();
        HANDLE folder = CreateFileW(directory.c_str(),FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,nullptr);
        if (folder == INVALID_HANDLE_VALUE) { ReloadSettings(); return; }
        HANDLE event = CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if (!event) { CloseHandle(folder); ReloadSettings(); return; }
        alignas(DWORD) unsigned char buffer[8192]{};
        bool changed = true;
        for (;;)
        {
            OVERLAPPED operation{}; operation.hEvent = event;
            ResetEvent(event);
            if (!ReadDirectoryChangesW(folder,buffer,sizeof(buffer),FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE,
                nullptr,&operation,nullptr)) break;
            if (changed)
            {
                // Rearm before reading: changes arriving during debounce/reload
                // remain queued rather than falling into an unwatched gap.
                for (int retry=0; retry<5; ++retry) { Sleep(100); if (ReloadSettings()) break; }
            }
            // An event wait consumes no recurring CPU or directory polling.
            if (WaitForSingleObject(event,INFINITE) != WAIT_OBJECT_0) break;
            DWORD transferred = 0;
            if (!GetOverlappedResult(folder,&operation,&transferred,FALSE)) break;
            changed = transferred == 0;
            for (DWORD offset = 0; offset + sizeof(FILE_NOTIFY_INFORMATION) <= transferred;)
            {
                const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer + offset);
                const std::wstring filename(info->FileName,info->FileNameLength / sizeof(wchar_t));
                if (_wcsicmp(filename.c_str(),L"config.ini") == 0) changed = true;
                if (!info->NextEntryOffset || info->NextEntryOffset > transferred - offset) break;
                offset += info->NextEntryOffset;
            }
        }
        CancelIoEx(folder,nullptr);
        CloseHandle(event); CloseHandle(folder);
    }
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
            ReloadSettings();
            {
                Support::HookSetupGuard guard;
                if (!guard || !Engine::Initialize()) return 0;
            }
            WatchConfigurationChanges();
        }
        catch (const std::exception&) { }
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
