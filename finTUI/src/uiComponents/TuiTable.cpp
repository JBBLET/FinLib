// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/uiComponents/TuiTable.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiTable::TuiTable(std::vector<ColumnDef> columns, std::function<int()> rowCountFn)
    : columns_{std::move(columns)}, rowCountFn_{std::move(rowCountFn)} {}

bool TuiTable::OnEvent(ftxui::Event e) {
    if (e == ftxui::Event::ArrowUp) return prevRow();
    if (e == ftxui::Event::ArrowDown) return nextRow();
    if (e == ftxui::Event::Return && onActivate_) {
        onActivate_(selectedRow_);
        return true;
    }
    if (e == ftxui::Event::Character('e') && onEdit_) {
        onEdit_(selectedRow_);
        return true;
    }
    if (e == ftxui::Event::Character('x') && onDelete_) {
        onDelete_(selectedRow_);
        return true;
    }
    if (e == ftxui::Event::Character('t') && onAdd_) {
        onAdd_();
        return true;
    }
    if (e == ftxui::Event::Character('i') && onImport_) {
        onImport_();
        return true;
    }
    return false;
}

ftxui::Element TuiTable::OnRender() {
    const int count = rowCountFn_ ? rowCountFn_() : 0;

    ftxui::Elements rows;
    if (hasGroups_()) rows.push_back(renderGroupRow_());
    rows.push_back(renderHeaderRow_());
    rows.push_back(ftxui::separator());

    if (count == 0) {
        rows.push_back(ftxui::text("  —") | ftxui::dim);
    } else {
        for (int i = 0; i < count; ++i) rows.push_back(renderDataRow_(i, i == selectedRow_));
    }

    return ftxui::vbox(std::move(rows)) | ftxui::vscroll_indicator | ftxui::frame | ftxui::flex_grow;
}

int TuiTable::selectedRow() const { return selectedRow_; }

void TuiTable::setRowCount(int n) {
    if (n <= 0) {
        selectedRow_ = 0;
        return;
    }
    selectedRow_ = std::min(selectedRow_, n - 1);
}

bool TuiTable::prevRow() {
    if (selectedRow_ > 0) {
        --selectedRow_;
        return true;
    }
    return false;
}

bool TuiTable::nextRow() {
    const int count = rowCountFn_ ? rowCountFn_() : 0;
    if (selectedRow_ < count - 1) {
        ++selectedRow_;
        return true;
    }
    return false;
}

void TuiTable::setOnActivate(std::function<void(int)> fn) { onActivate_ = std::move(fn); }
void TuiTable::setOnEdit(std::function<void(int)> fn) { onEdit_ = std::move(fn); }
void TuiTable::setOnDelete(std::function<void(int)> fn) { onDelete_ = std::move(fn); }
void TuiTable::setOnAdd(std::function<void()> fn) { onAdd_ = std::move(fn); }
void TuiTable::setOnImport(std::function<void()> fn) { onImport_ = std::move(fn); }

bool TuiTable::hasGroups_() const {
    for (const auto& c : columns_)
        if (!c.group.empty()) return true;
    return false;
}

ftxui::Element TuiTable::renderGroupRow_() const {
    ftxui::Elements cells;
    int i = 0;
    while (i < static_cast<int>(columns_.size())) {
        const std::string& grp = columns_[i].group;
        int totalWidth = columns_[i].width;
        int j = i + 1;
        if (!grp.empty()) {
            while (j < static_cast<int>(columns_.size()) && columns_[j].group == grp) {
                totalWidth += columns_[j].width;
                ++j;
            }
            cells.push_back(ftxui::text(" " + grp) | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, totalWidth));
        } else {
            cells.push_back(ftxui::text("") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, totalWidth));
            j = i + 1;
        }
        i = j;
    }
    return ftxui::hbox(std::move(cells));
}

ftxui::Element TuiTable::renderHeaderRow_() const {
    ftxui::Elements cells;
    for (const auto& c : columns_)
        cells.push_back(ftxui::text(" " + c.header) | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, c.width));
    return ftxui::hbox(std::move(cells));
}

ftxui::Element TuiTable::renderDataRow_(int idx, bool selected) const {
    ftxui::Elements cells;
    for (const auto& c : columns_) {
        const std::string val = c.valueFn ? c.valueFn(idx) : "";
        cells.push_back(ftxui::text(" " + val) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, c.width));
    }
    auto row = ftxui::hbox(std::move(cells));
    return selected ? row | ftxui::inverted | ftxui::focus : row;
}

TuiTableBuilder& TuiTableBuilder::addColumn(ColumnDef col) {
    columns_.push_back(std::move(col));
    return *this;
}

TuiTableBuilder& TuiTableBuilder::setRowCount(std::function<int()> fn) {
    rowCountFn_ = std::move(fn);
    return *this;
}

std::shared_ptr<TuiTable> TuiTableBuilder::build() {
    return std::make_shared<TuiTable>(std::move(columns_), std::move(rowCountFn_));
}

}  // namespace finui
