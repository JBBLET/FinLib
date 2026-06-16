// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/dom/elements.hpp"

namespace finui {

class TuiRangeSelector : public ftxui::ComponentBase {
 public:
    struct Range {
        std::string label;
        int64_t durationMs;   // 0 = ALL (caller maps to portfolio start)
        int64_t frequencyMs;
    };

    TuiRangeSelector(std::vector<Range> ranges, std::function<void(const Range&)> onSelect,
                     int initialSelected = 0);

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event e) override;

    const Range& selected() const { return ranges_[selectedIdx_]; }

 private:
    std::vector<Range> ranges_;
    std::function<void(const Range&)> onSelect_;
    int focusedIdx_ = 0;
    int selectedIdx_ = 0;
};

}  // namespace finui
