//
// Copyright (c) 2022-2025 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS

  #include "pinocchio/algorithm/constrained-dynamics.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void initConstraintDynamics<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &, Data &, const RigidConstraintModelVector &, const RigidConstraintDataVector &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
  constraintDynamics<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs,
    typename RigidConstraintModelVector::allocator_type,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const RigidConstraintModelVector &,
    RigidConstraintDataVector &,
    ProximalSettingsTpl<context::Scalar> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
  constraintDynamics<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs,
    typename RigidConstraintModelVector::allocator_type,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const RigidConstraintModelVector &,
    RigidConstraintDataVector &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs & contactABA<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs,
    typename RigidConstraintModelVector::allocator_type,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const RigidConstraintModelVector &,
    RigidConstraintDataVector &);
} // namespace pinocchio

#endif // PINOCCHIO_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS
