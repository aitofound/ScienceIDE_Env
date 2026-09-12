//
// Copyright (c) 2025 INRIA
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
    template<int RowsAtCompileTime, int ColsAtCompileTime = RowsAtCompileTime>
    struct MatrixInversionCodeGeneratedImpl
    {
      template<typename M1, typename M2>
      static EIGEN_STRONG_INLINE void run(
        const Eigen::MatrixBase<M1> & /*matrix*/, const Eigen::MatrixBase<M2> & /*matrix_inverse*/)
      {
        assert(false && "Not implemented.");
        PINOCCHIO_UNREACHABLE();
      }
    };

    template<typename M1, typename M2>
    EIGEN_STRONG_INLINE void matrix_inversion_code_generated(
      const Eigen::MatrixBase<M1> & matrix, const Eigen::MatrixBase<M2> & matrix_inverse)
    {
      typedef internal::MatrixInversionCodeGeneratedImpl<
        M1::RowsAtCompileTime, M1::ColsAtCompileTime>
        Runner;
      Runner::run(matrix, matrix_inverse.const_cast_derived());
    }

  } // namespace internal
} // namespace pinocchio
