#pragma once
#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/Animation.hpp>
#include <algorithm>
#include <string>



class GlossyIcon : public nxui::GlassWidget {
public:
    GlossyIcon();

    void setTitle(const std::string& t) { m_title = t; }
    const std::string& title() const    { return m_title; }

    void setTexture(nxui::Texture* tex) { m_tex = tex; }
    nxui::Texture* texture() const      { return m_tex; }

    void setGameCardTexture(nxui::Texture* tex) { m_gameCardTex = tex; }
    nxui::Texture* gameCardTexture() const      { return m_gameCardTex; }

    void setTitleId(uint64_t id)  { m_titleId = id; }
    uint64_t titleId() const      { return m_titleId; }

    void setSuspended(bool s)     { m_suspended = s; }
    bool isSuspended() const      { return m_suspended; }

    void setIsGameCard(bool gc)     { m_isGameCard = gc; }
    bool isGameCard() const         { return m_isGameCard; }

    void setNotLaunchable(bool nl)  { m_notLaunchable = nl; }
    bool isNotLaunchable() const    { return m_notLaunchable; }

    void startAppear(float delay);
    void forceVisible();

    // App icon shape.
    //   Square   – minimal rounding, like qlaunch (default)
    //   Rounded  – larger rounded corners, original SwitchU style
    //   Circular – full circle
    enum class Shape { Square, Rounded, Circular };
    void setShape(Shape s) { m_shape = s; }
    Shape shape() const    { return m_shape; }
    static Shape shapeFromString(const std::string& s) {
        if (s == "circular") return Shape::Circular;
        if (s == "rounded")  return Shape::Rounded;
        return Shape::Square;
    }
    static const char* shapeToString(Shape s) {
        switch (s) {
            case Shape::Circular: return "circular";
            case Shape::Rounded:  return "rounded";
            case Shape::Square:
            default:              return "square";
        }
    }
    // Radius used by the original SwitchU "Rounded" style (from the theme).
    void setRoundedRadius(float r) { m_roundedRadius = r; }

    // Pulsing focus glow (drop shadow) toggle, and the accent colour used for the
    // 4 px selection gap drawn around the tile when focused.
    void setGlowEnabled(bool e)            { m_glowEnabled = e; }
    void setSelectionColor(const nxui::Color& c) { m_selectionColor = c; }

    // Effective corner radius for a selection cursor framing this icon.
    float cursorRadius(const nxui::Rect& cursorRect) const {
        switch (m_shape) {
            case Shape::Circular: return std::min(cursorRect.width, cursorRect.height) * 0.5f;
            case Shape::Rounded:  return m_roundedRadius + 4.f;
            case Shape::Square:
            default:              return 4.f;   // subtle ~2px curve
        }
    }

    void setFocusable(bool f) { m_focusable = f; }
    bool isFocusable() const override { return m_focusable; }
    void onFocusGained() override;
    void onFocusLost() override;

protected:
    void onRender(nxui::Renderer& ren) override;
    void onContentUpdate(float dt) override;
    void onContentRender(nxui::Renderer& ren) override;

private:
    std::string m_title;
    nxui::Texture*    m_tex = nullptr;
    nxui::Texture*    m_gameCardTex = nullptr;
    uint64_t    m_titleId = 0;
    bool        m_focused = false;
    bool        m_focusable = true;
    Shape       m_shape = Shape::Square;   // qlaunch square (no rounding) by default
    float       m_roundedRadius = 12.f;    // radius for the "Rounded" (SwitchU) style
    bool        m_glowEnabled = false;     // pulsing drop-shadow glow on selection
    nxui::Color m_selectionColor {0.f, 0.f, 0.f, 0.f};   // accent gap colour
    bool        m_suspended = false;
    bool        m_isGameCard = false;
    bool        m_notLaunchable = false;
    float       m_suspendPulse = 0.f;

    nxui::AnimatedFloat m_animScale;
    nxui::AnimatedFloat m_appearOpacity;
    nxui::AnimatedFloat m_focusScale;
    nxui::AnimatedFloat m_focusGlow;
    float         m_appearDelay = 0.f;
    float         m_appearTimer = 0.f;
    bool          m_appearing   = false;
};
