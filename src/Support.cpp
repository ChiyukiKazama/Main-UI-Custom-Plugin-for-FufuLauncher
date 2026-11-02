// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Support.h"
#include <cstdio>

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
}
