// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"

namespace finui {

class TuiFileDialog : public ftxui::ComponentBase {
 public:
    explicit TuiFileDialog(std::string filter, std::function<void(std::string)> onSelect,
                           std::function<void()> onCancel);
    TuiFileDialog(TuiFileDialog&) = delete;
    TuiFileDialog(TuiFileDialog&&) = delete;
    TuiFileDialog& operator=(TuiFileDialog&) = delete;
    TuiFileDialog& operator=(TuiFileDialog&&) = delete;

    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event) override;
    ftxui::Element OnRender() override;
    void reset(std::filesystem::path startDir = std::filesystem::current_path());

 private:
    std::string filter_;
    std::function<void(std::string)> onSelect_;
    std::function<void()> onCancel_;

    std::filesystem::path currentDir_;
    std::vector<std::string> entries_;
    std::vector<std::filesystem::path> paths_;
    int cursor_ = 0;
    int cursorMax_;

    void scan_();
};

}  // namespace finui
