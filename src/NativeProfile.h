// Game-build selection and managed layouts. No game memory is accessed here.
#pragma once
#include "Native.h"
#include <array>
#include <cstddef>
#include <string_view>

namespace Native
{
    enum class Entry : size_t
    {
        ReadCanvasColor, SetCanvasAlpha, SetLocalPosition, StringNew, FindObject,
        ObjectTransform, ComponentObject, ComponentByName, ReadRendererEnabled,
        SetRendererEnabled, Name, InstanceId, FromId, ActiveSelf, Parent,
        ChildCount, Child, FindTransform, ReadScale, ReadLocalPosition,
        ReadRotation, SetScale, ReadPosition, SetPosition, ReadRect,
        MaskSetParamsDirty, MaskRebuild, RectMaskPerformClipping, WillRender,
        MainAwake, GraphicEnable, GraphicLightActivate, EffectEnable,
        PrefabInstance, QuestEnable, InteeDialogAdd, InteePickupAdd,
        QuestTraceTick, InteeKeyRefresh, InteeSwitchKeyRefresh, Count
    };

    // Semantic identities, not positions in the shuffled InLevelMainPage class.
    enum class PageField : size_t
    {
        MiniMap, Team, QuestTrace, SkillControls, QuestDistance, ReturnToQuest,
        Chat, TopLeftButtons, MapInfo, TopLeftExtra, TopLeftScene,
        TopRightButtons, Paimon, Activities, ActivitiesExtra, Bag, Character,
        Gacha, BattlePass, Handbook, HandbookExtra, Coop, PlayerStatus,
        QuestButton, Interaction, Ping, MiliastraEntrance, MiliastraEntranceExtra,
        ElementalSight, Count
    };

    struct Layout
    {
        std::array<size_t, static_cast<size_t>(PageField::Count)> pageFields;
        std::array<size_t, 2> interactionKeys;
        constexpr size_t Offset(PageField field) const { return pageFields[static_cast<size_t>(field)]; }
    };

    struct Profile
    {
        const char* name;
        std::wstring_view executableName;
        uint32_t timestamp;
        uint32_t imageSize;
        std::array<Method, static_cast<size_t>(Entry::Count)> entries;
        Layout layout;
        constexpr const Method& Get(Entry entry) const { return entries[static_cast<size_t>(entry)]; }
    };

    inline constexpr Profile CN72{
        "CN 7.2", L"YuanShen.exe", Timestamp, ImageSize,
        {{ReadCanvasColor, SetCanvasAlpha, SetLocalPosition, StringNew, FindObject,
          ObjectTransform, ComponentObject, ComponentByName, ReadRendererEnabled,
          SetRendererEnabled, Name, InstanceId, FromId, ActiveSelf, Parent,
          ChildCount, Child, FindTransform, ReadScale, ReadLocalPosition,
          ReadRotation, SetScale, ReadPosition, SetPosition, ReadRect,
          MaskSetParamsDirty, MaskRebuild, RectMaskPerformClipping, WillRender,
          MainAwake, GraphicEnable, GraphicLightActivate, EffectEnable,
          PrefabInstance, QuestEnable, InteeDialogAdd, InteePickupAdd,
          QuestTraceTick, InteeKeyRefresh, InteeSwitchKeyRefresh}},
        {{{0x78, 0x70, 0x2E0, 0x2D8, 0x490, 0x1D0,
           0x3B8, 0x88, 0x80, 0x130, 0x398,
           0x98, 0x110, 0x108, 0x1A0, 0xF0, 0xE0,
           0xF8, 0x1B0, 0x188, 0x190, 0xB8, 0x48,
           0xD8, 0x1F8, 0x218, 0x1B8, 0x1C0, 0x370}},
         {{0x50, 0x68}}}
    };

    constexpr bool Complete(const Profile& profile)
    {
        if (profile.executableName.empty() || !profile.timestamp || !profile.imageSize) return false;
        for (const auto& entry : profile.entries)
            if (!entry.rva || entry.rva >= profile.imageSize || !entry.bytes || !entry.bytes[0]) return false;
        for (size_t offset : profile.layout.pageFields)
            if (offset < 0x10 || offset % sizeof(uint64_t)) return false;
        for (size_t offset : profile.layout.interactionKeys)
            if (offset < 0x10 || offset % sizeof(uint64_t)) return false;
        return true;
    }
    static_assert(Complete(CN72), "Every profile must supply all entries and managed fields.");

    constexpr wchar_t FoldAscii(wchar_t value)
    { return value >= L'A' && value <= L'Z' ? value + (L'a' - L'A') : value; }
    constexpr bool SameExecutable(std::wstring_view lhs, std::wstring_view rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (size_t i = 0; i < lhs.size(); ++i) if (FoldAscii(lhs[i]) != FoldAscii(rhs[i])) return false;
        return true;
    }
    constexpr bool IsGameExecutable(std::wstring_view name)
    { return SameExecutable(name, L"YuanShen.exe") || SameExecutable(name, L"GenshinImpact.exe"); }

    inline constexpr const Profile* Profiles[]{&CN72};
    constexpr const Profile* SelectProfile(std::wstring_view executableName, uint32_t timestamp, uint32_t imageSize)
    {
        for (const auto* profile : Profiles)
            if (SameExecutable(executableName, profile->executableName) &&
                timestamp == profile->timestamp && imageSize == profile->imageSize) return profile;
        return nullptr;
    }
}
