//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_MODEL

  #include "pinocchio/algorithm/model.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  appendModel<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const Model &,
    const FrameIndex,
    const SE3Tpl<context::Scalar, context::Options> &,
    Model &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI Model
  appendModel<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const Model &,
    const FrameIndex,
    const SE3Tpl<context::Scalar, context::Options> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  appendModel<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const Model &,
    const GeometryModel &,
    const GeometryModel &,
    const FrameIndex,
    const SE3Tpl<context::Scalar, context::Options> &,
    Model &,
    GeometryModel &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void buildReducedModel<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &,
    const std::vector<JointIndex>,
    const Eigen::MatrixBase<context::VectorXs> &,
    Model &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI Model buildReducedModel<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &, const std::vector<JointIndex> &, const Eigen::MatrixBase<context::VectorXs> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void buildReducedModel<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &,
    const GeometryModel &,
    const std::vector<JointIndex> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    Model &,
    GeometryModel &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void buildReducedModel<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &,
    const std::vector<GeometryModel> &,
    const std::vector<JointIndex> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    Model &,
    std::vector<GeometryModel> &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  transformJointIntoMimic<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const JointIndex &,
    const JointIndex &,
    const context::Scalar &,
    const context::Scalar &,
    Model &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  buildMimicModel<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &,
    const std::vector<JointIndex> &,
    const std::vector<JointIndex> &,
    const std::vector<context::Scalar> &,
    const std::vector<context::Scalar> &,
    Model &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI JointIndex
  findCommonAncestor<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, JointIndex, JointIndex, size_t &, size_t &);
} // namespace pinocchio

#endif // PINOCCHIO_SKIP_ALGORITHM_MODEL
