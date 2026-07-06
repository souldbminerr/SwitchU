#pragma once
#include <string>
#include <cstdint>

struct AppConfig {
    bool  musicEnabled = false;
    float musicVolume  = 0.4f;
    float sfxVolume    = 0.7f;
    int   gridColumns  = 4;
    int   gridRows     = 1;
    std::string uiLanguageOverride = "auto";
    std::string soundPreset = "nx";
    bool  defaultProfileEnabled = false;
    std::string defaultProfileUid;
    bool  tutorialCompleted = false;
    bool  clockUse12Hour = false;
    bool  accessibilityEnabled = true;
    bool  accessibilitySpeakHints = true;
    bool  accessibilitySpeakContextEveryFocus = false;
    bool  accessibilitySpeakPosition = true;
    int   accessibilitySpeechRate = 190;

    std::string themePreset = "builtin:Basic Dark";
    bool  backgroundEffectEnabled = false;
    std::string appIconShape = "square";   // "square" (qlaunch) | "rounded" (SwitchU) | "circular"

    // "Custom" theme colours (0xRRGGBB), edited via the Themes color picker.
    bool     customThemeLight = false;          // light vs dark base (text/panels)
    uint32_t customBg        = 0x2D2D2D;
    uint32_t customText      = 0xFFFFFF;
    uint32_t customHighlight = 0x00C3E3;
    uint32_t customEnabled   = 0x07FDCC;
    uint32_t customDisabled  = 0x38393B;

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/qlaunch-ext";
    static constexpr const char* kConfigPath = "sdmc:/config/qlaunch-ext/config.json";
};
