//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_IMPULSE_DYNAMICS

  #include "pinocchio/algorithm/impulse-dynamics.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
  impulseDynamics<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    typename RigidConstraintModelVector::allocator_type,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const RigidConstraintModelVector &,
    RigidConstraintDataVector &,
    const context::Scalar,
    const ProximalSettingsTpl<context::Scalar> &);
} // namespace pinocchio

#endif // PINOCCHIO_SKIP_ALGORITHM_IMPULSE_DYNAMICS
