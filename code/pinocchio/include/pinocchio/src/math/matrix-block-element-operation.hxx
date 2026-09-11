//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {

    template<typename Derived>
    struct MatrixBlockElementPlain;

    template<typename Derived>
    struct MatrixBlockElementOperation : MatrixBlockElementBase<Derived>
    {

      typedef MatrixBlockElementBase<Derived> Base;
      using Base::derived;

      template<typename OtherDerived>
      void evalTo(MatrixBlockElementPlain<OtherDerived> & res) const
      {
        derived().evalTo(res.derived());
      }
    };
  } // namespace internal

} // namespace pinocchio
