#pragma once
#include <nxui/widgets/Widget.hpp>
#include <nxui/focus/FocusManager.hpp>
#include <nxui/core/Types.hpp>
#include <vector>
#include <memory>
#include <functional>


class GlossyIcon;

// Horizontally-scrolling strip of app icons (qlaunch home style).
//
// All icons live as children in one long row (column-major: each virtual
// "column" stacks `rows` icons). The strip scrolls smoothly to keep the
// focused icon in view; navigation is spatial (handled by the owning
// activity's FocusManager), so pressing left/right simply moves focus to the
// adjacent icon and the strip glides to follow.
//
// The page-based API (currentPage/iconsPerPage/onPageSwitched) is retained as a
// compatibility shim: "page" now means the icon-streamer window centred on the
// focused/visible icons, and `onPageSwitched` fires whenever that window moves
// so the streamer can (un)load textures.
class IconGrid : public nxui::Widget {
public:
    IconGrid();

    void setup(std::vector<std::shared_ptr<GlossyIcon>> icons,
               int cols, int rows,
               float cellW, float cellH,
               float padX, float padY);
    void reconfigureLayout(int cols, int rows,
                           float cellW, float cellH,
                           float padX, float padY);

    void setPage(int page);
    int  currentPage()  const { return m_streamPage; }
    int  totalPages()   const { return m_totalPages; }
    int  columns()      const { return m_cols; }
    int  rowsPerPage()  const { return m_rows; }
    int  iconsPerPage() const { return m_cols * m_rows; }

    nxui::FocusManager& focusManager() { return m_focus; }
    const std::vector<std::shared_ptr<GlossyIcon>>& allIcons() const { return m_allIcons; }

    // Icons currently within (or near) the viewport.
    std::vector<GlossyIcon*> pageIcons() const;

    // Returns the GLOBAL icon index under the point, or -1.
    int hitTest(float screenX, float screenY) const;

    int focusedGlobalIndex() const;
    bool focusGlobalIndex(int idx, bool immediate = false);
    bool swapSlots(int a, int b);

    void startAppearAnimation();

    // Compat: jump the view/window to a page (used by shoulder buttons/swipe).
    void startWaveTransition(int targetPage);
    bool isTransitioning() const;

    // Touch drag scrolling.
    void beginManualScroll();
    void scrollByPixels(float dx);
    void endManualScroll();
    int  nearestIndexToViewportCenter() const;

    void onPageSwitched(std::function<void()> cb) { m_onPageSwitched = std::move(cb); }

    // When true, scrolling eases and centres the selection; when false (qlaunch
    // default) the strip just stops where it is, scrolling minimally to keep the
    // selection visible without snapping to an icon.
    void setScrollEasing(bool e) { m_scrollEasing = e; }

    void render(nxui::Renderer& ren) override;

protected:
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;

private:
    void rebuildChildren();
    void relayout();
    void ensureIndexVisible(int idx, bool immediate);
    float columnPitch() const { return m_cellW + m_padX; }
    float rowPitch()    const { return m_cellH + m_padY; }
    float stripWidth()  const;
    float maxScroll()   const;
    float baseX()       const;
    int   columnOf(int idx) const { return m_rows > 0 ? idx / m_rows : 0; }
    void  updateStreamPage();

    std::vector<std::shared_ptr<GlossyIcon>> m_allIcons;
    nxui::FocusManager m_focus;

    int m_cols = 5, m_rows = 1;
    int m_numCols = 1;
    int m_streamPage = 0, m_totalPages = 1;
    float m_cellW = 200, m_cellH = 200;
    float m_padX  = 20,  m_padY  = 20;
    float m_originY = 0;

    float m_scrollX = 0.f, m_scrollTargetX = 0.f;
    bool  m_manualScroll = false;
    bool  m_scrollEasing = false;
    bool  m_layoutInit = false;

    std::function<void()> m_onPageSwitched;
};
