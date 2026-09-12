//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/aba.hpp"

namespace pinocchio
{
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs & aba<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    Eigen::Ref<const context::VectorXs>,
    Eigen::Ref<const context::VectorXs>,
    Eigen::Ref<const context::VectorXs>>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const Convention);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs & aba<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    Eigen::Ref<const context::VectorXs>,
    Eigen::Ref<const context::VectorXs>,
    Eigen::Ref<const context::VectorXs>,
    Force,
    std::allocator<Force>>(
    const Model &,
    Data &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
    const std::vector<Force> &,
    const Convention);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::RowMatrixXs &
  computeMinverse<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &);
} // namespace pinocchio
