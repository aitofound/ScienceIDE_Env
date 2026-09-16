//
// Copyright (c) 2020 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <vector>

#include <Eigen/Core>
#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"
#include "pinocchio/unsupported.hpp"

#include "pinocchio/utils/check.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/check.hpp"
#include "pinocchio/algorithm/constrained-dynamics-derivatives.hpp"
#include "pinocchio/algorithm/jacobian.hpp"
#include "pinocchio/algorithm/proximal.hpp"
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
    typename MatrixType4>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeImpulseDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data,
    const Scalar r_coeff,
    const ProximalSettingsTpl<Scalar> & settings,
    const Eigen::MatrixBase<MatrixType1> & dvimpulse_partial_dq,
    const Eigen::MatrixBase<MatrixType2> & dvimpulse_partial_dv,
    const Eigen::MatrixBase<MatrixType3> & impulse_partial_dq,
    const Eigen::MatrixBase<MatrixType4> & impulse_partial_dv);

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModelAllocator,
    class ConstraintDataAllocator>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void computeImpulseDynamicsDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ConstraintModelAllocator> &
      contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, ConstraintDataAllocator> & contact_data,
    const Scalar r_coeff,
    const ProximalSettingsTpl<Scalar> & settings)
  {
    computeImpulseDynamicsDerivatives(
      model, data, contact_models, contact_data, r_coeff, settings, data.ddq_dq, data.ddq_dv,
      data.dlambda_dq, data.dlambda_dv);
  };

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/impulse-dynamics-derivatives.hxx"
// IWYU pragma: end_exports
