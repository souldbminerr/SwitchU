#pragma once
#include <nxui/core/Types.hpp>

namespace nxui {

enum class ThemeMode { Dark, Light };

struct Theme {
    ThemeMode mode = ThemeMode::Dark;

    // Background
    Color primary;            // main background colour
    Color backgroundAccent;   // gradient / secondary bg tint

    // Glass panels
    Color panelBase;
    Color panelBorder;
    Color panelHighlight;     // top shine

    // Icons
    Color iconDefault;

    // Cursor
    Color cursorNormal;
    Color cursorGlow;

    // Text
    Color textPrimary;
    Color textSecondary;

    // Settings value colours (qlaunch): "Enabled"/editable values + "Disabled".
    Color accent  {0.027f, 0.992f, 0.800f, 1.f};   // #07fdcc
    Color secondary {0.220f, 0.224f, 0.231f, 1.f};   // #38393b

    // Page indicator dots
    Color pageIndicator;
    Color pageIndicatorActive;

    // Animated background shapes
    Color shapeColor;

    // Radii & sizes
    float iconCornerRadius   = 24.f;
    float cursorCornerRadius = 26.f;
    float cursorBorderWidth  = 3.f;
    float panelCornerRadius  = 24.f;
    float cellCornerRadius   = 20.f;

    static Theme dark();
    static Theme light();
};

} // namespace nxui
