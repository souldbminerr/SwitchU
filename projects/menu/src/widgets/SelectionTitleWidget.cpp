#include "SelectionTitleWidget.hpp"
#include <nxui/core/Renderer.hpp>
#include <algorithm>

namespace {
constexpr float kMaxWidth       = 496.f;   // marquee window
constexpr float kGameGap        = 14.f;    // text -> selection outline
constexpr float kBubbleGap      = 10.f;    // bubble bottom -> text
constexpr float kGamecardGap    = 12.f;    // gamecard -> text left edge
constexpr float kEdgeFade       = 24.f;    // faded edge width
constexpr float kScreenEdge     = 25.f;    // clear zone at each screen edge
constexpr float kFadeSpeed      = 30.f;    // ~2 frames (1 / (2/60 s))
constexpr float kMarqueeSpeed   = 36.f;    // 3 px / 5 frames  (0.6 px @60fps)
constexpr float kMarqueePause   = 2.0f;    // seconds between cycles
constexpr float kMarqueeTab     = 60.f;    // gap between wrapped repetitions
constexpr float kTitleScale     = 1.2f;    // game title font, one bracket larger
}

void SelectionTitleWidget::requestContent(Mode mode, const std::string& text,
                                          nxui::Texture* gamecard, bool gamecardTinted,
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
        m_mode = mode; m_text = text; m_gamecard = gamecard; m_gamecardTinted = gamecardTinted;
        m_anchor = anchor; m_outlineOffset = outlineOffset;
        m_marqueeOffset = 0.f; m_marqueeTimer = 0.f; m_marqueePhase = 0;
        m_fade = Fade::In;
        return;
    }
    // Fade the current content out (staying on its old anchor), then swap.
    m_pendingMode = mode; m_pendingText = text; m_pendingGamecard = gamecard;
    m_pendingGamecardTinted = gamecardTinted;
    m_pendingAnchor = anchor; m_pendingOutlineOffset = outlineOffset;
    m_hasPending = true;
    m_fade = Fade::Out;
}

void SelectionTitleWidget::showGame(const std::string& text, const nxui::Rect& iconRect,
                                    float outlineOffset, nxui::Texture* gamecard,
                                    bool gamecardTinted) {
    setVisible(true);
    requestContent(Mode::Game, text, gamecard, gamecardTinted, iconRect, outlineOffset);
}

void SelectionTitleWidget::showBubble(const std::string& text, const nxui::Rect& bubbleRect) {
    setVisible(true);
    requestContent(Mode::Bubble, text, nullptr, true, bubbleRect, 0.f);
}

void SelectionTitleWidget::hide() {
    requestContent(Mode::None, std::string(), nullptr, true, m_anchor, m_outlineOffset);
}

void SelectionTitleWidget::onUpdate(float dt) {
    // Fade transition.
    if (m_fade == Fade::Out) {
        m_alpha -= kFadeSpeed * dt;
        if (m_alpha <= 0.f) {
            m_alpha = 0.f;
            if (m_hasPending) {
                m_mode = m_pendingMode; m_text = m_pendingText; m_gamecard = m_pendingGamecard;
                m_gamecardTinted = m_pendingGamecardTinted;
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

    // Marquee (only when a game title is wider than the window). Continuous
    // wrap: pause 2 s with the first characters shown, then scroll left until the
    // text (plus a tab gap) has looped fully back to the start, and repeat.
    if (m_mode == Mode::Game && m_font) {
        float textW = m_font->measure(m_text).x * kTitleScale;
        if (textW > kMaxWidth + 0.5f) {
            float loop = textW + kMarqueeTab;
            if (m_marqueePhase == 0) {                 // pause at start
                m_marqueeTimer += dt;
                if (m_marqueeTimer >= kMarqueePause) { m_marqueePhase = 1; m_marqueeTimer = 0.f; }
            } else {                                   // scroll left, wrap seamlessly
                m_marqueeOffset += kMarqueeSpeed * dt;
                if (m_marqueeOffset >= loop) {
                    m_marqueeOffset = 0.f;
                    m_marqueePhase = 0;
                    m_marqueeTimer = 0.f;
                }
            }
        } else {
            m_marqueeOffset = 0.f; m_marqueePhase = 0; m_marqueeTimer = 0.f;
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

    // ---- Game ----  (title rendered one bracket larger via kTitleScale)
    const float gScale = kTitleScale;
    const float gW = ts.x * gScale;
    const float gH = ts.y * gScale;
    float outlineTop = m_anchor.y - m_outlineOffset;
    float textBottom = outlineTop - kGameGap;
    float textY = textBottom - gH;

    const float screenW = (float)ren.width();

    // Game-card glyph width, and the left padding that reserves room for it so it
    // never clips off the screen edge (the text always starts to its right).
    float gcW = 0.f;
    if (m_gamecard && m_gamecard->valid()) {
        float tw = (float)m_gamecard->width(), th = (float)m_gamecard->height();
        gcW = (th > 0.f) ? gH * (tw / th) : gH;
    }
    const float leftPad = kScreenEdge + (gcW > 0.f ? gcW + kGamecardGap : 0.f);

    auto drawGamecard = [&](float textLeft) {
        if (gcW <= 0.f) return;
        nxui::Rect r = {textLeft - kGamecardGap - gcW, textY, gcW, gH};
        // Inserted card -> natural colours; "no card" indicator -> title text colour.
        ren.drawTexture(m_gamecard, r,
                        m_gamecardTinted ? textCol : nxui::Color::white().withAlpha(a));
    };

    if (gW <= kMaxWidth) {
        // Centre on the tile, but keep the (card + text) group on-screen with
        // kScreenEdge padding on the card side and the text side.
        float x = std::clamp(centerX - gW * 0.5f, leftPad,
                             std::max(leftPad, screenW - kScreenEdge - gW));
        ren.drawText(m_text, {x, textY}, m_font, textCol, gScale);
        drawGamecard(x);
        return;
    }

    // Marquee: window centred on the tile but kept fully on-screen. The text
    // rests exactly kScreenEdge px from the screen edge (the requested padding);
    // the left fade band sits just *outside* that, between the edge and the text,
    // so the first characters stay crisp while text scrolling off still fades.
    float winLeft = std::clamp(centerX - kMaxWidth * 0.5f, leftPad,
                               std::max(leftPad, screenW - kScreenEdge - kMaxWidth));
    float winRight = winLeft + kMaxWidth;
    float restX = winLeft;                             // text padding = winLeft; card sits left of it
    float clipLeft = winLeft - kEdgeFade;              // include the left fade band
    nxui::Rect clip = {clipLeft, textY - 4.f, winRight - clipLeft, gH + 8.f};

    ren.pushClipRect(clip);
    float baseX = restX - m_marqueeOffset;
    ren.drawText(m_text, {baseX, textY}, m_font, textCol, gScale);
    ren.drawText(m_text, {baseX + gW + kMarqueeTab, textY}, m_font, textCol, gScale);
    ren.popClipRect();

    // Left fade [winLeft-kEdgeFade .. winLeft] (bg outer -> clear at the text);
    // right fade [winRight-kEdgeFade .. winRight].
    const int slices = 16;
    for (int i = 0; i < slices; ++i) {
        float f = (float)i / slices;
        float sw = kEdgeFade / slices + 0.5f;
        ren.drawRect({winLeft - kEdgeFade + kEdgeFade * f, clip.y, sw, clip.height},
                     m_bgColor.withAlpha(m_bgColor.a * (1.f - f) * a));
        ren.drawRect({winRight - kEdgeFade + kEdgeFade * f, clip.y, sw, clip.height},
                     m_bgColor.withAlpha(m_bgColor.a * f * a));
    }

    drawGamecard(restX);
}
