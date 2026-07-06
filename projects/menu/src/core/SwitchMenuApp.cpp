#include "SwitchMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "themeshop/ThemeHttp.hpp"
#include <nxui/core/Animation.hpp>
#include <nxui/core/I18n.hpp>
#include "DebugLog.hpp"
#include "bluetooth/BluetoothManager.hpp"
#include <switch.h>
#ifdef QLAUNCHEXT_MENU
#include "smi_commands.hpp"
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <system_error>

namespace {

static constexpr const char* kLayoutPath = "sdmc:/config/qlaunch-ext/layout.json";
static constexpr int kMinHomePages = 8;
static constexpr const char* kBuiltInSoundPreset = "nx";

// Switch home-menu tile strip: a single horizontal row of large square tiles,
// vertically centred in the band between the top HUD and the applet dock.
static constexpr float kGridRectX = 0.f;
// Band positioned so a 256 px tile, vertically centred, sits with its bottom edge
// 270 px above the screen bottom (720 - 270 = 450): originY = 172 + (300-256)/2
// = 194, tile spans 194..450.
static constexpr float kGridRectY = 172.f;
static constexpr float kGridRectW = 1280.f;
static constexpr float kGridRectH = 300.f;

// qlaunch tile: large square game icons (~256 px) with a small gap, matching the
// home-screen "few games" layout. Selection ring sits ~2 px outside the tile.
static constexpr float kGridBaseCellW = 256.f;
static constexpr float kGridBaseCellH = 256.f;
static constexpr float kGridBasePadX  = 24.f;
static constexpr float kGridBasePadY  = 24.f;

bool isPackageSoundPreset(const std::string& preset) {
    return preset.rfind("package:", 0) == 0;
}

bool pathExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool directoryExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::string resolveAudioOverridePath(const std::string& preferredBase,
                                    const std::string& fallbackBase,
                                    const char* relativePath) {
    if (!preferredBase.empty()) {
        const std::string preferredPath = preferredBase + "/" + relativePath;
        if (pathExists(preferredPath))
            return preferredPath;
    }
    return fallbackBase + "/" + relativePath;
}

std::string installedThemePathFromPackagePreset(const std::string& preset) {
    if (!isPackageSoundPreset(preset))
        return {};

    const std::string slug = preset.substr(std::strlen("package:"));
    if (slug.empty())
        return {};

    const std::string installPath = std::string("sdmc:/config/qlaunch-ext/themes/") + slug;
    return directoryExists(installPath) ? installPath : std::string();
}

std::string resolveThemeSoundBase(const std::string& installPath) {
    if (installPath.empty())
        return {};

    const std::string directSfx = installPath + "/sfx";
    const std::string directMusic = installPath + "/music";
    const std::string soundsRoot = installPath + "/sounds";

    const bool hasDirect = directoryExists(directSfx) || directoryExists(directMusic);
    if (hasDirect)
        return installPath;
    if (directoryExists(soundsRoot))
        return soundsRoot;
    return {};
}

// Vertical clearance kept above/below the tile strip within its band (the strip
// scrolls horizontally, so no horizontal safe margin is needed).
static constexpr float kGridSafeTopBottomMargin = 20.f;

std::string titleIdToHex(uint64_t v) {
    char buf[17] = {};
    std::snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)v);
    return std::string(buf);
}

bool hexToTitleId(const std::string& s, uint64_t& out) {
    if (s.empty()) {
        out = 0;
        return false;
    }
    char* end = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &end, 16);
    if (end == s.c_str() || *end != '\0') {
        out = 0;
        return false;
    }
    out = (uint64_t)v;
    return true;
}

int hexNibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

[[maybe_unused]] bool hexToAccountUid(const std::string& s, AccountUid& out) {
    if (s.size() != 32)
        return false;

    AccountUid uid{};
    for (int part = 0; part < 2; ++part) {
        uint64_t value = 0;
        for (int i = 0; i < 16; ++i) {
            int nibble = hexNibble(s[(size_t)(part * 16 + i)]);
            if (nibble < 0)
                return false;
            value = (value << 4) | (uint64_t)nibble;
        }
        uid.uid[part] = value;
    }

    if (!accountUidIsValid(&uid))
        return false;
    out = uid;
    return true;
}

const char* safeTag(const nxui::Widget* widget) {
    if (!widget || widget->tag().empty())
        return "<none>";
    return widget->tag().c_str();
}

std::string utf8Codepoint(uint32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back((char)cp);
    } else if (cp <= 0x7FF) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string buttonGlyph(nxui::Button button) {
    switch (button) {
        case nxui::Button::A: return utf8Codepoint(0xE0E0);
        case nxui::Button::B: return utf8Codepoint(0xE0E1);
        case nxui::Button::X: return utf8Codepoint(0xE0E2);
        case nxui::Button::Y: return utf8Codepoint(0xE0E3);
        case nxui::Button::L: return utf8Codepoint(0xE0E4);
        case nxui::Button::R: return utf8Codepoint(0xE0E5);
        case nxui::Button::ZL: return utf8Codepoint(0xE0E6);
        case nxui::Button::ZR: return utf8Codepoint(0xE0E7);
        case nxui::Button::Plus: return utf8Codepoint(0xE0F1);
        case nxui::Button::Minus: return utf8Codepoint(0xE0F2);
        default: return {};
    }
}

std::string dpadGlyph() {
    return utf8Codepoint(0xE0EA);
}

bool appEntriesRefreshEquivalent(const AppEntry& a, const AppEntry& b) {
    return a.id == b.id &&
           a.title == b.title &&
           a.titleId == b.titleId &&
           a.viewFlags == b.viewFlags &&
           a.userRequired == b.userRequired &&
           a.startupUserKnown == b.startupUserKnown &&
           a.startupUserAccount == b.startupUserAccount &&
           a.startupUserAccountOption == b.startupUserAccountOption;
}

[[maybe_unused]] bool gridModelsRefreshEquivalent(const GridModel& a, const GridModel& b) {
    if (a.count() != b.count())
        return false;
    for (int i = 0; i < a.count(); ++i) {
        if (!appEntriesRefreshEquivalent(a.at(i), b.at(i)))
            return false;
    }
    return true;
}

}


SwitchMenuApp::SwitchMenuApp() {}
SwitchMenuApp::~SwitchMenuApp() {
    rootBox().clearChildren();
}

void SwitchMenuApp::setTutorialStartupFade(bool enabled) {
    m_tutorialStartupFade = enabled;
}

#ifdef QLAUNCHEXT_MENU
void SwitchMenuApp::setStartupStatus(uint64_t suspendedTitleId, bool appRunning) {
    m_launcher.setStartupStatus(suspendedTitleId, appRunning);
}
#endif

bool SwitchMenuApp::onCreate() {
    DebugLog::log("[init] onCreate enter");
    m_config.load();
    loadMenuLayout();
    m_appLoader.setPendingTransform([this](std::vector<PendingApp>& apps) {
        applyMenuLayoutToPending(apps);
    });

    nxui::I18n::instance().initialize(std::string(SD_ASSETS) + "/i18n", "en-US");
    applyUiLanguage();

    m_audioFuture = m_threadPool.submit([this]() {
        m_audio.initialize();
        m_availablePresets = scanAvailablePresets();
        if (!isPackageSoundPreset(m_config.soundPreset) && m_config.soundPreset != kBuiltInSoundPreset) {
            DebugLog::log("[audio] preset '%s' is no longer shipped, falling back to '%s'",
                          m_config.soundPreset.c_str(),
                          kBuiltInSoundPreset);
            m_config.soundPreset = kBuiltInSoundPreset;
        }
        loadSoundPreset(resolveSoundPresetId(m_config.soundPreset));
    });
    DebugLog::log("[init] Audio loading started on background thread");

    if (m_launcher.suspendedTitleId() != 0) {
        m_deferredBluetoothInitFrames = 120;
        DebugLog::log("[init] Bluetooth manager initialization deferred for fast return");
    } else {
        bluetooth::Initialize();
        DebugLog::log("[init] Bluetooth manager initialized");
    }
    DebugLog::log("[init] Theme Shop HTTP runtime deferred until first request");

    DebugLog::log("[init] Config loaded (theme=%s, musicVol=%.2f, sfxVol=%.2f)",
                  m_config.themePreset.c_str(), m_config.musicVolume, m_config.sfxVolume);

    m_launcher.init({
        .playSfxModalHide = [this]() { m_audio.playSfx(Sfx::ModalHide); },
        .requestExit = [this]() { app().requestExit(); },
    });


    DebugLog::log("[init] loadResources...");
    loadResources();
    DebugLog::log("[init] buildGrid...");
    buildGrid();

#ifdef QLAUNCHEXT_DEBUG_UI
    m_debugOverlay = std::make_unique<DebugImGuiOverlay>();
    if (!m_debugOverlay->initialize(app().gpu(), app().renderer())) {
        DebugLog::log("[debug] ImGui overlay init failed");
        m_debugOverlay.reset();
    } else {
        DebugLog::log("[debug] ImGui overlay ready");
    }
#endif

#ifdef QLAUNCHEXT_MENU
    m_sysMsg.setCallback([this](SysAction a) { handleSystemAction(a); });
    DebugLog::log("[init] async notifications via AppletStorage only");
    qlaunchext::menu::smi_cmd::menuReady();
#endif

    DebugLog::log("[init] DONE");
    return true;
}

void SwitchMenuApp::onDestroy() {
    if (m_audioFuture.valid()) m_audioFuture.get();

#ifdef QLAUNCHEXT_DEBUG_UI
    if (m_debugOverlay) {
        m_debugOverlay->shutdown(app().gpu());
        m_debugOverlay.reset();
    }
#endif

    stopEditGhost();

    themeshop::http::shutdown();

    bluetooth::Finalize();

#ifdef QLAUNCHEXT_MENU
    qlaunchext::menu::smi_cmd::menuClosing();
#endif
    if (m_layoutDirty)
        saveMenuLayout();
    m_audio.shutdown();
}

void SwitchMenuApp::loadResources() {
    std::string fontPath = std::string(SD_ASSETS) + "/fonts/font.otf";
    if (m_fontNormal.load(app().gpu(), app().renderer(), fontPath, 24))
        m_loadedRegularFontPath = fontPath;
    if (m_fontSmall.load(app().gpu(), app().renderer(), fontPath, 18))
        m_loadedSmallFontPath = fontPath;
    m_fontIcons.load(app().gpu(), app().renderer(), std::string(SD_ASSETS) + "/fonts/switch_icons.ttf", 24);

    std::string gameCardPath = std::string(SD_ASSETS) + "/icons/gamecard.png";
    if (m_gameCardTex.loadFromFile(app().gpu(), app().renderer(), gameCardPath))
        m_loadedGameCardPath = gameCardPath;

    m_settingsGearTex.loadFromFile(app().gpu(), app().renderer(),
                                   std::string(SD_ASSETS) + "/icons/settings.png");

    m_appLoader.load(m_model, m_iconStreamer);
}

void SwitchMenuApp::buildUserAvatarBar() {
    m_userAvatarButtons.clear();

    m_userAvatarBar = std::make_shared<nxui::Box>(nxui::Axis::ROW);
    m_userAvatarBar->setMarginTop(40.f);   // 40 px down from the top of the screen
    m_userAvatarBar->setGap(6.f);          // 6 px between avatars
    m_userAvatarBar->setShrink(0.f);
    m_userAvatarBar->setSize(0.f, 56.f);
    m_userAvatarBar->setTag("userAvatarBar");
    m_userAvatarBar->setWireframeEnabled(false);

    AccountUid uids[8] = {};
    s32 count = 0;
    Result rc = accountListAllUsers(uids, 8, &count);
    DebugLog::log("[profiles] accountListAllUsers rc=0x%X count=%d", rc, count);
    if (R_FAILED(rc) || count <= 0)
        return;

    for (int i = 0; i < count; ++i) {
        AccountProfile profile{};
        rc = accountGetProfile(&profile, uids[i]);
        if (R_FAILED(rc))
            continue;

        auto avatar = std::make_shared<UserAvatarButton>();
        avatar->setSize(56.f, 56.f);
        avatar->setMinWidth(56.f);
        avatar->setMinHeight(56.f);
        avatar->setShrink(0.f);
        avatar->setCornerRadius(28.f);
        avatar->setUid(uids[i]);
        avatar->setFocusable(true);

        AccountProfileBase base{};
        AccountUserData userData{};
        if (R_SUCCEEDED(accountProfileGet(&profile, &userData, &base)))
            avatar->setNickname(base.nickname);

        u32 imgSize = 0;
        if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imgSize)) && imgSize > 0) {
            std::vector<uint8_t> imgBuf(imgSize);
            u32 realSize = 0;
            if (R_SUCCEEDED(accountProfileLoadImage(&profile, imgBuf.data(), imgSize, &realSize))
                    && realSize > 0) {
                avatar->loadAvatar(app().gpu(), app().renderer(), imgBuf.data(), realSize);
            }
        }

        AccountUid uid = uids[i];
        avatar->setOnActivate([this, uid]() {
            m_audio.playSfx(Sfx::Activate);
#ifdef QLAUNCHEXT_MENU
            m_launcher.launchUserPage(uid);
#endif
        });

        accountProfileClose(&profile);
        m_userAvatarButtons.push_back(avatar);
        m_userAvatarBar->addChild(avatar);
    }

    if (!m_userAvatarButtons.empty()) {
        const float countF = static_cast<float>(m_userAvatarButtons.size());
        m_userAvatarBar->setSize(countF * 56.f + (countF - 1.f) * 6.f, 56.f);
        m_userAvatarButtons.front()->setCustomNavigation(nxui::FocusDirection::LEFT,
                                                         m_userAvatarButtons.front().get());
        m_userAvatarButtons.back()->setCustomNavigation(nxui::FocusDirection::RIGHT,
                                                        m_userAvatarButtons.back().get());
    }
}

SwitchMenuApp::GridLayoutMetrics SwitchMenuApp::computeGridLayoutMetrics() const {
    const int rows = std::clamp(m_config.gridRows, 1, 5);

    // The strip scrolls horizontally, so only the vertical extent constrains the
    // tile size — keep full-size 256 px qlaunch tiles for the default single row,
    // shrinking only if extra rows wouldn't otherwise fit the band.
    const float baseGridH = rows * kGridBaseCellH + (rows - 1) * kGridBasePadY;
    const float safeH = std::max(1.f, kGridRectH - (kGridSafeTopBottomMargin * 2.f));
    const float scale = std::min(1.f, safeH / baseGridH);

    GridLayoutMetrics m;
    m.cellW = std::max(88.f, kGridBaseCellW * scale);
    m.cellH = std::max(88.f, kGridBaseCellH * scale);
    m.padX = std::max(8.f, kGridBasePadX * scale);
    m.padY = std::max(8.f, kGridBasePadY * scale);
    return m;
}

void SwitchMenuApp::reflowHomeGrid() {
    if (!m_grid)
        return;

    const int oldFocusedIndex = m_grid->focusedGlobalIndex();
    const int oldPage = m_grid->currentPage();
    uint64_t focusedTitleId = 0;
    if (oldFocusedIndex >= 0 && oldFocusedIndex < m_model.count())
        focusedTitleId = m_model.at(oldFocusedIndex).titleId;

    std::unordered_map<uint64_t, AppEntry> byId;
    std::vector<uint64_t> appOrder;
    byId.reserve((size_t)std::max(0, m_model.count()));
    appOrder.reserve((size_t)std::max(0, m_model.count()));
    for (const auto& entry : m_model.entries()) {
        if (entry.titleId == 0 || byId.count(entry.titleId))
            continue;
        appOrder.push_back(entry.titleId);
        byId.emplace(entry.titleId, entry);
    }

    std::vector<uint64_t> slots = m_layoutSlots;
    if (slots.empty())
        slots = appOrder;

    std::unordered_set<uint64_t> placed;
    placed.reserve(byId.size());
    for (auto& slotTid : slots) {
        if (slotTid == 0)
            continue;
        if (!byId.count(slotTid) || placed.count(slotTid)) {
            slotTid = 0;
            continue;
        }
        placed.insert(slotTid);
    }

    for (uint64_t tid : appOrder) {
        if (placed.count(tid))
            continue;

        auto emptyIt = std::find(slots.begin(), slots.end(), 0);
        if (emptyIt != slots.end())
            *emptyIt = tid;
        else
            slots.push_back(tid);
        placed.insert(tid);
    }

    const int cols = std::clamp(m_config.gridColumns, 3, 8);
    const int rows = std::clamp(m_config.gridRows, 1, 5);
    const int perPage = std::max(1, cols * rows);
    int minSlots = std::max(perPage * kMinHomePages, (int)slots.size());
    int roundedSlots = ((minSlots + perPage - 1) / perPage) * perPage;
    if ((int)slots.size() < roundedSlots)
        slots.resize(roundedSlots, 0);

    if (slots != m_layoutSlots) {
        m_layoutSlots = slots;
        m_layoutDirty = true;
    }

    GridModel rebuiltModel;
    for (uint64_t tid : slots) {
        if (tid == 0) {
            rebuiltModel.addEntry(AppEntry{});
            continue;
        }

        auto it = byId.find(tid);
        if (it != byId.end())
            rebuiltModel.addEntry(it->second);
        else
            rebuiltModel.addEntry(AppEntry{});
    }

    std::vector<std::shared_ptr<GlossyIcon>> icons;
    icons.reserve((size_t)std::max(0, rebuiltModel.count()));
    const auto& oldIcons = m_grid->allIcons();
    for (int i = 0; i < rebuiltModel.count(); ++i) {
        if (i < (int)oldIcons.size() && oldIcons[i] &&
            i < m_model.count() &&
            m_model.at(i).titleId == rebuiltModel.at(i).titleId) {
            icons.push_back(oldIcons[i]);
        } else {
            auto icon = makeIcon(rebuiltModel.at(i));
            icon->setBaseColor(m_theme.iconDefault);
            icons.push_back(std::move(icon));
        }
    }

    m_model = std::move(rebuiltModel);
    m_iconStreamer.resize(m_model.count());
    m_iconStreamer.setIconDataLoader(AppListLoader::loadIconData);
    for (int i = 0; i < m_model.count(); ++i)
        m_iconStreamer.setTitleId(i, m_model.at(i).titleId);

    GridLayoutMetrics gridMetrics = computeGridLayoutMetrics();
    m_grid->setup(std::move(icons), cols, rows,
                  gridMetrics.cellW, gridMetrics.cellH,
                  gridMetrics.padX, gridMetrics.padY);

    int targetIndex = -1;
    if (focusedTitleId != 0)
        targetIndex = findTitleIndex(focusedTitleId);
    if (targetIndex < 0 && oldFocusedIndex >= 0 && m_model.count() > 0)
        targetIndex = std::clamp(oldFocusedIndex, 0, m_model.count() - 1);

    if (targetIndex >= 0)
        m_grid->focusGlobalIndex(targetIndex, true);
    else
        m_grid->setPage(oldPage);

    for (auto* icon : m_grid->pageIcons()) {
        if (icon)
            icon->forceVisible();
    }

    m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                 app().gpu(), app().renderer(),
                                 m_grid->allIcons());

    const bool overlayActive =
        (m_dialog && m_dialog->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_userSelect && m_userSelect->isActive());
    if (!overlayActive) {
        if (auto* cur = m_grid->focusManager().current())
            focusManager().setFocus(cur);
        updateCursor();
    }

    DebugLog::log("[grid] reflowed layout cols=%d rows=%d apps=%d page=%d",
                  cols, rows, m_model.count(), m_grid->currentPage());
}

void SwitchMenuApp::loadMenuLayout() {
    m_layoutSlots.clear();

    std::ifstream f(kLayoutPath);
    if (!f.is_open())
        return;

    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return;
    }

    auto it = j.find("slots");
    if (it == j.end() || !it->is_array())
        return;

    for (const auto& v : *it) {
        uint64_t tid = 0;
        if (v.is_string()) {
            std::string s = v.get<std::string>();
            if (!hexToTitleId(s, tid))
                tid = 0;
        } else if (v.is_number_unsigned()) {
            tid = v.get<uint64_t>();
        } else if (v.is_number_integer()) {
            auto raw = v.get<int64_t>();
            tid = raw > 0 ? (uint64_t)raw : 0;
        }
        m_layoutSlots.push_back(tid);
    }
}

void SwitchMenuApp::saveMenuLayout() {
    std::error_code ec;
    std::filesystem::create_directory("sdmc:/config", ec);
    ec.clear();
    std::filesystem::create_directory("sdmc:/config/qlaunch-ext", ec);

    nlohmann::json j;
    j["version"] = 1;
    j["slots"] = nlohmann::json::array();
    for (uint64_t tid : m_layoutSlots) {
        if (tid == 0)
            j["slots"].push_back("0");
        else
            j["slots"].push_back(titleIdToHex(tid));
    }

    std::ofstream f(kLayoutPath, std::ios::trunc);
    if (!f.is_open())
        return;
    f << j.dump(2);
    m_layoutDirty = false;
}

void SwitchMenuApp::applyMenuLayoutToPending(std::vector<PendingApp>& apps) {
    const int cols = std::clamp(m_config.gridColumns, 3, 8);
    const int rows = std::clamp(m_config.gridRows, 1, 5);
    const int perPage = std::max(1, cols * rows);

    std::unordered_map<uint64_t, PendingApp> byId;
    byId.reserve(apps.size());
    for (auto& app : apps) {
        if (app.titleId != 0)
            byId.emplace(app.titleId, std::move(app));
    }

    std::vector<uint64_t> slots = m_layoutSlots;
    if (slots.empty()) {
        slots.reserve(apps.size());
        for (const auto& app : apps)
            if (app.titleId != 0)
                slots.push_back(app.titleId);
    }

    std::unordered_set<uint64_t> placed;
    placed.reserve(byId.size());
    for (auto& slotTid : slots) {
        if (slotTid == 0)
            continue;
        auto it = byId.find(slotTid);
        if (it == byId.end() || placed.count(slotTid)) {
            slotTid = 0;
            continue;
        }
        placed.insert(slotTid);
    }

    for (const auto& app : apps) {
        if (app.titleId == 0 || placed.count(app.titleId))
            continue;

        auto emptyIt = std::find(slots.begin(), slots.end(), 0);
        if (emptyIt != slots.end())
            *emptyIt = app.titleId;
        else
            slots.push_back(app.titleId);

        placed.insert(app.titleId);
    }

    int minSlots = std::max(perPage * kMinHomePages, (int)slots.size());
    int roundedSlots = ((minSlots + perPage - 1) / perPage) * perPage;
    if ((int)slots.size() < roundedSlots)
        slots.resize(roundedSlots, 0);

    std::vector<PendingApp> ordered;
    ordered.reserve(slots.size());
    for (uint64_t tid : slots) {
        if (tid == 0) {
            ordered.emplace_back();
            continue;
        }
        auto it = byId.find(tid);
        if (it != byId.end()) {
            ordered.push_back(std::move(it->second));
        } else {
            ordered.emplace_back();
        }
    }

    if (slots != m_layoutSlots) {
        m_layoutSlots = std::move(slots);
        m_layoutDirty = true;
    }

    apps = std::move(ordered);
}

std::shared_ptr<GlossyIcon> SwitchMenuApp::makeIcon(const AppEntry& entry) {
    auto icon = std::make_shared<GlossyIcon>();
    icon->setShape(GlossyIcon::shapeFromString(m_config.appIconShape));
    icon->setRoundedRadius(m_theme.iconCornerRadius);
    icon->setSelectionColor(m_theme.accent);
    icon->setGlowEnabled(m_config.appSelectionGlow);
    if (entry.titleId == 0) {
        icon->setTag("glossy_icon");
        icon->setTitle("");
        icon->setTitleId(0);
        icon->setFocusable(true);
        auto& i18n = nxui::I18n::instance();
        icon->setAccessibilityLabel(i18n.tr("accessibility.grid.empty_slot", "Empty slot"));
        icon->setAccessibilityRole(i18n.tr("accessibility.roles.slot", "slot"));
        icon->setAccessibilityHint(i18n.tr("accessibility.hints.grid_empty", "Use the directional pad to move to another slot."));
        icon->setNotLaunchable(false);
        icon->setCornerRadius(m_theme.iconCornerRadius);
        return icon;
    }

    icon->setTag("glossy_icon");
    icon->setTitle(entry.title);
    icon->setTitleId(entry.titleId);
    icon->setAccessibilityLabel(entry.title);
    auto& i18n = nxui::I18n::instance();
    icon->setAccessibilityRole(entry.isGameCard()
        ? i18n.tr("accessibility.roles.game_card", "game card")
        : i18n.tr("accessibility.roles.game", "game"));
    icon->setAccessibilityHint(entry.isLaunchable()
        ? i18n.tr("accessibility.hints.game_launchable", "A to launch. X for options. Y to move. ZL or ZR to change page.")
        : i18n.tr("accessibility.hints.game_blocked", "A to show why this item is blocked."));
    // Texture is set by IconStreamer::onPageChanged() — not here.
    icon->setCornerRadius(m_theme.iconCornerRadius);
    icon->setIsGameCard(entry.isGameCard());
    icon->setGameCardTexture(&m_gameCardTex);
    icon->setNotLaunchable(!entry.isLaunchable());

#ifdef QLAUNCHEXT_MENU
    if (m_launcher.suspendedTitleId() != 0 &&
        entry.titleId == m_launcher.suspendedTitleId())
        icon->setSuspended(true);

    GlossyIcon* raw = icon.get();
    icon->setOnActivate([this, raw]() {
        uint64_t tid = raw->titleId();
        if (m_launcher.isAppSuspended(tid)) {
            m_audio.playSfx(Sfx::LaunchGame);
            nxui::Rect   fr   = raw->focusRect();
            const nxui::Texture* tex = raw->texture();
            float  cr   = raw->cornerRadius();
            nxui::Color  base = m_theme.panelBase;
            nxui::Color  bord = m_theme.panelBorder;
            m_launchAnim->start(fr, tex, cr, base, bord, 0, {},
                nullptr,
                [this]() { m_launcher.resumeApplication(); });
        } else {
            AppEntry* entry = nullptr;
            int entryIndex = findTitleIndex(tid);
            if (entryIndex >= 0)
                entry = &m_model.at(entryIndex);
            if (entry && !entry->isLaunchable()) {
                m_audio.playSfx(Sfx::ModalShow);
                m_dialogReturnFocus = raw;
                std::string reason;
                auto& i18n = nxui::I18n::instance();
                if (entry->isGameCardNotInserted())
                    reason = i18n.tr("error.gamecard_not_inserted", "Game card is not inserted.");
                else if (entry->needsVerify())
                    reason = i18n.tr("error.needs_verify", "Game data needs verification.");
                else if (entry->needsUpdate())
                    reason = i18n.tr("error.needs_update", "A required update is available.");
                else if (!entry->hasContents())
                    reason = i18n.tr("error.no_contents", "Game data is missing.");
                else
                    reason = i18n.tr("error.cannot_launch", "This game cannot be launched.");
                m_dialog->show(
                    i18n.tr("error.title", "Cannot Launch"),
                    reason,
                    {{i18n.tr("button.ok", "OK"), [this]() {}, true}},
                    0, {}
                );
                focusManager().setFocus(m_dialog.get());
                return;
            }

            nxui::Rect   fr   = raw->focusRect();
            const nxui::Texture* tex = raw->texture();
            float  cr   = raw->cornerRadius();
            nxui::Color  base = m_theme.panelBase;
            nxui::Color  bord = m_theme.panelBorder;
            auto startLaunch = [this, fr, tex, cr, base, bord, tid](AccountUid uid) {
                m_audio.playSfx(Sfx::LaunchGame);
                m_launchAnim->start(fr, tex, cr, base, bord, tid, uid,
                    [this](uint64_t id, AccountUid u) { m_launcher.launchApplication(id, u); });
            };
            if (entry) {
                if (!entry->startupUserKnown) {
                    entry->startupUserAccount = 1;
                    entry->startupUserAccountOption = 0;
                    entry->userRequired = true;
                }
                DebugLog::log("[launcher] user decision tid=%016lX startup_user=%u option=%u interactive_user=%d",
                              tid,
                              (unsigned)entry->startupUserAccount,
                              (unsigned)entry->startupUserAccountOption,
                              entry->userRequired ? 1 : 0);

                if (entry->startupUserAccount == 0) {
                    AccountUid emptyUid = {};
                    DebugLog::log("[launcher] skipping user select: NACP StartupUserAccount=None");
                    startLaunch(emptyUid);
                    return;
                }

                if (m_config.defaultProfileEnabled) {
                    AccountUid defaultUid = {};
                    if (hexToAccountUid(m_config.defaultProfileUid, defaultUid)) {
                        DebugLog::log("[launcher] skipping user select: default profile configured uid[0]=0x%016lX uid[1]=0x%016lX",
                                      defaultUid.uid[0], defaultUid.uid[1]);
                        startLaunch(defaultUid);
                        return;
                    }
                    DebugLog::log("[launcher] default profile enabled but uid is invalid");
                }

                AccountUid silentUid = {};
                const bool networkRequired = entry->startupUserAccount == 2;
                Result silentRc = accountTrySelectUserWithoutInteraction(&silentUid, networkRequired);
                DebugLog::log("[launcher] TrySelectUserWithoutInteraction network_required=%d rc=0x%X uid_valid=%d uid[0]=0x%016lX uid[1]=0x%016lX",
                              networkRequired ? 1 : 0,
                              silentRc,
                              accountUidIsValid(&silentUid) ? 1 : 0,
                              silentUid.uid[0],
                              silentUid.uid[1]);
                if (R_SUCCEEDED(silentRc) && accountUidIsValid(&silentUid)) {
                    DebugLog::log("[launcher] skipping user select: silent account selection succeeded");
                    startLaunch(silentUid);
                    return;
                }

                if (!entry->userRequired) {
                    AccountUid emptyUid = {};
                    DebugLog::log("[launcher] skipping user select fallback: startup_user=%u option=%u did not require interactive picker",
                                  (unsigned)entry->startupUserAccount,
                                  (unsigned)entry->startupUserAccountOption);
                    startLaunch(emptyUid);
                    return;
                }
            }
            if (m_userSelect) {
                bool usersLoaded = m_userSelect->loadUsers(app().gpu(), app().renderer());
                DebugLog::log("[UserSelect] lazy load result=%d", usersLoaded ? 1 : 0);
                if (usersLoaded)
                    m_audio.playSfx(Sfx::ModalShow);
            }
            m_userSelect->showUserSelect([startLaunch](AccountUid uid) { startLaunch(uid); });
            focusManager().setFocus(m_userSelect.get());
        }
    });
#else
    icon->setOnActivate([this]() {
        m_audio.playSfx(Sfx::Activate);
    });
#endif
    return icon;
}

void SwitchMenuApp::buildGrid() {
    reloadThemePresets();

    m_activePresetName = m_config.themePreset;
    ThemePreset* preset = findPresetPtr(m_activePresetName);
    if (!preset) {
        m_activePresetName = "builtin:Basic Dark";
        preset = findPresetPtr(m_activePresetName);
    }
    if (!preset) {
        m_activePresetName = "Basic Dark";
        preset = findPresetPtr(m_activePresetName);
    }

    // Last-resort guard: never dereference a null preset (would crash startup).
    static ThemePreset s_fallbackPreset;
    if (!preset) {
        const auto& builtins = ThemePreset::builtInPresets();
        s_fallbackPreset = builtins.empty() ? ThemePreset{} : builtins.front();
        preset = &s_fallbackPreset;
    }

    if (preset)
        m_activePresetName = preset->id.empty() ? preset->name : preset->id;

    m_activeColors = preset->colors;
    m_activeMode = preset->mode;

    m_effectivePreset = buildEffectiveThemePreset();
    m_theme = m_effectivePreset.toTheme();
    applyCustomThemeColors();

    m_background = std::make_shared<WaraWaraBackground>();
    m_background->setRect({0, 0, 1280, 720});
    m_background->setFlat(!m_config.backgroundEffectEnabled);
    applyThemeResources(m_effectivePreset);

    std::vector<std::shared_ptr<GlossyIcon>> icons;
    for (int i = 0; i < m_model.count(); ++i)
        icons.push_back(makeIcon(m_model.at(i)));

    GridLayoutMetrics gridMetrics = computeGridLayoutMetrics();

    m_grid = std::make_shared<IconGrid>();
    m_grid->setRect({kGridRectX, kGridRectY, kGridRectW, kGridRectH});
    m_grid->setup(std::move(icons),
                  std::clamp(m_config.gridColumns, 3, 8),
                  std::clamp(m_config.gridRows, 1, 5),
                  gridMetrics.cellW, gridMetrics.cellH,
                  gridMetrics.padX, gridMetrics.padY);

    m_cursor = std::make_shared<SelectionCursor>();
    m_pointerCursor = std::make_shared<SelectionCursor>();
    m_pointerCursor->setVisible(false);

    // Switch-style flat header: clock and battery sit together at the top-right
    // with no glass pill background.
    m_clock = std::make_shared<DateTimeWidget>();
    m_clock->setSize(150, 62);
    m_clock->setMarginTop(14.f);
    m_clock->setFont(&m_fontNormal);
    m_clock->setSmallFont(&m_fontSmall);
    m_clock->setUse12HourClock(m_config.clockUse12Hour);
    m_clock->setCornerRadius(m_theme.cellCornerRadius);
    m_clock->setForceLiquidGlass(false);
    m_clock->setBlurEnabled(false);

    m_battery = std::make_shared<BatteryWidget>();
    m_battery->setMarginTop(14.f);
    m_battery->setSize(150, 62);
    m_battery->setFont(&m_fontSmall);
    m_battery->setCornerRadius(m_theme.cellCornerRadius);
    m_battery->setForceLiquidGlass(false);
    m_battery->setBlurEnabled(false);

    buildUserAvatarBar();

    // Selected game name, shown just below the tile strip (above the dock).
    m_titlePill = std::make_shared<TitlePillWidget>();
    m_titlePill->setPosition(0, 436.f);
    m_titlePill->setFont(&m_fontNormal);
    m_titlePill->setPadding(9.f, 22.f, 9.f, 22.f);
    m_titlePill->setForceLiquidGlass(false);
    m_titlePill->setBlurEnabled(false);

    m_pageIndicator = std::make_shared<PageIndicator>();
    m_pageIndicator->setRect({0, 685.f, 1280.f, 28.f});
    m_pageIndicator->setTheme(&m_theme);
    m_pageIndicator->setForceLiquidGlass(true);
    m_pageIndicator->setBlurEnabled(false);

    m_launchAnim = std::make_shared<LaunchAnimation>();

    m_userSelect = std::make_shared<OverlayDialog>();
    m_userSelect->setFont(&m_fontNormal);
    m_userSelect->setSmallFont(&m_fontSmall);
    m_userSelect->setTheme(&m_theme);
    m_userSelect->onNavigateSfx([this]() { m_audio.playSfx(Sfx::Navigate); });
    m_userSelect->onActivateSfx([this]() { m_audio.playSfx(Sfx::Activate); });
    m_userSelect->onCloseSfx([this]() { m_audio.playSfx(Sfx::ModalHide); });

    m_dialog = std::make_shared<OverlayDialog>();
    m_dialog->setFont(&m_fontNormal);
    m_dialog->setSmallFont(&m_fontSmall);
    m_dialog->setTheme(&m_theme);
    m_dialog->onNavigateSfx([this]() { m_audio.playSfx(Sfx::Navigate); });
    m_dialog->onActivateSfx([this]() { m_audio.playSfx(Sfx::Activate); });
    m_dialog->onCloseSfx([this]() { m_audio.playSfx(Sfx::ModalHide); });

    m_progressDialog = std::make_shared<ProgressDialog>();
    m_progressDialog->setFont(&m_fontNormal);
    m_progressDialog->setSmallFont(&m_fontSmall);
    m_progressDialog->setTheme(&m_theme);

    app().renderer().setBoxWireframeEnabled(m_showWireframe);

    wireFocusCallback();
    m_grid->onPageSwitched([this]() {
        // Stream icon textures for the new page.
        m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
        auto* target = m_grid->focusManager().current();
        if (target)
            focusManager().setFocus(target);
        updateCursor();
    });

#ifdef QLAUNCHEXT_MENU
    if (m_launcher.suspendedTitleId() != 0) {
        int suspendedIndex = findTitleIndex(m_launcher.suspendedTitleId());
        int initialPage = 0;
        if (suspendedIndex >= 0 && m_grid->iconsPerPage() > 0)
            initialPage = suspendedIndex / m_grid->iconsPerPage();
        if (initialPage > 0)
            m_grid->setPage(initialPage);
    }
#endif

    const bool returningFromSuspendedApp = m_launcher.suspendedTitleId() != 0;
    if (returningFromSuspendedApp) {
        m_deferredInitialAssetFrames = 1;
        DebugLog::log("[init] return path: deferring initial icon/sidebar uploads");
    } else {
        // Load textures for the initial visible page.
        m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
    }

    if (returningFromSuspendedApp) {
        for (auto& icon : m_grid->allIcons())
            icon->forceVisible();
        m_returnFadeTimer = kReturnFadeInDur;
    } else {
        m_grid->startAppearAnimation();
    }
    if (m_tutorialStartupFade)
        m_tutorialStartupFadeTimer = kTutorialStartupFadeDur;

    SidebarManager::Actions sidebarActions;
#ifdef QLAUNCHEXT_MENU
    sidebarActions.onAlbum       = [this]() { m_launcher.launchAlbum(); };
    sidebarActions.onNews        = [this]() { m_launcher.launchMiiEditor(); };
    sidebarActions.onControllers = [this]() { m_launcher.launchControllerPairing(); };
#else
    sidebarActions.onAlbum       = [this]() { m_audio.playSfx(Sfx::Activate); };
    sidebarActions.onNews        = [this]() { m_audio.playSfx(Sfx::Activate); };
    sidebarActions.onControllers = [this]() { m_audio.playSfx(Sfx::Activate); };
#endif
    sidebarActions.onSettings = [this]() {
        startSettingsOpenAnim();
    };
    sidebarActions.onSleep = [this]() {
        if (!m_dialog) return;
        auto& i18n = nxui::I18n::instance();
        m_audio.playSfx(Sfx::ModalShow);
        m_dialogReturnFocus = focusManager().current();
        m_dialog->show(
            i18n.tr("power.title", "Power"),
            i18n.tr("power.choose_action", "Choose a power action."),
            {
                {i18n.tr("button.cancel", "Cancel"), [this]() {  }, true},
                {i18n.tr("power.sleep", "Sleep"), [this]() {
#ifdef QLAUNCHEXT_MENU
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    m_launcher.enterSleep();
#else
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    app().requestExit();
#endif
                }, true},
                {i18n.tr("power.shutdown", "Shutdown"), [this]() {
#ifdef QLAUNCHEXT_MENU
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    m_launcher.shutdown();
#else
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    app().requestExit();
#endif
                }, true},
                {i18n.tr("power.reboot", "Reboot"), [this]() {
#ifdef QLAUNCHEXT_MENU
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    m_launcher.reboot();
#else
                    m_audio.playSfx(Sfx::ConfirmPositive);
                    app().requestExit();
#endif
                }, true}
            },
            0,
            {}
        );
        focusManager().setFocus(m_dialog.get());
    };
    sidebarActions.onMiiverse = [this]() {
        m_audio.playSfx(Sfx::ModalShow);
        if (!m_themeShop) return;
        if (m_settings && m_settings->isActive())
            m_settings->hide();
        refreshThemeShopState();
        m_themeShop->show();
        focusManager().setFocus(m_themeShop.get());
    };

    m_sidebar.build(app().gpu(), app().renderer(), SD_ASSETS, sidebarActions);
    m_settingsGearBtn = m_sidebar.settingsButton();
    if (!returningFromSuspendedApp) {
        m_sidebar.reloadAssets(app().gpu(), app().renderer(), SD_ASSETS,
                               resolveThemeAssetPath(m_effectivePreset, m_effectivePreset.icons.basePath));
    }

    wireGlobalActions();
    applyTheme();

    auto& root = rootBox();
    root.clearChildren();

    m_bgLayer = std::make_shared<nxui::Box>();
    m_bgLayer->setRect({0, 0, 1280, 720});
    m_bgLayer->setTag("bgLayer");
    m_bgLayer->setWireframeEnabled(false);
    m_bgLayer->addChild(m_background);

    m_contentLayer = std::make_shared<nxui::Box>();
    m_contentLayer->setRect({0, 0, 1280, 720});
    m_contentLayer->setTag("contentLayer");
    m_contentLayer->setWireframeEnabled(false);

    // Switch-style HUD: user avatars pinned top-left, clock + battery top-right.
    m_topHud = std::make_shared<nxui::Box>(nxui::Axis::ROW);
    m_topHud->setRect({0, 0, 1280, 90});
    m_topHud->setTag("topHud");
    m_topHud->setWireframeEnabled(false);
    m_topHud->setJustifyContent(nxui::JustifyContent::SPACE_BETWEEN);
    m_topHud->setAlignItems(nxui::AlignItems::FLEX_START);

    if (m_userAvatarBar) {
        m_userAvatarBar->setMarginLeft(60.f);   // 60 px in from the left edge
        m_topHud->addChild(m_userAvatarBar);
    } else {
        auto leftSpacer = std::make_shared<nxui::Box>(nxui::Axis::ROW);
        leftSpacer->setSize(24.f, 1.f);
        leftSpacer->setShrink(0.f);
        m_topHud->addChild(leftSpacer);
    }

    auto rightHud = std::make_shared<nxui::Box>(nxui::Axis::ROW);
    rightHud->setTag("rightHud");
    rightHud->setWireframeEnabled(false);
    rightHud->setShrink(0.f);
    rightHud->setGap(8.f);
    rightHud->setSize(316.f, 62.f);
    rightHud->addChild(m_clock);
    rightHud->addChild(m_battery);
    m_topHud->addChild(rightHud);
    m_topHud->layout();

    m_leftSidebar = std::make_shared<nxui::Box>(nxui::Axis::COLUMN);
    m_leftSidebar->setTag("leftSidebar");
    m_leftSidebar->setWireframeEnabled(false);
    for (auto& btn : m_sidebar.leftButtons())
        m_leftSidebar->addChild(btn);

    m_rightSidebar = std::make_shared<nxui::Box>(nxui::Axis::COLUMN);
    m_rightSidebar->setTag("rightSidebar");
    m_rightSidebar->setWireframeEnabled(false);
    for (auto& btn : m_sidebar.rightButtons())
        m_rightSidebar->addChild(btn);

    m_contentLayer->addChild(m_grid);
    m_contentLayer->addChild(m_leftSidebar);
    m_contentLayer->addChild(m_rightSidebar);
    m_contentLayer->addChild(m_topHud);
    m_contentLayer->addChild(m_titlePill);
    m_contentLayer->addChild(m_pageIndicator);

    m_overlayLayer = std::make_shared<nxui::Box>();
    m_overlayLayer->setRect({0, 0, 1280, 720});
    m_overlayLayer->setTag("overlayLayer");
    m_overlayLayer->setWireframeEnabled(false);
    m_overlayLayer->addChild(m_cursor);
    m_overlayLayer->addChild(m_userSelect);

    createSettings();
    createThemeShop();

    m_overlayLayer->addChild(m_dialog);
    m_overlayLayer->addChild(m_progressDialog);
    m_overlayLayer->addChild(m_launchAnim);
    m_overlayLayer->addChild(m_pointerCursor);

    root.addChild(m_bgLayer);
    root.addChild(m_contentLayer);
    root.addChild(m_overlayLayer);

    if (!focusTitle(m_launcher.suspendedTitleId())) {
        if (auto* firstIcon = m_grid->focusManager().current())
            focusManager().setFocus(firstIcon);
    }

    if (m_layoutDirty)
        saveMenuLayout();
}

std::string SwitchMenuApp::resolveSoundPresetId(const std::string& preset) const {
    std::string effectivePreset = preset;
    if (!isPackageSoundPreset(effectivePreset) && effectivePreset != kBuiltInSoundPreset) {
        DebugLog::log("[audio] preset '%s' blocked, using '%s' instead",
                      effectivePreset.c_str(),
                      kBuiltInSoundPreset);
        return kBuiltInSoundPreset;
    }

    if (!isPackageSoundPreset(effectivePreset))
        return effectivePreset;

    if (!resolveThemeSoundBase(installedThemePathFromPackagePreset(effectivePreset)).empty()) {
        DebugLog::log("[audio] package preset '%s' resolved from install directory", effectivePreset.c_str());
        return effectivePreset;
    }

    for (const auto& themePreset : m_allPresets) {
        if (themePreset.source != ThemePresetSource::InstalledPackage || themePreset.installPath.empty())
            continue;
        if (themePreset.id != effectivePreset && themePreset.soundPreset != effectivePreset)
            continue;
        return effectivePreset;
    }

    DebugLog::log("[audio] package preset '%s' unavailable, using '%s' instead",
                  effectivePreset.c_str(),
                  kBuiltInSoundPreset);
    return kBuiltInSoundPreset;
}

void SwitchMenuApp::loadSoundPreset(const std::string& preset) {
    std::string effectivePreset = preset;
    const bool useBuiltInBase = (effectivePreset == kBuiltInSoundPreset);
    const std::string builtInBase = std::string(SD_ASSETS) + "/sounds/" + kBuiltInSoundPreset;

    std::string base;
    if (!useBuiltInBase) {
        base = resolveThemeSoundBase(installedThemePathFromPackagePreset(effectivePreset));

        for (const auto& themePreset : m_allPresets) {
            if (!base.empty())
                break;
            if (themePreset.source != ThemePresetSource::InstalledPackage || themePreset.installPath.empty())
                continue;
            if (themePreset.id != effectivePreset && themePreset.soundPreset != effectivePreset)
                continue;

            base = resolveThemeSoundBase(themePreset.installPath);
            break;
        }
    }

    if (base.empty()) {
        if (isPackageSoundPreset(effectivePreset)) {
            base = installedThemePathFromPackagePreset(effectivePreset);
        } else {
            base = std::string(SD_ASSETS) + "/sounds/" + effectivePreset;
        }
    }
    DebugLog::log("[audio] Loading preset '%s' from %s", effectivePreset.c_str(), base.c_str());

    const bool hasCustomSfx = directoryExists(base + "/sfx");
    const bool hasCustomMusic = directoryExists(base + "/music");
    const std::string musicBase = hasCustomMusic ? base : builtInBase;
    const std::string preferredSfxBase = (!useBuiltInBase && hasCustomSfx) ? base : std::string();
    auto sfxPath = [&](const char* relativePath) {
        return resolveAudioOverridePath(preferredSfxBase, builtInBase, relativePath);
    };

    if (!useBuiltInBase && !hasCustomSfx) {
        DebugLog::log("[audio] preset '%s' has no custom SFX directory, using '%s' SFX fallback",
                      effectivePreset.c_str(),
                      kBuiltInSoundPreset);
    }
    if (!useBuiltInBase && !hasCustomMusic) {
        DebugLog::log("[audio] preset '%s' has no custom music, using '%s' music fallback",
                      effectivePreset.c_str(),
                      kBuiltInSoundPreset);
    }

    m_audio.loadSfx(Sfx::Navigate,        sfxPath("sfx/Tick.wav"));
    m_audio.loadSfx(Sfx::Activate,        sfxPath("sfx/activation.wav"));
    m_audio.loadSfx(Sfx::PageChange,      sfxPath("sfx/tab_transition.wav"));
    m_audio.loadSfx(Sfx::ModalShow,       sfxPath("sfx/show_modal.wav"));
    m_audio.loadSfx(Sfx::ModalHide,       sfxPath("sfx/hide_modal.wav"));
    m_audio.loadSfx(Sfx::LaunchGame,      sfxPath("sfx/Popup + Run Title.wav"));
    m_audio.loadSfx(Sfx::ToggleOn,        sfxPath("sfx/Turn On.wav"));
    m_audio.loadSfx(Sfx::ToggleOff,       sfxPath("sfx/Turn Off.wav"));
    m_audio.loadSfx(Sfx::SliderUp,        sfxPath("sfx/slider_up.wav"));
    m_audio.loadSfx(Sfx::SliderDown,      sfxPath("sfx/slider_down.wav"));
    m_audio.loadSfx(Sfx::ConfirmPositive, sfxPath("sfx/confirm.wav"));
    m_audio.loadSfx(Sfx::Volume,          sfxPath("sfx/volume.wav"));
    m_audio.loadSfx(Sfx::SettingsOpen,    sfxPath("sfx/Settings.wav"));

    std::string musicDir = musicBase + "/music";
    std::error_code ec;
    if (std::filesystem::is_directory(musicDir, ec)) {
        std::vector<std::string> tracks;
        ec.clear();
        for (const auto& entry : std::filesystem::directory_iterator(musicDir, ec)) {
            if (ec)
                break;

            std::string name = entry.path().filename().string();
            if (name.size() > 4 && name.substr(name.size() - 4) == ".mp3")
                tracks.push_back(name);
        }
        std::sort(tracks.begin(), tracks.end(), [](const std::string& left, const std::string& right) {
            const bool leftIsHome = (left == "home.mp3");
            const bool rightIsHome = (right == "home.mp3");
            if (leftIsHome != rightIsHome)
                return leftIsHome;
            return left < right;
        });
        for (const auto& t : tracks)
            m_audio.loadTrack(musicDir + "/" + t);
        DebugLog::log("[audio] Loaded %zu music tracks", tracks.size());
    } else {
        DebugLog::log("[audio] No music directory for preset '%s'", effectivePreset.c_str());
    }
}

void SwitchMenuApp::changeSoundPreset(const std::string& preset) {
    const std::string effectivePreset = resolveSoundPresetId(preset);
    if (m_presetChangePending && effectivePreset == m_pendingSoundPreset) {
        DebugLog::log("[audio] Preset change skipped; '%s' is already pending",
                      effectivePreset.c_str());
        return;
    }

    if (m_audioStarted && effectivePreset == m_loadedSoundPreset) {
        DebugLog::log("[audio] Preset change skipped; '%s' is already active",
                      effectivePreset.c_str());
        return;
    }

    m_audio.stop();
    m_audio.clearTracks();
    m_audio.clearSfx();

    m_presetChangePending = true;
    m_pendingSoundPreset = effectivePreset;
    m_audioFuture = m_threadPool.submit([this, effectivePreset]() {
        loadSoundPreset(effectivePreset);
    });
}

std::vector<std::string> SwitchMenuApp::scanAvailablePresets() {
    std::vector<std::string> presets;
    std::string soundsDir = std::string(SD_ASSETS) + "/sounds";
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(soundsDir, ec)) {
        if (ec)
            break;

        std::string name = entry.path().filename().string();
        if (name != kBuiltInSoundPreset) continue;

        std::string sub = entry.path().string();
        if (!entry.is_directory(ec)) {
            ec.clear();
            continue;
        }

        std::string sfxSub = sub + "/sfx";
        std::string musicSub = sub + "/music";
        bool hasSfx = directoryExists(sfxSub);
        bool hasMusic = directoryExists(musicSub);
        if (hasSfx || hasMusic)
            presets.push_back(name);
    }
    std::sort(presets.begin(), presets.end());
    return presets;
}

#ifdef QLAUNCHEXT_MENU
void SwitchMenuApp::refreshAppList() {
    DebugLog::log("[refresh] starting async app list fetch");

    if (m_editMode)
        exitEditMode();

    if (m_asyncRefreshPending) {
        DebugLog::log("[refresh] already in progress, queueing another pass");
        m_refreshQueued = true;
        return;
    }

    if (m_launchAnim && m_launchAnim->isPlaying()) m_launchAnim->stop();
    if (m_userSelect && m_userSelect->isActive()) m_userSelect->hide();

    m_refreshPrevPage = m_grid ? m_grid->currentPage() : 0;
    m_asyncRefreshPending = true;
    m_refreshQueued = false;

    m_appLoader.startAsync(m_threadPool);
}

void SwitchMenuApp::finalizeRefresh() {
    DebugLog::log("[refresh] finalizing (GPU upload)");
    m_asyncRefreshPending = false;

    GridModel refreshedModel;
    IconStreamer refreshedStreamer;
    m_appLoader.finalize(refreshedModel, refreshedStreamer);
    DebugLog::log("[refresh] found %d apps", refreshedModel.count());

    if (gridModelsRefreshEquivalent(m_model, refreshedModel)) {
        DebugLog::log("[refresh] unchanged, keeping existing grid");
        m_refreshCooldownFrames = 20;
        if (m_layoutDirty)
            saveMenuLayout();
        return;
    }

    app().gpu().waitIdle();
    m_grid->clearChildren();
    m_model = std::move(refreshedModel);
    m_iconStreamer = std::move(refreshedStreamer);

    std::vector<std::shared_ptr<GlossyIcon>> icons;
    for (int i = 0; i < m_model.count(); ++i) {
        auto icon = makeIcon(m_model.at(i));
        icon->setBaseColor(m_theme.iconDefault);
        icons.push_back(std::move(icon));
    }

    GridLayoutMetrics gridMetrics = computeGridLayoutMetrics();

    m_grid->setup(std::move(icons),
                  std::clamp(m_config.gridColumns, 3, 8),
                  std::clamp(m_config.gridRows, 1, 5),
                  gridMetrics.cellW, gridMetrics.cellH,
                  gridMetrics.padX, gridMetrics.padY);
    if (m_refreshPrevPage > 0) m_grid->setPage(m_refreshPrevPage);
    wireFocusCallback();
    m_grid->onPageSwitched([this]() {
        m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
        auto* target = m_grid->focusManager().current();
        if (target) focusManager().setFocus(target);
        updateCursor();
    });

    // Load textures for the restored page.
    int page = m_refreshPrevPage > 0 ? m_refreshPrevPage : 0;
    m_iconStreamer.onPageChanged(page, m_grid->iconsPerPage(),
                                 app().gpu(), app().renderer(),
                                 m_grid->allIcons());

    m_grid->startAppearAnimation();
    if (auto* firstIcon = m_grid->focusManager().current())
        focusManager().setFocus(firstIcon);

    // Keep a short cooldown to coalesce duplicate app-record notifications.
    m_refreshCooldownFrames = 20;
    applyTheme();
    if (m_layoutDirty)
        saveMenuLayout();
    DebugLog::log("[refresh] done, %d icons on page %d", m_model.count(), m_grid->currentPage());
}

#endif

void SwitchMenuApp::onUpdate(float dt) {
#ifdef QLAUNCHEXT_DEBUG_UI
    if (m_debugOverlay) {
        m_debugOverlay->setDeltaTime(dt);
    }
#endif

    if (m_returnFadeTimer > 0.f)
        m_returnFadeTimer = std::max(0.f, m_returnFadeTimer - dt);
    if (m_tutorialStartupFadeTimer > 0.f)
        m_tutorialStartupFadeTimer = std::max(0.f, m_tutorialStartupFadeTimer - dt);

    updateSettingsGearAnim(dt);

    syncThemePackageTransfer();

    if (m_deferredInitialAssetFrames > 0) {
        --m_deferredInitialAssetFrames;
        if (m_deferredInitialAssetFrames == 0) {
            DebugLog::log("[init] deferred initial icon/sidebar uploads start");
            if (m_grid) {
                m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                             app().gpu(), app().renderer(),
                                             m_grid->allIcons());
            }
            m_sidebar.reloadAssets(app().gpu(), app().renderer(), SD_ASSETS,
                                   resolveThemeAssetPath(m_effectivePreset,
                                                         m_effectivePreset.icons.basePath));
            DebugLog::log("[init] deferred initial icon/sidebar uploads done");
        }
    }

    if (m_deferredBluetoothInitFrames > 0) {
        --m_deferredBluetoothInitFrames;
        if (m_deferredBluetoothInitFrames == 0) {
            bluetooth::Initialize();
            DebugLog::log("[init] Bluetooth manager initialized (deferred)");
        }
    }

    if (!m_audioStarted && m_audioFuture.valid() &&
        m_audioFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_audioFuture.get();
        m_audio.setVolume(m_config.musicVolume);
        m_audio.setSfxVolume(m_config.sfxVolume);
        if (m_config.musicEnabled) m_audio.play();
        m_loadedSoundPreset = resolveSoundPresetId(m_config.soundPreset);
        m_audioStarted = true;
        DebugLog::log("[init] Audio ready (deferred)");
    }

    if (m_presetChangePending && m_audioFuture.valid() &&
        m_audioFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_audioFuture.get();
        m_audio.setVolume(m_config.musicVolume);
        m_audio.setSfxVolume(m_config.sfxVolume);
        if (m_config.musicEnabled)
            m_audio.play();
        m_loadedSoundPreset = m_pendingSoundPreset.empty() ? resolveSoundPresetId(m_config.soundPreset)
                                                           : m_pendingSoundPreset;
        m_pendingSoundPreset.clear();
        m_presetChangePending = false;
        DebugLog::log("[audio] Preset change complete: %s", m_config.soundPreset.c_str());
    }

    if (m_settingsNeedRefresh && m_settings) {
        m_settingsNeedRefresh = false;
        m_settings->refreshTranslations();
    }

    if (m_pendingNetConnect) {
        m_pendingNetConnect = false;
        m_launcher.launchNetConnect();
        return;
    }

#ifdef QLAUNCHEXT_MENU
    {
        AppletStorage notifySt;
        while (R_SUCCEEDED(appletPopInteractiveInData(&notifySt))) {
            qlaunchext::smi::DaemonNotification notif{};
            s64 sz = 0;
            appletStorageGetSize(&notifySt, &sz);
            if (sz >= (s64)sizeof(notif))
                appletStorageRead(&notifySt, 0, &notif, sizeof(notif));
            appletStorageClose(&notifySt);

            if (notif.magic != qlaunchext::smi::kNotifyMagic) continue;
            DebugLog::log("[notify] msg=%u", (unsigned)notif.msg);

            switch (notif.msg) {
            case qlaunchext::smi::MenuMessage::HomeRequest:
                m_sysMsg.pushAction(SysAction::HomeButton);
                break;
            case qlaunchext::smi::MenuMessage::ApplicationExited:
                m_launcher.setAppRunning(false);
                m_launcher.setAppHasForeground(false);
                m_launcher.setSuspendedTitleId(0);
                m_sysMsg.pushAction(SysAction::HomeButton);
                break;
            case qlaunchext::smi::MenuMessage::ApplicationSuspended:
                m_launcher.setAppRunning(true);
                m_launcher.setAppHasForeground(false);
                m_launcher.setSuspendedTitleId(notif.app_id);
                m_sysMsg.pushAction(SysAction::HomeButton);
                break;
            case qlaunchext::smi::MenuMessage::AppRecordsChanged:
            case qlaunchext::smi::MenuMessage::GameCardMountFailure:
                m_refreshQueued = true;
                m_deferredRefreshFrames = std::max(m_deferredRefreshFrames, 3);
                break;
            case qlaunchext::smi::MenuMessage::AppViewFlagsUpdate: {
                uint64_t tid = notif.app_id;
                uint32_t flags = notif.payload;
                m_model.updateViewFlags(tid, flags);
                for (auto& icon : m_grid->allIcons()) {
                    if (icon->titleId() == tid) {
                        bool launchable = (flags == 0) ||
                            (flags & qlaunchext::ns::AppViewFlag_CanLaunch);
                        icon->setNotLaunchable(!launchable);
                        icon->setIsGameCard(
                            flags & qlaunchext::ns::AppViewFlag_IsGameCard);
                        break;
                    }
                }
                break;
            }
            case qlaunchext::smi::MenuMessage::BatteryStatusChanged:
                if (m_battery) {
                    const uint32_t percent = qlaunchext::smi::batteryPayloadPercentage(notif.payload);
                    const bool charging = qlaunchext::smi::batteryPayloadCharging(notif.payload);
                    m_battery->setBatteryStatus(percent, charging);
                    DebugLog::log("[battery] daemon status percent=%u charging=%d",
                                  (unsigned)percent,
                                  charging ? 1 : 0);
                }
                break;
            default:
                break;
            }
        }
    }
    m_sysMsg.pump();
    if (m_refreshCooldownFrames > 0)
        --m_refreshCooldownFrames;
    if (m_deferredRefreshFrames > 0)
        --m_deferredRefreshFrames;
    if (m_refreshQueued && m_deferredRefreshFrames == 0 &&
        !m_asyncRefreshPending && m_refreshCooldownFrames == 0) {
        DebugLog::log("[update] deferred refresh triggered, starting refreshAppList");
        refreshAppList();
    }
    if (m_asyncRefreshPending && m_appLoader.isReady()) {
        finalizeRefresh();
    }
#endif

    bool debugTouchBlocked = false;
#ifdef QLAUNCHEXT_DEBUG_UI
    debugTouchBlocked = m_showDebugOverlay;
#endif

    if (m_plusExitPending) {
        m_plusExitPendingTimer -= dt;
        if (m_plusExitPendingTimer <= 0.f) {
            m_plusExitPending = false;
            m_plusExitPendingTimer = 0.f;
#ifdef QLAUNCHEXT_HOMEBREW
            m_audio.playSfx(Sfx::ModalHide);
            app().requestExit();
#endif
        }
    }

    if (!debugTouchBlocked
        && !m_launchAnim->isPlaying()
        && !(m_dialog && m_dialog->isActive())
        && !(m_themeShop && m_themeShop->isActive())
        && !(m_settings && m_settings->isActive())
        && !(m_userSelect && m_userSelect->isActive()))
    {
        handleTouch();
    }

    bool dialogActiveNow = (m_dialog && m_dialog->isActive());
    if (!debugTouchBlocked && dialogActiveNow)
        m_dialog->handleTouch(app().input());

    if (!debugTouchBlocked && m_themeShop && m_themeShop->isActive())
        m_themeShop->handleTouch(app().input());

    if (!debugTouchBlocked && m_settings && m_settings->isActive())
        m_settings->handleTouch(app().input());

    if (m_dialogWasActive && !dialogActiveNow) {
        if (isCurrentFocusableWidget(m_dialogReturnFocus)) {
            m_suppressNextNavigateSfx = true;
            focusManager().setFocus(m_dialogReturnFocus);
        }
        m_dialogReturnFocus = nullptr;
    }
    m_dialogWasActive = dialogActiveNow;

    if (!debugTouchBlocked && m_userSelect && m_userSelect->isActive())
        m_userSelect->handleTouch(app().input());

    if (!(m_userSelect && m_userSelect->isActive())
        && !(m_dialog && m_dialog->isActive())
        && !m_launchAnim->isPlaying())
    {
        auto* cur = focusManager().current();
        if (!cur || !cur->isFocusable()) {
            if (m_themeShop && m_themeShop->isActive()) {
                focusManager().setFocus(m_themeShop.get());
            } else if (m_settings && m_settings->isActive()) {
                focusManager().setFocus(m_settings.get());
            } else {
                auto* target = m_grid->focusManager().current();
                if (target)
                    focusManager().setFocus(target);
            }
        }
    }

    m_sidebar.update(dt, focusManager().current());

    if (m_pointerCursor) {
        bool showPointer = app().input().virtualPointerEnabled();
        m_pointerCursor->setVisible(showPointer);
        if (showPointer) {
            constexpr float kPointerSize = 30.f;
            float half = kPointerSize * 0.5f;
            nxui::Rect pointerRect {
                app().input().virtualPointerX() - half,
                app().input().virtualPointerY() - half,
                kPointerSize,
                kPointerSize,
            };
            m_pointerCursor->setOpacity(app().input().isTouching() ? 1.f : 0.92f);
            m_pointerCursor->moveTo(pointerRect, half, 0.06f);
        }
    }

    // Keep the home selection ring glued to its icon as the strip scrolls,
    // without restarting the fade transition every frame. A genuine selection
    // change is handled by updateCursor() (fade); here we only track motion.
    {
        bool overlayActive =
            (m_dialog && m_dialog->isActive()) ||
            (m_themeShop && m_themeShop->isActive()) ||
            (m_settings && m_settings->isActive()) ||
            (m_userSelect && m_userSelect->isActive());
        if (!overlayActive && m_cursor && m_cursor->isVisible()) {
            auto* cur = focusManager().current();
            if (cur && cur->tag() == "glossy_icon") {
                if (cur == m_cursorFollowIcon) {
                    nxui::Rect fr = cur->focusRect().expanded(4.f);
                    m_cursor->follow(fr, static_cast<GlossyIcon*>(cur)->cursorRadius(fr));
                }
                m_cursorFollowIcon = cur;
            } else {
                m_cursorFollowIcon = nullptr;
            }
        } else {
            m_cursorFollowIcon = nullptr;
        }
    }

    nxui::AnimationManager::instance().update(dt);

    // Sample the cursor after animation update to avoid one-frame lag.
    updateEditGhost(dt);

    if (m_editMode && m_editGhostIcon)
        m_editGhostIcon->update(dt);
}

std::vector<SwitchMenuApp::ActionHint> SwitchMenuApp::buildActionHints() {
    std::vector<ActionHint> hints;
    auto& i18n = nxui::I18n::instance();
    auto add = [&](const std::string& icon, const std::string& label) {
        if (!icon.empty() && !label.empty())
            hints.push_back({icon, label});
    };
    if (m_launchAnim && m_launchAnim->isPlaying())
        return hints;

    if (m_dialog && m_dialog->isActive()) {
        add(buttonGlyph(nxui::Button::A), i18n.tr("hint.confirm", "Confirm"));
        add(buttonGlyph(nxui::Button::B), i18n.tr("hint.back", "Back"));
        return hints;
    }

    if (m_userSelect && m_userSelect->isActive()) {
        add(dpadGlyph(), i18n.tr("hint.navigate", "Navigate"));
        add(buttonGlyph(nxui::Button::A), i18n.tr("hint.select", "Select"));
        add(buttonGlyph(nxui::Button::B), i18n.tr("hint.back", "Back"));
        return hints;
    }

    if (m_themeShop && m_themeShop->isActive()) {
        add(dpadGlyph(), i18n.tr("hint.navigate", "Navigate"));
        add(buttonGlyph(nxui::Button::A), i18n.tr("hint.select", "Select"));
        add(buttonGlyph(nxui::Button::B), i18n.tr("hint.back", "Back"));
        add(buttonGlyph(nxui::Button::X), i18n.tr("hint.search", "Search"));
        return hints;
    }

    if (m_settings && m_settings->isActive()) {
        add(dpadGlyph(), i18n.tr("hint.navigate", "Navigate"));
        add(buttonGlyph(nxui::Button::A), i18n.tr("hint.select", "Select"));
        add(buttonGlyph(nxui::Button::B), i18n.tr("hint.back", "Back"));
        return hints;
    }

    if (m_editMode) {
        add(dpadGlyph(), i18n.tr("hint.move", "Move"));
        add(buttonGlyph(nxui::Button::Y), i18n.tr("hint.place", "Place"));
        add(buttonGlyph(nxui::Button::B), i18n.tr("hint.cancel", "Cancel"));
        return hints;
    }

    auto addHint = [&](const std::string& icon, const std::string& label, bool enabled) {
        if (!icon.empty() && !label.empty())
            hints.push_back({icon, label, enabled});
    };

    nxui::Widget* cur = focusManager().current();
    if (cur && cur->tag() == "glossy_icon") {
        // On a game tile the Switch shows "+ Options" and "A Start". When the
        // slot is empty or the game's icon can't be found, the Options entry is
        // greyed out (disabled).
        auto* icon = static_cast<GlossyIcon*>(cur);
        const bool hasIcon = icon->texture() && icon->texture()->valid();
        const bool launchable = icon->titleId() != 0 && !icon->isNotLaunchable() && hasIcon;
        addHint(buttonGlyph(nxui::Button::Plus), i18n.tr("hint.options", "Options"), launchable);
        addHint(buttonGlyph(nxui::Button::A), i18n.tr("hint.start", "Start"), launchable);
    } else if (cur) {
        // Applets / user profiles: just "A OK" (no Options).
        add(buttonGlyph(nxui::Button::A), i18n.tr("hint.ok", "OK"));
    }

    return hints;
}

void SwitchMenuApp::renderActionHintBar(nxui::Renderer& ren) {
    // Switch-style bottom bar: a thin full-width separator line with flat,
    // right-aligned "icon label" hints. Disabled hints are greyed out.
    constexpr float kSideMargin = 30.f;
    constexpr float kBaselineY  = 686.f;   // vertical centre of the hint row
    constexpr float kSepY       = 645.f;   // separator line: 75 px above bottom
    constexpr float kIconScale  = 0.62f;
    constexpr float kTextScale  = 0.52f;
    constexpr float kIconGap    = 6.f;     // icon -> label
    constexpr float kItemGap    = 26.f;    // between hint items

    // Separator line with 30 px padding on each side, in #757575.
    ren.drawRect({30.f, kSepY, 1280.f - 60.f, 1.f},
                 nxui::Color(0.459f, 0.459f, 0.459f, 1.f));

    std::vector<ActionHint> hints = buildActionHints();
    if (hints.empty())
        return;

    // Measure total width, then lay out right-aligned.
    float totalW = 0.f;
    for (size_t i = 0; i < hints.size(); ++i) {
        nxui::Vec2 is = m_fontIcons.measure(hints[i].icon);
        nxui::Vec2 ls = m_fontSmall.measure(hints[i].label);
        totalW += is.x * kIconScale + kIconGap + ls.x * kTextScale;
        if (i + 1 < hints.size())
            totalW += kItemGap;
    }

    float x = 1280.f - kSideMargin - totalW;
    for (const auto& hint : hints) {
        const float a = hint.enabled ? 0.92f : 0.30f;
        nxui::Vec2 is = m_fontIcons.measure(hint.icon);
        nxui::Vec2 ls = m_fontSmall.measure(hint.label);
        float iconY = kBaselineY - (is.y * kIconScale) * 0.5f;
        float labelY = kBaselineY - (ls.y * kTextScale) * 0.5f;
        ren.drawText(hint.icon, {x, iconY}, &m_fontIcons,
                     m_theme.textPrimary.withAlpha(a), kIconScale);
        x += is.x * kIconScale + kIconGap;
        ren.drawText(hint.label, {x, labelY}, &m_fontSmall,
                     m_theme.textPrimary.withAlpha(a), kTextScale);
        x += ls.x * kTextScale + kItemGap;
    }
}

void SwitchMenuApp::onRender(nxui::Renderer& ren) {
    if (m_returnFadeTimer > 0.f) {
        float alpha = m_returnFadeTimer / kReturnFadeInDur;
        ren.drawRect({0, 0, 1280, 720}, nxui::Color(0, 0, 0, alpha));
    }
    if (m_tutorialStartupFadeTimer > 0.f) {
        float t = std::clamp(m_tutorialStartupFadeTimer / kTutorialStartupFadeDur, 0.f, 1.f);
        float alpha = nxui::Easing::outCubic(t);
        ren.drawRect({0, 0, 1280, 720}, nxui::Color(1.f, 1.f, 1.f, alpha));
    }

    if (m_touchHitIndex >= 0 && !m_touchOnFocused && app().input().isTouching()) {
        auto icons = m_grid->pageIcons();
        if (m_touchHitIndex < (int)icons.size()) {
            nxui::Rect r = icons[m_touchHitIndex]->focusRect();
            float cr = icons[m_touchHitIndex]->cornerRadius();
            ren.drawRoundedRect(r, nxui::Color(1.f, 1.f, 1.f, 0.18f), cr);
        }
    }

    m_pageIndicator->setPageCount(m_grid->totalPages());
    m_pageIndicator->setCurrentPage(m_grid->currentPage());

    if (m_themeRenderDebugFrames > 0) {
        nxui::Widget* focus = focusManager().current();
        nxui::Widget* focusParent = focus ? focus->parent() : nullptr;
        std::vector<GlossyIcon*> pageIcons = m_grid ? m_grid->pageIcons() : std::vector<GlossyIcon*>();
        GlossyIcon* firstPageIcon = pageIcons.empty() ? nullptr : pageIcons.front();
        const nxui::Texture* firstTexture = firstPageIcon ? firstPageIcon->texture() : nullptr;

        DebugLog::log("[theme-render] preset=%s focus=%s parent=%s rootChildren=%zu contentChildren=%zu overlayChildren=%zu grid(all=%zu page=%zu vis=%d op=%.2f firstTex=%d firstIconVis=%d firstIconOp=%.2f) settings(active=%d vis=%d op=%.2f) themeshop(active=%d vis=%d op=%.2f)",
                      m_activePresetName.c_str(),
                      safeTag(focus),
                      safeTag(focusParent),
                      rootBox().children().size(),
                      m_contentLayer ? m_contentLayer->children().size() : 0,
                      m_overlayLayer ? m_overlayLayer->children().size() : 0,
                      m_grid ? m_grid->allIcons().size() : 0,
                      pageIcons.size(),
                      m_grid && m_grid->isVisible() ? 1 : 0,
                      m_grid ? m_grid->opacity() : 0.f,
                      (firstTexture && firstTexture->valid()) ? 1 : 0,
                      (firstPageIcon && firstPageIcon->isVisible()) ? 1 : 0,
                      firstPageIcon ? firstPageIcon->opacity() : 0.f,
                      (m_settings && m_settings->isActive()) ? 1 : 0,
                      (m_settings && m_settings->isVisible()) ? 1 : 0,
                      m_settings ? m_settings->opacity() : 0.f,
                      (m_themeShop && m_themeShop->isActive()) ? 1 : 0,
                      (m_themeShop && m_themeShop->isVisible()) ? 1 : 0,
                      m_themeShop ? m_themeShop->opacity() : 0.f);
        --m_themeRenderDebugFrames;
    }

    // Final topmost pass for move-mode ghost.
    if (m_editMode && m_editGhostIcon)
        m_editGhostIcon->render(ren);

    // Overlays (settings, theme shop, dialogs) draw their own bottom hint bar;
    // don't stack the home-menu hint bar on top of them.
    const bool overlayActive =
        (m_settings && m_settings->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_dialog && m_dialog->isActive()) ||
        (m_userSelect && m_userSelect->isActive());
    if (!overlayActive)
        renderActionHintBar(ren);

#ifdef QLAUNCHEXT_DEBUG_UI
    if (m_debugOverlay) {
        m_debugOverlay->render(ren, app().input(), m_showDebugOverlay);
    }
#endif
}
