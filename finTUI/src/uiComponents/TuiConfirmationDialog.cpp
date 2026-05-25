// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finTUI/uiComponents/TuiConfirmationDialog.hpp"

#include <string>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiConfirmationDialog::TuiConfirmationDialog(std::string title, std::string message, std::string confirmLabel,
                                             std::function<void()> onConfirm, std::string cancelLabel,
                                             std::function<void()> onCancel)
    : title_{std::move(title)}, message_{std::move(message)}, cancelCallback_{onCancel} {
    Add(ftxui::Container::Horizontal({
        ftxui::Button(cancelLabel, std::move(onCancel)),
        ftxui::Button(confirmLabel, std::move(onConfirm)),
    }));
}
bool TuiConfirmationDialog::OnEvent(ftxui::Event e) {
    if (e == ftxui::Event::Escape) {
        cancelCallback_();
        return true;
    }
    return ftxui::ComponentBase::OnEvent(e);
}
void TuiConfirmationDialog::setMessage(std::string message) { message_ = std::move(message); }

ftxui::Element TuiConfirmationDialog::OnRender() {
    return ftxui::window(ftxui::text(" " + title_ + " "),
                         ftxui::vbox({
                             ftxui::text("  " + message_),
                             ftxui::separator(),
                             ftxui::hbox({ftxui::filler(), ChildAt(0)->Render(), ftxui::filler()}),
                             ftxui::text("  enter confirm  q cancel") | ftxui::dim,
                         })) |
           ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 48);
}

TuiConfirmationDialogBuilder::TuiConfirmationDialogBuilder(std::string title) : title_{title} {}

TuiConfirmationDialogBuilder& TuiConfirmationDialogBuilder::setTitle(std::string title) {
    title_ = std::move(title);
    return *this;
}
TuiConfirmationDialogBuilder& TuiConfirmationDialogBuilder::setMessage(std::string message) {
    message_ = std::move(message);
    return *this;
}
TuiConfirmationDialogBuilder& TuiConfirmationDialogBuilder::onCancel(std::string label, std::function<void()> fn) {
    cancelLabel_ = std::move(label);
    cancelFn_ = std::move(fn);
    return *this;
}
TuiConfirmationDialogBuilder& TuiConfirmationDialogBuilder::onConfirm(std::string label, std::function<void()> fn) {
    confirmLabel_ = std::move(label);
    confirmFn_ = std::move(fn);
    return *this;
}
TuiConfirmationDialog TuiConfirmationDialogBuilder::build() {
    return TuiConfirmationDialog(std::move(title_),
                                 std::move(message_),
                                 std::move(confirmLabel_),
                                 std::move(confirmFn_),
                                 std::move(cancelLabel_),
                                 std::move(cancelFn_));
}
}  // namespace finui
