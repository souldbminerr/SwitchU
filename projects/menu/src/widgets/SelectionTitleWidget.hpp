#pragma once
#include <nxui/widgets/Widget.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Texture.hpp>
#include <string>

// Selected-item title shown on the home screen. Two layouts:
//   Game   – centred on the tile, 14 px above the selection outline, clamped to a
//            496 px window with a left-scrolling marquee (faded edges) for long
//            titles; an optional game-card glyph sits to the left of the text.
//   Bubble – centred under a dock bubble / avatar with a 10 px gap, never scrolls.
// Changing the shown item does a fast 2-frame fade-out / fade-in.
class SelectionTitleWidget : public nxui::Widget {
public:
    enum class Mode { None, Game, Bubble };

    void setFont(nxui::Font* f)                { m_font = f; }
    void setTextColor(const nxui::Color& c)    { m_textColor = c; }
    void setBackgroundColor(const nxui::Color& c) { m_bgColor = c; }

    // Called every frame with the current selection. Position/anchor updates
    // immediately; changing the text/mode/game-card triggers the fade.
    // gamecardTinted: true tints the glyph in the text colour (the "no card
    // inserted" indicator); false draws it in its natural colours (an inserted
    // game card, so it isn't recoloured blue).
    void showGame(const std::string& text, const nxui::Rect& iconRect,
                  float outlineOffset, nxui::Texture* gamecard,
                  bool gamecardTinted = true);
    void showBubble(const std::string& text, const nxui::Rect& bubbleRect);
    void hide();

protected:
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;

private:
    void requestContent(Mode mode, const std::string& text, nxui::Texture* gamecard,
                        bool gamecardTinted, const nxui::Rect& anchor, float outlineOffset);
    float textScale() const { return 1.0f; }

    nxui::Font*  m_font = nullptr;
    nxui::Color  m_textColor {1.f, 1.f, 1.f, 1.f};
    nxui::Color  m_bgColor   {0.f, 0.f, 0.f, 1.f};

    // Currently-shown content and the pending (post-fade) content.
    Mode         m_mode = Mode::None;
    std::string  m_text;
    nxui::Texture* m_gamecard = nullptr;
    bool         m_gamecardTinted = true;

    Mode         m_pendingMode = Mode::None;
    std::string  m_pendingText;
    nxui::Texture* m_pendingGamecard = nullptr;
    bool         m_pendingGamecardTinted = true;
    bool         m_hasPending = false;

    // Anchor (updates live so the title tracks a scrolling tile). While fading
    // out the old title, m_anchor stays on the old tile; the pending anchor is
    // applied when the new title is swapped in.
    nxui::Rect   m_anchor {0.f, 0.f, 0.f, 0.f};
    float        m_outlineOffset = 4.f;
    nxui::Rect   m_pendingAnchor {0.f, 0.f, 0.f, 0.f};
    float        m_pendingOutlineOffset = 4.f;

    // Fade.
    enum class Fade { Idle, Out, In };
    Fade         m_fade = Fade::Idle;
    float        m_alpha = 0.f;

    // Marquee.
    float        m_marqueeOffset = 0.f;
    float        m_marqueeTimer = 0.f;
    int          m_marqueePhase = 0;   // 0 = pause at start, 1 = scrolling, 2 = pause at end
};
