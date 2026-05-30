// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

struct ColumnDef {
    std::string group;  // empty = no group; adjacent equal strings merge into one top header cell
    std::string header;
    int width;
    std::function<std::string(int rowIdx)> valueFn;
};

class TuiTable : public ftxui::ComponentBase {
 public:
    explicit TuiTable(std::vector<ColumnDef> columns, std::function<int()> rowCountFn);
    TuiTable(const TuiTable&) = delete;
    TuiTable(TuiTable&&) = delete;
    TuiTable& operator=(const TuiTable&) = delete;
    TuiTable& operator=(TuiTable&&) = delete;

    bool Focusable() const override { return true; }
    bool OnEvent(ftxui::Event) override;
    ftxui::Element OnRender() override;

    int selectedRow() const;
    void setRowCount(int n);
    void setOnActivate(std::function<void(int)> fn);  // Enter
    void setOnEdit(std::function<void(int)> fn);      // 'e'
    void setOnDelete(std::function<void(int)> fn);    // 'x'
    void setOnAdd(std::function<void()> fn);          // 't'
    void setOnImport(std::function<void()> fn);       // 'i'

 private:
    std::vector<ColumnDef> columns_;
    std::function<int()> rowCountFn_;
    int selectedRow_ = 0;
    std::function<void(int)> onActivate_;
    std::function<void(int)> onEdit_;
    std::function<void(int)> onDelete_;
    std::function<void()> onAdd_;
    std::function<void()> onImport_;

    bool prevRow();
    bool nextRow();
    bool hasGroups_() const;
    ftxui::Element renderGroupRow_() const;
    ftxui::Element renderHeaderRow_() const;
    ftxui::Element renderDataRow_(int idx, bool selected) const;
};

class TuiTableBuilder {
 public:
    TuiTableBuilder& addColumn(ColumnDef col);
    TuiTableBuilder& setRowCount(std::function<int()> fn);
    std::shared_ptr<TuiTable> build();

 private:
    std::vector<ColumnDef> columns_;
    std::function<int()> rowCountFn_;
};

}  // namespace finui
