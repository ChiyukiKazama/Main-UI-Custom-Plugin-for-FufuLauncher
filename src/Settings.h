// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#pragma once
#include <array>
#include <string>
#include <vector>
#include <map>

namespace UI
{
    inline constexpr const char* MainPagePath = "/Canvas/Pages/InLevelMainPage";
    enum Preset { AllMainUi, MiniMap, Team, QuestTracking, SkillControls, ChatButton, TopLeftButtons, TopRightButtons,
        Paimon, Activities, Bag, Character, Gacha, BattlePass, Handbook, Coop,
        PlayerHealthStatus, Interaction, Ping, MiliastraEntrance, ElementalSight, ChatPanel, QuestButton, SnezhnayaIcon, PresetCount };
    inline constexpr const char* PresetConfigKeys[] = {"HideAll", "HideMiniMap", "HideTeam", "HideQuestTrack",
        "HideSkillPanel", "HideChatButton", "HideTopButtons", "HideMainButtons", "HidePaimon",
        "HideActivities", "HideBag", "HideCharacter", "HideGacha", "HideBattlePass",
        "HideHandbook", "HideCoop", "HideHealthBar", "HideInteraction", "HidePing",
        "HideMiliastraEntrance", "HideElementalSight", "HideChatPanel", "HideQuestButton", "HideSnezhnayaIcon"};
    static_assert(sizeof(PresetConfigKeys) / sizeof(PresetConfigKeys[0]) == PresetCount);
    struct Settings
    {
        float scale = 1;
        bool alignRightButtons = false;
        float marginLeft = 0, marginRight = 0, marginTop = 0, marginBottom = 0;
        // Shipped defaults come from config.ini; omitted shortcuts stay disabled.
        int restoreHotkey = 0, exportHotkey = 0;
        std::array<bool, PresetCount> hiddenPresets{};
        std::vector<std::string> hiddenPaths;
    };
    bool ParseSettings(const std::string& text, Settings& result, std::string& error);
    bool NormalizeMainUiPath(const std::string& text, std::string& result);
    std::vector<Preset> GetOverrideParents(Preset preset);
    std::array<bool, PresetCount> GetEffectiveHiddenPresets(const Settings& settings);

}
