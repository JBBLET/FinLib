// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <stacktrace>
#include <stdexcept>
#include <string>

namespace ts {

class Traced {
 public:
    Traced();
    const std::stacktrace& trace() const noexcept { return trace_; };
    std::string formattedTrace() const;

 private:
    std::stacktrace trace_;
};

class Exception : public std::runtime_error, public Traced {
 public:
    explicit Exception(std::string msg);
};

class InvalidArgument : public std::invalid_argument, public Traced {
 public:
    explicit InvalidArgument(std::string msg);
};

std::string describe(const Traced& e);

}  // namespace ts
