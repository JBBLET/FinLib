// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finTUI/uiComponents/TUIFormDialog.hpp"

#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

FormDialog::FormDialog(std::string title, std::vector<FieldDef> fields, std::string hint)
    : title_{std::move(title)}, fields_{std::move(fields)}, hint_{std::move(hint)} {
    if (hint_ == "") {
        hint_ = std::move(" navigate enter submit q cancel");
    }
    for (FieldDef& element : fields_) {
        std::visit([this](auto& field) { buildComponent_(field); }, element);
    }
}

ftxui::Component FormDialog::inputComponent() { return ftxui::Container::Vertical(inputs_); }

ftxui::Element FormDialog::render() const {
    ftxui::Elements rows;
    rows.reserve(fields_.size());
    int idx = 0;
    for (const FieldDef& element : fields_) {
        std::visit([this, &rows, &idx](auto& field) { renderRow_(field, rows, idx); }, element);
    }
    rows.push_back(ftxui::text(" " + hint_) | ftxui::dim);
    return ftxui::window(ftxui::text(" " + title_ + " "), ftxui::vbox(rows))
           | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 48);
}

void FormDialog::reset() {
    for (FieldDef& element : fields_)
        std::visit([this](auto& field) { reset(field); }, element);
}
// Helpers
void FormDialog::buildComponent_(TextDef& field) { inputs_.push_back(ftxui::Input(field.value, field.placeholder)); }
void FormDialog::buildComponent_(NumberDef& field) { inputs_.push_back(ftxui::Input(field.value, field.placeholder)); }
void FormDialog::buildComponent_(DateDef& field) { inputs_.push_back(ftxui::Input(field.value, "YYYY-MM-DD")); }
void FormDialog::buildComponent_(SelectDef& field) {
    inputs_.push_back(ftxui::Dropdown(&field.options, field.selected));
}
void FormDialog::buildComponent_(ToggleDef& field) { inputs_.push_back(ftxui::Checkbox(field.label, field.value)); }
void FormDialog::buildComponent_(SubmitButtonDef& field) {
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

void FormDialog::renderRow_(const TextDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void FormDialog::renderRow_(const NumberDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void FormDialog::renderRow_(const DateDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void FormDialog::renderRow_(const SelectDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(labeledRow(field.label, inputs_[idx++]->Render()));
}
void FormDialog::renderRow_(const ToggleDef& field, ftxui::Elements& rows, int& idx) const {
    rows.push_back(ftxui::hbox({ftxui::text(" "), inputs_[idx++]->Render()}));
}
void FormDialog::renderRow_(const SubmitButtonDef& /*field*/, ftxui::Elements& rows, int& idx) const {
    rows.push_back(ftxui::hbox({ftxui::filler(), inputs_[idx++]->Render(), ftxui::filler()}));
}

void FormDialog::reset(TextDef& field) { *field.value = ""; }
void FormDialog::reset(NumberDef& field) { *field.value = ""; }
void FormDialog::reset(DateDef& field) { *field.value = ""; }
void FormDialog::reset(SelectDef& field) { *field.selected = field.defaultValue; }
void FormDialog::reset(ToggleDef& field) { *field.value = field.defaultValue; }
void FormDialog::reset(SubmitButtonDef& /*field*/) {}
}  // namespace finui
