#pragma once
#include <nxui/widgets/Widget.hpp>

struct HudAssets;

// Wi-Fi signal indicator drawn from the real qlaunch IcoWifi_*_32 textures,
// tinted in the theme colour. Signal 0-3 bars; -1 = no/searching; -2 = airplane.
class WifiWidget : public nxui::Widget {
public:
    void setColor(const nxui::Color& c) { m_color = c; }
    void setAssets(const HudAssets* a)  { m_assets = a; }
    void setSignal(int bars)            { m_bars = bars; }

protected:
    void onRender(nxui::Renderer& ren) override;
    void onUpdate(float dt) override;

private:
    void pollSignal();   // queries nifm for the live connection state

    nxui::Color m_color {1.f, 1.f, 1.f, 1.f};
    const HudAssets* m_assets = nullptr;
    int   m_bars = 3;          // 0-3 wifi bars, -1 none/searching, -2 airplane
    float m_pollTimer = 1e9f;  // large -> poll immediately on the first update
};
