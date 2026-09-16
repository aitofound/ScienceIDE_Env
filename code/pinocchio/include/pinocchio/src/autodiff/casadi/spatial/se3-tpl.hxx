//
// Copyright (c) 2020 INRIA
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
    template<typename Scalar, int Options, typename NewScalar>
    struct cast_call_normalize_method<
      SE3Tpl<::casadi::Matrix<Scalar>, Options>,
      NewScalar,
      ::casadi::Matrix<Scalar>>
    {
      template<typename T>
      static void run(T &)
      {
        // do nothing
      }
    };

    template<typename Scalar, int Options, typename NewScalar>
    struct cast_call_normalize_method<SE3Tpl<Scalar, Options>, ::casadi::Matrix<NewScalar>, Scalar>
    {
      template<typename T>
      static void run(T &)
      {
        // do nothing
      }
    };

  } // namespace internal

} // namespace pinocchio
