// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "finapp/common/logger/ILogger.hpp"

namespace finapp::logging {

class PrefixedLogger : public ILogger {
 public:
    PrefixedLogger(ILogger* inner, std::string tag) : inner_(inner), prefix_("[" + std::move(tag) + "] ") {}

    void write(Level lvl, const std::string& msg) override {
        if (inner_) inner_->write(lvl, prefix_ + msg);
    }

    static std::unique_ptr<ILogger> wrap(ILogger* inner, const std::string& tag) {
        if (!inner) return nullptr;
        return std::make_unique<PrefixedLogger>(inner, tag);
    }

 private:
    ILogger* inner_;
    std::string prefix_;
};

}  // namespace finapp::logging
