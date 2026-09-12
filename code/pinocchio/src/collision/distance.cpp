//
// Copyright (c) 2024 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/collision/distance.hpp"

namespace pinocchio
{

  template PINOCCHIO_COLLISION_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI std::size_t
  computeDistances<context::Scalar, context::Options, JointCollectionDefaultTpl, context::VectorXs>(
    const Model &,
    Data &,
    const GeometryModel &,
    GeometryData &,
    const Eigen::MatrixBase<context::VectorXs> &);

} // namespace pinocchio
