#include "WifiWidget.hpp"
#include "HudAssets.hpp"
#include <nxui/core/Renderer.hpp>
#include <switch.h>
#include <algorithm>

void WifiWidget::onUpdate(float dt) {
    m_pollTimer += dt;
    if (m_pollTimer < 2.0f) return;   // refresh the live status ~every 2 s
    m_pollTimer = 0.f;
    pollSignal();
}

void WifiWidget::pollSignal() {
    // Scoped nifm query (the UI is single-threaded, so this can't overlap the
    // settings tab's own scoped nifm usage).
    if (R_FAILED(nifmInitialize(NifmServiceType_User)))
        return;

    NifmInternetConnectionType type = (NifmInternetConnectionType)0;
    u32 strength = 0;
    NifmInternetConnectionStatus status = (NifmInternetConnectionStatus)0;
    Result rc = nifmGetInternetConnectionStatus(&type, &strength, &status);
    nifmExit();

    if (R_FAILED(rc)) { m_bars = -1; return; }

    switch (type) {
        case NifmInternetConnectionType_WiFi:
            m_bars = std::clamp((int)strength, 0, 3);
            break;
        case NifmInternetConnectionType_Ethernet:
            m_bars = 3;      // wired: show full signal
            break;
        default:
            m_bars = -1;     // no connection
            break;
    }
}

void WifiWidget::onRender(nxui::Renderer& ren) {
    if (!m_visible || m_opacity <= 0.f || !m_assets || !m_assets->loaded) return;

    const nxui::Texture* tex = nullptr;
    if (m_bars <= -2)      tex = &m_assets->airplane;
    else if (m_bars <= -1) tex = &m_assets->wifiNone;
    else                   tex = &m_assets->wifi[std::clamp(m_bars, 0, 3)];
    if (!tex || !tex->valid()) return;

    // qlaunch N_Signal is a 32 px icon; draw it native-size, centred in the band.
    const float side = 32.f;
    float cx = m_rect.x + m_rect.width * 0.5f;
    float cy = m_rect.y + m_rect.height * 0.5f;
    nxui::Rect dst = {cx - side * 0.5f, cy - side * 0.5f, side, side};
    ren.drawTexture(tex, dst, m_color.withAlpha(m_color.a * m_opacity));
}
