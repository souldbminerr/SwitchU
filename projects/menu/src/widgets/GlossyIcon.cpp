#include "GlossyIcon.hpp"
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>
#include <algorithm>
#include <cmath>


GlossyIcon::GlossyIcon() {
    m_animScale.setImmediate(0.f);
    m_appearOpacity.setImmediate(0.f);
    m_focusScale.setImmediate(1.f);
    m_focusGlow.setImmediate(0.f);
    // Switch-style flat tile: no liquid-glass gloss, slight rounding, artwork
    // fills the tile. The flat base colour shows through only for empty slots.
    setCornerRadius(12.f);
    setPadding(4.f);
    setLiquidGlassEnabled(false);
    setBlurEnabled(false);
}

void GlossyIcon::onFocusGained() {
    m_focused = true;
    m_focusScale.set(m_expandOnSelect ? 1.075f : 1.f, 0.18f, nxui::Easing::outBack);
    // Snap the accent frame to full instantly. A timed fade-in is shorter than a
    // held-scroll key repeat, so the frame would never finish fading before focus
    // jumps on -> the tile flickers dark->blue mid-scroll. Snapping keeps the
    // selected tile solidly framed and ensures only one tile is ever highlighted.
    m_focusGlow.setImmediate(1.f);
}

void GlossyIcon::onFocusLost() {
    m_focused = false;
    m_focusScale.set(1.f, 0.20f, nxui::Easing::outCubic);
    m_focusGlow.setImmediate(0.f);
}

void GlossyIcon::startAppear(float delay) {
    m_appearDelay = delay;
    m_appearTimer = 0.f;
    m_appearing   = true;
    m_animScale.setImmediate(0.f);
    m_appearOpacity.setImmediate(0.f);
}

void GlossyIcon::forceVisible() {
    m_appearing = false;
    m_appearDelay = 0.f;
    m_appearTimer = 0.f;
    m_animScale.setImmediate(1.f);
    m_appearOpacity.setImmediate(1.f);
}

void GlossyIcon::onContentUpdate(float dt) {
    if (m_appearing) {
        m_appearTimer += dt;
        if (m_appearTimer >= m_appearDelay) {
            m_appearing = false;
            m_animScale.set(1.f, 0.4f, nxui::Easing::outExpo);
            m_appearOpacity.set(1.f, 0.3f, nxui::Easing::outExpo);
        }
    }
    m_suspendPulse += dt * 2.2f;
}

void GlossyIcon::onRender(nxui::Renderer& ren) {
    float externalScale = scale();
    float focusS = m_focusScale.value();
    float s = m_animScale.value() * externalScale * focusS;
    float a = m_appearOpacity.value();
    if (s < 0.01f || a < 0.01f) return;

    nxui::Rect savedRect = m_rect;
    nxui::Rect drawRect = savedRect;
    if (std::abs(s - 1.f) > 0.001f) {
        float w = savedRect.width * s;
        float h = savedRect.height * s;
        drawRect.x += (savedRect.width - w) * 0.5f;
        drawRect.y += (savedRect.height - h) * 0.5f;
        drawRect.width = w;
        drawRect.height = h;
        m_rect = drawRect;
    }
    // Corner radius by shape: Square = sharp (qlaunch), Rounded = SwitchU style,
    // Circular = half the (scaled) tile so it renders as a disc. cornerRadius()
    // flows through to onContentRender() below.
    float shapeRad = 0.f;
    switch (m_shape) {
        case Shape::Circular: shapeRad = std::min(drawRect.width, drawRect.height) * 0.5f; break;
        case Shape::Rounded:  shapeRad = m_roundedRadius; break;
        case Shape::Square:
        default:              shapeRad = 0.f; break;
    }
    setCornerRadius(shapeRad);
    setScale(1.f);
    float savedShade = liquidGlassShade();
    setLiquidGlassShade(m_notLaunchable ? 0.58f : 0.0f);

    float savedOp = m_opacity;
    m_opacity = a * savedOp;

    nxui::GlassWidget::onRender(ren);

    m_opacity = savedOp;
    m_rect = savedRect;
    setLiquidGlassShade(savedShade);
    setScale(externalScale);

    nxui::Rect r = drawRect;
    float rad = cornerRadius();

    // Empty slots get a 4 px border (2 px each side -> inner content 252 in a
    // 256 tile) so the empty tile reads as an outlined placeholder.
    if (m_titleId == 0 && m_emptyBorderColor.a > 0.f && s > 0.5f) {
        ren.drawRoundedRectOutline(r.shrunk(1.f),
                                   m_emptyBorderColor.withAlpha(m_emptyBorderColor.a * a),
                                   std::max(0.f, rad - 1.f), 2.f);
    }

    // Accent selection frame around the selected tile (the whole game selection
    // indicator). Sized 4 px larger than the tile so it clears the empty border;
    // fades with focus.
    float focusGlow = m_focusGlow.value();
    if (focusGlow > 0.01f && m_selectionColor.a > 0.f && s > 0.5f) {
        ren.drawRoundedRectOutline(r.expanded(4.f),
                                   m_selectionColor.withAlpha(m_selectionColor.a * a * focusGlow),
                                   rad + 4.f, 4.f);
    }

    // Optional pulsing drop-shadow glow (config-gated; off by default like qlaunch).
    if (m_glowEnabled && focusGlow > 0.01f && s > 0.5f) {
        float breathe = 0.5f + 0.5f * std::sin(m_suspendPulse * 1.8f + 0.4f);
        nxui::Color glowBase = (m_selectionColor.a > 0.f)
            ? m_selectionColor : nxui::Color(0.65f, 0.90f, 1.f, 1.f);
        nxui::Color focusColor = glowBase.withAlpha((0.10f + 0.06f * breathe) * focusGlow * a);
        ren.drawRoundedRect(r.expanded(10.f * focusGlow), focusColor, rad + 10.f);
    }

    // Game-card indicator is shown only in the selected-item title, never on
    // the tile itself.

    if (m_suspended && s > 0.5f) {
        float pulse = 0.5f + 0.5f * std::sin(m_suspendPulse);
        float glowAlpha = 0.35f + 0.25f * pulse;

        nxui::Color glow(0.18f, 0.85f, 0.45f, glowAlpha * a);
        ren.drawRoundedRectOutline(r.expanded(2.f), glow, rad + 2.f, 2.5f);

        float badgeSize = 26.f * s;
        float badgeX = r.x + r.width  - badgeSize - 4.f * s;
        float badgeY = r.y + r.height - badgeSize - 4.f * s;

        nxui::Vec2 badgeCenter = { badgeX + badgeSize * 0.5f, badgeY + badgeSize * 0.5f };
        ren.drawCircle(badgeCenter, badgeSize * 0.5f,
                       nxui::Color(0.1f, 0.1f, 0.1f, 0.85f * a), 16);

        float triH = badgeSize * 0.45f;
        float triW = triH * 0.85f;
        nxui::Vec2 p1 = { badgeCenter.x - triW * 0.35f, badgeCenter.y - triH * 0.5f };
        nxui::Vec2 p2 = { badgeCenter.x - triW * 0.35f, badgeCenter.y + triH * 0.5f };
        nxui::Vec2 p3 = { badgeCenter.x + triW * 0.65f, badgeCenter.y };
        ren.drawTriangle(p1, p2, p3, nxui::Color(0.18f, 0.85f, 0.45f, 0.95f * a));
    }
}

void GlossyIcon::onContentRender(nxui::Renderer& ren) {
    if (!m_tex || !m_tex->valid()) return;

    float s = scale();
    float rad = cornerRadius();

    nxui::Rect r = m_rect;
    if (s < 1.f) {
        float w = r.width  * s;
        float h = r.height * s;
        r.x += (r.width  - w) * 0.5f;
        r.y += (r.height - h) * 0.5f;
        r.width  = w;
        r.height = h;
    }

    float inset = 3.f * s;
    nxui::Rect texRect = r.shrunk(inset);
    nxui::Color iconTint = nxui::Color::white().withAlpha(m_opacity);
    if (m_notLaunchable) {
        iconTint.r = 0.80f;
        iconTint.g = 0.80f;
        iconTint.b = 0.80f;
    }
    ren.drawTextureRounded(m_tex, texRect, std::max(0.f, rad - 3.f), iconTint);
}
