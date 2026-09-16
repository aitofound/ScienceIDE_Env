//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/context.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include <Eigen/Core>

  #define PINOCCHIO_SCALAR_TYPE double
  #define PINOCCHIO_OPTIONS_DEFAULT 0
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace context
  {
    typedef PINOCCHIO_SCALAR_TYPE Scalar;
    static constexpr int Options = PINOCCHIO_OPTIONS_DEFAULT;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Matrix<Scalar, 6, Eigen::Dynamic, Options> Matrix6xs;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor | Options>
      RowMatrixXs;
    typedef Eigen::Matrix<Scalar, 3, Eigen::Dynamic, Options> Matrix3x;
    typedef Eigen::Matrix<Scalar, 3, 1, Options> Vector3;
    typedef Eigen::Matrix<Scalar, 6, 10, Options> BodyRegressorType;
  } // namespace context
} // namespace pinocchio
