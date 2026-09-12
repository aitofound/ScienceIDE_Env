//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/autodiff/cppad.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/autodiff/cppad.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename Scalar>
  struct TaylorSeriesExpansion<CppAD::AD<Scalar>> : TaylorSeriesExpansion<Scalar>
  {
    typedef TaylorSeriesExpansion<Scalar> Base;
    typedef CppAD::AD<Scalar> ADScalar;

    template<int degree>
    static ADScalar precision()
    {
      return ADScalar(Base::template precision<degree>());
    }
  };

  template<typename Scalar, typename ADScalar>
  struct ScalarCast<Scalar, CppAD::AD<ADScalar>>
  {
    static Scalar cast(const CppAD::AD<ADScalar> & value)
    {
      return scalar_cast<Scalar>(CppAD::Value(value));
    }
  };

} // namespace pinocchio
