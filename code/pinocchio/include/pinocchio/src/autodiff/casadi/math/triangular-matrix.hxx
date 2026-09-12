//
// Copyright (c) 2022-2023 INRIA
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
    template<Eigen::UpLoType info, typename RhsMatrix, typename Scalar>
    struct TriangularMatrixMatrixProduct<info, RhsMatrix, ::casadi::Matrix<Scalar>, true>
    {
      template<typename LhsMatrix, typename ResMat>
      static void run(
        const Eigen::MatrixBase<LhsMatrix> & lhs_mat,
        const Eigen::MatrixBase<RhsMatrix> & rhs_vec,
        const Eigen::MatrixBase<ResMat> & res)
      {
        res.const_cast_derived().col(0).noalias() = lhs_mat.derived() * rhs_vec.derived();
      }
    };

    template<Eigen::UpLoType info, typename RhsMatrix, typename Scalar>
    struct TriangularMatrixMatrixProduct<info, RhsMatrix, ::casadi::Matrix<Scalar>, false>
    {
      template<typename LhsMatrix, typename ResMat>
      static void run(
        const Eigen::MatrixBase<LhsMatrix> & lhs_mat,
        const Eigen::MatrixBase<RhsMatrix> & rhs_mat,
        const Eigen::MatrixBase<ResMat> & res)
      {
        res.const_cast_derived().noalias() = lhs_mat.derived() * rhs_mat.derived();
      }
    };
  } // namespace internal
} // namespace pinocchio
