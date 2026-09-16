//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/common/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  struct DataEntity;
  template<typename Derived>
  struct ModelEntity;

} // namespace pinocchio
