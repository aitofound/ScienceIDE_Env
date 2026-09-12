//
// Copyright (c) 2015-2021 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <type_traits>
#include <vector>

#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/fwd.hpp"
//
#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/visitor.hpp"

#include "pinocchio/algorithm/center-of-mass.hpp"
#include "pinocchio/algorithm/energy.hpp"
#include "pinocchio/algorithm/check.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief Computes efficiently all the terms needed for dynamic simulation. It is equivalent to
  /// the call at the same time to:
  ///         - pinocchio::forwardKinematics
  ///         - pinocchio::crba
  ///         - pinocchio::nonLinearEffects
  ///         - pinocchio::computeJointJacobians
  ///         - pinocchio::centerOfMass
  ///         - pinocchio::jacobianCenterOfMass
  ///         - pinocchio::ccrba
  ///         - pinocchio::computeKineticEnergy
  ///         - pinocchio::computePotentialEnergy
  ///         - pinocchio::computeGeneralizedGravity
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType Type of the joint velocity vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  ///
  /// \note All the results are stored in data. Please refer to the specific algorithm for further
  /// details.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType>
  void computeAllTerms(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/compute-all-terms.hxx"
// IWYU pragma: end_exports
