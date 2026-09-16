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
    struct BlockDiagonalMatrixBase
    {
      /// @brief Cast to Derived.
      Derived & derived()
      {
        return *static_cast<Derived *>(this);
      }

      /// @brief Const cast to Derived.
      const Derived & derived() const
      {
        return *static_cast<const Derived *>(this);
      }

      /// @brief Returns the total number of rows of the full matrix.
      Eigen::Index rows() const
      {
        return derived().rows();
      }

      /// @brief Returns the total number of cols of the full matrix.
      Eigen::Index cols() const
      {
        return derived().cols();
      }

      /// @brief Returns the total number of elements in the full matrix (rows * cols).
      Eigen::Index size() const
      {
        return derived().size();
      }
    };

  } // namespace internal
} // namespace pinocchio
