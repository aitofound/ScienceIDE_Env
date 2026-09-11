//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/centroidal.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const
    ForceTpl<context::Scalar, context::Options> &
    computeCentroidalMomentum<context::Scalar, context::Options, JointCollectionDefaultTpl>(
      const Model &, Data &);
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const
      ForceTpl<context::Scalar, context::Options> &
      computeCentroidalMomentum<
        context::Scalar,
        context::Options,
        JointCollectionDefaultTpl,
        Eigen::Ref<const context::VectorXs>,
        Eigen::Ref<const context::VectorXs>>(
        const Model &,
        Data &,
        const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
        const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);
  }
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const
    ForceTpl<context::Scalar, context::Options> &
    computeCentroidalMomentumTimeVariation<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl>(const Model &, Data &);
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const
      ForceTpl<context::Scalar, context::Options> &
      computeCentroidalMomentumTimeVariation<
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

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::Matrix6xs & ccrba<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::Matrix6xs &
    computeCentroidalMap<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>>(
      const Model &, Data &, const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::Matrix6xs & dccrba<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>,
      Eigen::Ref<const context::VectorXs>>(
      const Model &,
      Data &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &,
      const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);

    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI const context::Matrix6xs &
    computeCentroidalMapTimeVariation<
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
} // namespace pinocchio
