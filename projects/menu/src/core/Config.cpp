#include "Config.hpp"
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <system_error>

namespace {

template <typename T>
void readJsonOpt(const nlohmann::json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        try {
            out = it->get<T>();
        } catch (...) {
        }
    }
}

} // namespace

bool AppConfig::load() {
    std::ifstream f(kConfigPath);
    if (!f.is_open()) return false;

    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return false;
    }

    readJsonOpt(j, "musicEnabled", musicEnabled);
    readJsonOpt(j, "musicVolume", musicVolume);
    readJsonOpt(j, "sfxVolume", sfxVolume);
    readJsonOpt(j, "gridColumns", gridColumns);
    readJsonOpt(j, "gridRows", gridRows);
    readJsonOpt(j, "uiLanguageOverride", uiLanguageOverride);
    readJsonOpt(j, "soundPreset", soundPreset);
    readJsonOpt(j, "defaultProfileEnabled", defaultProfileEnabled);
    readJsonOpt(j, "defaultProfileUid", defaultProfileUid);
    readJsonOpt(j, "clockUse12Hour", clockUse12Hour);
    readJsonOpt(j, "accessibilityEnabled", accessibilityEnabled);
    readJsonOpt(j, "accessibilitySpeakHints", accessibilitySpeakHints);
    readJsonOpt(j, "accessibilitySpeakContextEveryFocus", accessibilitySpeakContextEveryFocus);
    readJsonOpt(j, "accessibilitySpeakPosition", accessibilitySpeakPosition);
    readJsonOpt(j, "accessibilitySpeechRate", accessibilitySpeechRate);
    readJsonOpt(j, "themePreset", themePreset);
    readJsonOpt(j, "backgroundEffectEnabled", backgroundEffectEnabled);
    readJsonOpt(j, "appIconShape", appIconShape);
    readJsonOpt(j, "appSelectionGlow", appSelectionGlow);
    readJsonOpt(j, "appSelectionExpand", appSelectionExpand);
    readJsonOpt(j, "appScrollEasing", appScrollEasing);
    readJsonOpt(j, "customThemeLight", customThemeLight);
    readJsonOpt(j, "customPrimary", customPrimary);
    readJsonOpt(j, "customText", customText);
    readJsonOpt(j, "customHighlight", customHighlight);
    readJsonOpt(j, "customAccent", customAccent);
    readJsonOpt(j, "customSecondary", customSecondary);

    if (musicVolume < 0.f) musicVolume = 0.f;
    if (musicVolume > 1.f) musicVolume = 1.f;
    if (sfxVolume   < 0.f) sfxVolume   = 0.f;
    if (sfxVolume   > 1.f) sfxVolume   = 1.f;
    gridColumns = std::clamp(gridColumns, 3, 8);
    gridRows = std::clamp(gridRows, 1, 5);
    if (uiLanguageOverride.empty()) uiLanguageOverride = "auto";
    if (soundPreset.empty()) soundPreset = "nx";
    if (!defaultProfileEnabled) defaultProfileUid.clear();
    accessibilitySpeechRate = std::clamp(accessibilitySpeechRate, 120, 320);
    if (themePreset.empty()) themePreset = "builtin:Switch Dark";
    if (appIconShape != "square" && appIconShape != "rounded" && appIconShape != "circular")
        appIconShape = "square";

    return true;
}

bool AppConfig::save() const {
    std::error_code ec;
    std::filesystem::create_directory("sdmc:/config", ec);
    ec.clear();
    std::filesystem::create_directory(kConfigDir, ec);

    nlohmann::json j;
    j["musicEnabled"] = musicEnabled;
    j["musicVolume"] = musicVolume;
    j["sfxVolume"] = sfxVolume;
    j["gridColumns"] = std::clamp(gridColumns, 3, 8);
    j["gridRows"] = std::clamp(gridRows, 1, 5);
    j["uiLanguageOverride"] = uiLanguageOverride;
    j["soundPreset"] = soundPreset;
    j["defaultProfileEnabled"] = defaultProfileEnabled;
    j["defaultProfileUid"] = defaultProfileEnabled ? defaultProfileUid : std::string();
    j["clockUse12Hour"] = clockUse12Hour;
    j["accessibilityEnabled"] = accessibilityEnabled;
    j["accessibilitySpeakHints"] = accessibilitySpeakHints;
    j["accessibilitySpeakContextEveryFocus"] = accessibilitySpeakContextEveryFocus;
    j["accessibilitySpeakPosition"] = accessibilitySpeakPosition;
    j["accessibilitySpeechRate"] = std::clamp(accessibilitySpeechRate, 120, 320);
    j["themePreset"] = themePreset;
    j["backgroundEffectEnabled"] = backgroundEffectEnabled;
    j["appIconShape"] = appIconShape;
    j["appSelectionGlow"] = appSelectionGlow;
    j["appSelectionExpand"] = appSelectionExpand;
    j["appScrollEasing"] = appScrollEasing;
    j["customThemeLight"] = customThemeLight;
    j["customPrimary"] = customPrimary;
    j["customText"] = customText;
    j["customHighlight"] = customHighlight;
    j["customAccent"] = customAccent;
    j["customSecondary"] = customSecondary;

    std::ofstream f(kConfigPath, std::ios::trunc);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}
