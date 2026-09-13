//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename LhsType, typename RhsType>
  struct MultiplicationOperatorReturnType;

  template<typename LhsMatrixDerived, typename RhsMatrixDerived>
  struct MultiplicationOperatorReturnType<
    Eigen::MatrixBase<LhsMatrixDerived>,
    Eigen::MatrixBase<RhsMatrixDerived>> : MatrixMatrixProduct<LhsMatrixDerived, RhsMatrixDerived>
  {
    typedef MatrixMatrixProduct<LhsMatrixDerived, RhsMatrixDerived> Base;
    typedef typename Base::type type;
  };
} // namespace pinocchio
