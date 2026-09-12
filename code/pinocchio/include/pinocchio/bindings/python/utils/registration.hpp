//
// Copyright (c) 2019-2025 INRIA
//

#pragma once

#include "pinocchio/deprecated.hpp"
#include <eigenpy/registration.hpp>

PINOCCHIO_DEPRECATED_HEADER("Directly include <eigenpy/registration.hpp> instead.")

namespace pinocchio
{
  namespace python
  {
    using eigenpy::register_symbolic_link_to_registered_type;
  } // namespace python
} // namespace pinocchio
