#pragma once
#include <nxui/widgets/Widget.hpp>
#include <nxui/core/Animation.hpp>


class SelectionCursor : public nxui::Widget {
public:
    SelectionCursor();

    void moveTo(const nxui::Rect& target, float duration = 0.2f);
    void moveTo(const nxui::Rect& target, float cornerRadius, float duration);
    // Track a moving target (e.g. list scrolling) without triggering the
    // fade-out/in transition, so the highlight follows the row smoothly.
    void follow(const nxui::Rect& target, float cornerRadius);
    nxui::Rect currentRect() const;
    void setColor(const nxui::Color& c) { m_color = c; }
    void setCornerRadius(float r)  { m_cornerRadius.setImmediate(r); }
    void setBorderWidth(float w)   { m_borderWidth = w; }

protected:
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;

private:
    float computeAdaptiveDuration(const nxui::Rect& target, float targetCornerRadius, float baseDuration) const;

    nxui::AnimatedFloat m_x, m_y, m_w, m_h;
    nxui::AnimatedFloat m_cornerRadius;
    nxui::Color m_color {0.f, 0.75f, 1.f, 1.f};
    float m_borderWidth  = 4.f;   // selection outline thickness
    float m_time = 0.f;
    float m_waveSpeed = 3.5f;
    bool  m_initialized = false;

    // qlaunch-style transition: the highlight does not slide. When the target
    // changes it fades out at the old spot then snaps + fades in at the new one.
    enum class Fade { Idle, Out, In };
    Fade  m_fadePhase = Fade::Idle;
    float m_fade = 1.f;
    float m_pendingX = 0.f, m_pendingY = 0.f, m_pendingW = 0.f, m_pendingH = 0.f;
    float m_pendingR = 18.f;
    bool  m_hasPending = false;
};

