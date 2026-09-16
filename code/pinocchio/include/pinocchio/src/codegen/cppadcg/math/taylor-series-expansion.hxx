//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/codegen/cppadcg.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/codegen/cppadcg.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename Scalar>
  struct TaylorSeriesExpansion<CppAD::cg::CG<Scalar>>
  {
    typedef TaylorSeriesExpansion<Scalar> Base;
    typedef CppAD::cg::CG<Scalar> CGScalar;

    template<int degree>
    static CGScalar precision()
    {
      return CGScalar(Base::template precision<degree>());
    }
  };
} // namespace pinocchio
