// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finTUI/uiComponents/TuiFormDialog.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

TuiFormDialog::TuiFormDialog(std::string title, std::vector<FieldDef> fields, std::string hint)
    : title_{std::move(title)}, fields_{std::move(fields)}, hint_{std::move(hint)} {
    bool hasButtonDefinition = std::any_of(
        fields_.begin(), fields_.end(), [](const auto& v) { return std::holds_alternative<SubmitButtonDef>(v); });
    if (!hasButtonDefinition) {
        throw(std::runtime_error("Form Definition is missing a Cancel and submit function"));
    }
    if (hint_ == "") {
        hint_ = std::move(" navigate enter submit q cancel");
    }
    for (FieldDef& f : fields_) {
        std::visit([this](auto& field) { buildComponent_(field); }, f);
    }
    Add(ftxui::Container::Vertical(inputs_));
}

ftxui::Element TuiFormDialog::OnRender() {
    ftxui::Elements rows;
    rows.reserve(fields_.size());
    int idx = 0;
    for (const FieldDef& element : fields_) {
        std::visit([this, &rows, &idx](auto& field) { renderRow_(field, rows, idx); }, element);
    }
    rows.push_back(ftxui::text(" " + hint_) | ftxui::dim);
    return ftxui::window(ftxui::text(" " + title_ + " "), ftxui::vbox(rows)) |
           ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 48);
}

bool TuiFormDialog::OnEvent(ftxui::Event e) {
    if (e == ftxui::Event::Escape) {
        reset();
        cancelFn_();
        return true;
    }
    return ftxui::ComponentBase::OnEvent(e);
}

void TuiFormDialog::reset() {
    for (FieldDef& element : fields_) {
        std::visit([this](auto& field) { reset(field); }, element);
    }
    if (!inputs_.empty()) inputs_[0]->TakeFocus();
}

// Helpers
void TuiFormDialog::buildComponent_(TextDef& field) { inputs_.push_back(ftxui::Input(field.value, field.placeholder)); }
void TuiFormDialog::buildComponent_(NumberDef& field) {
    inputs_.push_back(ftxui::Input(field.value, field.placeholder));
}
void TuiFormDialog::buildComponent_(DateDef& field) { inputs_.push_back(ftxui::Input(field.value, "YYYY-MM-DD")); }
void TuiFormDialog::buildComponent_(SelectDef& field) {
    inputs_.push_back(ftxui::Dropdown(&field.options, field.selected));
}
void TuiFormDialog::buildComponent_(ToggleDef& field) { inputs_.push_back(ftxui::Checkbox(field.label, field.value)); }
void TuiFormDialog::buildComponent_(SubmitButtonDef& field) {
    cancelFn_ = field.cancelFunction;
    confirmFn_ = field.submitFunction;
    inputs_.push_back(
        ftxui::Container::Horizontal({ftxui::Button(field.cancelLabel,
                                                    [&] {
                                                        reset();
                                                        field.cancelFunction();
                                                    }),
                                      ftxui::Button(field.submitLabel, [&] { field.submitFunction(); })}));
}

static ftxui::Element labeledRow(const std::string& label, ftxui::Element input) {
    return ftxui::hbox({
        ftxui::text(" " + label + ": ") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 14) | ftxui::vcenter,
        std::move(input) | ftxui::flex_grow,
    });
}

void TuiFormDialog::renderRow_(const TextDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void TuiFormDialog::renderRow_(const NumberDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void TuiFormDialog::renderRow_(const DateDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void TuiFormDialog::renderRow_(const SelectDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void TuiFormDialog::renderRow_(const ToggleDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(ftxui::hbox({ftxui::text(" "), inputs_[idx++]->Render()}));
}
void TuiFormDialog::renderRow_(const SubmitButtonDef& /*field*/, ftxui::Elements& rows, int& idx) const {
    rows.push_back(ftxui::hbox({ftxui::filler(), inputs_[idx++]->Render(), ftxui::filler()}));
}

void TuiFormDialog::reset(TextDef& field) { *field.value = ""; }
void TuiFormDialog::reset(NumberDef& field) { *field.value = ""; }
void TuiFormDialog::reset(DateDef& field) { *field.value = ""; }
void TuiFormDialog::reset(SelectDef& field) { *field.selected = field.defaultValue; }
void TuiFormDialog::reset(ToggleDef& field) { *field.value = field.defaultValue; }
void TuiFormDialog::reset(SubmitButtonDef& /*field*/) {}

// ─── FormDialogBuilder ────────────────────────────────────────────────────────

TuiFormDialogBuilder::TuiFormDialogBuilder(std::string title) : title_{std::move(title)} {}

TuiFormDialogBuilder& TuiFormDialogBuilder::add(FieldDef field) {
    fields_.push_back(std::move(field));
    return *this;
}

TuiFormDialogBuilder& TuiFormDialogBuilder::setOnCancel(std::string label, std::function<void()> fn) {
    cancelLabel_ = std::move(label);
    cancelFn_ = std::move(fn);
    return *this;
}

TuiFormDialogBuilder& TuiFormDialogBuilder::setOnConfirm(std::string label, std::function<void()> fn) {
    confirmLabel_ = std::move(label);
    confirmFn_ = std::move(fn);
    return *this;
}

std::shared_ptr<TuiFormDialog> TuiFormDialogBuilder::build() {
    fields_.push_back(SubmitButtonDef{cancelLabel_, std::move(cancelFn_), confirmLabel_, std::move(confirmFn_)});
    return std::make_shared<TuiFormDialog>(std::move(title_), std::move(fields_));
}

}  // namespace finui
