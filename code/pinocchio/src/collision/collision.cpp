//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/collision/collision.hpp"

namespace pinocchio
{

  template PINOCCHIO_COLLISION_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI bool computeCollisions<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    context::VectorXs>(
    const Model &,
    Data &,
    const GeometryModel &,
    GeometryData &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const bool stopAtFirstCollision);

  template PINOCCHIO_COLLISION_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  computeBodyRadius<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, const GeometryModel &, GeometryData &);

} // namespace pinocchio
