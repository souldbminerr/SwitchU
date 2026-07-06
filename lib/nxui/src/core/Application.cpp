#include <nxui/Application.hpp>
#include <nxui/Activity.hpp>
#include <nxui/core/Animation.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Input.hpp>
#include <nxui/focus/FocusManager.hpp>
#include <switch.h>

namespace nxui {

Application::~Application() {
    shutdown();
}

void Application::setActivity(std::unique_ptr<Activity> activity) {
    m_activity = std::move(activity);
    if (m_activity) m_activity->m_app = this;
}

void Application::requestActivity(std::unique_ptr<Activity> activity) {
    m_pendingActivity = std::move(activity);
    if (m_pendingActivity)
        m_pendingActivity->m_app = this;
}

bool Application::applyPendingActivity() {
    if (!m_pendingActivity)
        return true;

    AnimationManager::instance().clear();
    if (m_activity)
        m_activity->onDestroy();

    m_activity = std::move(m_pendingActivity);
    m_navDebounce = 0;

    if (m_activity) {
        m_activity->m_rootBox->setRect({0, 0, (float)m_gpu.width(), (float)m_gpu.height()});
        return m_activity->onCreate();
    }
    return true;
}

bool Application::initialize() {
    if (!m_gpu.initialize()) return false;

    m_renderer = std::make_unique<Renderer>(m_gpu);
    if (!m_renderer->initialize()) return false;

    m_input.initialize();

    // Present one clean frame immediately so that stale framebuffer
    // content from a previous process is never visible on screen.
    m_gpu.beginFrame();
    m_renderer->beginFrame();
    m_renderer->endFrame();
    m_gpu.endFrame();

    if (m_activity) {
        // Set the root box to cover the entire screen
        m_activity->m_rootBox->setRect({0, 0, (float)m_gpu.width(), (float)m_gpu.height()});
        if (!m_activity->onCreate()) return false;
    }
    return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// Automatic input dispatch
// ══════════════════════════════════════════════════════════════════════════════

void Application::dispatchInput() {
    if (!m_activity) return;

    Widget* root = m_activity->focusRoot();
    if (!root) return;      // nullptr = all input blocked this frame

    auto& fm = m_activity->focusManager();

    // Keep focus constrained to the currently active input root.
    // Without this, stale focus from another UI layer (e.g. home grid while
    // settings/dialog is active) can still receive A/button dispatches.
    auto isUnderRoot = [root](Widget* w) {
        for (Widget* it = w; it != nullptr; it = it->parent()) {
            if (it == root) return true;
        }
        return false;
    };
    Widget* curFocus = fm.current();
    if (!curFocus || !isUnderRoot(curFocus)) {
        if (root->isFocusable()) {
            fm.setFocus(root);
        } else {
            std::vector<Widget*> focusables;
            root->collectFocusable(focusables);
            if (!focusables.empty())
                fm.setFocus(focusables[0]);
        }
    }

    // D-pad / stick navigation with key-repeat: a fresh press fires immediately,
    // then holding repeats after an initial delay at ~4 presses/second.
    struct NavDir { Button dpad, leftStick, rightStick; FocusDirection dir; };
    static const NavDir kDirs[4] = {
        { Button::DLeft,  Button::LStickL, Button::RStickL, FocusDirection::LEFT  },
        { Button::DRight, Button::LStickR, Button::RStickR, FocusDirection::RIGHT },
        { Button::DUp,    Button::LStickU, Button::RStickU, FocusDirection::UP    },
        { Button::DDown,  Button::LStickD, Button::RStickD, FocusDirection::DOWN  },
    };
    constexpr int kNavInitialDelay = 16;  // ~0.27 s before repeat starts
    constexpr int kNavRepeatEvery  = 5;   // ~0.083 s -> ~12 / second

    auto dirDown = [&](const NavDir& d) {
        return m_input.isDown(d.dpad) || m_input.isDown(d.leftStick) || m_input.isDown(d.rightStick);
    };
    auto dirHeld = [&](const NavDir& d) {
        return m_input.isHeld(d.dpad) || m_input.isHeld(d.leftStick) || m_input.isHeld(d.rightStick);
    };

    int fireDir = -1;
    bool fireFresh = false;
    for (int i = 0; i < 4; ++i) {
        if (dirDown(kDirs[i])) {            // fresh press takes over immediately
            fireDir = i;
            fireFresh = true;
            m_navActiveDir = i;
            m_navRepeatTimer = kNavInitialDelay;
            break;
        }
    }
    if (fireDir < 0 && m_navActiveDir >= 0) {
        if (dirHeld(kDirs[m_navActiveDir])) {
            if (--m_navRepeatTimer <= 0) {
                fireDir = m_navActiveDir;
                m_navRepeatTimer = kNavRepeatEvery;
            }
        } else {
            m_navActiveDir = -1;            // direction released
        }
    }

    if (fireDir >= 0) {
        const NavDir& d = kDirs[fireDir];
        Widget* cur = fm.current();
        bool consumed = false;
        if (cur) {
            if (m_input.isHeld(d.dpad)       && cur->fireAction(static_cast<uint64_t>(d.dpad)))       consumed = true;
            else if (m_input.isHeld(d.leftStick)  && cur->fireAction(static_cast<uint64_t>(d.leftStick)))  consumed = true;
            else if (m_input.isHeld(d.rightStick) && cur->fireAction(static_cast<uint64_t>(d.rightStick))) consumed = true;
        }
        if (!consumed) {
            bool moved = fm.navigate(d.dir, root);
            // A fresh press that couldn't move (focus is at an edge) lets the
            // activity implement wrap-around. Held repeats are excluded, so
            // holding pauses at the edge until the button is pressed again.
            if (!moved && fireFresh)
                m_activity->onEdgeNavigate(d.dir);
        }
    }

    // Dispatch non-D-pad actions with parent bubbling.
    // Exclude D-pad buttons so they aren't fired a second time.
    constexpr uint64_t kDpadMask =
        static_cast<uint64_t>(Button::DLeft)   | static_cast<uint64_t>(Button::DRight)  |
        static_cast<uint64_t>(Button::DUp)     | static_cast<uint64_t>(Button::DDown)   |
        static_cast<uint64_t>(Button::LStickL) | static_cast<uint64_t>(Button::LStickR) |
        static_cast<uint64_t>(Button::LStickU) | static_cast<uint64_t>(Button::LStickD) |
        static_cast<uint64_t>(Button::RStickL) | static_cast<uint64_t>(Button::RStickR) |
        static_cast<uint64_t>(Button::RStickU) | static_cast<uint64_t>(Button::RStickD);

    constexpr uint64_t kA = static_cast<uint64_t>(Button::A);
    bool pointerConsumesA = m_input.pointerConsumesButton(Button::A);
    uint64_t actionExcludeMask = kDpadMask;
    if (pointerConsumesA)
        actionExcludeMask |= kA;

    uint64_t consumed = fm.dispatchActions(m_input, actionExcludeMask);

    // Auto-activate with A.
    // If the focused widget didn't register an explicit addAction(A, ...),
    // fall through to the legacy activate() / setOnActivate() mechanism.
    if (!pointerConsumesA && !(consumed & kA) && m_input.isDown(Button::A)) {
        if (auto* w = fm.current())
            w->activate();
    }

    // Touch-based focus navigation.
    // Some screens own richer touch handling locally (drag/scroll/menus) and
    // should not also receive the generic focus-manager tap model.
    if (root->frameworkTouchEnabled())
        fm.handleTouch(m_input, root);
}

// ══════════════════════════════════════════════════════════════════════════════
// Main loop
// ══════════════════════════════════════════════════════════════════════════════

void Application::run() {
    uint64_t prevTick = armGetSystemTick();

    while (m_running) {
        uint64_t nowTick = armGetSystemTick();
        float dt = static_cast<float>(nowTick - prevTick)
                 / static_cast<float>(armGetSystemTickFreq());
        prevTick = nowTick;
        if (dt > 0.1f) dt = 0.016f;

        m_input.update();
        dispatchInput();

        if (m_activity) {
            m_activity->onUpdate(dt);
            if (!applyPendingActivity()) {
                m_running = false;
                break;
            }
            m_activity->m_rootBox->update(dt);

            if (m_renderEnabled) {
                m_gpu.beginFrame();
                m_renderer->beginFrame();
                m_activity->m_rootBox->render(*m_renderer);
                m_activity->onRender(*m_renderer);
                m_renderer->endFrame();
                m_gpu.endFrame();
            } else {
                // Yield CPU while another app owns the foreground.
                svcSleepThread(100000000LL); // 100 ms
            }
        }
    }
}

void Application::shutdown() {
    // Clear all pending animations before destroying the activity so that
    // tween callbacks don't fire on already-destroyed widgets.
    AnimationManager::instance().clear();

    m_input.shutdown();

    if (m_activity) {
        m_activity->onDestroy();
        m_activity.reset();
    }
    m_pendingActivity.reset();
    m_renderer.reset();
    m_gpu.shutdown();
}

} // namespace nxui
