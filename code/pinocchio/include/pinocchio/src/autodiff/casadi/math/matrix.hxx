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
    template<typename Scalar>
    struct CallCorrectMatrixInverseAccordingToScalar<::casadi::Matrix<Scalar>>
    {
      typedef ::casadi::Matrix<Scalar> SX;
      template<typename MatrixIn, typename MatrixOut>
      static void
      run(const Eigen::MatrixBase<MatrixIn> & mat, const Eigen::MatrixBase<MatrixOut> & dest)
      {
        SX cs_mat(mat.rows(), mat.cols());
        casadi::copy(mat.derived(), cs_mat);

        SX cs_mat_inv = SX::inv(cs_mat);

        MatrixOut & dest_ = PINOCCHIO_EIGEN_CONST_CAST(MatrixOut, dest);
        casadi::copy(cs_mat_inv, dest_);
      }
    };
  } // namespace internal
} // namespace pinocchio
