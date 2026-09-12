//
// Copyright (c) 2018 CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/liegroup/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/liegroup/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename LieGroupCollection>
  struct LieGroupGenericTpl;

} // namespace pinocchio
