#include "IconGrid.hpp"
#include "GlossyIcon.hpp"
#include <nxui/core/Renderer.hpp>
#include <algorithm>
#include <cmath>


// Left/right breathing room between the screen edge and the first/last tile,
// matching the original qlaunch home strip.
static constexpr float kEdgeInset = 110.f;

IconGrid::IconGrid() {}

void IconGrid::setup(std::vector<std::shared_ptr<GlossyIcon>> icons,
                     int cols, int rows,
                     float cellW, float cellH,
                     float padX, float padY)
{
    m_allIcons = std::move(icons);
    m_layoutInit = false;
    reconfigureLayout(cols, rows, cellW, cellH, padX, padY);
}

void IconGrid::reconfigureLayout(int cols, int rows,
                                 float cellW, float cellH,
                                 float padX, float padY)
{
    m_cols  = std::max(1, cols);  m_rows = std::max(1, rows);
    m_cellW = cellW; m_cellH = cellH;
    m_padX  = padX;  m_padY  = padY;

    int count = (int)m_allIcons.size();
    m_numCols = std::max(1, (count + m_rows - 1) / m_rows);

    int perPage = std::max(1, iconsPerPage());
    m_totalPages = std::max(1, (count + perPage - 1) / perPage);

    float gridH = m_rows * m_cellH + (m_rows - 1) * m_padY;
    m_originY = m_rect.y + (m_rect.height - gridH) * 0.5f;

    rebuildChildren();

    // Keep the focused icon framed; snap on the first layout so the initial
    // frame is correct.
    int fi = focusedGlobalIndex();
    ensureIndexVisible(fi >= 0 ? fi : 0, !m_layoutInit);
    m_layoutInit = true;
    updateStreamPage();
    relayout();
}

float IconGrid::stripWidth() const {
    if (m_numCols <= 0) return 0.f;
    return m_numCols * m_cellW + (m_numCols - 1) * m_padX;
}

float IconGrid::maxScroll() const {
    return std::max(0.f, stripWidth() + 2.f * kEdgeInset - m_rect.width);
}

float IconGrid::baseX() const {
    float sw = stripWidth();
    if (sw + 2.f * kEdgeInset <= m_rect.width)
        return m_rect.x + (m_rect.width - sw) * 0.5f;   // fits: centre the group
    return m_rect.x + kEdgeInset;                        // overflows: scroll w/ inset
}

void IconGrid::rebuildChildren() {
    nxui::Widget* prevFocused = m_focus.current();

    clearChildren();
    std::vector<nxui::Widget*> fItems;
    for (auto& icon : m_allIcons) {
        addChild(icon);
        // Every icon is "shown" by default so that tiles scrolled into view
        // later render immediately. startAppearAnimation() re-animates only the
        // initially on-screen ones for the entrance effect.
        icon->forceVisible();
        if (icon->isFocusable())
            fItems.push_back(icon.get());
    }

    // Column count for the focus grid arithmetic is unused (navigation is
    // spatial), but keep it sane.
    m_focus.setGrid(fItems, std::max(1, m_numCols));
    if (prevFocused) {
        for (auto* item : fItems) {
            if (item == prevFocused) {
                m_focus.setFocus(prevFocused);
                break;
            }
        }
    }
}

void IconGrid::relayout() {
    if (m_allIcons.empty()) return;

    float bx = baseX();
    float vpLeft  = m_rect.x;
    float vpRight = m_rect.x + m_rect.width;
    float cullPad = 2.f * columnPitch();

    int count = (int)m_allIcons.size();
    for (int i = 0; i < count; ++i) {
        int col = i / m_rows;
        int row = i % m_rows;
        float x = bx + col * columnPitch() - m_scrollX;
        float y = m_originY + row * rowPitch();
        m_allIcons[i]->setRect({x, y, m_cellW, m_cellH});

        // Keep icons near the viewport visible (and thus navigable); cull the
        // rest so we don't render an unbounded strip.
        bool near = (x + m_cellW >= vpLeft - cullPad) && (x <= vpRight + cullPad);
        m_allIcons[i]->setVisible(near);
    }
}

void IconGrid::ensureIndexVisible(int idx, bool immediate) {
    float ms = maxScroll();
    if (ms <= 0.f) {
        m_scrollTargetX = 0.f;
        if (immediate) m_scrollX = 0.f;
        return;
    }
    idx = std::clamp(idx, 0, (int)m_allIcons.size() - 1);
    int col = columnOf(idx);
    float colLeft = col * columnPitch();

    float want;
    if (m_scrollEasing) {
        // Centre the focused column in the viewport.
        want = colLeft + m_cellW * 0.5f - m_rect.width * 0.5f;
    } else {
        // Deadzone follow (qlaunch): the selection roams freely as long as it
        // stays at least one icon in from either edge; only then does the strip
        // scroll, keeping that one-icon margin. No centring, so releasing the
        // button just leaves it where it is.
        float bx = baseX();
        float margin = columnPitch();
        float screenLeft = bx + colLeft - m_scrollX;
        float screenRight = screenLeft + m_cellW;
        want = m_scrollX;
        if (screenLeft < m_rect.x + margin)
            want = bx + colLeft - (m_rect.x + margin);
        else if (screenRight > m_rect.x + m_rect.width - margin)
            want = bx + colLeft + m_cellW - (m_rect.x + m_rect.width - margin);
    }

    want = std::clamp(want, 0.f, ms);
    m_scrollTargetX = want;
    if (immediate) m_scrollX = want;
}

int IconGrid::nearestIndexToViewportCenter() const {
    if (m_allIcons.empty()) return -1;
    float centerX = m_scrollX + m_rect.width * 0.5f;   // in strip space
    int col = (int)std::floor((centerX + m_padX * 0.5f) / columnPitch());
    col = std::clamp(col, 0, std::max(0, m_numCols - 1));
    int idx = std::clamp(col * m_rows, 0, (int)m_allIcons.size() - 1);
    return idx;
}

void IconGrid::updateStreamPage() {
    int perPage = std::max(1, iconsPerPage());
    int centerIdx = focusedGlobalIndex();
    if (centerIdx < 0) centerIdx = nearestIndexToViewportCenter();
    if (centerIdx < 0) centerIdx = 0;
    int p = std::clamp(centerIdx / perPage, 0, std::max(0, m_totalPages - 1));
    if (p != m_streamPage) {
        m_streamPage = p;
        if (m_onPageSwitched)
            m_onPageSwitched();
    }
}

void IconGrid::setPage(int page) {
    int perPage = std::max(1, iconsPerPage());
    m_streamPage = std::clamp(page, 0, std::max(0, m_totalPages - 1));
    ensureIndexVisible(m_streamPage * perPage, true);
    relayout();
}

int IconGrid::focusedGlobalIndex() const {
    auto* cur = m_focus.current();
    if (!cur) return -1;
    for (int i = 0; i < (int)m_allIcons.size(); ++i)
        if (m_allIcons[i].get() == cur)
            return i;
    return -1;
}

bool IconGrid::focusGlobalIndex(int idx, bool immediate) {
    if (idx < 0 || idx >= (int)m_allIcons.size())
        return false;
    if (!m_allIcons[idx] || !m_allIcons[idx]->isFocusable())
        return false;

    m_focus.setFocus(m_allIcons[idx].get());
    ensureIndexVisible(idx, immediate);
    updateStreamPage();
    return true;
}

bool IconGrid::swapSlots(int a, int b) {
    if (a < 0 || b < 0 || a >= (int)m_allIcons.size() || b >= (int)m_allIcons.size())
        return false;
    if (a == b)
        return true;

    std::swap(m_allIcons[a], m_allIcons[b]);
    rebuildChildren();
    relayout();
    return true;
}

std::vector<GlossyIcon*> IconGrid::pageIcons() const {
    std::vector<GlossyIcon*> out;
    for (auto& icon : m_allIcons)
        if (icon && icon->isVisible())
            out.push_back(icon.get());
    return out;
}

int IconGrid::hitTest(float screenX, float screenY) const {
    int count = (int)m_allIcons.size();
    for (int i = 0; i < count; ++i) {
        if (!m_allIcons[i] || !m_allIcons[i]->isVisible())
            continue;
        nxui::Rect r = m_allIcons[i]->focusRect();
        if (r.contains(screenX, screenY))
            return i;
    }
    return -1;
}

void IconGrid::startAppearAnimation() {
    int count = (int)m_allIcons.size();
    float bx = baseX();
    float vpLeft  = m_rect.x;
    float vpRight = m_rect.x + m_rect.width;
    for (int i = 0; i < count; ++i) {
        int col = i / m_rows;
        int row = i % m_rows;
        float x = bx + col * columnPitch() - m_scrollX;
        if (x + m_cellW < vpLeft || x > vpRight)
            continue;   // only stagger visible icons
        float t = (m_numCols > 1) ? (float)col / (m_numCols - 1) : 0.f;
        float delay = std::clamp(t, 0.f, 1.f) * 0.10f + row * 0.03f;
        m_allIcons[i]->startAppear(delay);
    }
}

void IconGrid::startWaveTransition(int targetPage) {
    int perPage = std::max(1, iconsPerPage());
    targetPage = std::clamp(targetPage, 0, std::max(0, m_totalPages - 1));
    focusGlobalIndex(targetPage * perPage, false);
}

bool IconGrid::isTransitioning() const {
    return std::abs(m_scrollTargetX - m_scrollX) > 1.f;
}

void IconGrid::beginManualScroll() {
    m_manualScroll = true;
}

void IconGrid::scrollByPixels(float dx) {
    m_scrollX = std::clamp(m_scrollX + dx, 0.f, maxScroll());
    m_scrollTargetX = m_scrollX;
    updateStreamPage();
    relayout();
}

void IconGrid::endManualScroll() {
    m_manualScroll = false;
    // Snap focus to whatever is nearest the centre so auto-follow doesn't yank
    // the strip back to a now-off-screen selection.
    int idx = nearestIndexToViewportCenter();
    if (idx >= 0)
        m_focus.setFocus(m_allIcons[idx].get());
}

void IconGrid::onUpdate(float dt) {
    if (!m_manualScroll) {
        int fi = focusedGlobalIndex();
        if (fi >= 0)
            ensureIndexVisible(fi, false);
    }

    // Always smooth. In deadzone mode the target only moves when the selection
    // reaches the edge margin, so it eases briefly and then stops; in centre
    // mode it eases to keep the selection centred.
    float d = std::min(1.f, dt * 12.f);
    m_scrollX += (m_scrollTargetX - m_scrollX) * d;
    if (std::abs(m_scrollTargetX - m_scrollX) < 0.5f)
        m_scrollX = m_scrollTargetX;

    updateStreamPage();
    relayout();
}

void IconGrid::render(nxui::Renderer& ren) {
    if (!m_visible || m_opacity <= 0.f) return;

    // Clip horizontally to the strip viewport so icons entering/leaving the
    // edges are cut cleanly; keep vertical generous for icon shadows/labels.
    nxui::Rect clip = { m_rect.x, m_rect.y - 240.f, m_rect.width, m_rect.height + 480.f };
    ren.pushClipRect(clip);
    for (auto& c : m_children) c->render(ren);
    ren.popClipRect();
}

void IconGrid::onRender(nxui::Renderer&) {
}
