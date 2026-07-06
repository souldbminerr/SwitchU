#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>
#include <cmath>
#include <string>

SettingsScreen::Tab settings::tabs::ThemesTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;

    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.themes", "Themes");

    // Theme picker.
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.theme", "Theme");
        it.description = i18n.tr("settings.themes.theme_desc", "Choose the menu colour theme.");
        it.type = ItemType::Selector;
        it.options = screen.m_themeNames;
        it.intVal = std::clamp(screen.m_themeIndex, 0,
                               std::max(0, (int)screen.m_themeNames.size() - 1));
        it.onChange = [&screen](SettingItem& self) {
            screen.m_themeIndex = self.intVal;
            if (screen.m_themeChangeCb)
                screen.m_themeChangeCb(self.intVal);
        };
        t.items.push_back(std::move(it));
    }

    // Background effect toggle.
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.background_effect", "Background Effect");
        it.description = i18n.tr("settings.themes.background_effect_desc",
                                 "Show the animated background effect instead of a flat colour.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_bgEffectEnabled;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_bgEffectEnabled = self.boolVal;
            if (screen.m_bgEffectCb)
                screen.m_bgEffectCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // App icon shape: Circular / Square (qlaunch) / Rounded (SwitchU).
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.icon_shape", "App Icon Shape");
        it.description = i18n.tr("settings.themes.icon_shape_desc",
                                 "Circular, Square (like the Switch), or Rounded.");
        it.type = ItemType::Selector;
        it.options = screen.m_iconShapeNames;
        it.intVal = std::clamp(screen.m_iconShapeIndex, 0,
                               std::max(0, (int)screen.m_iconShapeNames.size() - 1));
        it.onChange = [&screen](SettingItem& self) {
            screen.m_iconShapeIndex = self.intVal;
            if (screen.m_iconShapeCb)
                screen.m_iconShapeCb(self.intVal);
        };
        t.items.push_back(std::move(it));
    }

    // Pulsing drop-shadow glow on the selected app.
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.selection_glow", "Selection Glow");
        it.description = i18n.tr("settings.themes.selection_glow_desc",
                                 "Pulsing drop shadow around the selected app.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_selectionGlow;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_selectionGlow = self.boolVal;
            if (screen.m_selectionGlowCb)
                screen.m_selectionGlowCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // Scale the selected app up (qlaunch keeps it flat).
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.selection_expand", "Expand On Select");
        it.description = i18n.tr("settings.themes.selection_expand_desc",
                                 "Scale the selected app up slightly.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_selectionExpand;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_selectionExpand = self.boolVal;
            if (screen.m_selectionExpandCb)
                screen.m_selectionExpandCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // Smooth (eased/centred) scrolling vs. just stopping (qlaunch).
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.scroll_center", "Center Selection");
        it.description = i18n.tr("settings.themes.scroll_center_desc",
                                 "Keep the selected app centred while scrolling (off = qlaunch deadzone).");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_scrollEasing;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_scrollEasing = self.boolVal;
            if (screen.m_scrollEasingCb)
                screen.m_scrollEasingCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // ---- Custom theme colour picker ----
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.custom_colors", "Custom Colors");
        it.type = ItemType::Section;
        t.items.push_back(std::move(it));
    }
    {
        SettingItem it;
        it.label = i18n.tr("settings.themes.light_base", "Light Base");
        it.description = i18n.tr("settings.themes.light_base_desc",
                                 "Use light panels/text for the Custom theme.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_customLight;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_customLight = self.boolVal;
            screen.fireCustomColors();
        };
        t.items.push_back(std::move(it));
    }

    auto addColor = [&](const char* name, unsigned SettingsScreen::* member) {
        {
            SettingItem sec;
            sec.label = name;
            sec.type = ItemType::Section;
            t.items.push_back(std::move(sec));
        }
        static const char* kCh[3] = {"Red", "Green", "Blue"};
        static const int kShift[3] = {16, 8, 0};
        for (int c = 0; c < 3; ++c) {
            SettingItem it;
            it.label = kCh[c];
            it.type = ItemType::Slider;
            it.sliderSteps = 255;
            unsigned val = (screen.*member >> kShift[c]) & 0xFF;
            it.floatVal = val / 255.f;
            it.infoText = std::to_string(val);
            const int shift = kShift[c];
            it.onChange = [&screen, member, shift](SettingItem& self) {
                unsigned v = (unsigned)std::lround(std::clamp(self.floatVal, 0.f, 1.f) * 255.f);
                screen.*member = (screen.*member & ~(0xFFu << shift)) | (v << shift);
                self.infoText = std::to_string(v);
                screen.fireCustomColors();
            };
            t.items.push_back(std::move(it));
        }
    };

    addColor(i18n.tr("settings.themes.color.primary", "Primary").c_str(),     &SettingsScreen::m_customPrimary);
    addColor(i18n.tr("settings.themes.color.text", "Text").c_str(),           &SettingsScreen::m_customText);
    addColor(i18n.tr("settings.themes.color.highlight", "Highlight").c_str(), &SettingsScreen::m_customHighlight);
    addColor(i18n.tr("settings.themes.color.accent", "Accent").c_str(),       &SettingsScreen::m_customAccent);
    addColor(i18n.tr("settings.themes.color.secondary", "Secondary").c_str(), &SettingsScreen::m_customSecondary);

    return t;
}
