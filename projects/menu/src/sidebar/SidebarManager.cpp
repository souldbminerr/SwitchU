#include "SidebarManager.hpp"
#include "core/DebugLog.hpp"
#include <nxui/core/I18n.hpp>
#include <filesystem>
#include <system_error>

namespace {

constexpr int kSidebarIconCount = 6;

bool pathExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string joinPath(const std::string& base, const std::string& name) {
    if (base.empty())
        return name;
    if (base.back() == '/')
        return base + name;
    return base + "/" + name;
}

} // namespace


void SidebarManager::build(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                           const std::string& assetsBase,
                           const Actions& actions) {
    // Switch-style applet dock: a single horizontal row of circular buttons,
    // centred horizontally just below the game tile strip.
    // qlaunch applet dock: 90 px buttons, bottom edge 130 px above the screen
    // bottom (top = 720 - 130 - 90 = 500).
    constexpr float btnSize   = 90.f;
    constexpr float gap       = 38.f;
    constexpr int   dockCount = 5;
    constexpr float dockY     = 500.f;
    const float dockW      = dockCount * btnSize + (dockCount - 1) * gap;
    const float dockStartX = (1280.f - dockW) * 0.5f;
    auto slotRect = [&](int i) -> nxui::Rect {
        return {dockStartX + i * (btnSize + gap), dockY, btnSize, btnSize};
    };

    m_leftButtons.clear();
    m_rightButtons.clear();
    m_settingsButton = nullptr;
    m_themeShopButton = nullptr;
    m_albumButton    = nullptr;
    m_anims.clear();
    m_icons.clear();
    m_icons.resize(kSidebarIconCount);
    invalidateAssetsCache();

    auto makeBtn = [](nxui::Texture* tex, const std::string& labelKey,
                      const std::string& fallback, std::function<void()> action) {
        auto btn = std::make_shared<AppletButton>();
        btn->setIcon(tex);
        btn->setLabelKey(labelKey, fallback);
        btn->setAccessibilityLabel(fallback);
        btn->setAccessibilityRole(nxui::I18n::instance().tr("accessibility.roles.button", "button"));
        btn->setAccessibilityHint(nxui::I18n::instance().tr("accessibility.hints.open", "A to open."));
        btn->setOnActivate(std::move(action));
        btn->setFocusable(true);
        return btn;
    };

    // Dock slots left-to-right: News, Controllers, Album, Settings, Power.
    // (Theme Shop removed from the dock.) Buttons keep their original left/right
    // vector grouping; only their screen position changes to form one row.
    // Per-icon tint colours, matching the real dock glyphs.
    const nxui::Color kGreen(0.30f, 0.72f, 0.38f, 1.f);   // News
    const nxui::Color kBlue (0.24f, 0.60f, 0.95f, 1.f);   // Album (homebrew)
    const nxui::Color kLight(0.88f, 0.88f, 0.92f, 1.f);   // Controllers/Settings/Power

    // Dock order left-to-right: News, Album, Controllers, Settings, Sleep.
    {
        auto album = makeBtn(&m_icons[0], "sidebar.album", "Album", actions.onAlbum);
        m_albumButton = album.get();
        album->setIconTint(kBlue);
        album->setRect(slotRect(1));
        m_leftButtons.push_back(std::move(album));

        auto news = makeBtn(&m_icons[1], "sidebar.news", "News", actions.onNews);
        news->setIconTint(kGreen);
        news->setRect(slotRect(0));
        m_leftButtons.push_back(std::move(news));

        auto settings = makeBtn(&m_icons[5], "sidebar.settings", "Settings", actions.onSettings);
        m_settingsButton = settings.get();
        settings->setIconTint(kLight);
        settings->setRect(slotRect(3));
        m_leftButtons.push_back(std::move(settings));
    }

    {
        auto ctrl = makeBtn(&m_icons[2], "sidebar.controllers", "Controllers", actions.onControllers);
        ctrl->setIconTint(kLight);
        ctrl->setRect(slotRect(2));
        m_rightButtons.push_back(std::move(ctrl));

        auto sleep = makeBtn(&m_icons[3], "sidebar.sleep", "Sleep", actions.onSleep);
        sleep->setIconTint(kLight);
        sleep->setRect(slotRect(4));
        m_rightButtons.push_back(std::move(sleep));
    }

    // Horizontal end-stops: leftmost button clamps LEFT to itself, rightmost
    // clamps RIGHT. DOWN self-loops on every button (nothing below the dock);
    // UP is left free so it escapes upward to the tile strip via spatial nav.
    AppletButton* dockOrder[dockCount] = {
        m_leftButtons[1].get(),  // News        (slot 0)
        m_leftButtons[0].get(),  // Album       (slot 1)
        m_rightButtons[0].get(), // Controllers (slot 2)
        m_leftButtons[2].get(),  // Settings    (slot 3)
        m_rightButtons[1].get(), // Sleep       (slot 4)
    };
    for (AppletButton* btn : dockOrder)
        btn->setCustomNavigation(nxui::FocusDirection::DOWN, btn);
    dockOrder[0]->setCustomNavigation(nxui::FocusDirection::LEFT, dockOrder[0]);
    dockOrder[dockCount - 1]->setCustomNavigation(nxui::FocusDirection::RIGHT, dockOrder[dockCount - 1]);

    (void)gpu;
    (void)ren;
    (void)assetsBase;
}

void SidebarManager::reloadAssets(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                  const std::string& assetsBase,
                                  const std::string& customIconsBase) {
    if (m_leftButtons.empty() || m_rightButtons.empty())
        return;

    if (m_assetsLoaded
        && m_loadedAssetsBase == assetsBase
        && m_loadedCustomIconsBase == customIconsBase) {
        DebugLog::log("[sidebar-anim] reload skipped: assets unchanged (custom=%s)",
                      customIconsBase.empty() ? "<empty>" : customIconsBase.c_str());
        return;
    }

    loadAssets(gpu, ren, assetsBase, customIconsBase);
}

void SidebarManager::invalidateAssetsCache() {
    m_loadedAssetsBase.clear();
    m_loadedCustomIconsBase.clear();
    m_assetsLoaded = false;
}

void SidebarManager::loadAssets(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                const std::string& assetsBase,
                                const std::string& customIconsBase) {
    gpu.waitIdle();

    std::string defaultIconsBase = joinPath(assetsBase, "icons");
    const bool useCustomStaticIcons = !customIconsBase.empty();
    auto defaultAssetPath = [&](const char* fileName) {
        return joinPath(defaultIconsBase, fileName);
    };
    auto resolveAsset = [&](const char* fileName) {
        if (!customIconsBase.empty()) {
            std::string customPath = joinPath(customIconsBase, fileName);
            if (pathExists(customPath))
                return customPath;
        }
        return defaultAssetPath(fileName);
    };
    auto loadIconTexture = [&](int iconIdx, const char* fileName) {
        if (useCustomStaticIcons) {
            std::string customPath = joinPath(customIconsBase, fileName);
            if (pathExists(customPath)) {
                if (m_icons[iconIdx].loadFromFile(gpu, ren, customPath))
                    return;
                DebugLog::log("[sidebar-assets] custom icon load failed, falling back: %s",
                              customPath.c_str());
            }
        }

        const std::string fallbackPath = defaultAssetPath(fileName);
        if (!m_icons[iconIdx].loadFromFile(gpu, ren, fallbackPath)) {
            DebugLog::log("[sidebar-assets] fallback icon load failed: %s",
                          fallbackPath.c_str());
        }
    };

    static const char* iconFiles[] = {
        "album.png", "news.png", "controller.png", "power.png", "themes.png", "settings.png",
    };
    if ((int)m_icons.size() != kSidebarIconCount)
        m_icons.resize(kSidebarIconCount);
    for (int i = 0; i < kSidebarIconCount; ++i)
        loadIconTexture(i, iconFiles[i]);

    static const struct { int iconIdx; const char* webpFile; bool useFirstFrame; } animDefs[] = {
        { 0, "album.webp",      false },
        { 1, "news.webp",       false },
        { 2, "controller.webp", true  },
        { 3, "power.webp",      false },
        { 5, "settings.webp",   false },
    };

    m_anims.clear();
    if (useCustomStaticIcons) {
        DebugLog::log("[sidebar-anim] custom theme icons use PNG only; skipping WebP animations (%s)",
                      customIconsBase.c_str());
    }

    for (const auto& def : animDefs) {
        AppletButton* btn = nullptr;
        if (def.iconIdx == 0) btn = m_leftButtons[0].get();
        else if (def.iconIdx == 1) btn = m_leftButtons[1].get();
        else if (def.iconIdx == 2) btn = m_rightButtons[0].get();
        else if (def.iconIdx == 3) btn = m_rightButtons[1].get();
        else if (def.iconIdx == 5) btn = m_leftButtons[2].get();
        if (!btn)
            continue;

        nxui::Texture* staticTex = &m_icons[def.iconIdx];
        if (useCustomStaticIcons) {
            btn->setIcon(staticTex);
            continue;
        }

        nxui::Texture* idleTex = def.useFirstFrame ? nullptr : staticTex;
        tryLoadAnimation(gpu, ren, resolveAsset(def.webpFile), btn, idleTex);
        btn->setIcon(idleTex ? idleTex : staticTex);
    }

    m_loadedAssetsBase = assetsBase;
    m_loadedCustomIconsBase = customIconsBase;
    m_assetsLoaded = true;
}

void SidebarManager::tryLoadAnimation(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                      const std::string& webpPath,
                                      AppletButton* button,
                                      nxui::Texture* staticIcon) {
    AnimEntry entry;
    entry.button    = button;
    entry.staticTex = staticIcon;
    if (entry.anim.load(gpu, ren, webpPath)) {
        m_anims.push_back(std::move(entry));
    }
}

void SidebarManager::update(float dt, nxui::Widget* focusedWidget) {
    for (auto& e : m_anims) {
        if (!e.button) continue;
        bool focused = (focusedWidget == e.button);
        e.anim.update(dt, focused);
        if (focused && e.anim.hasFrames()) {
            e.button->setIcon(e.anim.currentFrame());
        } else {
            // staticTex == nullptr: use frame 0 of the animation as idle
            nxui::Texture* idle = e.staticTex ? e.staticTex
                                              : (e.anim.hasFrames() ? e.anim.currentFrame() : nullptr);
            e.button->setIcon(idle);
        }
    }
}


void SidebarManager::applyTheme(const nxui::Theme& theme) {
    (void)theme;
    // Switch-style dock: subtle circle behind a coloured glyph. Circle colour
    // and neutral glyph colour follow the theme (light vs dark); the coloured
    // glyphs (News green, Album blue) keep their fixed tint set in build().
    const bool light = (theme.mode == nxui::ThemeMode::Light);
    // Dock bubble fill. Near-black ("Black") themes use #292929; other dark
    // themes use #505050; light themes match the empty app-slot fill.
    nxui::Color circle;
    if (light) {
        circle = theme.iconDefault;
    } else {
        float lum = 0.299f * theme.primary.r + 0.587f * theme.primary.g
                  + 0.114f * theme.primary.b;
        circle = (lum < 0.05f)
            ? nxui::Color(0.161f, 0.161f, 0.161f, 1.f)   // #292929 (Black theme)
            : nxui::Color(0.314f, 0.314f, 0.314f, 1.f);  // #505050 (Dark theme)
    }
    const nxui::Color neutral = light ? nxui::Color(0.20f, 0.20f, 0.22f, 1.f)
                                      : nxui::Color(0.88f, 0.88f, 0.92f, 1.f);
    auto apply = [&](std::shared_ptr<AppletButton>& btn) {
        btn->setBaseColor(circle);
        btn->setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        btn->setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
        btn->setLiquidGlassEnabled(false);
        btn->setForceLiquidGlass(false);
        btn->setBlurEnabled(false);
        btn->setBorderWidth(0.f);
        btn->setIconCircular(false);
    };
    for (auto& btn : m_leftButtons)  apply(btn);
    for (auto& btn : m_rightButtons) apply(btn);
    // Neutral glyphs (Controllers, Settings, Power) follow the theme; coloured
    // ones (Album=leftButtons[0], News=leftButtons[1]) keep their tint.
    if (m_rightButtons.size() > 0) m_rightButtons[0]->setIconTint(neutral); // Controllers
    if (m_rightButtons.size() > 1) m_rightButtons[1]->setIconTint(neutral); // Power
    if (m_leftButtons.size()  > 2) m_leftButtons[2]->setIconTint(neutral);  // Settings
}
