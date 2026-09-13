//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/rnea.hpp"

namespace pinocchio
{
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs & rnea<
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
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs & rnea<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>,
      Force>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const std::vector<Force> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
    nonLinearEffects<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
    computeGeneralizedGravity<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>>(
      const Model &, Data &, const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::VectorXs &
    computeStaticTorque<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const std::vector<Force> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::MatrixXs &
    computeCoriolisMatrix<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);
  } // namespace impl
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::MatrixXs &
  getCoriolisMatrix<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &);
} // namespace pinocchio
