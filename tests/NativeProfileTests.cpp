#include "../src/ElementPolicy.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

using namespace Native;
static_assert(static_cast<size_t>(Entry::Count)==40);
static_assert(static_cast<size_t>(PageField::Count)==29);
static_assert(IsGameExecutable(L"YuanShen.exe"));
static_assert(IsGameExecutable(L"GENSHINIMPACT.EXE"));
static_assert(!IsGameExecutable(L"pwsh.exe"));
static_assert(!IsGameExecutable(L"GenshinImpact.exe.bak"));
static_assert(!IsGameExecutable(L"C:\\games\\YuanShen.exe"));
static_assert(SelectProfile(L"YUANSHEN.EXE",0x6AD886FB,0x1B65F000)==&CN72);
static_assert(!SelectProfile(L"YuanShen.exe",0x6AD886FA,0x1B65F000));
static_assert(!SelectProfile(L"YuanShen.exe",0x6AD886FB,0x1B65E000));
static_assert(!SelectProfile(L"GenshinImpact.exe",0x6AD886FB,0x1B65F000));
static_assert(!SelectProfile(L"pwsh.exe",0x6AD886FB,0x1B65F000));
static_assert(SelectProfile(L"GENSHINIMPACT.EXE",0x6AD8992C,0x1B690000)==&OS72);
static_assert(!SelectProfile(L"YuanShen.exe",0x6AD8992C,0x1B690000));
static_assert(!SelectProfile(L"GenshinImpact.exe",0x6AD8992D,0x1B690000));
static_assert(!SelectProfile(L"GenshinImpact.exe",0x6AD8992C,0x1B691000));
static_assert(CN72.Get(Entry::StringNew).rva==0x19C21C90);
static_assert(OS72.Get(Entry::StringNew).rva==0x19C54AD0);
static_assert(OS72.Get(Entry::WillRender).rva==0x1A320650);
static_assert(CN72.Get(Entry::InteeSwitchKeyRefresh).rva==0xCF18ED0);
static_assert(CN72.layout.interactionKeys[0]==0x50 && CN72.layout.interactionKeys[1]==0x68);

namespace
{
    template<class T> T ReadAt(std::ifstream& file, uint64_t offset)
    {
        T value{};
        file.seekg(static_cast<std::streamoff>(offset));
        file.read(reinterpret_cast<char*>(&value),sizeof(value));
        return value;
    }

    void VerifyImage(const std::filesystem::path& path)
    {
        std::ifstream file;
        file.exceptions(std::ios::failbit|std::ios::badbit);
        file.open(path,std::ios::binary);
        const auto dos=ReadAt<IMAGE_DOS_HEADER>(file,0);
        if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<=0 || dos.e_lfanew>0x1000)throw std::runtime_error("Invalid DOS header");
        const auto nt=ReadAt<IMAGE_NT_HEADERS64>(file,static_cast<uint64_t>(dos.e_lfanew));
        if(nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
           nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)throw std::runtime_error("Expected x64 PE image");
        const auto* profile=SelectProfile(path.filename().wstring(),nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage);
        if(!profile)throw std::runtime_error("Executable name or PE fingerprint is unsupported");
        if(!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections>96)throw std::runtime_error("Invalid section count");
        std::vector<IMAGE_SECTION_HEADER> sections;
        const uint64_t sectionTable=static_cast<uint64_t>(dos.e_lfanew)+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
        for(size_t i=0;i<nt.FileHeader.NumberOfSections;++i)sections.push_back(ReadAt<IMAGE_SECTION_HEADER>(file,sectionTable+i*sizeof(IMAGE_SECTION_HEADER)));
        size_t checked=0;
        for(const auto& entry:profile->entries)
        {
            bool matchedSection=false;
            for(const auto& section:sections)
            {
                if(entry.rva<section.VirtualAddress || entry.rva-section.VirtualAddress>=section.SizeOfRawData)continue;
                const uint64_t offset=section.PointerToRawData+(entry.rva-section.VirtualAddress);
                const std::string_view expected(entry.bytes);
                for(size_t pos=0;pos<expected.size();pos+=3)
                {
                    unsigned byte=0;
                    const auto result=std::from_chars(expected.data()+pos,expected.data()+pos+2,byte,16);
                    if(result.ec!=std::errc{} || result.ptr!=expected.data()+pos+2 ||
                       entry.rva-section.VirtualAddress+pos/3>=section.SizeOfRawData ||
                       ReadAt<unsigned char>(file,offset+pos/3)!=byte)
                    {
                        std::fprintf(stderr,"Entry %zu at RVA 0x%llx does not match\n",checked,static_cast<unsigned long long>(entry.rva));
                        throw std::runtime_error("Entry byte verification failed");
                    }
                }
                matchedSection=true;break;
            }
            if(!matchedSection)throw std::runtime_error("Entry is outside file-backed sections");
            ++checked;
        }
        std::printf("%s: all %zu entry signatures match the supplied executable.\n",profile->name,checked);
    }
}

int wmain(int argc,wchar_t** argv)
{
    // Original CN bindings, in policy order, including repeated/shared fields.
    constexpr std::array<size_t,41> expected{{
        0x78,0x70,0x2E0,0x2D8,0x490,0x1D0,0x3B8,0x3B8,0x3B8,0x3B8,
        0x88,0x80,0x80,0x80,0x130,0x398,0x80,0x98,0x110,0x108,0x1A0,
        0xF0,0xE0,0xF8,0x1B0,0x188,0x190,0xB8,0x48,0x48,0x48,0x48,
        0x48,0x48,0xD8,0x80,0x1F8,0x218,0x1B8,0x1C0,0x370
    }};
    const auto& bindings=HUD::Bindings();
    if(bindings.size()!=expected.size()){std::fprintf(stderr,"Binding count changed: %zu\n",bindings.size());return 1;}
    std::array<bool,static_cast<size_t>(PageField::Count)> used{};
    for(size_t i=0;i<bindings.size();++i)
    {
        const auto field=bindings[i].field;
        if(CN72.layout.Offset(field)!=expected[i] || OS72.layout.Offset(field)!=expected[i]){std::fprintf(stderr,"HUD binding %zu changed\n",i);return 1;}
        used[static_cast<size_t>(field)]=true;
    }
    for(bool present:used)if(!present){std::fputs("Unbound page field\n",stderr);return 1;}
    std::puts("Native profiles: CN/OS host selection, fingerprints, 40 entries and HUD bindings passed.");
    if(argc>2){std::fputs("Usage: NativeProfileTests.exe [game-executable]\n",stderr);return 2;}
    try { if(argc==2)VerifyImage(argv[1]); }
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
