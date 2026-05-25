// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <vector>

#include "finTUI/modules/IModule.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

namespace finui {

// Top-level app shell. Renders the active module full-screen with no visible
// chrome. Press 'm' to open the module selector overlay (only when multiple
// modules are registered). Press 'q' to quit.
class AppShell {
 public:
    AppShell(std::vector<std::shared_ptr<IModule>> modules, ftxui::ScreenInteractive& screen);
    AppShell(const AppShell&)            = delete;
    AppShell(AppShell&&)                 = delete;
    AppShell& operator=(const AppShell&) = delete;
    AppShell& operator=(AppShell&&)      = delete;

    ftxui::Component component();

 private:
    std::vector<std::shared_ptr<IModule>> modules_;
    ftxui::ScreenInteractive&             screen_;
    int                                   activeModule_ = 0;
    bool                                  menuOpen_     = false;
    int                                   menuCursor_   = 0;
    ftxui::Component                      component_;

    ftxui::Element renderModuleMenu_() const;
};

}  // namespace finui
