// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/common/Exception.hpp"

#include <string>
namespace finapp {

std::string Traced::formattedTrace() const { return std::to_string(trace_); }
std::string describe(const Traced& e) { return e.formattedTrace(); }
}  // namespace finapp
