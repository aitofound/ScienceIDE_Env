//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/autodiff/casadi.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/autodiff/casadi.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename Scalar>
  struct TaylorSeriesExpansion<::casadi::Matrix<Scalar>> : TaylorSeriesExpansion<Scalar>
  {
    typedef TaylorSeriesExpansion<Scalar> Base;

    template<int degree>
    static ::casadi::Matrix<Scalar> precision()
    {
      return ::casadi::Matrix<Scalar>(Base::template precision<degree>());
    }
  };

} // namespace pinocchio
