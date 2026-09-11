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
    struct BlockDiagonalMatrixExpression : BlockDiagonalMatrixBase<Derived>
    {
      typedef BlockDiagonalMatrixBase<Derived> Base;
      using Base::derived;

      /// @brief Evaluates this expression and stores it in res.
      template<typename Scalar, int Options, std::size_t Alignment>
      void evalTo(BlockDiagonalMatrixTpl<Scalar, Options, Alignment> & res) const
      {
        derived().evalTo(res.derived());
      }
    }; // struct BlockDiagonalMatrixExpression

  } // namespace internal
} // namespace pinocchio
