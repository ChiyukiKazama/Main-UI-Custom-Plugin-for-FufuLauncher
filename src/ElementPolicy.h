#pragma once
#include "Settings.h"
#include "NativeProfile.h"
#include <cstdint>
#include <string>
#include <vector>
namespace HUD
{
    enum Edge : unsigned { Left=1,Right=2,Top=4,Bottom=8 };
    struct Binding { UI::Preset preset; Native::PageField field; bool gameObject; const char* child=nullptr; const char* type=nullptr; };
    struct Region { std::string path; UI::Preset preset; unsigned edges=0; int object=0,transform=0; void* token=nullptr; int source=0; void* sourceToken=nullptr; };
    struct Match { uint32_t presets=0; unsigned edges=0; std::string unit; bool topRight=false; };
    const std::vector<Binding>& Bindings();
    const std::vector<Region>& FixedRegions();
    unsigned Edges(UI::Preset preset);
    bool Within(const std::string& path,const std::string& parent);
    bool Forbidden(const std::string& path);
    bool ValidRegion(const std::string& path,UI::Preset preset);
    Match Classify(const std::string& path,const std::vector<Region>& regions);
}
