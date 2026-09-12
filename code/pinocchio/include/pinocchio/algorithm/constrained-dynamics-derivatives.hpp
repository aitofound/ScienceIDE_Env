//
// Copyright (c) 2020-2022 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <vector>

#include <Eigen/Core>

#include <boost/fusion/container/vector.hpp>

#include <pinocchio/fwd.hpp>
#include <pinocchio/context.hpp>
#include <pinocchio/config.hpp>
#include <pinocchio/macros.hpp>
#include <pinocchio/unsupported.hpp>

#include "pinocchio/utils/check.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/proximal.hpp"
#include "pinocchio/algorithm/check-model.hpp"
#include "pinocchio/algorithm/frames.hpp"
#include "pinocchio/algorithm/rnea-derivatives.hpp"
#include "pinocchio/algorithm/frames-derivatives.hpp"
#include "pinocchio/algorithm/kinematics-derivatives.hpp"
#include "pinocchio/algorithm/constraint-cholesky.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModelAllocator,
    class ConstraintDataAllocator,
    typename MatrixType1,
    typename MatrixType2,
    typename MatrixType3,
    typename MatrixType4,
    typename MatrixType5,
    typename MatrixType6>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeConstraintDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data,
    const ProximalSettingsTpl<Scalar> & settings,
    const Eigen::MatrixBase<MatrixType1> & ddq_partial_dq,
    const Eigen::MatrixBase<MatrixType2> & ddq_partial_dv,
    const Eigen::MatrixBase<MatrixType3> & ddq_partial_dtau,
    const Eigen::MatrixBase<MatrixType4> & lambda_partial_dq,
    const Eigen::MatrixBase<MatrixType5> & lambda_partial_dv,
    const Eigen::MatrixBase<MatrixType6> & lambda_partial_dtau);

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModelAllocator,
    class ConstraintDataAllocator,
    typename MatrixType1,
    typename MatrixType2,
    typename MatrixType3,
    typename MatrixType4,
    typename MatrixType5,
    typename MatrixType6>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeConstraintDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data,
    const Eigen::MatrixBase<MatrixType1> & ddq_partial_dq,
    const Eigen::MatrixBase<MatrixType2> & ddq_partial_dv,
    const Eigen::MatrixBase<MatrixType3> & ddq_partial_dtau,
    const Eigen::MatrixBase<MatrixType4> & lambda_partial_dq,
    const Eigen::MatrixBase<MatrixType5> & lambda_partial_dv,
    const Eigen::MatrixBase<MatrixType6> & lambda_partial_dtau)
  {
    ProximalSettingsTpl<Scalar> settings;
    computeConstraintDynamicsDerivatives(
      model, data, contact_models, contact_data, settings, ddq_partial_dq.const_cast_derived(),
      ddq_partial_dv.const_cast_derived(), ddq_partial_dtau.const_cast_derived(),
      lambda_partial_dq.const_cast_derived(), lambda_partial_dv.const_cast_derived(),
      lambda_partial_dtau.const_cast_derived());
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModelAllocator,
    class ConstraintDataAllocator>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeConstraintDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data,
    const ProximalSettingsTpl<Scalar> & settings)
  {
    computeConstraintDynamicsDerivatives(
      model, data, contact_models, contact_data, settings, data.ddq_dq, data.ddq_dv, data.ddq_dtau,
      data.dlambda_dq, data.dlambda_dv, data.dlambda_dtau);
  };

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModelAllocator,
    class ConstraintDataAllocator>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeConstraintDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data)
  {
    ProximalSettingsTpl<Scalar> settings;
    computeConstraintDynamicsDerivatives(model, data, contact_models, contact_data, settings);
  };

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/constrained-dynamics-derivatives.hxx"
// IWYU pragma: end_exports
