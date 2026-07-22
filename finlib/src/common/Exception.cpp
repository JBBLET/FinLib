// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/common/Exception.hpp"

#include <stacktrace>
#include <string>
#include <utility>

namespace ts {

Traced::Traced() : trace_{std::move(std::stacktrace::current(1))} {}

std::string Traced::formattedTrace() const { return std::to_string(trace_); }

Exception::Exception(std::string msg) : std::runtime_error(std::move(msg)), Traced() {}

InvalidArgument::InvalidArgument(std::string msg) : std::invalid_argument(std::move(msg)), Traced() {}

}  // namespace ts
