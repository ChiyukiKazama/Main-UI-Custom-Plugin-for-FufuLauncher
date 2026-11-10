#include "../src/ElementPolicy.h"
#include <array>
#include <cstdio>

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
static_assert(CN72.Get(Entry::StringNew).rva==0x19C21C90);
static_assert(CN72.Get(Entry::InteeSwitchKeyRefresh).rva==0xCF18ED0);
static_assert(CN72.layout.interactionKeys[0]==0x50 && CN72.layout.interactionKeys[1]==0x68);

int main()
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
        if(CN72.layout.Offset(field)!=expected[i]){std::fprintf(stderr,"CN binding %zu changed\n",i);return 1;}
        used[static_cast<size_t>(field)]=true;
    }
    for(bool present:used)if(!present){std::fputs("Unbound page field\n",stderr);return 1;}
    std::puts("Native profiles: host selection, fingerprints, 40 entries and CN binding parity passed.");
}
