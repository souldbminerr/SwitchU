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
    bool  clockUse12Hour = false;
    bool  accessibilityEnabled = true;
    bool  accessibilitySpeakHints = true;
    bool  accessibilitySpeakContextEveryFocus = false;
    bool  accessibilitySpeakPosition = true;
    int   accessibilitySpeechRate = 190;

    std::string themePreset = "builtin:Basic Dark";
    bool  backgroundEffectEnabled = false;
    std::string appIconShape = "square";   // "square" (qlaunch) | "rounded" (SwitchU) | "circular"
    bool  appSelectionGlow = false;        // pulsing drop-shadow glow on selected apps
    bool  appSelectionExpand = false;      // scale the selected app up (qlaunch: off)
    bool  appScrollEasing = false;         // ease/center scroll to stops (qlaunch: off)

    // "Custom" theme colours (0xRRGGBB), edited via the Themes color picker.
    bool     customThemeLight = false;          // light vs dark base (text/panels)
    uint32_t customPrimary        = 0x2D2D2D;
    uint32_t customText      = 0xFFFFFF;
    uint32_t customHighlight = 0x00C3E3;
    uint32_t customAccent   = 0x07FDCC;
    uint32_t customSecondary  = 0x38393B;

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/qlaunch-ext";
    static constexpr const char* kConfigPath = "sdmc:/config/qlaunch-ext/config.json";
};
