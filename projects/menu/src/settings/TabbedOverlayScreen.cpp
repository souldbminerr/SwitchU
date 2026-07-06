#include "TabbedOverlayScreen.hpp"
#include "SettingsGlassTuning.hpp"
#include "SettingItemWidgets.hpp"
#include "core/DebugLog.hpp"
#include <nxui/core/I18n.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/widgets/GlassBox.hpp>
#include <nxui/widgets/Label.hpp>
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
static constexpr float kSettingsBlurRadius = 6.0f;
static constexpr int kSettingsBlurIter = 1;

namespace {

std::string utf8FromCodepoint(uint32_t cp) {
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

static constexpr float kTabRailInset = 14.f;
static constexpr float kTabCardGap = 10.f;
static constexpr float kContentCardInsetX = 18.f;
static constexpr float kContentCardInsetY = 8.f;
static constexpr int kSettingsBackdropCacheTarget = 2;

class SettingsTabWidget final : public nxui::GlassBox {
public:
    explicit SettingsTabWidget(const std::string& text)
        : nxui::GlassBox(nxui::Axis::ROW) {
        setCornerRadius(18.f);
        setBorderWidth(1.f);
        setWireframeEnabled(false);

        m_label = std::make_shared<nxui::Label>(text);
        m_label->setHAlign(nxui::Label::HAlign::Left);
        m_label->setVAlign(nxui::Label::VAlign::Center);
        addChild(m_label);
    }

    void sync(const std::string& text,
              nxui::Font* font,
              const nxui::Theme* theme,
              bool selected,
              bool focused,
              float uiTime,
              float accentWidth) {
        (void)uiTime;
        m_selected = selected;
        m_focused = focused;
        m_accentWidth = accentWidth;
        m_accentColor = theme ? theme->accent : nxui::Color::white();

        if (font != m_cachedFont) {
            m_cachedFont = font;
            if (font) {
                m_label->setFont(font);
            }
        }
        if (m_cachedText != text) {
            m_cachedText = text;
            m_label->setText(m_cachedText);
        }

        float textScale = selected ? 1.06f : 1.02f;
        if (std::abs(m_cachedTextScale - textScale) > 0.001f) {
            m_cachedTextScale = textScale;
            m_label->setScale(textScale);
        }
        m_label->setOpacity(opacity());
        m_label->setRect({rect().x + 22.f, rect().y,
                          std::max(0.f, rect().width - 44.f), rect().height});

        if (theme) {
            // qlaunch category rail: flat (no card). Selected = accent text +
            // accent left bar; others = secondary text.
            nxui::Color textColor = selected ? theme->accent : theme->textSecondary;

            setBaseColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
            setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
            setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
            setBorderWidth(0.f);
            setScale(1.f);

            m_label->setTextColor(textColor);
        }
    }

protected:
    void onRender(nxui::Renderer& ren) override {
        nxui::GlassBox::onRender(ren);

        if (!m_selected || opacity() <= 0.01f)
            return;

        nxui::Rect r = rect();
        // Selected category indicator: a 4x52 accent bar sitting 9 px to the left
        // of the label's first letter (the label starts at r.x + 22).
        nxui::Rect accent = {r.x + 9.f, r.y, 4.f, r.height};
        ren.drawRect(accent, m_accentColor.withAlpha(0.95f * opacity()));
    }

private:
    std::shared_ptr<nxui::Label> m_label;
    bool m_selected = false;
    bool m_focused = false;
    float m_accentWidth = 3.f;
    nxui::Color m_accentColor = nxui::Color::white();
    nxui::Font* m_cachedFont = nullptr;
    std::string m_cachedText;
    float m_cachedTextScale = -1.f;
};

class SettingsItemCard final : public nxui::GlassBox {
public:
    SettingsItemCard(TabbedOverlayScreen::SettingItem& item,
                     std::shared_ptr<nxui::Box> content)
        : nxui::GlassBox(nxui::Axis::ROW)
        , m_item(item)
        , m_content(std::move(content)) {
        setAlignItems(nxui::AlignItems::CENTER);
        setJustifyContent(nxui::JustifyContent::FLEX_START);
        setWireframeEnabled(false);
        addChild(m_content);
    }

    void sync(const nxui::Theme* theme, bool selected, float alpha) {
        const bool isSection = (m_item.type == TabbedOverlayScreen::ItemType::Section);

        setCornerRadius(isSection ? 14.f : 18.f);
        setBorderWidth(isSection ? 0.f : 1.f);
        setScale(selected ? 1.008f : 1.f);

        if (theme) {
            // qlaunch content rows: flat. The focus cursor draws the cyan
            // selection box, so rows themselves stay transparent (a faint fill
            // marks the selected row underneath the cursor).
            float baseAlpha = (selected && !isSection) ? 0.05f : 0.0f;
            setBaseColor(theme->panelBase.withAlpha(baseAlpha));
            setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
            setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
            setBorderWidth(0.f);
        }

        float insetX = isSection ? 6.f : 14.f;
        float insetY = isSection ? 4.f : 6.f;
        m_content->setRect({
            rect().x + insetX,
            rect().y + insetY,
            std::max(0.f, rect().width - insetX * 2.f),
            std::max(0.f, rect().height - insetY * 2.f)
        });
        m_content->setOpacity(alpha);
    }

private:
    TabbedOverlayScreen::SettingItem& m_item;
    std::shared_ptr<nxui::Box> m_content;
};

} // namespace


TabbedOverlayScreen::TabbedOverlayScreen(ScreenMode mode)
    : m_mode(mode) {
    setFrameworkTouchEnabled(false);
    // Full-screen, flat System-Settings-style layout.
    setRect({0.f, 0.f, 1280.f, 720.f});
    setVisible(false);
    setOpacity(0.001f);
    setScale(1.f);
    setCornerRadius(kPanelRadius);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setBlurEnabled(false);
    setBlurRadius(kSettingsBlurRadius);
    setBlurPasses(kSettingsBlurIter);
    setPanelOpacity(0.82f);

    m_focusCursor.setBorderWidth(4.f);   // 4 px selection outline
    m_focusCursor.setCornerRadius(10.f);
    m_tabReveal.setImmediate(1.f);
    m_dropdownAnim.setImmediate(0.f);
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_contentSlideAnim.setImmediate(1.f);
    m_tabAccentW.setImmediate(3.f);

    m_tabBar = std::make_shared<nxui::GlassBox>(nxui::Axis::COLUMN);
    m_tabBar->setTag("tabBar");
    m_tabBar->setWireframeEnabled(false);

    m_tabContent = std::make_shared<nxui::GlassBox>(nxui::Axis::COLUMN);
    m_tabContent->setTag("tabContent");
    m_tabContent->setWireframeEnabled(false);

    rebuildTabBar();
    rebuildContentItems();

    m_i18nListenerId = nxui::I18n::instance().addLanguageChangedListener([this]() {
        m_deferredRefresh = true;
    });
}
TabbedOverlayScreen::~TabbedOverlayScreen() {
    nxui::I18n::instance().removeLanguageChangedListener(m_i18nListenerId);
}

void TabbedOverlayScreen::setTheme(const nxui::Theme* t) {
    m_theme = t;
    if (!m_theme)
        return;

    setBaseColor(m_theme->panelBase.withAlpha(std::clamp(m_theme->panelBase.a * 0.72f, 0.12f, 0.30f)));
    setBorderColor(m_theme->panelBorder.withAlpha(std::clamp(m_theme->panelBorder.a * 0.92f, 0.14f, 0.42f)));
    setHighlightColor(m_theme->panelHighlight.withAlpha(std::clamp(m_theme->panelHighlight.a * 0.92f, 0.05f, 0.18f)));
    setLiquidGlassShade(m_theme->mode == nxui::ThemeMode::Dark ? 0.08f : -0.03f);
    invalidateBackdropCache();

    if (!usesCustomContentLayout()) {
        m_cachedTabContentWidgets.clear();
        m_cachedTabContentWidgets.resize(m_tabs.size());

        if (isActive())
            rebuildContentItems();
    }
}

void TabbedOverlayScreen::show() {
    if (m_active) return;
    DebugLog::log("[settings] show()");
    if (m_tabs.empty())
        warmup();
    m_active    = true;
    m_animating = true;
    m_showing   = true;
    m_animT     = 0.f;
    m_focusArea  = FocusArea::Tabs;
    m_tabIndex   = 0;
    m_contentIdx = 0;
    m_scrollY    = 0.f;
    m_scrollTarget = 0.f;
    m_tabReveal.setImmediate(0.f);
    m_tabReveal.set(1.f, 0.24f, nxui::Easing::outCubic);
    m_dropdownOpen = false;
    m_dropdownClosing = false;
    m_dropdownRawIdx = -1;
    m_dropdownHover = 0;
    m_dropdownAnim.setImmediate(0.f);
    m_touchDirectControl = false;
    m_ignoreInitialTouchRelease = true;
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_contentSlideAnim.setImmediate(1.f);
    m_tabAccentW.setImmediate(3.f);
    if (m_tabBar) rebuildTabBar();
    if (m_tabContent) rebuildContentItems();
    invalidateBackdropCache();

    setVisible(true);
    syncPanelState(0.f);
    setFocusable(true);
    setupActions();
    announceCurrentFocus();
}

void TabbedOverlayScreen::hide() {
    if (!m_active) return;
    DebugLog::log("[settings] hide()");
    if (m_closeSfxCb) m_closeSfxCb();
    closeDropdown(false);
    m_trackToastAnim.setImmediate(0.f);
    m_trackToastHold = 0.f;
    m_trackToastFading = false;
    m_animating = true;
    m_showing   = false;
    m_animT     = 0.f;

    setFocusable(false);
    clearActions();
}

void TabbedOverlayScreen::openDropdown(int rawIdx) {
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size())
        return;

    auto& items = m_tabs[m_tabIndex].items;
    if (rawIdx < 0 || rawIdx >= (int)items.size())
        return;

    auto& item = items[rawIdx];
    if (item.type != ItemType::Selector || item.options.empty())
        return;

    m_dropdownOpen = true;
    m_dropdownClosing = false;
    m_dropdownRawIdx = rawIdx;
    m_dropdownHover = std::clamp(item.intVal, 0, std::max(0, (int)item.options.size() - 1));
    int visible = std::min((int)item.options.size(), 6);
    m_dropdownVisualStart = (item.options.size() > (size_t)visible)
        ? (float)std::clamp(m_dropdownHover - visible / 2, 0, (int)item.options.size() - visible)
        : 0.f;
    m_dropdownAnim.set(1.f, 0.18f, nxui::Easing::outCubic);
}

void TabbedOverlayScreen::closeDropdown(bool animated) {
    m_dropdownOpen = false;
    m_dropdownClosing = animated && m_dropdownRawIdx >= 0;
    if (animated && m_dropdownRawIdx >= 0) {
        m_dropdownAnim.set(0.f, 0.16f, nxui::Easing::outCubic);
    } else {
        m_dropdownClosing = false;
        m_dropdownRawIdx = -1;
        m_dropdownAnim.setImmediate(0.f);
    }
}


void TabbedOverlayScreen::rebuildTabBar() {
    m_tabBar->clearChildren();
    nxui::Rect tr = tabsRect();
    m_tabBar->setRect(tr);

    float tabX = tr.x + kTabRailInset;
    float tabY = tr.y + kTabRailInset;
    float tabW = std::max(0.f, tr.width - kTabRailInset * 2.f);
    float tabH = kTabRowHeight - kTabCardGap;

    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        auto tabBox = std::make_shared<SettingsTabWidget>(m_tabs[i].name);
        tabBox->setTag(m_tabs[i].name);
        tabBox->setRect({tabX, tabY, tabW, tabH});
        m_tabBar->addChild(tabBox);
        tabY += tabH + kTabCardGap;
    }
}

void TabbedOverlayScreen::rebuildContentItems() {
    m_tabContent->clearChildren();
    nxui::Rect cr = contentRect();
    m_tabContent->setRect(cr);

    if (usesCustomContentLayout())
        return;

    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;
    auto& items = m_tabs[m_tabIndex].items;
    auto& cache = m_cachedTabContentWidgets[(size_t)m_tabIndex];
    DebugLog::log("[settings] rebuildContent tab=%d items=%d cache=%s",
                  m_tabIndex, (int)items.size(), cache.empty() ? "miss" : "hit");

    if (cache.empty()) {
        cache.reserve(items.size());

        float y = 0.f;
        for (int i = 0; i < (int)items.size(); ++i) {
            float h = (items[i].type == ItemType::Section) ? kSectionHeight : kRowHeight;
            float insetY = (items[i].type == ItemType::Section) ? 1.f : kContentCardInsetY;
            float cardH = std::max(0.f, h - (items[i].type == ItemType::Section ? 2.f : 6.f));
            auto itemBox = makeItemWidget(items[i]);
            itemBox->setTag(items[i].label);
            itemBox->setRect({cr.x + kContentCardInsetX, cr.y + y + insetY,
                              std::max(0.f, cr.width - kContentCardInsetX * 2.f), cardH});
            cache.push_back(itemBox);
            y += h;
        }
    }

    for (auto& itemBox : cache) {
        m_tabContent->addChild(itemBox);
    }
}

std::shared_ptr<nxui::Box> TabbedOverlayScreen::makeItemWidget(SettingItem& item) {
    settings::widgets::SettingWidgetContext ctx;
    ctx.font = &m_font;
    ctx.smallFont = &m_smallFont;
    ctx.theme = &m_theme;
    auto content = settings::widgets::createSettingItemWidget(item, ctx);
    return std::make_shared<SettingsItemCard>(item, content);
}
void TabbedOverlayScreen::onRender(nxui::Renderer& ren) {
    if (!m_active && !m_animating)
        return;

    nxui::Rect p = panelRect(scale());

    if (m_theme)
        m_focusCursor.setColor(m_theme->cursorNormal);   // entry highlight keeps the normal color
    // No selection outline on the category rail — the selected tab shows only its
    // accent bar. The cursor is used for content items only.
    m_focusCursor.setOpacity(m_focusArea == FocusArea::Tabs ? 0.f : contentOpacity());

    if (m_tabBar) m_tabBar->setRect(tabsRect(p));
    if (m_tabContent) m_tabContent->setRect(contentRect(p));

    // Flat full-screen background fades in first; the content fades in after a
    // short hold (qlaunch-style, no floating glass panel).
    drawBackground(ren, p, bgOpacity());

    onContentRender(ren);

    if (ren.boxWireframeEnabled()) {
        syncDebugWireframeRects(p);
        if (m_tabBar) m_tabBar->render(ren);
        if (m_tabContent) m_tabContent->render(ren);
    }

    m_focusCursor.render(ren);
}

void TabbedOverlayScreen::onContentRender(nxui::Renderer& ren) {
    nxui::Rect p = panelRect(scale());
    float textOp = contentOpacity();

    ren.pushClipRect(p);
    drawHeader(ren, p, textOp);
    drawTabs(ren, p, textOp);

    // Vertical rail/content divider: a 1 px line 410 px from the left edge,
    // spanning between the header and footer separators, in the Off/disabled
    // colour used for toggle "Off" text.
    if (m_theme && textOp > 0.01f) {
        float dx = p.x + kRailDividerX;
        float top = p.y + kHeaderH;
        float bot = p.bottom() - kFooterH;
        ren.drawRect({dx, top, 1.f, std::max(0.f, bot - top)},
                     m_theme->secondary.withAlpha(textOp));
    }

    drawContent(ren, p, textOp);
    drawFooter(ren, p, textOp);
    drawDropdown(ren, p, textOp);
    drawTrackChangedToast(ren, p, textOp);
    ren.popClipRect();
}

void TabbedOverlayScreen::drawHeader(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_theme || opacity <= 0.01f)
        return;
    const float margin = 30.f;
    float x = panel.x + margin;
    const float cy = panel.y + kHeaderH * 0.5f;

    // Gear icon to the left of the title.
    if (m_headerIcon && m_headerIcon->valid()) {
        const float sz = 40.f;
        ren.drawTexture(m_headerIcon, {x, cy - sz * 0.5f, sz, sz},
                        m_theme->textPrimary.withAlpha(opacity));
        x += sz + 16.f;
    }

    const std::string title = m_headerTitle.empty() ? "Settings" : m_headerTitle;
    nxui::Vec2 ts = m_font->measure(title);
    const float scale = 1.35f;
    // Optically centre the caps on the gear (the measured box sits low because
    // it includes the font's descent).
    ren.drawText(title, {x, cy - ts.y * scale * 0.62f}, m_font,
                 m_theme->textPrimary.withAlpha(opacity), scale);

    // Full-width separator under the header.
    ren.drawRect({panel.x + margin, panel.y + kHeaderH - 1.f,
                  panel.width - margin * 2.f, 1.f},
                 m_theme->textSecondary.withAlpha(0.30f * opacity));
}

void TabbedOverlayScreen::drawFooter(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_smallFont || !m_theme || opacity <= 0.01f)
        return;
    const float margin = 30.f;
    float sepY = panel.bottom() - kFooterH + 1.f;
    ren.drawRect({panel.x + margin, sepY, panel.width - margin * 2.f, 1.f},
                 m_theme->textSecondary.withAlpha(0.30f * opacity));

    const float cy = panel.bottom() - kFooterH * 0.5f;
    const nxui::Color tc = m_theme->textPrimary.withAlpha(0.92f * opacity);

    // Bottom-left handheld glyph (nintendo_ext font).
    if (m_iconFont) {
        const std::string handheld = utf8FromCodepoint(0xE121);
        const float gs = 0.9f;
        nxui::Vec2 gsz = m_iconFont->measure(handheld);
        ren.drawText(handheld, {panel.x + margin, cy - gsz.y * gs * 0.5f}, m_iconFont, tc, gs);
    }

    // Bottom-right: "(B) Back   (A) OK" with real button glyphs.
    const float gs = 0.72f, ls = 0.62f, gap = 8.f, itemGap = 30.f;
    struct Hint { uint32_t cp; const char* label; };
    const Hint hints[] = { {0xE0E1, "Back"}, {0xE0E0, "OK"} };
    float total = 0.f;
    for (auto& h : hints) {
        std::string g = utf8FromCodepoint(h.cp);
        total += (m_iconFont ? m_iconFont->measure(g).x * gs : 0.f) + gap
               + m_smallFont->measure(h.label).x * ls + itemGap;
    }
    float hx = panel.right() - margin - total + itemGap;
    for (auto& h : hints) {
        std::string g = utf8FromCodepoint(h.cp);
        if (m_iconFont) {
            nxui::Vec2 gsz = m_iconFont->measure(g);
            ren.drawText(g, {hx, cy - gsz.y * gs * 0.5f}, m_iconFont, tc, gs);
            hx += gsz.x * gs + gap;
        }
        nxui::Vec2 lsz = m_smallFont->measure(h.label);
        ren.drawText(h.label, {hx, cy - lsz.y * ls * 0.5f}, m_smallFont, tc, ls);
        hx += lsz.x * ls + itemGap;
    }
}

void TabbedOverlayScreen::syncDebugWireframeRects(const nxui::Rect& panel) {
    if (!m_tabBar || !m_tabContent) return;

    nxui::Rect tr = tabsRect(panel);
    nxui::Rect cr = contentRect(panel);

    m_tabBar->setRect(tr);
    auto& tabChildren = m_tabBar->children();
    float tabY = tr.y + kTabRailInset;
    float tabW = std::max(0.f, tr.width - kTabRailInset * 2.f);
    float tabH = kTabRowHeight - kTabCardGap;
    for (int i = 0; i < (int)tabChildren.size(); ++i) {
        tabChildren[i]->setRect({tr.x + kTabRailInset, tabY, tabW, tabH});
        tabY += tabH + kTabCardGap;
    }

    m_tabContent->setRect(cr);
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;

    auto& items = m_tabs[m_tabIndex].items;
    auto& itemChildren = m_tabContent->children();

    float slideT = std::clamp(m_contentSlideAnim.value(), 0.f, 1.f);
    float slideOffset = (1.f - slideT) * 24.f * (float)m_tabSwitchDir;

    float y = cr.y - m_scrollY + slideOffset;
    float x = cr.x;
    int n = std::min((int)itemChildren.size(), (int)items.size());
    for (int i = 0; i < n; ++i) {
        float h = (items[i].type == ItemType::Section) ? kSectionHeight : kRowHeight;
        float insetY = (items[i].type == ItemType::Section) ? 1.f : kContentCardInsetY;
        float cardH = std::max(0.f, h - (items[i].type == ItemType::Section ? 2.f : 6.f));
        itemChildren[i]->setRect({x + kContentCardInsetX, y + insetY,
                                  std::max(0.f, cr.width - kContentCardInsetX * 2.f), cardH});
        y += h;
    }
}

void TabbedOverlayScreen::drawBackground(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_theme || opacity <= 0.01f)
        return;
    (void)panel;
    // Flat, full-screen System-Settings background.
    nxui::Rect screen = {0.f, 0.f, (float)ren.width(), (float)ren.height()};
    ren.drawRect(screen, m_theme->primary.withAlpha(opacity));
}

void TabbedOverlayScreen::drawTabs(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_theme || !m_tabBar) return;
    nxui::Rect tr = tabsRect(panel);
    auto* tabPanel = static_cast<nxui::GlassBox*>(m_tabBar.get());

    tabPanel->setRect(tr);
    tabPanel->setOpacity(opacity);
    tabPanel->setCornerRadius(0.f);
    tabPanel->setBorderWidth(0.f);
    tabPanel->setBaseColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    tabPanel->setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    tabPanel->setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    tabPanel->setPanelOpacity(0.f);

    auto& tabChildren = m_tabBar->children();
    float reveal = std::clamp(m_tabReveal.value(), 0.f, 1.f);
    float tabW = std::max(0.f, tr.width - kTabRailInset * 2.f);
    // Fixed, comfortable row height (qlaunch-sized); the rail scrolls when the
    // list is taller than the band, keeping the selected category visible.
    const int nTabs = (int)m_tabs.size();
    const float rowPitch = kTabRowHeight;
    const float tabH = rowPitch - kTabCardGap;

    // Smoothly-animated scroll (target computed in onContentUpdate).
    float tabY = tr.y + kTabRailInset - m_tabScrollY;
    float rowOpacity = opacity * reveal;
    float rowYOffset = (1.f - reveal) * 6.f;

    for (int i = 0; i < (int)tabChildren.size() && i < nTabs; ++i) {
        auto* tab = static_cast<SettingsTabWidget*>(tabChildren[i].get());
        tab->setRect({tr.x + kTabRailInset, tabY + rowYOffset, tabW, tabH});
        tab->setOpacity(rowOpacity);
        tab->sync(m_tabs[i].name,
                  m_font,
                  m_theme,
                  i == m_tabIndex,
                  m_focusArea == FocusArea::Tabs && i == m_tabIndex,
                  m_uiTime,
                  m_tabAccentW.value());
        tabY += rowPitch;
    }

    // (No focus-cursor outline on the rail: the selected tab is shown by its
    //  accent bar only.)

    // Clip the rail so nothing bleeds into the header/footer bands.
    ren.pushClipRect(tr);
    m_tabBar->render(ren);
    ren.popClipRect();
}

void TabbedOverlayScreen::drawContent(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_font || !m_smallFont || !m_theme) return;
    if (m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;
    if (!m_tabContent) return;

    nxui::Rect cr = contentRect(panel);
    auto* contentPanel = static_cast<nxui::GlassBox*>(m_tabContent.get());
    contentPanel->setRect(cr);
    contentPanel->setOpacity(opacity);
    contentPanel->setCornerRadius(0.f);
    contentPanel->setBorderWidth(0.f);
    contentPanel->setBaseColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    contentPanel->setBorderColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    contentPanel->setHighlightColor(nxui::Color(0.f, 0.f, 0.f, 0.f));
    contentPanel->setPanelOpacity(0.f);

    if (usesCustomContentLayout()) {
        contentPanel->render(ren);
        ren.pushClipRect(cr);
        drawCustomContent(ren, panel, cr, opacity);
        ren.popClipRect();
        return;
    }

    auto& items = m_tabs[m_tabIndex].items;
    auto& itemChildren = m_tabContent->children();
    int focusedRawIdx = (m_focusArea == FocusArea::Content && focusableCount() > 0)
        ? rawIndexFromFocusable(m_contentIdx) : -1;
    if (focusedRawIdx < 0) m_focusCursorItem = -1;

    float slideT = std::clamp(m_contentSlideAnim.value(), 0.f, 1.f);
    float slideOffset = (1.f - slideT) * 18.f * (float)m_tabSwitchDir;
    float slideOpacity = opacity * slideT * std::clamp(m_tabReveal.value(), 0.f, 1.f);

    float y = cr.y + 16.f - m_scrollY + slideOffset;
    float x = cr.x;
    int n = std::min((int)itemChildren.size(), (int)items.size());
    for (int i = 0; i < n; ++i) {
        float h = (items[i].type == ItemType::Section) ? kSectionHeight : kRowHeight;
        float insetY = (items[i].type == ItemType::Section) ? 1.f : kContentCardInsetY;
        float cardH = std::max(0.f, h - (items[i].type == ItemType::Section ? 2.f : 6.f));
        nxui::Rect itemRect = {
            x + kContentCardInsetX,
            y + insetY,
            std::max(0.f, cr.width - kContentCardInsetX * 2.f),
            cardH
        };
        bool visible = itemRect.bottom() >= cr.y - 8.f && itemRect.y <= cr.bottom() + 8.f;

        itemChildren[i]->setVisible(visible);
        if (!visible) {
            y += h;
            continue;
        }

        itemChildren[i]->setRect(itemRect);
        itemChildren[i]->setOpacity(slideOpacity);

        auto* card = static_cast<SettingsItemCard*>(itemChildren[i].get());
        bool selected = (i == focusedRawIdx);
        card->sync(m_theme, selected, slideOpacity);

        // qlaunch draws a thin separator under each non-section row.
        if (items[i].type != ItemType::Section) {
            float sy = y + h - 1.f;
            if (sy >= cr.y && sy <= cr.bottom()) {
                ren.drawRect({cr.x, sy, cr.width, 1.f},
                             m_theme->textSecondary.withAlpha(0.20f * slideOpacity));
            }
        }

        if (selected) {
            nxui::Rect target = itemChildren[i]->rect().expanded(1.f);
            float rad = items[i].type == ItemType::Section ? 8.f : 9.f;
            if (m_focusCursorItem != i) {
                // Selection changed -> qlaunch fade-out/in transition.
                m_focusCursor.moveTo(target, rad, 0.08f);
                m_focusCursorItem = i;
            } else {
                // Same row, only scrolling -> track without flicker.
                m_focusCursor.follow(target, rad);
            }
        }
        y += h;
    }

    ren.pushClipRect(cr);
    m_tabContent->render(ren);
    ren.popClipRect();

    float totalH = contentTotalHeight() + 16.f;
    if (totalH > cr.height + 1.f) {
        float maxScroll = std::max(1.f, totalH - cr.height + 20.f);
        float trackH = std::max(42.f, cr.height * std::clamp(cr.height / totalH, 0.12f, 1.f));
        float trackX = cr.right() - 8.f;
        float trackY = cr.y + 16.f;
        float trackAreaH = std::max(1.f, cr.height - 32.f);
        float thumbY = trackY + (trackAreaH - trackH) * std::clamp(m_scrollY / maxScroll, 0.f, 1.f);
        nxui::Rect rail = {trackX, trackY, 3.f, trackAreaH};
        nxui::Rect thumb = {trackX - 0.5f, thumbY, 4.f, trackH};
        ren.drawRoundedRect(rail, m_theme->panelBorder.withAlpha(0.16f * opacity), 1.5f);
        ren.drawRoundedRect(thumb, m_theme->accent.withAlpha(0.46f * opacity), 2.f);
    }
}

void TabbedOverlayScreen::drawDropdown(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_theme || !m_smallFont || m_tabIndex < 0 || m_tabIndex >= (int)m_tabs.size()) return;

    float open = m_dropdownAnim.value();
    if (open <= 0.01f) return;

    auto& items = m_tabs[m_tabIndex].items;
    if (m_dropdownRawIdx < 0 || m_dropdownRawIdx >= (int)items.size()) return;
    auto& item = items[m_dropdownRawIdx];
    if (item.type != ItemType::Selector || item.options.empty()) return;

    nxui::Rect cr = contentRect(panel);

    float y = cr.y + 16.f - m_scrollY;
    for (int i = 0; i < m_dropdownRawIdx; ++i)
        y += (items[i].type == ItemType::Section) ? kSectionHeight : kRowHeight;
    float rowH = (item.type == ItemType::Section) ? kSectionHeight : kRowHeight;

    float ctrlX = cr.x + cr.width * 0.40f;
    float ctrlW = cr.width * 0.60f;

    int total = (int)item.options.size();
    int visible = std::min(total, 6);
    float optH = 46.f;
    float listH = visible * optH + 16.f;

    float visualStart = 0.f;
    if (total > visible)
        visualStart = std::clamp(m_dropdownVisualStart, 0.f, (float)(total - visible));
    int start = std::clamp((int)std::floor(visualStart), 0, std::max(0, total - visible));
    float rowOffset = (float)start - visualStart;

    float dy = y + rowH + 6.f;
    if (dy + listH > cr.bottom() - 4.f)
        dy = y - listH - 6.f;

    float scale = 0.965f + 0.035f * open;
    float w = ctrlW * scale;
    float h = listH * scale;
    float dx = ctrlX + (ctrlW - w) * 0.5f;
    float fy = dy + (listH - h) * 0.5f + (1.f - open) * 8.f;

    nxui::Rect pop = { dx, fy, w, h };

    float a = opacity * open;
    nxui::Color bg = m_theme->mode == nxui::ThemeMode::Dark
        ? nxui::Color::lerp(m_theme->panelBase, nxui::Color(0.055f, 0.060f, 0.075f, 1.f), 0.36f).withAlpha(0.94f * a)
        : nxui::Color::lerp(m_theme->panelBase, nxui::Color(0.98f, 0.985f, 1.f, 1.f), 0.30f).withAlpha(0.96f * a);
    float radius = 15.f;

    nxui::Rect contact = pop;
    contact.y += 6.f;
    ren.drawRoundedRect(contact.expanded(3.f), nxui::Color::black().withAlpha(0.12f * a), radius + 3.f);
    ren.drawRoundedRect(pop.expanded(1.f), m_theme->panelHighlight.withAlpha(0.08f * a), radius + 1.f);
    ren.drawRoundedRect(pop, bg, radius);
    ren.drawRoundedRectOutline(pop,
                               m_theme->panelBorder.withAlpha(0.70f * a),
                               radius, 1.4f);
    ren.drawRoundedRectOutline(pop.shrunk(2.f),
                               m_theme->panelHighlight.withAlpha(0.10f * a),
                               std::max(0.f, radius - 2.f), 1.f);

    nxui::Rect listClip = pop.shrunk(6.f);
    ren.pushClipRect(listClip);

    for (int i = 0; i < visible + 1; ++i) {
        int idx = start + i;
        if (idx < 0 || idx >= total)
            continue;
        float rowReveal = std::clamp((open - i * 0.025f) / 0.25f, 0.f, 1.f);
        float ry = listClip.y + 3.f + (rowOffset + (float)i) * optH + (1.f - rowReveal) * 5.f;
        nxui::Rect rr = { listClip.x + 3.f, ry, listClip.width - 6.f, optH - 2.f };
        if (rr.bottom() < listClip.y || rr.y > listClip.bottom())
            continue;

        bool hovered = idx == m_dropdownHover;
        bool active = idx == item.intVal;
        float rowAlpha = a * rowReveal;
        if (active) {
            ren.drawRoundedRect(rr,
                                m_theme->panelHighlight.withAlpha(0.065f * rowAlpha),
                                10.f);
            ren.drawRoundedRectOutline(rr.shrunk(1.f),
                                       m_theme->panelHighlight.withAlpha(0.075f * rowAlpha),
                                       9.f,
                                       1.f);
        }
        if (hovered) {
            float pulse = 0.9f + 0.1f * (std::sin(m_uiTime * 5.2f) * 0.5f + 0.5f);
            nxui::Color hi = m_theme->cursorNormal.withAlpha(0.20f * pulse * rowAlpha);
            ren.drawRoundedRect(rr, hi, 10.f);
            ren.drawRoundedRectOutline(rr,
                                       m_theme->cursorNormal.withAlpha(0.42f * rowAlpha),
                                       10.f,
                                       1.2f);
            if (m_dropdownOpen)
                m_focusCursor.moveTo(rr.shrunk(0.5f), 10.f, 0.08f);
        }

        nxui::Color tc = active ? m_theme->textPrimary : m_theme->textSecondary;
        std::string displayText = item.options[idx];
        float maxWidth = std::max(0.f, rr.width - 16.f);
        if (!displayText.empty()) {
            nxui::Vec2 tsz = m_smallFont->measure(displayText);
            if (tsz.x > maxWidth) {
                std::string ellipsis = "...";
                while (!displayText.empty()) {
                    displayText.pop_back();
                    tsz = m_smallFont->measure(displayText + ellipsis);
                    if (tsz.x <= maxWidth || displayText.empty())
                        break;
                }
                displayText += ellipsis;
            }
        }
        nxui::Vec2 tsz = m_smallFont->measure(displayText);
        float tx = rr.x + 14.f;
        float ty = rr.y + (rr.height - tsz.y * 0.84f) * 0.5f;
        ren.drawText(displayText, {tx, ty}, m_smallFont, tc.withAlpha(rowAlpha), 0.84f);
    }

    ren.popClipRect();

    if (total > visible) {
        float railH = std::max(1.f, pop.height - 24.f);
        float thumbH = std::max(24.f, railH * ((float)visible / (float)total));
        float maxStart = (float)std::max(1, total - visible);
        float thumbY = pop.y + 12.f + (railH - thumbH) * (visualStart / maxStart);
        nxui::Rect rail = {pop.right() - 10.f, pop.y + 12.f, 3.f, railH};
        nxui::Rect thumb = {pop.right() - 10.5f, thumbY, 4.f, thumbH};
        ren.drawRoundedRect(rail, m_theme->panelBorder.withAlpha(0.22f * a), 1.5f);
        ren.drawRoundedRect(thumb, m_theme->accent.withAlpha(0.56f * a), 2.f);
    }
}

void TabbedOverlayScreen::drawTrackChangedToast(nxui::Renderer& ren, const nxui::Rect& panel, float opacity) {
    if (!m_smallFont || !m_theme) return;
    float t = m_trackToastAnim.value();
    if (t <= 0.01f || m_toastText.empty()) return;

    std::string displayText = m_toastText;
    float maxTextWidth = 420.f - 40.f;
    nxui::Vec2 tsz = m_smallFont->measure(displayText);
    if (tsz.x * 0.78f > maxTextWidth) {
        while (!displayText.empty() && m_smallFont->measure(displayText + "...").x * 0.78f > maxTextWidth)
            displayText.pop_back();
        displayText += "...";
        tsz = m_smallFont->measure(displayText);
    }

    float textWidth = std::min(maxTextWidth, tsz.x * 0.78f);
    float scale = 0.96f + 0.04f * t;
    float w = std::clamp(textWidth + 40.f, 220.f, 420.f) * scale;
    float h = 40.f * scale;
    float x = panel.right() - 24.f - w;
    float y = panel.y + 18.f;
    nxui::Rect r = {x, y, w, h};

    nxui::Color bg = (m_theme->mode == nxui::ThemeMode::Dark)
        ? nxui::Color(0.10f, 0.14f, 0.20f, 0.92f * t * opacity)
        : nxui::Color(0.90f, 0.95f, 1.00f, 0.94f * t * opacity);
    nxui::Color bd = m_theme->cursorNormal.withAlpha(0.65f * t * opacity);

    ren.drawRoundedRect(r, bg, 10.f);
    ren.drawRoundedRectOutline(r, bd, 10.f, 1.5f);

    float tx = r.x + 12.f;
    float ty = r.y + (r.height - tsz.y * 0.72f) * 0.5f;
    ren.drawText(displayText, {tx, ty}, m_smallFont, m_theme->textPrimary.withAlpha(t * opacity), 0.78f);
}
