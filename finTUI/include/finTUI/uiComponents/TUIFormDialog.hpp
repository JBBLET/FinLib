// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"

namespace finui {
struct TextDef {
    std::string label, placeholder;
    std::string* value;
};

struct NumberDef {
    std::string label, placeholder;
    std::string* value;
};

struct DateDef {
    std::string label;
    std::string* value;
};

struct SelectDef {
    std::string label;
    std::vector<std::string> options;
    int* selected;
    int defaultValue = 0;
};

struct ToggleDef {
    std::string label;
    bool* value;
    bool defaultValue = false;
};

struct SubmitButtonDef {
    std::string cancelLabel;
    std::function<void()> cancelFunction;
    std::string submitLabel;
    std::function<void()> submitFunction;
};

using FieldDef = std::variant<TextDef, NumberDef, DateDef, SelectDef, ToggleDef, SubmitButtonDef>;

class FormDialog {
 public:
    FormDialog(std::string title, std::vector<FieldDef> fields, std::string hint = "");

    ftxui::Component inputComponent();
    ftxui::Element render() const;
    void reset();

 private:
    std::string title_, hint_;
    std::vector<FieldDef> fields_;

    std::vector<ftxui::Component> inputs_;
    std::vector<ftxui::Element> rows_;
    // helpers
    //
    void buildComponent_(TextDef& field);
    void buildComponent_(NumberDef& field);
    void buildComponent_(DateDef& field);
    void buildComponent_(SelectDef& field);
    void buildComponent_(ToggleDef& field);
    void buildComponent_(SubmitButtonDef& field);

    void renderRow_(const TextDef& field, ftxui::Elements& rows, int& idx) const;
    void renderRow_(const NumberDef& field, ftxui::Elements& rows, int& idx) const;
    void renderRow_(const DateDef& field, ftxui::Elements& rows, int& idx) const;
    void renderRow_(const SelectDef& field, ftxui::Elements& rows, int& idx) const;
    void renderRow_(const ToggleDef& field, ftxui::Elements& rows, int& idx) const;
    void renderRow_(const SubmitButtonDef& field, ftxui::Elements& rows, int& idx) const;

    void reset(TextDef& field);
    void reset(NumberDef& field);
    void reset(DateDef& field);
    void reset(SelectDef& field);
    void reset(ToggleDef& field);
    void reset(SubmitButtonDef& field);
};
}  // namespace finui
