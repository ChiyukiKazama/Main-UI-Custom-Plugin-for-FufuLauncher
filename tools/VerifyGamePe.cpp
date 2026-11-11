// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "../src/Native.h"
#include <windows.h>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
    template<class T> T ReadAt(std::ifstream& file, uint64_t offset)
    {
        T value{};
        file.seekg(static_cast<std::streamoff>(offset));
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
    }

    bool VerifyEntry(std::ifstream& file, const std::vector<IMAGE_SECTION_HEADER>& sections,
        const Native::Method& entry)
    {
        if (!entry.rva || !entry.bytes || !entry.bytes[0]) return false;
        const std::string_view expected(entry.bytes);
        for (const auto& section : sections)
        {
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE) || entry.rva < section.VirtualAddress)
                continue;
            const auto relative = entry.rva - section.VirtualAddress;
            if (relative >= section.SizeOfRawData) continue;
            const auto offset = static_cast<uint64_t>(section.PointerToRawData) + relative;
            file.seekg(static_cast<std::streamoff>(offset));
            for (std::size_t pos = 0; pos < expected.size(); pos += 3)
            {
                if (pos + 2 > expected.size() || relative + pos / 3 >= section.SizeOfRawData)
                    return false;
                unsigned byte = 0;
                const auto first = expected.data() + pos;
                const auto result = std::from_chars(first, first + 2, byte, 16);
                if (result.ec != std::errc{} || result.ptr != first + 2 ||
                    (pos + 2 < expected.size() && expected[pos + 2] != ' ') ||
                    file.get() != static_cast<int>(byte))
                    return false;
            }
            return true;
        }
        return false;
    }

    void VerifyImage(const std::filesystem::path& path)
    {
        std::ifstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path, std::ios::binary);
        const auto dos = ReadAt<IMAGE_DOS_HEADER>(file, 0);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE ||
            dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 0x1000)
            throw std::runtime_error("Invalid DOS header");

        const auto ntOffset = static_cast<uint64_t>(dos.e_lfanew);
        const auto nt = ReadAt<IMAGE_NT_HEADERS64>(file, ntOffset);
        if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            throw std::runtime_error("Expected an x64 PE32+ image");

        const auto* profile = Native::FindProfile(path.filename().native());
        if (!profile) throw std::runtime_error("Unsupported executable name");
        if (!profile->MatchesFingerprint(nt.FileHeader.TimeDateStamp, nt.OptionalHeader.SizeOfImage))
            throw std::runtime_error("PE fingerprint does not match the executable name");

        const auto sectionCount = nt.FileHeader.NumberOfSections;
        if (!sectionCount || sectionCount > 96) throw std::runtime_error("Invalid section count");
        const auto sectionTable = ntOffset + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
            nt.FileHeader.SizeOfOptionalHeader;
        std::vector<IMAGE_SECTION_HEADER> sections;
        sections.reserve(sectionCount);
        for (unsigned i = 0; i < sectionCount; ++i)
            sections.push_back(ReadAt<IMAGE_SECTION_HEADER>(file, sectionTable + i * sizeof(IMAGE_SECTION_HEADER)));

        for (const auto& entry : profile->entries)
        {
            if (VerifyEntry(file, sections, entry)) continue;
            std::fprintf(stderr, "Entry at RVA 0x%llX failed verification.\n",
                static_cast<unsigned long long>(entry.rva));
            throw std::runtime_error("Entry bytes are missing or do not match");
        }
        std::wprintf(L"%ls: PE fingerprint and all %zu entry signatures match.\n",
            profile->executableName.data(), profile->entries.size());
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::fputs("Usage: VerifyGamePe.exe <path-to-game-executable>\n", stderr);
        return 2;
    }
    try
    {
        VerifyImage(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "PE verification failed: %s\n", error.what());
        return 1;
    }
}
