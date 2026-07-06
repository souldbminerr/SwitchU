#include "SelectionCursor.hpp"
#include <nxui/core/Renderer.hpp>
#include <cmath>


SelectionCursor::SelectionCursor() {
    m_x.setImmediate(0); m_y.setImmediate(0);
    m_w.setImmediate(0); m_h.setImmediate(0);
    m_cornerRadius.setImmediate(18.f);
}

float SelectionCursor::computeAdaptiveDuration(const nxui::Rect& target,
                                               float targetCornerRadius,
                                               float baseDuration) const {
    float sx = m_x.value();
    float sy = m_y.value();
    float sw = std::max(1.f, m_w.value());
    float sh = std::max(1.f, m_h.value());
    float sr = m_cornerRadius.value();

    float sourceCx = sx + sw * 0.5f;
    float sourceCy = sy + sh * 0.5f;
    float targetCx = target.x + target.width * 0.5f;
    float targetCy = target.y + target.height * 0.5f;

    float dx = targetCx - sourceCx;
    float dy = targetCy - sourceCy;
    float distance = std::sqrt(dx * dx + dy * dy);

    float sourceDiag = std::sqrt(sw * sw + sh * sh);
    float targetW = std::max(1.f, target.width);
    float targetH = std::max(1.f, target.height);
    float targetDiag = std::sqrt(targetW * targetW + targetH * targetH);

    float sizeDelta = std::abs(targetDiag - sourceDiag) / std::max(1.f, sourceDiag);

    float sourceAspect = sw / sh;
    float targetAspect = targetW / targetH;
    float aspectDelta = std::abs(targetAspect - sourceAspect) / std::max(0.35f, sourceAspect);

    float radiusNorm = std::max(4.f, std::max(std::abs(sr), std::abs(targetCornerRadius)));
    float radiusDelta = std::abs(targetCornerRadius - sr) / radiusNorm;

    float distFactor = std::clamp(distance / 320.f, 0.f, 2.4f);
    float deformFactor = std::clamp(sizeDelta + aspectDelta + radiusDelta, 0.f, 1.8f);

    float scale = 1.f + distFactor * 0.50f + deformFactor * 0.35f;
    float adaptive = baseDuration * scale;

    return std::clamp(adaptive, baseDuration * 0.95f, baseDuration * 2.8f);
}

void SelectionCursor::moveTo(const nxui::Rect& target, float duration) {
    moveTo(target, m_cornerRadius.value(), duration);
}

void SelectionCursor::moveTo(const nxui::Rect& target, float cornerRadius, float duration) {
    (void)duration;
    if (!m_initialized) {
        m_x.setImmediate(target.x);
        m_y.setImmediate(target.y);
        m_w.setImmediate(target.width);
        m_h.setImmediate(target.height);
        m_cornerRadius.setImmediate(cornerRadius);
        m_fade = 1.f;
        m_fadePhase = Fade::Idle;
        m_hasPending = false;
        m_initialized = true;
        return;
    }

    // If we are already targeting this rect, do nothing.
    const float curX = m_hasPending ? m_pendingX : m_x.value();
    const float curY = m_hasPending ? m_pendingY : m_y.value();
    const float curW = m_hasPending ? m_pendingW : m_w.value();
    const float curH = m_hasPending ? m_pendingH : m_h.value();
    constexpr float eps = 0.5f;
    if (std::abs(curX - target.x) < eps &&
        std::abs(curY - target.y) < eps &&
        std::abs(curW - target.width) < eps &&
        std::abs(curH - target.height) < eps)
        return;

    // qlaunch does not slide: fade the old highlight out, then snap + fade in.
    m_pendingX = target.x;
    m_pendingY = target.y;
    m_pendingW = target.width;
    m_pendingH = target.height;
    m_pendingR = cornerRadius;
    m_hasPending = true;
    m_fadePhase = Fade::Out;
}

void SelectionCursor::follow(const nxui::Rect& target, float cornerRadius) {
    if (!m_initialized) {
        moveTo(target, cornerRadius, 0.f);
        return;
    }
    // Snap to the moving target without disturbing the fade state. If a
    // fade transition is in-flight, redirect its pending destination instead.
    if (m_fadePhase == Fade::Out && m_hasPending) {
        m_pendingX = target.x;
        m_pendingY = target.y;
        m_pendingW = target.width;
        m_pendingH = target.height;
        m_pendingR = cornerRadius;
        return;
    }
    m_x.setImmediate(target.x);
    m_y.setImmediate(target.y);
    m_w.setImmediate(target.width);
    m_h.setImmediate(target.height);
    m_cornerRadius.setImmediate(cornerRadius);
}

nxui::Rect SelectionCursor::currentRect() const {
    return nxui::Rect{
        std::roundf(m_x.value()),
        std::roundf(m_y.value()),
        std::roundf(m_w.value()),
        std::roundf(m_h.value())
    };
}

void SelectionCursor::onUpdate(float dt) {
    m_time += dt;

    // Fast fade-out / fade-in on selection change (~2-3 frames each way).
    constexpr float kFadeSpeed = 1.f / 0.05f;
    if (m_fadePhase == Fade::Out) {
        m_fade -= dt * kFadeSpeed;
        if (m_fade <= 0.f) {
            m_fade = 0.f;
            if (m_hasPending) {
                m_x.setImmediate(m_pendingX);
                m_y.setImmediate(m_pendingY);
                m_w.setImmediate(m_pendingW);
                m_h.setImmediate(m_pendingH);
                m_cornerRadius.setImmediate(m_pendingR);
                m_hasPending = false;
            }
            m_fadePhase = Fade::In;
        }
    } else if (m_fadePhase == Fade::In) {
        m_fade += dt * kFadeSpeed;
        if (m_fade >= 1.f) {
            m_fade = 1.f;
            m_fadePhase = Fade::Idle;
        }
    }
}

void SelectionCursor::onRender(nxui::Renderer& ren) {
    if (!m_initialized) return;
    float a = m_opacity * m_fade;
    if (a <= 0.01f) return;

    float x = m_x.value(), y = m_y.value();
    float w = m_w.value(), h = m_h.value();
    if (w < 1.f || h < 1.f) return;

    nxui::Rect r = {x, y, w, h};
    float cr = m_cornerRadius.value();

    // Single transparent-centre outline (qlaunch): one crisp accent border,
    // nothing fills the middle.
    ren.drawRoundedRectOutline(r, m_color.withAlpha(a), cr, m_borderWidth);
}

