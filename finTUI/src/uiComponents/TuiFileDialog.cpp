// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/uiComponents/TuiFileDialog.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiFileDialog::TuiFileDialog(std::string filter, std::function<void(std::string)> onSelect,
                             std::function<void()> onCancel)
    : filter_{std::move(filter)}, onSelect_{std::move(onSelect)}, onCancel_{std::move(onCancel)} {}

void TuiFileDialog::reset(std::filesystem::path startDir) {
    currentDir_ = std::move(startDir);
    try {
        scan_();
    } catch (...) {
        currentDir_ = std::filesystem::current_path();
        scan_();
    }
}

bool TuiFileDialog::OnEvent(ftxui::Event e) {
    if (entries_.empty()) {
        if (e == ftxui::Event::Escape) {
            onCancel_();
            return true;
        }
        return false;
    }
    if (e == ftxui::Event::ArrowUp) {
        if (cursor_ > 0) --cursor_;
        return true;
    }
    if (e == ftxui::Event::ArrowDown) {
        if (cursor_ < cursorMax_) ++cursor_;
        return true;
    }
    if (e == ftxui::Event::Return || e == ftxui::Event::ArrowRight) {
        if (std::filesystem::is_directory(paths_[cursor_])) {
            currentDir_ = paths_[cursor_];
            scan_();
        } else {
            onSelect_(paths_[cursor_].string());
        }
        return true;
    }
    if (e == ftxui::Event::ArrowLeft) {
        if (currentDir_ != currentDir_.root_path()) {
            currentDir_ = currentDir_.parent_path();
            scan_();
        }
        return true;
    }
    if (e == ftxui::Event::Escape) {
        onCancel_();
        return true;
    }
    return false;
}

ftxui::Element TuiFileDialog::OnRender() {
    ftxui::Elements rows;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        auto row = ftxui::text(" " + entries_[i] + " ");
        if (i == cursor_) row = row | ftxui::inverted;
        rows.push_back(row);
    }
    if (rows.empty()) rows.push_back(ftxui::text("  (empty)") | ftxui::dim);

    return ftxui::window(ftxui::text(" Open File "),
                         ftxui::vbox({
                             ftxui::text(" " + currentDir_.string()) | ftxui::dim,
                             ftxui::separator(),
                             ftxui::vbox(std::move(rows)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex_grow,
                             ftxui::separator(),
                             ftxui::text("  ↑↓ Navigate   Enter / → Open   ← Up   Esc Cancel  ") | ftxui::dim,
                         }));
}

void TuiFileDialog::scan_() {
    entries_.clear();
    paths_.clear();
    cursor_ = 0;

    if (currentDir_.has_parent_path() && currentDir_ != currentDir_.root_path()) {
        entries_.push_back("../");
        paths_.push_back(currentDir_.parent_path());
    }

    std::vector<std::filesystem::path> dirs, files;
    for (auto& entry : std::filesystem::directory_iterator(currentDir_)) {
        if (entry.is_directory())
            dirs.push_back(entry.path());
        else if (entry.path().extension() == filter_)
            files.push_back(entry.path());
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    for (auto& d : dirs) {
        entries_.push_back(d.filename().string() + "/");
        paths_.push_back(d);
    }
    for (auto& f : files) {
        entries_.push_back(f.filename().string());
        paths_.push_back(f);
    }
    cursorMax_ = static_cast<int>(entries_.size()) - 1;
}

}  // namespace finui
