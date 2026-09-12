//
// Copyright (c) INRIA 2026
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  // Blank constraint model and data
  struct BlankConstraintModel : boost::blank
  {
  };

  struct BlankConstraintData : boost::blank
  {
  };
} // namespace pinocchio
