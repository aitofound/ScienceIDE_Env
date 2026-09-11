//
// Copyright (c) 2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/cast.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/cast.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename NewScalar, typename Scalar>
  NewScalar cast(const Scalar & value)
  {
    return Eigen::internal::cast_impl<Scalar, NewScalar>::run(value);
  }
} // namespace pinocchio
