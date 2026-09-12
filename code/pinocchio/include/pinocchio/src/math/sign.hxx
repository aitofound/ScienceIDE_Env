//
// Copyright (c) 2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  ///
  /// \brief Returns the robust sign of t
  ///
  template<typename Scalar>
  Scalar sign(const Scalar & t)
  {
    return (t > Scalar(0)) - (t < Scalar(0));
  }
} // namespace pinocchio
