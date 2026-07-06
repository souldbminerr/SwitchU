#pragma once
#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/core/Types.hpp>
#include <string>



class AppletButton : public nxui::GlassWidget {
public:
    AppletButton();
    ~AppletButton() override;

    void setIcon(nxui::Texture* tex)        { m_icon = tex; }
    void setIconCircular(bool enabled)      { m_iconCircular = enabled; }
    void setIconTint(const nxui::Color& c)  { m_iconTint = c; }
    nxui::Texture* icon() const             { return m_icon; }

    // Activation animation hooks (used by the Settings gear open sequence).
    void setIconRotation(float radians)     { m_iconRotation = radians; }
    void setShakeOffset(const nxui::Vec2& o){ m_shakeOffset = o; }

    void setLabel(const std::string& l);
    void setLabelKey(const std::string& key, const std::string& fallback = "");
    const std::string& label() const    { return m_label; }

    bool hitTest(float sx, float sy) const { return m_rect.contains(sx, sy); }

protected:
    void onRender(nxui::Renderer& ren) override;
    void onContentRender(nxui::Renderer& ren) override;

private:
    void refreshLocalizedLabel();

    nxui::Texture* m_icon = nullptr;
    bool           m_iconCircular = false;
    float          m_iconRotation = 0.f;          // radians, gear spin
    nxui::Vec2     m_shakeOffset { 0.f, 0.f };     // whole-button shake
    nxui::Color    m_iconTint = nxui::Color(1.f, 1.f, 1.f, 1.f);
    std::string    m_label;
    std::string    m_labelSource;
    std::string    m_labelKey;
    std::string    m_labelFallback;
    bool           m_useLabelKey = false;
    int            m_i18nListenerId = -1;
};

