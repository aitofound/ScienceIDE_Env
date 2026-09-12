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
  template<typename NewScalar, typename Scalar>
  struct ScalarCast<NewScalar, CppAD::cg::CG<Scalar>>
  {
    static NewScalar cast(const CppAD::cg::CG<Scalar> & cg_value)
    {
      return static_cast<NewScalar>(cg_value.getValue());
    }
  };
} // namespace pinocchio
