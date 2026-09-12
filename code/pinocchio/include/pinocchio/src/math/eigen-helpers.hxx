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

#define PINOCCHIO_EIGEN_HELPER_FUNC(method)                                                        \
  template<typename Matrix>                                                                        \
  void method(const Eigen::MatrixBase<Matrix> & mat)                                               \
  {                                                                                                \
    mat.const_cast_derived().method();                                                             \
  }

  PINOCCHIO_EIGEN_HELPER_FUNC(setZero);
  PINOCCHIO_EIGEN_HELPER_FUNC(setOnes);
  PINOCCHIO_EIGEN_HELPER_FUNC(setIdentity);

#undef PINOCCHIO_EIGEN_HELPER_FUNC

} // namespace pinocchio
