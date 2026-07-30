#include "SwitchMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "widgets/AppletButton.hpp"
#include "DebugLog.hpp"

#include <algorithm>
#include <cmath>
#include <nxui/core/Animation.hpp>
#include <nxui/core/I18n.hpp>

bool SwitchMenuApp::isEditableIcon(nxui::Widget* w) const {
    if (!w || w->tag() != "glossy_icon")
        return false;
    auto* icon = static_cast<GlossyIcon*>(w);
    return icon->titleId() != 0;
}

void SwitchMenuApp::startEditGhost(GlossyIcon* sourceIcon) {
    stopEditGhost();
    if (!sourceIcon)
        return;

    if (m_editSourceIndex >= 0)
        m_iconStreamer.setPinnedIndex(m_editSourceIndex);

    m_editSourceIcon = sourceIcon;
    m_editSourceIcon->setOpacity(0.10f);

    auto ghost = std::make_shared<GlossyIcon>();
    ghost->setTag("edit_ghost");
    ghost->setFocusable(false);
    ghost->setTitle(sourceIcon->title());
    ghost->setTitleId(sourceIcon->titleId());
    ghost->setTexture(sourceIcon->texture());
    ghost->setIsGameCard(sourceIcon->isGameCard());
    ghost->setGameCardTexture(sourceIcon->gameCardTexture());
    ghost->setNotLaunchable(sourceIcon->isNotLaunchable());
    ghost->setCornerRadius(sourceIcon->cornerRadius());
    ghost->setBlurEnabled(false);
    ghost->setPanelOpacity(0.84f);
    ghost->setOpacity(0.84f);
    ghost->setScale(1.06f);
    ghost->forceVisible();

    m_editGhostTargetRect = sourceIcon->focusRect().expanded(4.f);
    ghost->setRect(m_editGhostTargetRect);
    m_editGhostPulse = 0.f;

    m_editGhostIcon = ghost;
}

void SwitchMenuApp::stopEditGhost() {
    m_iconStreamer.clearPinnedIndex();

    if (m_editSourceIcon)
        m_editSourceIcon->setOpacity(1.f);
    m_editSourceIcon = nullptr;

    m_editGhostIcon.reset();
    m_editGhostPulse = 0.f;
}

void SwitchMenuApp::updateEditGhost(float dt) {
    if (!m_editMode || !m_editGhostIcon)
        return;

    if (m_cursor && m_cursor->isVisible()) {
        m_editGhostTargetRect = m_cursor->currentRect();
    } else if (auto* cur = focusManager().current()) {
        if (cur->tag() == "glossy_icon")
            m_editGhostTargetRect = cur->focusRect().expanded(4.f);
    }

    m_editGhostPulse += dt;
    float pulse = 0.80f + 0.08f * std::sin(m_editGhostPulse * 8.f);
    m_editGhostIcon->setOpacity(pulse);
    m_editGhostIcon->setPanelOpacity(std::min(1.f, pulse + 0.12f));
    m_editGhostIcon->setScale(1.07f + 0.025f * std::sin(m_editGhostPulse * 7.f));

    m_editGhostIcon->setRect(m_editGhostTargetRect);
}

void SwitchMenuApp::unbindEditActions() {
    if (!m_editBoundIcon)
        return;
    m_editBoundIcon->clearActions();
    m_editBoundIcon = nullptr;
}

void SwitchMenuApp::bindEditActions(GlossyIcon* icon) {
    if (!icon)
        return;
    if (m_editBoundIcon == icon)
        return;

    unbindEditActions();
    m_editBoundIcon = icon;

    icon->addAction(static_cast<uint64_t>(nxui::Button::A), []() {});
    icon->addAction(static_cast<uint64_t>(nxui::Button::B), [this]() {
        exitEditMode();
        m_audio.playSfx(Sfx::ModalHide);
    });
}

void SwitchMenuApp::enterEditMode() {
    auto* cur = focusManager().current();
    if (!isEditableIcon(cur))
        return;

    auto* icon = static_cast<GlossyIcon*>(cur);
    m_editMode = true;
    m_editSourceIndex = m_grid ? m_grid->focusedGlobalIndex() : -1;
    m_editHeldTitle = icon->title();
    startEditGhost(icon);
    bindEditActions(icon);
    updateSelectionTitle();
}

void SwitchMenuApp::exitEditMode() {
    if (!m_editMode)
        return;

    m_editMode = false;
    unbindEditActions();
    m_editSourceIndex = -1;
    m_editHeldTitle.clear();
    stopEditGhost();

    updateSelectionTitle();

    if (m_layoutDirty)
        saveMenuLayout();
}

bool SwitchMenuApp::commitEditModePlacement() {
    if (!m_editMode || !m_grid)
        return false;

    int from = m_editSourceIndex;
    int target = m_grid->focusedGlobalIndex();
    if (from < 0 || target < 0 || from >= m_model.count() || target >= m_model.count())
        return false;
    if (m_model.at(from).titleId == 0)
        return false;

    int oldPage = m_grid->currentPage();
    bool changed = (from != target);
    if (changed) {
        if (!m_layoutSlots.empty() && from < (int)m_layoutSlots.size() && target < (int)m_layoutSlots.size())
            std::swap(m_layoutSlots[from], m_layoutSlots[target]);

        m_model.swapEntries(from, target);
        m_iconStreamer.swapIndices(from, target);
        m_grid->swapSlots(from, target);
        m_editSourceIndex = target;
        m_iconStreamer.setPinnedIndex(m_editSourceIndex);
        m_layoutDirty = true;
    }

    m_grid->focusGlobalIndex(target);

    int newPage = m_grid->currentPage();
    if (changed || newPage != oldPage) {
        m_iconStreamer.onPageChanged(newPage, m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
    }
    for (auto* icon : m_grid->pageIcons()) {
        if (icon)
            icon->forceVisible();
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);

    if (m_editGhostIcon) {
        if (auto* focused = m_grid->focusManager().current())
            m_editGhostTargetRect = focused->focusRect().expanded(4.f);
    }

    updateCursor();
    return true;
}

bool SwitchMenuApp::moveFocusedIcon(nxui::FocusDirection dir) {
    if (!m_editMode || !m_grid)
        return false;

    int from = m_grid->focusedGlobalIndex();
    if (from < 0 || from >= m_model.count())
        return false;
    if (m_model.at(from).titleId == 0)
        return false;

    // The home strip lays icons out column-major: each virtual column stacks
    // `rows` icons, and columns scroll horizontally.
    int rows = std::max(1, m_grid->rowsPerPage());
    int count = m_model.count();
    int row = from % rows;

    int target = from;
    switch (dir) {
        case nxui::FocusDirection::LEFT:
            if (from - rows >= 0)
                target = from - rows;
            else
                return false;
            break;
        case nxui::FocusDirection::RIGHT:
            if (from + rows < count)
                target = from + rows;
            else
                return false;
            break;
        case nxui::FocusDirection::UP:
            if (row > 0)
                target = from - 1;
            else
                return false;
            break;
        case nxui::FocusDirection::DOWN:
            if (row + 1 < rows && from + 1 < count)
                target = from + 1;
            else
                return false;
            break;
    }

    if (target < 0 || target >= m_model.count())
        return false;
    if (target == from)
        return true;

    if (!m_layoutSlots.empty() && from < (int)m_layoutSlots.size() && target < (int)m_layoutSlots.size())
        std::swap(m_layoutSlots[from], m_layoutSlots[target]);

    m_model.swapEntries(from, target);
    m_iconStreamer.swapIndices(from, target);
    m_grid->swapSlots(from, target);
    m_editSourceIndex = target;
    m_iconStreamer.setPinnedIndex(m_editSourceIndex);
    m_grid->focusGlobalIndex(target);

    int newPage = m_grid->currentPage();
    m_iconStreamer.onPageChanged(newPage, m_grid->iconsPerPage(),
                                 app().gpu(), app().renderer(),
                                 m_grid->allIcons());
    for (auto* icon : m_grid->pageIcons()) {
        if (icon)
            icon->forceVisible();
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);

    auto* cur = focusManager().current();
    if (isEditableIcon(cur)) {
        auto* icon = static_cast<GlossyIcon*>(cur);
        bindEditActions(icon);
    }
    updateSelectionTitle();

    m_layoutDirty = true;
    updateCursor();
    return true;
}

void SwitchMenuApp::wireFocusCallback() {
    focusManager().onFocusChanged([this](nxui::Widget*, nxui::Widget* cur) {
        updateCursor();

        if ((m_dialog && m_dialog->isActive()) ||
            (m_themeShop && m_themeShop->isActive()) ||
            (m_settings && m_settings->isActive()) ||
            (m_userSelect && m_userSelect->isActive()))
            return;

        bool suppressSfx = m_suppressNextNavigateSfx;
        m_suppressNextNavigateSfx = false;
        if (!suppressSfx)
            m_audio.playSfx(Sfx::Navigate);

        // The selected-item title is driven each frame by updateSelectionTitle();
        // here we only maintain edit-mode binding / ghost state.
        if (cur && cur->tag() == "glossy_icon") {
            m_grid->focusManager().setFocus(cur);
            if (m_editMode) {
                auto* icon = static_cast<GlossyIcon*>(cur);
                bindEditActions(icon);
                m_editGhostTargetRect = icon->focusRect();
            }
        } else if (cur) {
            if (m_editMode)
                exitEditMode();
        }
    });
    updateCursor();
    updateSelectionTitle();
}

bool SwitchMenuApp::isCurrentFocusableWidget(nxui::Widget* w) const {
    if (!w) return false;
    if (m_themeShop && m_themeShop.get() == w) return w->isFocusable();
    if (m_settings && m_settings.get() == w) return w->isFocusable();
    for (const auto& btn : m_sidebar.leftButtons())
        if (btn.get() == w) return w->isFocusable();
    for (const auto& btn : m_sidebar.rightButtons())
        if (btn.get() == w) return w->isFocusable();
    for (const auto& avatar : m_userAvatarButtons)
        if (avatar.get() == w) return w->isFocusable();
    if (m_grid)
        for (const auto& icon : m_grid->allIcons())
            if (icon.get() == w) return w->isFocusable();
    return false;
}

int SwitchMenuApp::findTitleIndex(uint64_t titleId) const {
    if (titleId == 0)
        return -1;
    for (int i = 0; i < m_model.count(); ++i) {
        if (m_model.at(i).titleId == titleId)
            return i;
    }
    return -1;
}

bool SwitchMenuApp::focusTitle(uint64_t titleId) {
    if (!m_grid)
        return false;

    int idx = findTitleIndex(titleId);
    if (idx < 0)
        return false;

    int oldPage = m_grid->currentPage();
    if (!m_grid->focusGlobalIndex(idx))
        return false;

    if (m_grid->currentPage() != oldPage || titleId != 0) {
        m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);
    updateCursor();
    return true;
}

void SwitchMenuApp::pageJumpFocus(int dir) {
    if (!m_grid) return;
    if ((m_dialog && m_dialog->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_userSelect && m_userSelect->isActive()))
        return;

    int fi = m_grid->focusedGlobalIndex();
    if (fi < 0) return;

    int per = std::max(1, m_grid->iconsPerPage());
    int target = std::clamp(fi + dir * per, 0, m_model.count() - 1);
    if (target == fi) return;

    if (m_grid->focusGlobalIndex(target)) {
        if (auto* cur = m_grid->focusManager().current())
            focusManager().setFocus(cur);
        m_audio.playSfx(Sfx::PageChange);
    }
}

// Wrap horizontal navigation around the home strip: pressing left on the first
// app jumps to the last one (and vice-versa). Only fired on a fresh press, so
// holding pauses at the edge and re-pressing wraps.
bool SwitchMenuApp::onEdgeNavigate(nxui::FocusDirection dir) {
    if (!m_grid) return false;
    if ((m_dialog && m_dialog->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_userSelect && m_userSelect->isActive()))
        return false;
    if (m_editMode) return false;

    nxui::Widget* cur = focusManager().current();
    if (!cur || cur->tag() != "glossy_icon") return false;

    // Up from any app with nothing above it jumps to the user page (avatar).
    if (dir == nxui::FocusDirection::UP) {
        if (!m_userAvatarButtons.empty()) {
            focusManager().setFocus(m_userAvatarButtons.front().get());
            return true;
        }
        return false;
    }

    const auto& icons = m_grid->allIcons();
    int firstIdx = -1, lastIdx = -1;
    for (int i = 0; i < (int)icons.size(); ++i) {
        if (icons[i] && icons[i]->isFocusable()) {
            if (firstIdx < 0) firstIdx = i;
            lastIdx = i;
        }
    }
    if (firstIdx < 0 || firstIdx == lastIdx) return false;

    int curIdx = m_grid->focusedGlobalIndex();
    int target = -1;
    if (dir == nxui::FocusDirection::LEFT && curIdx == firstIdx)
        target = lastIdx;
    else if (dir == nxui::FocusDirection::RIGHT && curIdx == lastIdx)
        target = firstIdx;
    if (target < 0) return false;

    if (m_grid->focusGlobalIndex(target)) {
        // Activity focus change below already plays the navigate SFX.
        if (auto* c = m_grid->focusManager().current())
            focusManager().setFocus(c);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings gear open animation: the gear shakes (~2 px, 0.25 s) while Settings.wav
// plays, spins with an exponential-bounce 0→100°→0 over 0.5 s, holds 0.25 s, then
// opens the System Settings screen.
// ─────────────────────────────────────────────────────────────────────────────
void SwitchMenuApp::openSettingsNow() {
    m_audio.playSfx(Sfx::ModalShow);
    if (m_settings) {
        if (m_themeShop && m_themeShop->isActive())
            m_themeShop->hide();
        m_settings->show();
        focusManager().setFocus(m_settings.get());
    }
}

void SwitchMenuApp::startSettingsOpenAnim() {
    if (m_gearPhase != GearAnim::None)
        return;   // already animating
    if (!m_settingsGearBtn) {
        openSettingsNow();
        return;
    }
    m_gearPhase = GearAnim::Shake;
    m_gearTimer = 0.f;
    m_audio.playSfx(Sfx::SettingsOpen);
}

void SwitchMenuApp::updateSettingsGearAnim(float dt) {
    if (m_gearPhase == GearAnim::None)
        return;

    constexpr float kShakeDur  = 0.25f;
    constexpr float kRotateDur = 0.50f;
    constexpr float kWaitDur   = 0.25f;
    constexpr float kPeakDeg   = 100.f;
    constexpr float kDeg2Rad   = 3.14159265f / 180.f;

    m_gearTimer += dt;
    AppletButton* g = m_settingsGearBtn;

    switch (m_gearPhase) {
    case GearAnim::Shake: {
        // ~2 px jitter that eases out toward the end of the phase.
        float u = std::clamp(m_gearTimer / kShakeDur, 0.f, 1.f);
        float amp = 2.f * (1.f - u);
        if (g) g->setShakeOffset({ std::sin(m_gearTimer * 90.f) * amp,
                                   std::sin(m_gearTimer * 74.f + 1.3f) * amp });
        if (m_gearTimer >= kShakeDur) {
            if (g) g->setShakeOffset({0.f, 0.f});
            m_gearPhase = GearAnim::Rotate;
            m_gearTimer = 0.f;
        }
        break;
    }
    case GearAnim::Rotate: {
        float u = std::clamp(m_gearTimer / kRotateDur, 0.f, 1.f);
        // Exponential rise to the peak, then a bounce back to zero.
        float curve = (u < 0.45f)
            ? nxui::Easing::outExpo(u / 0.45f)
            : 1.f - nxui::Easing::outBounce((u - 0.45f) / 0.55f);
        if (g) g->setIconRotation(kPeakDeg * curve * kDeg2Rad);
        if (m_gearTimer >= kRotateDur) {
            if (g) g->setIconRotation(0.f);
            m_gearPhase = GearAnim::Wait;
            m_gearTimer = 0.f;
        }
        break;
    }
    case GearAnim::Wait:
        if (m_gearTimer >= kWaitDur) {
            m_gearPhase = GearAnim::None;
            m_gearTimer = 0.f;
            openSettingsNow();
        }
        break;
    default:
        m_gearPhase = GearAnim::None;
        break;
    }
}

void SwitchMenuApp::markSuspendedIcon(uint64_t titleId) {
    if (!m_grid)
        return;
    for (auto& icon : m_grid->allIcons())
        icon->setSuspended(titleId != 0 && icon->titleId() == titleId);
    if (titleId != 0)
        focusTitle(titleId);

    updateSelectionTitle();
}

void SwitchMenuApp::closeActiveOverlays() {
    if (m_editMode)
        exitEditMode();
    if (m_userSelect && m_userSelect->isActive())
        m_userSelect->hide();
    if (m_dialog && m_dialog->isActive())
        m_dialog->hide();
    if (m_settings && m_settings->isActive())
        m_settings->hide();
    if (m_themeShop && m_themeShop->isActive())
        m_themeShop->hide();
}

nxui::Widget* SwitchMenuApp::focusRoot() {
    if (m_launchAnim && m_launchAnim->isPlaying()) return nullptr;
    if (m_dialog && m_dialog->isActive()) return m_dialog.get();
    if (m_themeShop && m_themeShop->isActive()) return m_themeShop.get();
    if (m_settings && m_settings->isActive()) return m_settings.get();
    if (m_userSelect && m_userSelect->isActive()) return m_userSelect.get();
    return &rootBox();
}

void SwitchMenuApp::wireGlobalActions() {
    auto& root = rootBox();

    root.addAction(static_cast<uint64_t>(nxui::Button::ZL), [this]() {
        pageJumpFocus(-1);
    });
    root.addAction(static_cast<uint64_t>(nxui::Button::ZR), [this]() {
        pageJumpFocus(+1);
    });
    root.addAction(static_cast<uint64_t>(nxui::Button::Y), [this]() {
        if ((m_dialog && m_dialog->isActive()) ||
            (m_themeShop && m_themeShop->isActive()) ||
            (m_settings && m_settings->isActive()) ||
            (m_userSelect && m_userSelect->isActive())) {
            return;
        }

        if (m_editMode) {
            bool changed = commitEditModePlacement();
            exitEditMode();
            m_audio.playSfx(changed ? Sfx::ConfirmPositive : Sfx::ModalHide);
            return;
        }

        auto* cur = focusManager().current();
        if (!isEditableIcon(cur))
            return;

        enterEditMode();
        m_audio.playSfx(Sfx::Activate);
    });
#ifdef QLAUNCHEXT_DEBUG_UI
    root.addAction(static_cast<uint64_t>(nxui::Button::Minus), [this]() {
        m_showDebugOverlay = !m_showDebugOverlay;
        DebugLog::log("[debug] ImGui overlay toggled: %d", m_showDebugOverlay ? 1 : 0);
    });
#endif
#ifdef QLAUNCHEXT_HOMEBREW
    root.addAction(static_cast<uint64_t>(nxui::Button::Plus), [this]() {
        m_plusExitPending = true;
        m_plusExitPendingTimer = 0.80f;
    });
#endif

#ifdef QLAUNCHEXT_MENU
    root.addAction(static_cast<uint64_t>(nxui::Button::X), [this]() {
        if (m_editMode) return;
        if (m_launcher.suspendedTitleId() == 0) return;
        auto* cur = focusManager().current();
        if (!cur || cur->tag() != "glossy_icon") return;
        auto* icon = static_cast<GlossyIcon*>(cur);
        if (!m_launcher.isAppSuspended(icon->titleId())) return;

        m_audio.playSfx(Sfx::ModalShow);
        m_dialogReturnFocus = cur;
        auto& i18n = nxui::I18n::instance();
        m_dialog->show(
            i18n.tr("game.close_title", "Close game"),
            i18n.tr("game.close_prefix", "Close") + std::string(" ") + icon->title()
                + i18n.tr("game.close_suffix", "?\nUnsaved progress will be lost."),
            {
                {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
                {i18n.tr("button.close", "Close"),  [this]() {
                    m_launcher.terminateApplication();
                    m_launcher.setAppRunning(false);
                    m_launcher.setAppHasForeground(false);
                    m_launcher.setSuspendedTitleId(0);
                    for (auto& ic : m_grid->allIcons())
                        ic->setSuspended(false);
                    updateSelectionTitle();
                }, true}
            },
            1,
            {}
        );
        focusManager().setFocus(m_dialog.get());
    });
#endif
}

void SwitchMenuApp::handleTouch() {
    constexpr float kLongPressThreshold = 0.55f;
    constexpr float kLongPressMoveThreshold = 18.f;

    auto& input = app().input();

    auto hitAvatar = [this](float x, float y) -> UserAvatarButton* {
        for (auto& avatar : m_userAvatarButtons) {
            if (avatar && avatar->isVisible() && avatar->hitTest(x, y))
                return avatar.get();
        }
        return nullptr;
    };

    auto focusTouchedIcon = [this](int global) -> GlossyIcon* {
        if (!m_grid || global < 0)
            return nullptr;

        if (!m_grid->focusGlobalIndex(global))
            return nullptr;

        auto* cur = m_grid->focusManager().current();
        if (!cur)
            return nullptr;

        focusManager().setFocus(cur);
        if (m_cursor) {
            m_cursor->moveTo(cur->focusRect().expanded(4.f), 0.f);
            m_cursor->setVisible(true);
        } else {
            updateCursor();
        }

        if (!isEditableIcon(cur))
            return nullptr;
        return static_cast<GlossyIcon*>(cur);
    };

    if (input.touchDown()) {
        float tx = input.touchX();
        float ty = input.touchY();
        m_touchAvatarTarget = hitAvatar(tx, ty);
        m_touchAvatarWasFocused = m_touchAvatarTarget && (focusManager().current() == m_touchAvatarTarget);
        if (m_touchAvatarTarget) {
            m_touchHitIndex = -1;
            m_touchOnFocused = false;
            m_touchEditDragActive = false;
            return;
        }

        int hit = m_grid->hitTest(tx, ty);
        m_touchHitIndex = hit;   // GLOBAL icon index (or -1)
        m_touchOnFocused = false;
        m_touchEditDragActive = false;
        m_gridDragScrolling = false;
        m_gridLastTouchX = tx;
        if (hit >= 0 && hit < (int)m_grid->allIcons().size())
            m_touchOnFocused = (m_grid->allIcons()[hit].get() == focusManager().current());
    }

    // Horizontal drag anywhere over the strip scrolls it (unless editing).
    if (input.isTouching() && !m_editMode) {
        float dx = input.touchDeltaX();
        float dy = input.touchDeltaY();
        if (!m_gridDragScrolling && std::abs(dx) > 16.f && std::abs(dx) > std::abs(dy)) {
            m_gridDragScrolling = true;
            m_grid->beginManualScroll();
            m_gridLastTouchX = input.touchX();
            m_touchHitIndex = -1;   // cancel tap-select
        }
        if (m_gridDragScrolling) {
            float cx = input.touchX();
            m_grid->scrollByPixels(-(cx - m_gridLastTouchX));
            m_gridLastTouchX = cx;
        }
    }

    if (input.isTouching() && m_touchHitIndex >= 0 && !m_gridDragScrolling) {
        float dx = input.touchDeltaX();
        float dy = input.touchDeltaY();

        if (!m_editMode
            && std::abs(dx) <= kLongPressMoveThreshold
            && std::abs(dy) <= kLongPressMoveThreshold
            && input.touchDuration() >= kLongPressThreshold)
        {
            if (auto* icon = focusTouchedIcon(m_touchHitIndex)) {
                enterEditMode();
                if (m_editMode) {
                    m_touchEditDragActive = true;
                    m_audio.playSfx(Sfx::Activate);
                    m_editGhostTargetRect = icon->focusRect().expanded(4.f);
                }
            }
        }

        if (m_editMode && m_touchEditDragActive) {
            int dragHit = m_grid->hitTest(input.touchX(), input.touchY());
            if (dragHit >= 0)
                focusTouchedIcon(dragHit);
        }
    }

    if (input.touchUp()) {
        if (m_touchAvatarTarget) {
            float dx = input.touchDeltaX();
            float dy = input.touchDeltaY();
            UserAvatarButton* avatar = m_touchAvatarTarget;
            m_touchAvatarTarget = nullptr;
            if (std::abs(dx) < 20.f && std::abs(dy) < 20.f &&
                hitAvatar(input.touchX(), input.touchY()) == avatar)
            {
                focusManager().setFocus(avatar);
                if (!m_touchAvatarWasFocused)
                    avatar->activate();
            }
            m_touchAvatarWasFocused = false;
            return;
        }

        if (m_editMode && m_touchEditDragActive) {
            bool changed = commitEditModePlacement();
            exitEditMode();
            m_audio.playSfx(changed ? Sfx::ConfirmPositive : Sfx::ModalHide);
            m_touchHitIndex = -1;
            m_touchEditDragActive = false;
            return;
        }

        if (m_gridDragScrolling) {
            m_grid->endManualScroll();
            if (auto* cur = m_grid->focusManager().current())
                focusManager().setFocus(cur);
            m_gridDragScrolling = false;
            m_touchHitIndex = -1;
            return;
        }

        // Plain taps (small movement) are handled by the framework's
        // tap-to-focus/activate model; just clear local drag state here.
        m_touchHitIndex = -1;
        m_touchEditDragActive = false;
    }
}

#ifdef QLAUNCHEXT_MENU
void SwitchMenuApp::handleSystemAction(SysAction a) {
    switch (a) {
        case SysAction::HomeButton:
            DebugLog::log("[pump] HomeButton -> UI update");
            m_launcher.setAppHasForeground(false);

            markSuspendedIcon(m_launcher.suspendedTitleId());
            closeActiveOverlays();
            focusTitle(m_launcher.suspendedTitleId());
            break;
        default:
            break;
    }
}
#endif

void SwitchMenuApp::updateCursor() {
    if ((m_themeShop && m_themeShop->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_dialog && m_dialog->isActive()) ||
        (m_userSelect && m_userSelect->isActive()))
        return;

    auto* cur = focusManager().current();
    // Whether the cursor was on-screen last update. When it re-appears after
    // being hidden (e.g. moving off a game tile onto a dock bubble), we snap it
    // in rather than fading from its stale position (which flickers).
    const bool wasVisible = m_cursor->isVisible();
    if (cur) {
        nxui::Rect fr = cur->focusRect().expanded(4.f);

        // Dock applet buttons and user avatars get a circular selection ring;
        // game tiles keep a ring matching the tile corner radius.
        bool isCircular = false;
        for (const auto& b : m_sidebar.leftButtons())  if (b.get() == cur) { isCircular = true; break; }
        if (!isCircular)
            for (const auto& b : m_sidebar.rightButtons()) if (b.get() == cur) { isCircular = true; break; }
        if (!isCircular)
            for (const auto& a : m_userAvatarButtons) if (a.get() == cur) { isCircular = true; break; }

        if (isCircular) {
            float radius = std::min(fr.width, fr.height) * 0.5f;
            if (wasVisible) m_cursor->moveTo(fr, radius, 0.18f);
            else            m_cursor->snap(fr, radius);
            m_cursor->setVisible(true);
        } else if (cur->tag() == "glossy_icon") {
            // Games use the accent gap frame drawn by the tile itself as their
            // selection; keep the cursor tracking (for the edit-move ghost) but
            // only render it in edit mode.
            float radius = static_cast<GlossyIcon*>(cur)->cursorRadius(fr);
            if (wasVisible) m_cursor->moveTo(fr, radius, 0.18f);
            else            m_cursor->snap(fr, radius);
            m_cursor->setVisible(m_editMode);
        } else {
            if (wasVisible) m_cursor->moveTo(fr);
            else            m_cursor->snap(fr, fr.height * 0.5f);
            m_cursor->setVisible(true);
        }
    } else {
        m_cursor->setVisible(false);
    }
}

void SwitchMenuApp::updateSelectionTitle() {
    if (!m_titlePill) return;
    if ((m_dialog && m_dialog->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_userSelect && m_userSelect->isActive()) ||
        (m_launchAnim && m_launchAnim->isPlaying())) {
        m_titlePill->hide();
        return;
    }

    auto* cur = focusManager().current();
    if (!cur) { m_titlePill->hide(); return; }

    if (cur->tag() == "glossy_icon") {
        auto* icon = static_cast<GlossyIcon*>(cur);
        auto& i18n = nxui::I18n::instance();
        if (m_editMode) {
            const std::string held = !m_editHeldTitle.empty() ? m_editHeldTitle : icon->title();
            std::string text = held.empty()
                ? i18n.tr("game.move", "Move")
                : i18n.tr("game.move_prefix", "Move: ") + held;
            m_titlePill->showGame(text, icon->focusRect(), 6.f, nullptr);
            return;
        }
        if (icon->titleId() == 0) { m_titlePill->hide(); return; }
        // Game-card indicator beside the title: the card icon (natural colours)
        // when inserted, or the text-coloured "no card inserted" icon when out.
        nxui::Texture* gc = nullptr;
        bool gcTinted = true;
        if (icon->isGameCard()) {
            const bool inserted = !icon->isNotLaunchable();
            gc = inserted ? &m_gameCardTex : &m_gameCardNotInsertTex;
            gcTinted = !inserted;
        }
        m_titlePill->showGame(icon->title(), icon->focusRect(), 6.f, gc, gcTinted);
        return;
    }

    for (const auto& b : m_sidebar.leftButtons())
        if (b.get() == cur) { m_titlePill->showBubble(b->label(), b->focusRect()); return; }
    for (const auto& b : m_sidebar.rightButtons())
        if (b.get() == cur) { m_titlePill->showBubble(b->label(), b->focusRect()); return; }
    for (const auto& av : m_userAvatarButtons)
        if (av.get() == cur) {
            if (av->nickname().empty()) m_titlePill->hide();
            else m_titlePill->showBubble(av->nickname(), av->focusRect());
            return;
        }

    m_titlePill->hide();
}
