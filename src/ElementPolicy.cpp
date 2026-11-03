#include "ElementPolicy.h"
#include <algorithm>
#include <sstream>
namespace HUD
{
    bool Within(const std::string& path,const std::string& parent)
    {return path==parent || (path.size()>parent.size() && path.compare(0,parent.size(),parent)==0 && path[parent.size()]=='/');}
    bool Forbidden(const std::string& path)
    {
        if(!Within(path,"GrpMainPage"))return true;
        std::istringstream stream(path);std::string part;
        while(std::getline(stream,part,'/'))
            if(part=="InputController" || part=="EventSystem" || part=="BossHPBar" || part=="CombatBars" || part=="TxtUID" || part=="UID" || part=="MonsterDebugInput")return true;
        return false;
    }
    unsigned Edges(UI::Preset preset)
    {
        switch(preset)
        {
        case UI::MiniMap:case UI::TopLeftButtons:case UI::Paimon:case UI::Coop:case UI::QuestButton:case UI::SnezhnayaIcon:case UI::ElementalSight:return Left|Top;
        case UI::TopRightButtons:case UI::Activities:case UI::Bag:case UI::Character:case UI::Gacha:case UI::BattlePass:case UI::Handbook:case UI::MiliastraEntrance:case UI::Ping:return Right|Top;
        case UI::Team:return Right;
        case UI::SkillControls:return Right|Bottom;
        case UI::ChatButton:case UI::ChatPanel:return Left|Bottom;
        case UI::PlayerHealthStatus:return Bottom;
        case UI::QuestTracking:return Left;
        default:return 0;
        }
    }
    const std::vector<Binding>& Bindings()
    {
        static const std::vector<Binding> values={
            {UI::MiniMap,0x78,false},{UI::Team,0x70,false},{UI::QuestTracking,0x2E0,false},{UI::SkillControls,0x2D8,false},
            {UI::ChatButton,0x3B8,true,"Content/Title/Key_PC/ChatIcon"},{UI::ChatButton,0x3B8,true,"Content/ActionButton_PS/Key_PS4"},
            {UI::ChatPanel,0x3B8,true,"Content/ScrollView"},{UI::ChatPanel,0x3B8,true,"Content/ImgBg"},
            {UI::TopLeftButtons,0x88,true},{UI::TopLeftButtons,0x80,true,"GrpFactor"},{UI::TopLeftButtons,0x80,true,"GrpFactorContainer"},
            {UI::TopLeftButtons,0x80,true,"BtnToggle"},{UI::TopLeftButtons,0x130,false},{UI::TopLeftButtons,0x398,false},{UI::TopLeftButtons,0x80,true,"BtnClock"},
            {UI::TopRightButtons,0x98,true},{UI::Paimon,0x110,false},{UI::Activities,0x108,false},{UI::Activities,0x1A0,false},
            {UI::Bag,0xF0,false},{UI::Character,0xE0,false},{UI::Gacha,0xF8,false},{UI::BattlePass,0x1B0,false},
            {UI::Handbook,0x188,false},{UI::Handbook,0x190,false},{UI::Coop,0xB8,false},
            {UI::PlayerHealthStatus,0x48,false,"HPBarContainer"},{UI::PlayerHealthStatus,0x48,false,"HPBarCustomLevelContainer"},
            {UI::PlayerHealthStatus,0x48,false,"Layout"},{UI::PlayerHealthStatus,0x48,false,"SPBar"},{UI::PlayerHealthStatus,0x48,false,"SPBarV2"},{UI::PlayerHealthStatus,0x48,false,"ClimateBar"},
            {UI::QuestButton,0xD8,false},{UI::SnezhnayaIcon,0x80,true,"GrpFactor/GrpSituationButton/SituationButton_SnezhnayaBook(Clone)"},
            {UI::Interaction,0x1F8,false},{UI::Ping,0x218,false},{UI::MiliastraEntrance,0x1B8,false},{UI::MiliastraEntrance,0x1C0,false},{UI::ElementalSight,0x370,false}
        };return values;
    }
    const std::vector<Region>& FixedRegions()
    {
        static const std::vector<Region> values={
            {"GrpMainPage/MapInfo/GrpMiniMap",UI::MiniMap,Left|Top},
            {"GrpMainPage/MapInfo/BtnPlayerProfile",UI::Paimon,Left|Top},
            {"GrpMainPage/MapInfo/GrpButtons",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/GrpFactor",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/GrpFactorContainer",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/BtnClock",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/BtnExitDungeon",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/BtnFeedback",UI::TopLeftButtons,Left|Top},
            {"GrpMainPage/MapInfo/BtnToggleQuest",UI::QuestButton,Left|Top},
            {"GrpMainPage/MapInfo/GrpButtons/BtnTogglePlayerList",UI::Coop,Left|Top},
            {"GrpMainPage/MapInfo/GrpButtons/BtnElemView",UI::ElementalSight,Left|Top},
            {"GrpMainPage/MapInfo/GrpFactor/GrpSituationButton/SituationButton_SnezhnayaBook(Clone)",UI::SnezhnayaIcon,Left|Top},
            {"GrpMainPage/GrpMainBtn/GrpMainToggle/GrpTopPanel",UI::TopRightButtons,Right|Top},
            {"GrpMainPage/TeamBtnContainer",UI::Team,Right},
            {"GrpMainPage/ActionPanelContainer",UI::SkillControls,Right|Bottom},
            {"GrpMainPage/ChatArea/Chat/Content/Title/Key_PC/ChatIcon",UI::ChatButton,Left|Bottom},
            {"GrpMainPage/ChatArea/Chat/Content/ActionButton_PS/Key_PS4",UI::ChatButton,Left|Bottom},
            {"GrpMainPage/ChatArea/Chat/Content/ScrollView",UI::ChatPanel,Left|Bottom},
            {"GrpMainPage/ChatArea/Chat/Content/ImgBg",UI::ChatPanel,Left|Bottom},
            {"GrpMainPage/GameInfo/HPBarContainer",UI::PlayerHealthStatus,Bottom},
            {"GrpMainPage/GameInfo/HPBarCustomLevelContainer",UI::PlayerHealthStatus,Bottom},
            {"GrpMainPage/GameInfo/Layout",UI::PlayerHealthStatus,Bottom},
            {"GrpMainPage/GameInfo/SPBar",UI::PlayerHealthStatus,Bottom},{"GrpMainPage/GameInfo/SPBarV2",UI::PlayerHealthStatus,Bottom},
            {"GrpMainPage/GameInfo/ClimateBar",UI::PlayerHealthStatus,Bottom},
            {"GrpMainPage/InteePanel",UI::Interaction,0},{"GrpMainPage/NetworkLatency",UI::Ping,Right|Top}
        };return values;
    }
    bool ValidRegion(const std::string& path,UI::Preset preset)
    {
        if(Forbidden(path) || path=="GrpMainPage" || preset==UI::AllMainUi)return false;
        if(preset==UI::QuestTracking)return Within(path,"GrpMainPage") && path.find("Quest")!=path.npos;
        for(const auto& region:FixedRegions())if(region.preset==preset && Within(path,region.path))return true;
        // Other top-right entries are typed serialized fields, but their region
        // must still be inside the reviewed top-panel namespace.
        if(preset==UI::Activities || preset==UI::Bag || preset==UI::Character || preset==UI::Gacha || preset==UI::BattlePass || preset==UI::Handbook || preset==UI::MiliastraEntrance)
            return Within(path,"GrpMainPage/GrpMainBtn/GrpMainToggle/GrpTopPanel");
        if(preset==UI::TopLeftButtons)return Within(path,"GrpMainPage/MapInfo") && !Within(path,"GrpMainPage/MapInfo/GrpMiniMap");
        return false;
    }
    Match Classify(const std::string& path,const std::vector<Region>& regions)
    {
        Match result;if(Forbidden(path))return result;
        size_t longest=0;
        for(const auto& region:regions)if(Within(path,region.path))
        {
            result.presets|=uint32_t{1}<<region.preset;result.edges|=region.edges;
            if(region.path.size()>longest){longest=region.path.size();result.unit=region.path;}
        }
        if(!result.presets)return result;
        std::vector<std::string> parts;std::istringstream stream(path);std::string part;
        while(std::getline(stream,part,'/'))parts.push_back(part);
        auto prefix=[&](size_t count){std::string text;for(size_t i=0;i<std::min(count,parts.size());++i){if(i)text+='/';text+=parts[i];}return text;};
        for(size_t i=0;i<parts.size();++i)
        {
            if(parts[i]=="GrpTopPanel" && i+1<parts.size()){result.unit=prefix(i+2);result.topRight=true;break;}
            if(parts[i]=="TeamBtnContainer" && i+2<parts.size() && parts[i+1]=="Content")
            {if(!parts[i+2].empty() && parts[i+2].find_first_not_of("0123456789")==part.npos)result.unit=prefix(i+3);break;}
            if(parts[i]=="ActionBtnPanel" && i+1<parts.size())
            {result.unit=prefix(i+(parts[i+1]=="GrpSkill" && i+2<parts.size()?3:2));result.particle=true;break;}
            if((parts[i]=="GrpButtons" || parts[i]=="GrpFactor" || parts[i]=="GrpFactorContainer") && i+1<parts.size() && parts[i+1]!="GrpSituationButton")
            {result.unit=prefix(i+2);break;}
            if(parts[i]=="MapInfo" && i+1<parts.size() && parts[i+1]!="GrpButtons" && parts[i+1]!="GrpFactor" && parts[i+1]!="GrpFactorContainer")
            {result.unit=prefix(i+2);break;}
            if(parts[i]=="GrpSituationButton" && i+1<parts.size()){result.unit=prefix(i+2);break;}
            if(parts[i]=="GameInfo" && i+1<parts.size()){result.unit=prefix(i+(parts[i+1]=="Layout" && i+2<parts.size()?3:2));break;}
        }
        return result;
    }
}
