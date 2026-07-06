#include "SettingsScreen.hpp"
#include "tabs/TabBuilders.hpp"
#include "core/DebugLog.hpp"

void SettingsScreen::buildTabs() {
    DebugLog::log("[settings] buildTabs() start");
    m_tabs.clear();
    // qlaunch-style ordering: display/network/storage first, System near the
    // bottom, About last.
    m_tabs.push_back(settings::tabs::DisplayTab::build(*this));
    m_tabs.push_back(settings::tabs::InternetTab::build(*this));
    m_tabs.push_back(settings::tabs::StorageTab::build(*this));
    m_tabs.push_back(settings::tabs::ThemesTab::build(*this));
    m_tabs.push_back(settings::tabs::AudioTab::build(*this));
    m_tabs.push_back(settings::tabs::SleepTab::build(*this));
    m_tabs.push_back(settings::tabs::ControllersTab::build(*this));
    m_tabs.push_back(settings::tabs::BluetoothTab::build(*this));
    m_tabs.push_back(settings::tabs::SystemTab::build(*this));
    m_tabs.push_back(settings::tabs::AboutTab::build(*this));
    DebugLog::log("[settings] buildTabs() done (%d tabs)", (int)m_tabs.size());

    m_cachedTabContentWidgets.clear();
    m_cachedTabContentWidgets.resize(m_tabs.size());

    if (m_tabBar) rebuildTabBar();
    if (m_tabContent) rebuildContentItems();
}
