#include "AppletButton.hpp"
#include <nxui/core/Renderer.hpp>
#include <algorithm>


AppletButton::AppletButton() {
    // Switch-style dock button: a subtle solid grey circle with a coloured
    // glyph on top. Selection is shown by the SelectionCursor ring.
    setCornerRadius(999.f);
    setPadding(14.f);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setBlurEnabled(false);
    setBorderWidth(0.f);
    setBaseColor(nxui::Color(0.235f, 0.235f, 0.255f, 1.f));
    setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    m_iconCircular = false;
    m_i18nListenerId = nxui::I18n::instance().addLanguageChangedListener([this]() {
        refreshLocalizedLabel();
    });
}

AppletButton::~AppletButton() {
    nxui::I18n::instance().removeLanguageChangedListener(m_i18nListenerId);
}

void AppletButton::setLabel(const std::string& l) {
    m_useLabelKey = false;
    m_labelSource = l;
    refreshLocalizedLabel();
}

void AppletButton::setLabelKey(const std::string& key, const std::string& fallback) {
    m_useLabelKey = true;
    m_labelKey = key;
    m_labelFallback = fallback.empty() ? key : fallback;
    refreshLocalizedLabel();
}

void AppletButton::refreshLocalizedLabel() {
    if (m_useLabelKey)
        m_label = nxui::I18n::instance().tr(m_labelKey, m_labelFallback);
    else
        m_label = nxui::I18n::instance().tr(m_labelSource, m_labelSource);
    setAccessibilityLabel(m_label);
}

void AppletButton::onRender(nxui::Renderer& ren) {
    // Apply the shake offset to the whole button (circle + glyph) by nudging the
    // rect during the base render, then restoring it.
    if (m_shakeOffset.x != 0.f || m_shakeOffset.y != 0.f) {
        nxui::Rect saved = m_rect;
        m_rect.x += m_shakeOffset.x;
        m_rect.y += m_shakeOffset.y;
        nxui::GlassWidget::onRender(ren);
        m_rect = saved;
    } else {
        nxui::GlassWidget::onRender(ren);
    }
}

void AppletButton::onContentRender(nxui::Renderer& ren) {
    if (!m_icon || !m_icon->valid()) return;

    const float texW = (float)m_icon->width();
    const float texH = (float)m_icon->height();
    if (texW <= 0.f || texH <= 0.f) return;

    // qlaunch dock glyphs are a fixed 40x40 px, centred on the bubble (aspect
    // preserved, so tall glyphs like power/news fit within a 40 px box).
    constexpr float kIconSize = 40.f;
    float s = kIconSize / std::max(texW, texH);
    float dw = texW * s;
    float dh = texH * s;
    float ix = m_rect.x + (m_rect.width  - dw) * 0.5f;
    float iy = m_rect.y + (m_rect.height - dh) * 0.5f;

    const nxui::Color tint = m_iconTint.withAlpha(m_iconTint.a * m_opacity);
    if (m_iconRotation != 0.f) {
        ren.drawTextureRotated(m_icon, {ix, iy, dw, dh}, m_iconRotation, tint);
    } else {
        float iconCorner = m_iconCircular ? (std::min(dw, dh) * 0.5f) : 0.f;
        ren.drawTextureRounded(m_icon, {ix, iy, dw, dh}, iconCorner, tint);
    }
}
