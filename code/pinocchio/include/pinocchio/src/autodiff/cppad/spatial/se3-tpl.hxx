//
// Copyright (c) 2020 INRIA
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
    template<typename Scalar, int Options, typename NewScalar>
    struct cast_call_normalize_method<
      SE3Tpl<CppAD::AD<Scalar>, Options>,
      NewScalar,
      CppAD::AD<Scalar>>
    {
      template<typename T>
      static void run(T &)
      {
        // do nothing
      }
    };

    template<typename Scalar, int Options, typename NewScalar>
    struct cast_call_normalize_method<SE3Tpl<Scalar, Options>, CppAD::AD<NewScalar>, Scalar>
    {
      template<typename T>
      static void run(T &)
      {
        // do nothing
      }
    };

  } // namespace internal

} // namespace pinocchio
