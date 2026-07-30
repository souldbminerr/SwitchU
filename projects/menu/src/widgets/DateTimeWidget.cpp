#include "DateTimeWidget.hpp"
#include "HudAssets.hpp"
#include <nxui/core/Renderer.hpp>
#include <ctime>
#include <cstdio>

void DateTimeWidget::setUse12HourClock(bool enabled) {
    if (m_use12HourClock == enabled)
        return;

    m_use12HourClock = enabled;
    m_timeStr.clear();
    m_timer = 1.f;
}

void DateTimeWidget::onContentUpdate(float dt) {
    m_timer += dt;
    if (m_timer < 1.f && !m_timeStr.empty()) return;
    m_timer = 0.f;

    std::time_t t = std::time(nullptr);
    std::tm* tm   = std::localtime(&t);
    if (!tm) return;
    char buf[64];
    if (m_use12HourClock) {
        int hour = tm->tm_hour % 12;
        if (hour == 0)
            hour = 12;
        std::snprintf(buf, sizeof(buf), "%d:%02d %s", hour, tm->tm_min,
                      tm->tm_hour >= 12 ? "PM" : "AM");
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    }
    m_timeStr = buf;
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
                  tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    m_dateStr = buf;
}

namespace {
// Per-glyph metrics for the qlaunch clock, keyed by the character in m_timeStr
// ("6:04 PM"). digits 14x24, colon 8x28, A/P/M 16x16, space advances 5 px.
struct Glyph { const nxui::Texture* tex; float w, h, yoff; };

// AM/PM letters sit ~5 px lower than the digit centre (baseline-aligned).
constexpr float kAmpmDrop = 3.f;

Glyph glyphFor(char c, const HudAssets& a) {
    if (c >= '0' && c <= '9') return {&a.digit[c - '0'], 14.f, 24.f, 0.f};
    switch (c) {
        case ':': return {&a.colon, 8.f, 28.f, 0.f};
        case 'A': return {&a.am, 16.f, 16.f, kAmpmDrop};
        case 'P': return {&a.pm, 16.f, 16.f, kAmpmDrop};
        case 'M': return {&a.mm, 16.f, 16.f, kAmpmDrop};
        case ' ': return {nullptr, 5.f, 0.f, 0.f};
        default:  return {nullptr, 0.f, 0.f, 0.f};
    }
}
constexpr float kKern = 1.f;   // spacing between adjacent glyphs
}

void DateTimeWidget::onContentRender(nxui::Renderer& ren) {
    if (!m_assets || !m_assets->loaded || m_timeStr.empty()) return;

    nxui::Rect cr = contentRect();
    const nxui::Color tint = m_textColor.withAlpha(m_textColor.a * m_opacity);

    // Total advance width (right-align toward the wifi icon).
    float totalW = 0.f;
    for (char c : m_timeStr) totalW += glyphFor(c, *m_assets).w + kKern;
    if (totalW > 0.f) totalW -= kKern;

    float x  = cr.right() - totalW;
    float cy = cr.y + cr.height * 0.5f;
    for (char c : m_timeStr) {
        Glyph g = glyphFor(c, *m_assets);
        if (g.tex && g.tex->valid() && g.h > 0.f)
            ren.drawTexture(g.tex, {x, cy - g.h * 0.5f + g.yoff, g.w, g.h}, tint);
        x += g.w + kKern;
    }
}

nxui::Vec2 DateTimeWidget::computeContentSize() const {
    // "12:00 PM" worst case ~ 5 digits(14) + colon(8) + space(5) + 2 letters(16).
    return {m_use12HourClock ? 118.f : 66.f, 28.f};
}
