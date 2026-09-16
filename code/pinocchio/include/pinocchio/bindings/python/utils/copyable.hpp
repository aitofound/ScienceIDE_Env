//
// Copyright (c) 2016-2018 CNRS
// Copyright (c) 2018-2025 INRIA
//

#pragma once

#include <eigenpy/copyable.hpp>

#include "pinocchio/deprecated.hpp"

PINOCCHIO_DEPRECATED_HEADER("Directly include <eigenpy/copyable.hpp> instead.")

namespace pinocchio
{
  namespace python
  {

    using ::eigenpy::CopyableVisitor;

  } // namespace python
} // namespace pinocchio
