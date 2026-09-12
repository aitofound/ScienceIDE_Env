//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/spatial.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/spatial.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  // This function is defined here because it need se3-tpl and explog
  // headers
  template<typename Scalar, int Options>
  template<typename OtherScalar>
  SE3Tpl<Scalar, Options> SE3Tpl<Scalar, Options>::Interpolate(
    const SE3Tpl & A, const SE3Tpl & B, const OtherScalar & alpha)
  {
    typedef SE3Tpl<Scalar, Options> ReturnType;
    typedef MotionTpl<Scalar, Options> Motion;

    Motion dv = log6(A.actInv(B));
    ReturnType res = A * exp6(alpha * dv);
    return res;
  }
} // namespace pinocchio
