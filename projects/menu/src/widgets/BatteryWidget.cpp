#include "BatteryWidget.hpp"
#include "HudAssets.hpp"
#include <nxui/core/Renderer.hpp>
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

void BatteryWidget::setBatteryStatus(uint32_t percentage, bool charging) {
    if (percentage > 100)
        percentage = 100;
    m_level = static_cast<float>(percentage) / 100.f;
    m_charging = charging;
    m_timer = 0.f;
}

void BatteryWidget::onContentUpdate(float dt) {
    m_chargeAnim += dt;
    m_timer += dt;
    if (m_timer < 1.f && m_level >= 0.f) return;
    m_timer = 0.f;

    u32 charge = 100;
    if (R_SUCCEEDED(psmGetBatteryChargePercentage(&charge))) {
        if (charge > 100)
            charge = 100;
        m_level = charge / 100.f;
    }

    PsmChargerType ct = PsmChargerType_Unconnected;
    if (R_SUCCEEDED(psmGetChargerType(&ct)))
        m_charging = (ct != PsmChargerType_Unconnected);
}

void BatteryWidget::onContentRender(nxui::Renderer& ren) {
    if (!m_assets || !m_assets->loaded)
        return;

    nxui::Rect cr = contentRect();
    const float op = m_opacity;
    float level = (m_level < 0.f) ? 1.f : std::clamp(m_level, 0.f, 1.f);
    int pct = (int)std::lround(level * 100.f);

    // Native qlaunch sizes (OceanFont digits 14x24, % 16x16, IcoBattM 36x20).
    const float s = 1.0f;
    const float dw = 14.f * s, dh = 24.f * s;   // digit cell
    const float pw = 16.f * s, ph = 16.f * s;   // percent glyph
    const float bw = 36.f * s, bh = 20.f * s;   // battery frame
    const float gapPct  = 1.f * s;              // digits -> %
    const float gapBatt = 8.f * s;              // % -> battery

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", pct);
    int n = (int)std::strlen(buf);
    float totalW = n * dw + gapPct + pw + gapBatt + bw;

    float x  = cr.right() - totalW;              // right-aligned (battery on right)
    float cy = cr.y + cr.height * 0.5f;
    const nxui::Color tint = m_textColor.withAlpha(m_textColor.a * op);

    for (int i = 0; i < n; ++i) {
        int d = buf[i] - '0';
        if (d >= 0 && d <= 9)
            ren.drawTexture(&m_assets->digit[d], {x, cy - dh * 0.5f, dw, dh}, tint);
        x += dw;
    }
    x += gapPct;
    ren.drawTexture(&m_assets->percent, {x, cy - ph * 0.5f + 2.f, pw, ph}, tint);
    x += pw + gapBatt;

    // Battery: colored gauge bar inside the frame, then the frame on top.
    nxui::Rect body = {x, cy - bh * 0.5f, bw, bh};
    nxui::Color fill = tint;
    if (level < 0.05f)
        fill = nxui::Color(1.f, 0.f, 0.f, op);                 // red < 5%
    else if (m_charging && level < 1.f)
        fill = nxui::Color(0.42f, 0.87f, 0.29f, op);           // charging green
    // IcoBattM interior cavity is cols 3..29, rows 3..16 of the 36x20 frame.
    // Inset the bar 2 px so it stays isolated from the outline (qlaunch look).
    const float g = 2.f * s;
    float fx = body.x + 3.f * s + g;
    float fy = body.y + 3.f * s + g;
    float fh = 14.f * s - 2.f * g;
    float fmaxW = 27.f * s - 2.f * g;
    float fw = fmaxW * level;
    if (fw > 0.5f)
        ren.drawRect({fx, fy, fw, fh}, fill);
    ren.drawTexture(&m_assets->batteryBody, body, tint);
}

nxui::Vec2 BatteryWidget::computeContentSize() const {
    // 3 digits + % + gap + battery, at native scale.
    return {3 * 14.f + 1.f + 16.f + 8.f + 36.f, 24.f};
}
