// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/uiComponents/TuiRangeSelector.hpp"

#include <utility>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace finui {

TuiRangeSelector::TuiRangeSelector(std::vector<Range> ranges, std::function<void(const Range&)> onSelect,
                                   int initialSelected)
    : ranges_(std::move(ranges)),
      onSelect_(std::move(onSelect)),
      focusedIdx_(initialSelected),
      selectedIdx_(initialSelected) {}

ftxui::Element TuiRangeSelector::OnRender() {
    ftxui::Elements buttons;
    const bool focused = Focused();
    for (int i = 0; i < static_cast<int>(ranges_.size()); ++i) {
        auto label = " " + ranges_[i].label + " ";
        ftxui::Element elem = ftxui::text(label);
        if (i == selectedIdx_) {
            elem = elem | ftxui::inverted | ftxui::bold;
        } else if (focused && i == focusedIdx_) {
            elem = elem | ftxui::underlined;
        } else {
            elem = elem | ftxui::dim;
        }
        buttons.push_back(elem);
    }
    return ftxui::hbox(std::move(buttons));
}

bool TuiRangeSelector::OnEvent(ftxui::Event e) {
    if (e == ftxui::Event::ArrowLeft) {
        if (focusedIdx_ > 0) {
            --focusedIdx_;
            return true;
        }
        return false;
    }
    if (e == ftxui::Event::ArrowRight) {
        if (focusedIdx_ < static_cast<int>(ranges_.size()) - 1) {
            ++focusedIdx_;
            return true;
        }
        return false;
    }
    if (e == ftxui::Event::Return) {
        selectedIdx_ = focusedIdx_;
        if (onSelect_) onSelect_(ranges_[selectedIdx_]);
        return true;
    }
    return false;
}

}  // namespace finui
