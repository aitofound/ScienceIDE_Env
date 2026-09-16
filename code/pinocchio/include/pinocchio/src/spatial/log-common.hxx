//
// Copyright (c) 2015-2021 CNRS INRIA

#pragma once

// IWYU pragma: private, include "pinocchio/spatial.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/spatial.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Scalar>
  struct log3_impl;
  template<typename Scalar>
  struct Jlog3_impl;

  template<typename Scalar>
  struct log6_impl;
  template<typename Scalar>
  struct Jlog6_impl;

  template<typename Matrix3>
  inline typename PINOCCHIO_EIGEN_PLAIN_TYPE(Matrix3)
    renormalize_rotation_matrix(const Eigen::MatrixBase<Matrix3> & R);

} // namespace pinocchio
