//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#include "pinocchio/algorithm/energy.hpp"

namespace pinocchio
{

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
  computeKineticEnergy<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &);
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
    computeKineticEnergy<
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
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
  computePotentialEnergy<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &);
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
    computePotentialEnergy<
      context::Scalar,
      context::Options,
      JointCollectionDefaultTpl,
      Eigen::Ref<const context::VectorXs>>(
      const Model &, Data &, const Eigen::MatrixBase<Eigen::Ref<const context::VectorXs>> &);
  } // namespace impl
  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
  computeMechanicalEnergy<context::Scalar, context::Options, JointCollectionDefaultTpl>(
    const Model &, Data &);
  namespace impl
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::Scalar
    computeMechanicalEnergy<
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
