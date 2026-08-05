// Copyright 2026 JBBLET
#pragma once
#include <concepts>
#include <cstddef>
#include <format>
#include <memory>
#include <print>
#include <string>

#include "finlib/common/Error.hpp"
#include "finlib/common/Format.hpp"

namespace ts::models {

class IModel : public std::enable_shared_from_this<IModel> {
 public:
    IModel() = default;
    virtual ~IModel() = default;
    IModel(const IModel&) = default;
    IModel& operator=(const IModel&) = default;
    IModel(IModel&&) = default;
    IModel& operator=(IModel&&) = default;

    virtual std::string name() const = 0;

    // Display. Identity is the one-liner that goes in a log or an exception; describe is the
    // fitted summary — for a regression, the coefficient table. Implementations must stay
    // safe on an unfitted model, whose parameter vectors are sized but uninitialised.
    virtual std::string toString(const fmt::FormatSpec& spec) const = 0;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const {
        std::println("{}", toString(spec));
    }
    // Retained spelling of the identity form.
    std::string print() const { return toString({}); }

    virtual void fit() = 0;

    virtual bool requiresRegularSpacing() const = 0;
    virtual double regularityTolerance() const = 0;
    bool isFitted() const { return isFitted_; }

    virtual size_t contextSize() const = 0;
    virtual std::unique_ptr<IModel> createFresh() const = 0;

 protected:
    bool isFitted_ = false;
};

template <class T>
std::unique_ptr<T> createFreshAs(const ts::models::IModel& m) {
    auto p = m.createFresh();
    auto* q = dynamic_cast<T*>(p.get());
    ts::ensure(q != nullptr, "{}: createFresh returned an incompatible type", m.name());
    p.release();
    return std::unique_ptr<T>(q);
}
}  // namespace ts::models

// One specialization covers every model, present and future, because toString is virtual —
// formatting through an IModel& dispatches to the concrete override.
template <class T>
    requires std::derived_from<T, ts::models::IModel>
struct std::formatter<T> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const T& model, std::format_context& ctx) const -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", model.toString(spec));
    }
};
