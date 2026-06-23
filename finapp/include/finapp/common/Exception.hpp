// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <stacktrace>
#include <stdexcept>
#include <string>
#include <utility>
namespace finapp {

class Traced {
 public:
    explicit Traced(std::stacktrace trace = std::stacktrace::current()) : trace_{std::move(trace)} {}
    const std::stacktrace& trace() const noexcept { return trace_; };
    std::string formattedTrace() const;

 private:
    std::stacktrace trace_;
};

class Exception : public std::runtime_error, public Traced {
 public:
    explicit Exception(std::string msg) : std::runtime_error(std::move(msg)), Traced() {}
};

class InvalidArgument : public std::invalid_argument, public Traced {
 public:
    explicit InvalidArgument(std::string msg) : std::invalid_argument(std::move(msg)), Traced() {}
};

std::string describe(const Traced& e);

}  // namespace finapp
