//
// Copyright (c) 2019-2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/autodiff/cppad.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/autodiff/cppad.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    template<typename Scalar, typename ThenType, typename ElseType>
    struct if_then_else_impl<CppAD::AD<Scalar>, CppAD::AD<Scalar>, ThenType, ElseType>
    {
      typedef typename internal::traits<if_then_else_impl>::ReturnType ReturnType;

      static inline ReturnType run(
        const ComparisonOperators op,
        const CppAD::AD<Scalar> & lhs_value,
        const CppAD::AD<Scalar> & rhs_value,
        const ThenType & then_value,
        const ElseType & else_value)
      {
        switch (op)
        {
        case LT:
          return ::CppAD::CondExpLt<Scalar>(lhs_value, rhs_value, then_value, else_value);
        case LE:
          return ::CppAD::CondExpLe<Scalar>(lhs_value, rhs_value, then_value, else_value);
        case EQ:
          return ::CppAD::CondExpEq<Scalar>(lhs_value, rhs_value, then_value, else_value);
        case GE:
          return ::CppAD::CondExpGe<Scalar>(lhs_value, rhs_value, then_value, else_value);
        case GT:
          return ::CppAD::CondExpGt<Scalar>(lhs_value, rhs_value, then_value, else_value);
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }
    };
  } // namespace internal
} // namespace pinocchio
