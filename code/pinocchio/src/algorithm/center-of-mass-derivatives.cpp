//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/center-of-mass-derivatives.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  getCenterOfMassVelocityDerivatives<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::Matrix3x>(const Model &, Data &, const Eigen::MatrixBase<context::Matrix3x> &);
} // namespace pinocchio
