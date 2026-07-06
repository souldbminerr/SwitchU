#include "SelectionTitleWidget.hpp"
#include <nxui/core/Renderer.hpp>
#include <algorithm>

namespace {
constexpr float kMaxWidth       = 496.f;   // marquee window
constexpr float kGameGap        = 14.f;    // text -> selection outline
constexpr float kBubbleGap      = 10.f;    // bubble bottom -> text
constexpr float kGamecardGap    = 12.f;    // gamecard -> text left edge
constexpr float kEdgeFade       = 24.f;    // faded edge width
constexpr float kFadeSpeed      = 30.f;    // ~2 frames (1 / (2/60 s))
constexpr float kMarqueeSpeed   = 36.f;    // 3 px / 5 frames  (0.6 px @60fps)
constexpr float kMarqueePause   = 5.0f;    // seconds
}

void SelectionTitleWidget::requestContent(Mode mode, const std::string& text,
                                          nxui::Texture* gamecard,
                                          const nxui::Rect& anchor, float outlineOffset) {
    // Same content as shown and not mid-swap -> just track the anchor live.
    if (!m_hasPending && mode == m_mode && text == m_text && gamecard == m_gamecard) {
        m_anchor = anchor;
        m_outlineOffset = outlineOffset;
        return;
    }
    // Already fading toward this target -> keep its pending anchor fresh.
    if (m_hasPending && mode == m_pendingMode && text == m_pendingText &&
        gamecard == m_pendingGamecard) {
        m_pendingAnchor = anchor;
        m_pendingOutlineOffset = outlineOffset;
        return;
    }

    if (m_mode == Mode::None && !m_hasPending && m_alpha <= 0.01f && mode != Mode::None) {
        // Nothing on screen -> show immediately with a fade-in.
        m_mode = mode; m_text = text; m_gamecard = gamecard;
        m_anchor = anchor; m_outlineOffset = outlineOffset;
        m_marqueeOffset = 0.f; m_marqueeTimer = 0.f; m_marqueePhase = 0;
        m_fade = Fade::In;
        return;
    }
    // Fade the current content out (staying on its old anchor), then swap.
    m_pendingMode = mode; m_pendingText = text; m_pendingGamecard = gamecard;
    m_pendingAnchor = anchor; m_pendingOutlineOffset = outlineOffset;
    m_hasPending = true;
    m_fade = Fade::Out;
}

void SelectionTitleWidget::showGame(const std::string& text, const nxui::Rect& iconRect,
                                    float outlineOffset, nxui::Texture* gamecard) {
    setVisible(true);
    requestContent(Mode::Game, text, gamecard, iconRect, outlineOffset);
}

void SelectionTitleWidget::showBubble(const std::string& text, const nxui::Rect& bubbleRect) {
    setVisible(true);
    requestContent(Mode::Bubble, text, nullptr, bubbleRect, 0.f);
}

void SelectionTitleWidget::hide() {
    requestContent(Mode::None, std::string(), nullptr, m_anchor, m_outlineOffset);
}

void SelectionTitleWidget::onUpdate(float dt) {
    // Fade transition.
    if (m_fade == Fade::Out) {
        m_alpha -= kFadeSpeed * dt;
        if (m_alpha <= 0.f) {
            m_alpha = 0.f;
            if (m_hasPending) {
                m_mode = m_pendingMode; m_text = m_pendingText; m_gamecard = m_pendingGamecard;
                m_anchor = m_pendingAnchor; m_outlineOffset = m_pendingOutlineOffset;
                m_hasPending = false;
                m_marqueeOffset = 0.f; m_marqueeTimer = 0.f; m_marqueePhase = 0;
            }
            m_fade = (m_mode == Mode::None) ? Fade::Idle : Fade::In;
        }
    } else if (m_fade == Fade::In) {
        m_alpha += kFadeSpeed * dt;
        if (m_alpha >= 1.f) { m_alpha = 1.f; m_fade = Fade::Idle; }
    }

    // Marquee (only when a game title is wider than the window).
    if (m_mode == Mode::Game && m_font) {
        float textW = m_font->measure(m_text).x;
        float maxOffset = textW - kMaxWidth;
        if (maxOffset > 0.5f) {
            if (m_marqueePhase == 0) {                 // pause at start
                m_marqueeTimer += dt;
                if (m_marqueeTimer >= kMarqueePause) { m_marqueePhase = 1; m_marqueeTimer = 0.f; }
            } else if (m_marqueePhase == 1) {          // scroll left
                m_marqueeOffset += kMarqueeSpeed * dt;
                if (m_marqueeOffset >= maxOffset) { m_marqueeOffset = maxOffset; m_marqueePhase = 2; m_marqueeTimer = 0.f; }
            } else {                                   // pause at end, then revert
                m_marqueeTimer += dt;
                if (m_marqueeTimer >= kMarqueePause) { m_marqueePhase = 0; m_marqueeOffset = 0.f; m_marqueeTimer = 0.f; }
            }
        } else {
            m_marqueeOffset = 0.f;
        }
    }
}

void SelectionTitleWidget::onRender(nxui::Renderer& ren) {
    if (m_mode == Mode::None || m_alpha <= 0.01f || !m_font) return;

    const float a = m_alpha;
    const nxui::Color textCol = m_textColor.withAlpha(m_textColor.a * a);
    nxui::Vec2 ts = m_font->measure(m_text);
    const float textW = ts.x, textH = ts.y;
    const float centerX = m_anchor.x + m_anchor.width * 0.5f;

    if (m_mode == Mode::Bubble) {
        float x = centerX - textW * 0.5f;
        float y = m_anchor.y + m_anchor.height + kBubbleGap;
        ren.drawText(m_text, {x, y}, m_font, textCol);
        return;
    }

    // ---- Game ----
    float outlineTop = m_anchor.y - m_outlineOffset;
    float textBottom = outlineTop - kGameGap;
    float textY = textBottom - textH;

    auto drawGamecard = [&](float leftEdge) {
        if (!m_gamecard || !m_gamecard->valid()) return;
        float gh = textH;
        float tw = (float)m_gamecard->width(), th = (float)m_gamecard->height();
        float gw = gh;
        if (tw > 0.f && th > 0.f) gw = gh * (tw / th);
        nxui::Rect r = {leftEdge - kGamecardGap - gw, textY + (textH - gh) * 0.5f, gw, gh};
        ren.drawTexture(m_gamecard, r, nxui::Color::white().withAlpha(a));
    };

    if (textW <= kMaxWidth) {
        float x = centerX - textW * 0.5f;
        ren.drawText(m_text, {x, textY}, m_font, textCol);
        drawGamecard(x);
        return;
    }

    // Marquee: 496 px window centred on the tile, text scrolls left, edges faded.
    float winLeft = centerX - kMaxWidth * 0.5f;
    nxui::Rect clip = {winLeft, textY - 4.f, kMaxWidth, textH + 8.f};
    ren.pushClipRect(clip);
    ren.drawText(m_text, {winLeft - m_marqueeOffset, textY}, m_font, textCol);
    ren.popClipRect();

    // Faded edges (blend into the background colour).
    const int slices = 16;
    for (int i = 0; i < slices; ++i) {
        float f = (float)i / slices;
        float sw = kEdgeFade / slices + 0.5f;
        ren.drawRect({winLeft + kEdgeFade * f, clip.y, sw, clip.height},
                     m_bgColor.withAlpha(m_bgColor.a * (1.f - f) * a));
        ren.drawRect({winLeft + kMaxWidth - kEdgeFade + kEdgeFade * f, clip.y, sw, clip.height},
                     m_bgColor.withAlpha(m_bgColor.a * f * a));
    }

    drawGamecard(winLeft);
}
