// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finTUI/uiComponents/TuiTabbedPanel.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiTabbedPanel::TuiTabbedPanel(std::vector<Tab> tabs) : tabs_{std::move(tabs)} {
    std::vector<ftxui::Component> contents;
    contents.reserve(tabs_.size());
    for (auto& t : tabs_) {
        contents.push_back(t.content);
    }
    Add(ftxui::Container::Tab(std::move(contents), &activeTabIndex_));
}

ftxui::Element TuiTabbedPanel::OnRender() {
    ftxui::Elements tabBars;
    tabBars.reserve(tabs_.size());
    for (size_t i = 0; i < tabs_.size(); i++) {
        const std::string key(1, tabs_[i].shortcutKey);
        const std::string label = activeTabIndex_ == static_cast<int>(i) ? " [" + key + "] " + tabs_[i].label + " "
                                                                         : "  " + key + "  " + tabs_[i].label + "  ";
        tabBars.push_back(ftxui::text(label) | (activeTabIndex_ == static_cast<int>(i) ? ftxui::bold : ftxui::dim));
    }
    tabBars.push_back(ftxui::filler());
    return ftxui::window(ftxui::hbox(tabBars), ChildAt(0)->Render() | ftxui::flex_grow);
}

bool TuiTabbedPanel::OnEvent(ftxui::Event e) {
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        if (tabs_[i].shortcutKey && e == ftxui::Event::Character(tabs_[i].shortcutKey)) {
            activeTabIndex_ = i;
            if (onTabChanged_) onTabChanged_(i);
            return true;
        }
    }
    return ftxui::ComponentBase::OnEvent(e);
}

void TuiTabbedPanel::setOnTabChanged(std::function<void(int)> fn) { onTabChanged_ = std::move(fn); }

int TuiTabbedPanel::activeTab() const { return activeTabIndex_; }

TuiTabbedPanelBuilder::TuiTabbedPanelBuilder() {}

TuiTabbedPanelBuilder& TuiTabbedPanelBuilder::addTab(Tab tab) {
    tabs_.push_back(std::move(tab));
    return *this;
}
TuiTabbedPanelBuilder& TuiTabbedPanelBuilder::addAllTabs(std::vector<Tab> tabs) {
    tabs_.insert(tabs_.end(), std::make_move_iterator(tabs.begin()), std::make_move_iterator(tabs.end()));
    return *this;
}
std::shared_ptr<TuiTabbedPanel> TuiTabbedPanelBuilder::build() {
    return std::make_shared<TuiTabbedPanel>(std::move(tabs_));
}

}  // namespace finui
