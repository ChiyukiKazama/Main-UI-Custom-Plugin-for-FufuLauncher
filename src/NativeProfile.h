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

    // Semantic identities; per-build InLevelMainPage offsets belong to Layout.
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

    // International 7.2.0 (OSRELWin7.2.0_R49086080_S49291486_D49291486).
    // Native destinations and entry bytes are from GenshinImpact.exe; managed
    // methods were cross-checked against their live declaring classes.
    inline constexpr Profile OS72{
        "OS 7.2", L"GenshinImpact.exe", 0x6AD8992C, 0x1B690000,
        {{
            {0x117F860, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 44 E8 E6 D5 2D 00 48 85 C0 74 3A 48"}, // ReadCanvasColor
            {0x117F980, "40 53 48 83 EC 40 0F 29 74 24 30 0F 28 F1 48 8B D9 48 85 C9 74 42 E8 C5 D4 2D 00 48 85 C0 74 38"}, // SetCanvasAlpha
            {0x1448C10, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 36 42 01 00 48 85 C0 74 15 48"}, // SetLocalPosition
            {0x19C54AD0, "56 48 83 EC 20 48 85 C9 74 17 48 89 CE E8 FE 2C 06 E8 48 89 F1 89 C2 48 83 C4 20 5E E9 7F BF 8D"}, // StringNew
            {0x1465800, "40 53 48 83 EC 50 48 89 4C 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 96 E8 06 00 48 8B 08 48 85 C9"}, // FindObject
            {0x1464E80, "40 53 48 83 EC 20 48 8B D9 48 85 C9 0F 84 85 00 00 00 E8 C9 7F FF FF 48 85 C0 74 7B 48 8B 40 40"}, // ObjectTransform
            {0x1465D40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 4A E8 0D 71 FF FF 48 85 C0 74 40 48 89 7C 24 38 48 8B 78"}, // ComponentObject
            {0x1464870, "40 53 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 23 F8 06 00 48 85 DB"}, // ComponentByName
            {0x98D0B0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1C E8 9D FD AC 00 48 85 C0 74 12 48 8B 10 48 8B C8 48 83"}, // ReadRendererEnabled
            {0x98D100, "48 89 5C 24 10 57 48 83 EC 20 0F B6 FA 48 8B D9 48 85 C9 74 27 E8 46 FD AC 00 48 85 C0 74 1D 4C"}, // SetRendererEnabled
            {0x144A820, "40 53 48 81 EC 80 00 00 00 48 8B D9 48 85 C9 0F 84 DC 00 00 00 E8 26 26 01 00 48 85 C0 0F 84 CE"}, // Name
            {0x1A258E80, "56 48 83 EC 20 48 89 CE 48 8B 49 10 48 85 C9 74 25 8B 05 69 33 61 EB 83 F8 FF 75 0F E8 9F 17 1F"}, // InstanceId
            {0x144AF50, "48 83 EC 28 8B D1 48 8B 0D F3 EA 35 04 48 85 C9 75 14 33 D2 48 8D 4C 24 38 E8 22 00 09 00 48 8B"}, // FromId
            {0x1465000, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 17 E8 4D 7E FF FF 48 85 C0 74 0D 0F B6 80 F6 01 00 00 48"}, // ActiveSelf
            {0x14492E0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 24 E8 6D 3B 01 00 48 85 C0 74 1A 48 8B 90 A0 00 00 00 48"}, // Parent
            {0x1449B40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 16 E8 0D 33 01 00 48 85 C0 74 0C 8B 80 90 00 00 00 48 83"}, // ChildCount
            {0x1449EA0, "48 89 5C 24 10 57 48 83 EC 20 48 63 FA 48 8B D9 48 85 C9 74 39 E8 A6 2F 01 00 48 85 C0 74 2F 85"}, // Child
            {0x1449CE0, "48 89 5C 24 18 57 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 AF A3 08"}, // FindTransform
            {0x1448DA0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 A6 40 01 00 48 85 C0 74 26 48"}, // ReadScale
            {0x1448BC0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 86 42 01 00 48 85 C0 74 26 48"}, // ReadLocalPosition
            {0x1448D00, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 28 E8 46 41 01 00 48 85 C0 74 1E 48"}, // ReadRotation
            {0x1449040, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 06 3E 01 00 48 85 C0 74 15 48"}, // SetScale
            {0x1455EC0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 C9 74 4A E8 81 6F 00 00 48"}, // ReadPosition
            {0x1455F30, "48 89 5C 24 10 57 48 83 EC 20 F3 0F 10 02 48 8B D9 F3 0F 10 4A 04 F3 0F 11 44 24 30 F3 0F 11 4C"}, // SetPosition
            {0x1455C90, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 23 E8 B6 71 00 00 48 85 C0 74 19 48"}, // ReadRect
            {0x1A3679E0, "C6 81 30 01 00 00 01 C3"}, // MaskSetParamsDirty
            {0x1A369E10, "56 53 48 83 EC 28 48 89 CE C6 81 50 01 00 00 00 80 B9 30 01 00 00 00 75 35 48 8B 4E 70 48 85 C9"}, // MaskRebuild
            {0x1A34B3A0, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 44 0F 29 A5 80"}, // RectMaskPerformClipping
            {0x1A320650, "48 8B 05 29 AE 54 EB 48 8B 80 80 12 00 00 48 85 C0 74 0F 4C 8B 40 18 48 8B 50 28 48 8B 48 40 49"}, // WillRender
            {0x12601150, "56 57 53 48 83 EC 20 48 89 CE 80 3D 69 12 21 F3 00 0F 85 A1 01 00 00 48 8B 7E 50 48 8B 5E 60 48"}, // MainAwake
            {0x1A342F90, "56 57 48 83 EC 28 48 89 CE E8 02 FB FF FF 48 8B 4E 48 48 85 C9 74 07 48 83 79 10 00 75 0C 48 89"}, // GraphicEnable
            {0x1A343080, "56 48 83 EC 20 48 89 CE 48 8B 01 31 D2 FF 90 C8 01 00 00 80 7E 79 00 74 12 48 8B 06 48 89 F1 31"}, // GraphicLightActivate
            {0xABF77F0, "56 48 83 EC 20 48 89 CE 80 3D A0 68 C3 FA 00 75 3B 48 89 F1 E8 F7 F9 FF FF 48 8B 4E 20 48 85 C9"}, // EffectEnable
            {0xCF56180, "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 68 01 00 00 4D 89 CD 4D 89 C1 41 89 D0 48 89 CA C7"}, // PrefabInstance
            {0xABCEC00, "56 57 48 83 EC 28 48 89 CE 80 3D BD 18 C2 FA 00 0F 85 33 01 00 00 8B 86 D0 00 00 00 83 C0 FE 83"}, // QuestEnable
            {0xCF21000, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D 25 F5 8A F8 00 75 76 48 8B 4A 20 45 85 C0 78 26 48 85 C9"}, // InteeDialogAdd
            {0xCF21380, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D AA F1 8A F8 00 75 76 48 8B 4A 28 45 85 C0 78 26 48 85 C9"}, // InteePickupAdd
            {0x8779D20, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 0F 29 BD 80 00"}, // QuestTraceTick
            {0xCF20800, "41 56 56 57 53 48 83 EC 28 48 89 CE 80 3D 1E FD 8A F8 00 0F 85 1A 02 00 00 48 8B 46 50 48 85 C0"}, // InteeKeyRefresh
            {0xCF1FE50, "56 57 53 48 83 EC 30 48 89 CF 80 3D CE 06 8B F8 00 0F 85 BF 00 00 00 48 8B 77 68 48 85 F6 74 64"} // InteeSwitchKeyRefresh
        }},
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
    static_assert(Complete(CN72) && Complete(OS72), "Every profile must supply all entries and managed fields.");

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

    inline constexpr const Profile* Profiles[]{&CN72, &OS72};
    constexpr const Profile* SelectProfile(std::wstring_view executableName, uint32_t timestamp, uint32_t imageSize)
    {
        for (const auto* profile : Profiles)
            if (SameExecutable(executableName, profile->executableName) &&
                timestamp == profile->timestamp && imageSize == profile->imageSize) return profile;
        return nullptr;
    }
}
