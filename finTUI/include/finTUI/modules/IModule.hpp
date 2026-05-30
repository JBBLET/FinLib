// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

#include "ftxui/component/component.hpp"

namespace finui {

class IModule {
 public:
    virtual ~IModule() = default;
    virtual ftxui::Component component() = 0;
    virtual std::string title() const = 0;
};

}  // namespace finui
