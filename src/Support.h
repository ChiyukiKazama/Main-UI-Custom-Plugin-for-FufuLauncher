#pragma once
#include <Windows.h>
#include <filesystem>
namespace Support
{
    std::filesystem::path PluginDirectory();
    bool MatchesEntryBytes(const void* address, const char* bytes);
    bool IsCompatibleSetActiveEntry(void* address, const char* bytes);
}
