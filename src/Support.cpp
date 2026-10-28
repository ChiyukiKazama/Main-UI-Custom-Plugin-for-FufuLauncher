// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Support.h"
#include <cstdio>
#include <cstring>

namespace Support
{
    std::filesystem::path PluginDirectory()
    {
        HMODULE module = nullptr;
        wchar_t path[32768]{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&PluginDirectory), &module) || !GetModuleFileNameW(module, path, 32768)) return {};
        return std::filesystem::path(path).parent_path();
    }
    bool MatchesEntryBytes(const void* address, const char* bytes)
    {
        __try
        {
            const auto data = static_cast<const unsigned char*>(address);
            for (size_t i = 0; *bytes; ++i)
            {
                unsigned expected = 0;
                if (sscanf_s(bytes, "%2x", &expected) != 1 || data[i] != expected) return false;
                bytes += 2;
                if (*bytes == ' ') ++bytes;
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    static bool TryReadMinHookRelay(void* address, const char* bytes, void** target)
    {
        __try
        {
            const auto code = static_cast<unsigned char*>(address);
            // MinHook x64 replaces exactly five bytes and jumps to an absolute
            // relay. Do not treat an arbitrary changed function as compatible.
            if (code[0] != 0xE9 || !MatchesEntryBytes(code + 5, bytes + 15)) return false;
            const auto relay = code + 5 + *reinterpret_cast<const int32_t*>(code + 1);
            const unsigned char prefix[] = {0xFF, 0x25, 0, 0, 0, 0};
            if (std::memcmp(relay, prefix, sizeof(prefix)) != 0) return false;
            std::memcpy(target, relay + 6, sizeof(*target));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool IsCompatibleSetActiveEntry(void* address, const char* bytes)
    {
        if (MatchesEntryBytes(address, bytes)) return true;
        void* target = nullptr;
        if (!TryReadMinHookRelay(address, bytes, &target)) return false;
        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCWSTR>(target), &owner)) return false;
        wchar_t path[32768]{};
        if (!GetModuleFileNameW(owner, path, 32768)) return false;
        const auto moduleFileName = std::filesystem::path(path).filename().wstring();
        if (_wcsicmp(moduleFileName.c_str(), L"FufuLauncher.UnlockerIsland.dll") != 0 &&
            _wcsicmp(moduleFileName.c_str(), L"DetachedRuntime.dll") != 0) return false;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(target, &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
        // Calls still enter the broker; observers/filters retain normal behavior.
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            static_cast<LPCWSTR>(target), &owner) != FALSE;
    }
}
