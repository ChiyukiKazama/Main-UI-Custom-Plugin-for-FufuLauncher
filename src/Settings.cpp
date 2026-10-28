#include "Settings.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace UI
{
    namespace
    {
        std::string TrimWhitespace(const std::string& text)
        {
            const auto start = text.find_first_not_of(" \t\r\n");
            return start == std::string::npos ? "" : text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
        }
        std::string ToLowerAscii(std::string text)
        { for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return text; }
        bool ParseBoolean(const std::string& text, bool& value)
        {
            const auto token = ToLowerAscii(TrimWhitespace(text));
            if (token == "1" || token == "true") { value = true; return true; }
            if (token == "0" || token == "false") { value = false; return true; }
            return false;
        }
        bool ParseFloatInRange(const std::string& text, float& value, float low, float high)
        {
            try { size_t used = 0; const float n = std::stof(text, &used);
                if (!TrimWhitespace(text.substr(used)).empty() || !std::isfinite(n) || n < low || n > high) return false;
                value = n; return true; } catch (...) { return false; }
        }
        bool ParseVirtualKey(const std::string& text, int& value)
        {
            try { size_t used = 0; const int n = std::stoi(text, &used);
                if (!TrimWhitespace(text.substr(used)).empty() || n < 0 || n > 254) return false;
                value = n; return true; } catch (...) { return false; }
        }
    }
    bool NormalizeMainUiPath(const std::string& text, std::string& result)
    {
        result = TrimWhitespace(text);
        std::replace(result.begin(), result.end(), '\\', '/');
        while (!result.empty() && result.back() == '/') result.pop_back();
        if (result.empty() || result.size() > 1024 || result.find_first_of("*?\t\r\n") != std::string::npos) return false;
        if (result[0] != '/')
        {
            if (result.compare(0, 7, "Canvas/") == 0) result = "/" + result;
            else result = std::string(MainPagePath) + "/" + result;
        }
        std::istringstream stream(result.substr(1));
        std::string part;
        while (std::getline(stream, part, '/')) if (part.empty() || part == "." || part == "..") return false;
        return result == MainPagePath || result.compare(0, std::string(MainPagePath).size() + 1, std::string(MainPagePath) + "/") == 0;
    }
    std::vector<Preset> GetOverrideParents(Preset preset)
    {
        std::vector<Preset> parents;
        if (preset == AllMainUi) return parents;
        parents.push_back(AllMainUi);
        if (preset == Paimon || preset == Coop || preset == ElementalSight || preset == QuestButton || preset == SnezhnayaIcon)
            parents.push_back(TopLeftButtons);
        if (preset == Activities || preset == Bag || preset == Character || preset == Gacha ||
            preset == BattlePass || preset == Handbook || preset == MiliastraEntrance)
            parents.push_back(TopRightButtons);
        return parents;
    }
    std::array<bool, PresetCount> GetEffectiveHiddenPresets(const Settings& settings)
    {
        auto effective = settings.hiddenPresets;
        bool changed;
        do
        {
            changed = false;
            for (int i=0;i<PresetCount;++i) if (!effective[i])
                for (auto parentPreset : GetOverrideParents(static_cast<Preset>(i)))
                    if (effective[parentPreset]) { effective[i]=true; changed=true; break; }
        } while (changed);
        return effective;
    }
    bool ParseSettings(const std::string& input, Settings& result, std::string& error)
    {
        std::string text = input;
        if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
        if (text.find('\0') != std::string::npos || text.compare(0, 2, "\xFF\xFE") == 0)
        { error = "Configuration must be UTF-8, not UTF-16."; return false; }
        Settings next;
        bool hasKnownSetting = false;
        std::istringstream stream(text);
        std::string line, section;
        std::map<std::string, std::string> values;
        while (std::getline(stream, line))
        {
            line = TrimWhitespace(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') { section = ToLowerAscii(TrimWhitespace(line.substr(1, line.size() - 2))); continue; }
            const auto equal = line.find('=');
            if (equal != std::string::npos && ToLowerAscii(TrimWhitespace(line.substr(0, equal))) == "value")
                values[section] = TrimWhitespace(line.substr(equal + 1));
        }
        for (const auto& pair : values)
        {
            const auto& key = pair.first;
            const auto value = TrimWhitespace(pair.second.substr(0, pair.second.find(';')));
            bool valid = true, recognized = true;
            if (key == "scale") valid = ParseFloatInRange(value, next.scale, .25f, 2.5f);
            else if (key == "alignrightbuttons") valid = ParseBoolean(value, next.alignRightButtons);
            else if (key == "marginleft") valid=ParseFloatInRange(value,next.marginLeft,-4000,4000);
            else if (key == "marginright") valid=ParseFloatInRange(value,next.marginRight,-4000,4000);
            else if (key == "margintop") valid=ParseFloatInRange(value,next.marginTop,-4000,4000);
            else if (key == "marginbottom") valid=ParseFloatInRange(value,next.marginBottom,-4000,4000);
            else if (key == "togglekey") valid = ParseVirtualKey(value, next.restoreHotkey);
            else if (key == "dumpkey") valid = ParseVirtualKey(value, next.exportHotkey);
            else if (key == "hiddenpaths")
            {
                std::istringstream paths(pair.second);
                std::string path;
                while (std::getline(paths, path, '|'))
                {
                    if (TrimWhitespace(path).empty()) continue;
                    std::string normalized;
                    if (next.hiddenPaths.size() >= 128 || !NormalizeMainUiPath(path, normalized)) { valid = false; break; }
                    if (std::find(next.hiddenPaths.begin(), next.hiddenPaths.end(), normalized) == next.hiddenPaths.end()) next.hiddenPaths.push_back(normalized);
                }
            }
            else
            {
                recognized = false;
                for (size_t i = 0; i < PresetCount; ++i) if (key == ToLowerAscii(PresetConfigKeys[i]))
                { recognized = true; valid = ParseBoolean(value, next.hiddenPresets[i]); break; }
            }
            hasKnownSetting = hasKnownSetting || recognized;
            if (!valid) { error = "Invalid configuration value: " + key; return false; }
        }
        // Empty/metadata-only snapshots may be partial file replacements.
        if (!hasKnownSetting) { error = "Configuration contains no recognized settings; retaining the previous snapshot."; return false; }
        if (next.restoreHotkey && next.restoreHotkey == next.exportHotkey) { error = "ToggleKey and DumpKey must differ."; return false; }
        result = std::move(next);
        error.clear();
        return true;
    }
}
