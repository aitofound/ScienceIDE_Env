//
// Copyright (c) 2022-2024 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/regressor.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void computeJointKinematicRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::Matrix6xs>(
    const Model &,
    const Data &,
    const JointIndex,
    const ReferenceFrame,
    const SE3Tpl<context::Scalar, context::Options> &,
    const Eigen::MatrixBase<context::Matrix6xs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Matrix6xs
  computeJointKinematicRegressor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const Data &,
    const JointIndex,
    const ReferenceFrame,
    const SE3Tpl<context::Scalar, context::Options> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void computeJointKinematicRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::Matrix6xs>(
    const Model &,
    const Data &,
    const JointIndex,
    const ReferenceFrame,
    const Eigen::MatrixBase<context::Matrix6xs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Matrix6xs
  computeJointKinematicRegressor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, const Data &, const JointIndex, const ReferenceFrame);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void computeFrameKinematicRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::Matrix6xs>(
    const Model &,
    Data &,
    const FrameIndex,
    const ReferenceFrame,
    const Eigen::MatrixBase<context::Matrix6xs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Matrix6xs
  computeFrameKinematicRegressor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &, const FrameIndex, const ReferenceFrame);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Matrix3x &
  computeStaticRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(const Model &, Data &, const Eigen::MatrixBase<context::VectorXs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  bodyRegressor<Motion, Motion, context::BodyRegressorType>(
    const MotionDense<Motion> &,
    const MotionDense<Motion> &,
    const Eigen::MatrixBase<context::BodyRegressorType> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::BodyRegressorType
  bodyRegressor<Motion, Motion>(const MotionDense<Motion> &, const MotionDense<Motion> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::BodyRegressorType &
  jointBodyRegressor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &, JointIndex);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::BodyRegressorType &
  frameBodyRegressor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &, FrameIndex);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::MatrixXs &
  computeJointTorqueRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const Data::RowVectorXs &
  computeKineticEnergyRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs,
    context::VectorXs>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const Eigen::MatrixBase<context::VectorXs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const Data::RowVectorXs &
  computePotentialEnergyRegressor<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(const Model &, Data &, const Eigen::MatrixBase<context::VectorXs> &);

} // namespace pinocchio
