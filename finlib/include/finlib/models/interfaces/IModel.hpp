// Copyright 2026 JBBLET
#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace models {

class IModel : public std::enable_shared_from_this<IModel> {
 public:
    virtual ~IModel() = default;

    virtual std::string name() const = 0;
    virtual std::string print() const = 0;

    virtual void fit() = 0;

    virtual bool requiresRegularSpacing() const = 0;
    virtual double regularityTolerance() const = 0;
    bool isFitted() const { return isFitted_; }

    virtual size_t contextSize() const = 0;

 protected:
    bool isFitted_ = false;
};

}  // namespace models
