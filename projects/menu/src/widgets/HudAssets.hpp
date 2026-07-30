#pragma once
#include <nxui/core/Texture.hpp>
#include <string>

namespace nxui { class GpuDevice; class Renderer; }

// The real qlaunch HUD glyphs/icons, extracted 1:1 from ResidentMenu.szs'
// __Combined.bntx (OceanFont digits + Ico* icons). All are white alpha masks
// meant to be tinted with the theme colour when drawn. Native pixel sizes:
//   digit  14x24   colon 8x28   percent 16x16   am/pm 16x16
//   wifi   32x32   batteryBody 36x20   charging 22x22   userDefault 128x128
struct HudAssets {
    nxui::Texture digit[10];
    nxui::Texture colon, percent, am, pm, mm;   // mm = shared "M" for AM/PM
    nxui::Texture wifi[4], wifiNone, airplane;
    nxui::Texture batteryBody, charging, userDefault;
    bool loaded = false;

    // base = SD asset root (e.g. "romfs:"); files live under /textures/hud/.
    void load(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& base);
};
