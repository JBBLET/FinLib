// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

struct Tab {
    std::string label;
    char shortcutKey;
    ftxui::Component content;  // handles events + renders content for this tab
};

class TuiTabbedPanel : public ftxui::ComponentBase {
 public:
    explicit TuiTabbedPanel(std::vector<Tab> tabs);
    TuiTabbedPanel(const TuiTabbedPanel&) = delete;
    TuiTabbedPanel(TuiTabbedPanel&&) = delete;
    TuiTabbedPanel& operator=(const TuiTabbedPanel&) = delete;
    TuiTabbedPanel& operator=(TuiTabbedPanel&&) = delete;

    ftxui::Element OnRender() override;
    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event) override;
    int activeTab() const;
    void setActiveTab(int idx);
    void setOnTabChanged(std::function<void(int)> fn);

 private:
    std::vector<Tab> tabs_;
    int activeTabIndex_ = 0;
    std::function<void(int)> onTabChanged_;
};

class TuiTabbedPanelBuilder {
 public:
    TuiTabbedPanelBuilder();
    TuiTabbedPanelBuilder& addTab(Tab tab);
    TuiTabbedPanelBuilder& addAllTabs(std::vector<Tab> tabs);
    std::shared_ptr<TuiTabbedPanel> build();

 private:
    std::vector<Tab> tabs_;
};

}  // namespace finui
