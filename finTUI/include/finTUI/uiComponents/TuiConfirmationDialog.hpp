// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <functional>
#include <string>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"

namespace finui {

class TuiConfirmationDialog : public ftxui::ComponentBase {
 public:
    TuiConfirmationDialog(std::string title, std::string message, std::string confirmLabel,
                          std::function<void()> onConfirm, std::string cancelLabel, std::function<void()> onCancel);

    ftxui::Element OnRender() override;
    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event) override;
    void setMessage(std::string message);

 private:
    std::string title_, message_;
    std::function<void()> cancelCallback_;
};

class TuiConfirmationDialogBuilder {
 public:
    explicit TuiConfirmationDialogBuilder(std::string title = "");

    TuiConfirmationDialogBuilder& setTitle(std::string title);
    TuiConfirmationDialogBuilder& setMessage(std::string message);
    TuiConfirmationDialogBuilder& onCancel(std::string label, std::function<void()> fn);
    TuiConfirmationDialogBuilder& onConfirm(std::string label, std::function<void()> fn);
    TuiConfirmationDialog build();

 private:
    std::string title_;
    std::string message_;
    std::string cancelLabel_ = "Cancel";
    std::string confirmLabel_ = "Ok";
    std::function<void()> cancelFn_;
    std::function<void()> confirmFn_;
};
}  // namespace finui
