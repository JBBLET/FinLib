// Copyright 2026 JBBLET
#pragma once

#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

namespace ts {

struct AppContext {
    ITimeSeriesSaver* saver_;
};
}  // namespace ts
