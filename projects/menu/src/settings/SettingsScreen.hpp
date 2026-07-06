#pragma once

#include "TabbedOverlayScreen.hpp"

namespace settings::tabs {
class SystemTab;
class ThemesTab;
class AudioTab;
class DisplayTab;
class InternetTab;
class ControllersTab;
class BluetoothTab;
class SleepTab;
class StorageTab;
class AboutTab;
}

class SettingsScreen : public TabbedOverlayScreen {
public:
    SettingsScreen();
    ~SettingsScreen() override = default;

    void onWireframeChange(BoolCb cb)   { m_wireframeCb = std::move(cb); }
    void onGridColumnsChange(IntCb cb)  { m_gridColumnsCb = std::move(cb); }
    void onGridRowsChange(IntCb cb)     { m_gridRowsCb = std::move(cb); }
    void onUiLanguageChange(StringCb cb) { m_uiLanguageCb = std::move(cb); }
    void onDefaultProfileChange(StringCb cb) { m_defaultProfileCb = std::move(cb); }
    void onClockUse12HourChange(BoolCb cb) { m_clockUse12HourCb = std::move(cb); }
    void onAccessibilityEnabledChange(BoolCb cb) { m_accessibilityEnabledCb = std::move(cb); }
    void onAccessibilitySpeakHintsChange(BoolCb cb) { m_accessibilitySpeakHintsCb = std::move(cb); }
    void onAccessibilitySpeakContextEveryFocusChange(BoolCb cb) { m_accessibilitySpeakContextEveryFocusCb = std::move(cb); }
    void onAccessibilitySpeakPositionChange(BoolCb cb) { m_accessibilitySpeakPositionCb = std::move(cb); }
    void onAccessibilitySpeechRateChange(IntCb cb) { m_accessibilitySpeechRateCb = std::move(cb); }
    void onThemeChange(IntCb cb)             { m_themeChangeCb = std::move(cb); }
    void onBackgroundEffectChange(BoolCb cb) { m_bgEffectCb = std::move(cb); }
    void onIconShapeChange(IntCb cb)         { m_iconShapeCb = std::move(cb); }
    void onSelectionGlowChange(BoolCb cb)    { m_selectionGlowCb = std::move(cb); }
    using CustomColorsCb = std::function<void(bool light, unsigned bg, unsigned text,
                                              unsigned highlight, unsigned enabled, unsigned disabled)>;
    void onCustomColorsChange(CustomColorsCb cb) { m_customColorsCb = std::move(cb); }
    void setCustomThemeState(bool active, bool light, unsigned bg, unsigned text,
                             unsigned highlight, unsigned enabled, unsigned disabled) {
        m_customActive = active; m_customLight = light;
        m_customPrimary = bg; m_customText = text; m_customHighlight = highlight;
        m_customAccent = enabled; m_customSecondary = disabled;
    }
    void fireCustomColors() {
        if (m_customColorsCb)
            m_customColorsCb(m_customLight, m_customPrimary, m_customText,
                             m_customHighlight, m_customAccent, m_customSecondary);
    }
    void onMusicEnabledChange(BoolCb cb)     { m_musicEnabledCb = std::move(cb); }
    void onNetConnect(VoidCb cb)        { m_netConnectCb = std::move(cb); }
    void onSleepRequest(VoidCb cb)      { m_sleepCb = std::move(cb); }
    void onShutdownRequest(VoidCb cb)   { m_shutdownCb = std::move(cb); }
    void onRebootRequest(VoidCb cb)     { m_rebootCb = std::move(cb); }

    void setWireframeState(bool enabled) { m_wireframeEnabled = enabled; }
    void setGridLayoutState(int columns, int rows) {
        m_gridColumns = std::clamp(columns, 3, 8);
        m_gridRows = std::clamp(rows, 2, 5);
    }
    void setUiLanguageOverride(const std::string& tag) {
        m_uiLanguageOverride = tag.empty() ? "auto" : tag;
    }
    void setDefaultProfileState(bool enabled, const std::string& uidHex) {
        m_defaultProfileUid = enabled ? uidHex : std::string();
    }
    void setClockUse12HourState(bool enabled) {
        m_clockUse12Hour = enabled;
    }
    void setThemeState(std::vector<std::string> names, int index, bool effectEnabled) {
        m_themeNames = std::move(names);
        m_themeIndex = index;
        m_bgEffectEnabled = effectEnabled;
    }
    void setMusicEnabledState(bool enabled) { m_musicEnabled = enabled; }
    void setIconShapeState(std::vector<std::string> names, int index) {
        m_iconShapeNames = std::move(names);
        m_iconShapeIndex = index;
    }
    void setSelectionGlowState(bool enabled) { m_selectionGlow = enabled; }
    void setAccessibilityEnabledState(bool enabled) {
        m_accessibilityEnabled = enabled;
        setAccessibilityVoiceEnabled(enabled);
    }
    void setAccessibilitySpeechState(bool speakHints, bool speakContextEveryFocus,
                                     bool speakPosition, int speechRate) {
        m_accessibilitySpeakHints = speakHints;
        m_accessibilitySpeakContextEveryFocus = speakContextEveryFocus;
        m_accessibilitySpeakPosition = speakPosition;
        m_accessibilitySpeechRate = std::clamp(speechRate, 120, 320);
        setAccessibilitySpeechPreferences(speakHints, speakPosition);
    }

protected:
    void buildTabs() override;

private:
    friend class settings::tabs::SystemTab;
    friend class settings::tabs::ThemesTab;
    friend class settings::tabs::AudioTab;
    friend class settings::tabs::DisplayTab;
    friend class settings::tabs::InternetTab;
    friend class settings::tabs::ControllersTab;
    friend class settings::tabs::BluetoothTab;
    friend class settings::tabs::SleepTab;
    friend class settings::tabs::StorageTab;
    friend class settings::tabs::AboutTab;

    BoolCb m_wireframeCb;
    IntCb m_gridColumnsCb;
    IntCb m_gridRowsCb;
    StringCb m_uiLanguageCb;
    StringCb m_defaultProfileCb;
    BoolCb m_clockUse12HourCb;
    IntCb  m_themeChangeCb;
    BoolCb m_bgEffectCb;
    IntCb  m_iconShapeCb;
    BoolCb m_selectionGlowCb;
    BoolCb m_musicEnabledCb;
    CustomColorsCb m_customColorsCb;
    BoolCb m_accessibilityEnabledCb;
    BoolCb m_accessibilitySpeakHintsCb;
    BoolCb m_accessibilitySpeakContextEveryFocusCb;
    BoolCb m_accessibilitySpeakPositionCb;
    IntCb m_accessibilitySpeechRateCb;
    VoidCb m_netConnectCb;
    VoidCb m_sleepCb;
    VoidCb m_shutdownCb;
    VoidCb m_rebootCb;

    bool m_wireframeEnabled = false;
    int m_gridColumns = 5;
    int m_gridRows = 3;
    std::string m_uiLanguageOverride = "auto";
    std::string m_defaultProfileUid;
    bool m_clockUse12Hour = false;
    std::vector<std::string> m_themeNames;
    int  m_themeIndex = 0;
    bool m_bgEffectEnabled = false;
    std::vector<std::string> m_iconShapeNames;
    int  m_iconShapeIndex = 1;   // default: Square
    bool m_selectionGlow = false;
    bool m_musicEnabled = false;
    bool m_customActive = false;
    bool m_customLight = false;
    unsigned m_customPrimary = 0x2D2D2D, m_customText = 0xFFFFFF, m_customHighlight = 0x00C3E3;
    unsigned m_customAccent = 0x07FDCC, m_customSecondary = 0x38393B;
    bool m_accessibilityEnabled = true;
    bool m_accessibilitySpeakHints = true;
    bool m_accessibilitySpeakContextEveryFocus = false;
    bool m_accessibilitySpeakPosition = true;
    int m_accessibilitySpeechRate = 190;
};
