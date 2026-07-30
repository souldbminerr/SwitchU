#include "HudAssets.hpp"
#include <cstdio>

void HudAssets::load(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& base) {
    auto L = [&](nxui::Texture& t, const char* name) {
        char p[320];
        std::snprintf(p, sizeof(p), "%s/textures/hud/%s.png", base.c_str(), name);
        t.loadFromFile(gpu, ren, p, 256);
    };
    for (int i = 0; i < 10; ++i) {
        char n[16];
        std::snprintf(n, sizeof(n), "digit_%d", i);
        L(digit[i], n);
    }
    L(colon, "colon");
    L(percent, "percent");
    L(am, "am");
    L(pm, "pm");
    L(mm, "m");
    L(wifi[0], "wifi_0");
    L(wifi[1], "wifi_1");
    L(wifi[2], "wifi_2");
    L(wifi[3], "wifi_3");
    L(wifiNone, "wifi_none");
    L(airplane, "airplane");
    L(batteryBody, "battery_body");
    L(charging, "charging");
    L(userDefault, "user_default");
    loaded = true;
}
