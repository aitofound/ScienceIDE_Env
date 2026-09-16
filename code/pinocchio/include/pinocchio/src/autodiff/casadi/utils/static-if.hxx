//
// Copyright (c) 2019-2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/autodiff/casadi.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/autodiff/casadi.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {

    template<typename Scalar, typename ThenType, typename ElseType>
    struct if_then_else_impl<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>, ThenType, ElseType>
    {
      typedef typename internal::traits<if_then_else_impl>::ReturnType ReturnType;

      typedef ::casadi::Matrix<Scalar> CasadiType;

      static inline ReturnType run(
        const ComparisonOperators op,
        const CasadiType & lhs_value,
        const CasadiType & rhs_value,
        const ThenType & then_value,
        const ElseType & else_value)
      {
        switch (op)
        {
        case LT:
          return ::casadi::Matrix<Scalar>::if_else(lhs_value < rhs_value, then_value, else_value);
        case LE:
          return ::casadi::Matrix<Scalar>::if_else(lhs_value <= rhs_value, then_value, else_value);
        case EQ:
          return ::casadi::Matrix<Scalar>::if_else(lhs_value == rhs_value, then_value, else_value);
        case GE:
          return ::casadi::Matrix<Scalar>::if_else(lhs_value >= rhs_value, then_value, else_value);
        case GT:
          return ::casadi::Matrix<Scalar>::if_else(lhs_value > rhs_value, then_value, else_value);
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }
    };

    template<typename Scalar>
    struct comparison_eq_impl<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>>
    {
      typedef ::casadi::Matrix<Scalar> CasadiType;

      static inline bool run(const CasadiType & lhs_value, const CasadiType & rhs_value)
      {
        return (lhs_value == rhs_value).is_zero();
      }
    };

  } // namespace internal
} // namespace pinocchio

namespace std
{

  template<typename Scalar>
  struct equal_to<::casadi::Matrix<Scalar>>
  {
    bool operator()(
      const ::casadi::Matrix<Scalar> & lhs_value, const ::casadi::Matrix<Scalar> & rhs_value) const
    {
      return (lhs_value == rhs_value).is_zero();
    }
  };
} // namespace std
