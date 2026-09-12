//
// Copyright (c) 2022-2024 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_CONTACT_JACOBIAN

  #include "pinocchio/constraints.hpp"

namespace pinocchio
{
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void getConstraintJacobian<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    RigidConstraintData,
    context::MatrixXs>(
    const Model &,
    const Data &,
    const ConstraintModelBase<RigidConstraintModel> &,
    const ConstraintDataBase<RigidConstraintData> &,
    const Eigen::MatrixBase<context::MatrixXs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void getConstraintsJacobian<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type,
    context::MatrixXs>(
    const Model &,
    const Data &,
    const RigidConstraintModelVector &,
    const RigidConstraintDataVector &,
    const Eigen::MatrixBase<context::MatrixXs> &);

} // namespace pinocchio

#endif // PINOCCHIO_SKIP_ALGORITHM_CONTACT_JACOBIAN
