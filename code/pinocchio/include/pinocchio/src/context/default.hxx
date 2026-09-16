//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/context.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include <Eigen/Core>

  #define PINOCCHIO_SCALAR_TYPE_DEFAULT double
  #define PINOCCHIO_OPTIONS_DEFAULT 0
#endif // PINOCCHIO_LSP

#define PINOCCHIO_SCALAR_TYPE PINOCCHIO_SCALAR_TYPE_DEFAULT

// IWYU pragma: begin_exports
#include "pinocchio/src/context/generic.hxx"
// IWYU pragma: end_exports

#undef PINOCCHIO_SCALAR_TYPE
