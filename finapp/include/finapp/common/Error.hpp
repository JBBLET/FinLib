// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include "cpputils/error.hpp"

namespace finapp {
using cpputils::ensure;
using cpputils::Exception;
using cpputils::expect;
using cpputils::InvalidArgument;
}  // namespace finapp

// The domain-model layer is part of the same library, so it gets the same error
// vocabulary — otherwise every site there needs finapp::ensure<finapp::InvalidArgument>.
namespace finance {
using cpputils::ensure;
using cpputils::Exception;
using cpputils::expect;
using cpputils::InvalidArgument;
}  // namespace finance
