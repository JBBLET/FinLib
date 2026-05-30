// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/AppShell.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

AppShell::AppShell(std::vector<std::shared_ptr<IModule>> modules, ftxui::ScreenInteractive& screen)
    : modules_(std::move(modules)), screen_(screen) {
    // Use the active module's component directly as the CatchEvent inner.
    // Container::Tab gates OnEvent behind Focused(), which isn't reliably
    // established when the module sits below AppShell's extra wrapper layer.
    // With the module component as the direct inner, FTXUI's TakeFocus()
    // traversal reaches the module's interactive widgets (portfolioMenu_, etc.)
    // so focus is correctly set up and arrow keys / Enter reach them.
    auto inner = modules_[activeModule_]->component();

    auto withShellKeys = ftxui::CatchEvent(inner, [this](ftxui::Event e) -> bool {
        if (menuOpen_) {
            if (e == ftxui::Event::ArrowUp)   { if (menuCursor_ > 0) --menuCursor_; return true; }
            if (e == ftxui::Event::ArrowDown)  { if (menuCursor_ < (int)modules_.size() - 1) ++menuCursor_; return true; }
            if (e == ftxui::Event::Return)     { activeModule_ = menuCursor_; menuOpen_ = false; return true; }
            if (e == ftxui::Event::Escape)     { menuOpen_ = false; return true; }
            return true;  // swallow all keys while menu is open
        }
        if (e == ftxui::Event::Character('q')) { screen_.Exit(); return true; }
        if ((int)modules_.size() > 1 && e == ftxui::Event::Character('m')) {
            menuCursor_ = activeModule_;
            menuOpen_   = true;
            return true;
        }
        return false;
    });

    component_ = ftxui::Renderer(withShellKeys, [this, withShellKeys]() mutable {
        auto base = withShellKeys->Render();
        if (!menuOpen_) return base;
        return ftxui::dbox({base, renderModuleMenu_() | ftxui::clear_under | ftxui::center});
    });
}

ftxui::Component AppShell::component() { return component_; }

ftxui::Element AppShell::renderModuleMenu_() const {
    ftxui::Elements rows;
    for (int i = 0; i < (int)modules_.size(); ++i) {
        auto label = ftxui::text("  " + modules_[i]->title() + "  ");
        rows.push_back(i == menuCursor_ ? label | ftxui::inverted : label);
    }
    rows.push_back(ftxui::separator());
    rows.push_back(ftxui::text("  ↑↓ Navigate   Enter Select   Esc Cancel  ") | ftxui::dim);
    return ftxui::window(ftxui::text(" Switch Module "), ftxui::vbox(std::move(rows)));
}

}  // namespace finui
