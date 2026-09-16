//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>

#include <Eigen/Core>

#include "pinocchio/fwd.hpp"
#include "pinocchio/macros.hpp"

#include "pinocchio/utils/std-vector.hpp"
#include "pinocchio/utils/reference.hpp"
#include "pinocchio/utils/check.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/multibody.hpp"

#include "pinocchio/algorithm/proximal.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/constraints.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief Compute the contact forces given a target velocity of contact points.
  ///
  /// \param[in] constraint_models The vector of constraint models.
  /// \param[in] constraint_datas Vector of constraint datas.
  /// \param[in] c_ref The desired constraint velocity.
  /// \param[in,out] _lambda Vector of solution. Should be initialized with zeros or from an initial
  /// estimate.
  /// \param[in,out] settings The settings for the proximal algorithm
  /// \param[in] solve_ncp whether to solve the NCP (true) or CCP (false).
  ///
  template<
    typename Scalar,
    class PointContactConstraintModelVector,
    class PointContactConstraintDataVector,
    typename VectorLikeC,
    typename VectorLikeResult>
  bool computeInverseDynamicsConstraintForces(
    const PointContactConstraintModelVector & constraint_models,
    const PointContactConstraintDataVector & constraint_datas,
    const Eigen::MatrixBase<VectorLikeC> & c_ref,
    const Eigen::MatrixBase<VectorLikeResult> & _lambda,
    ProximalSettingsTpl<Scalar> & settings,
    bool solve_ncp = true);

  ///
  /// \brief The Contact Inverse Dynamics algorithm. It computes the inverse dynamics in the
  /// presence of contacts, aka the joint torques according to the current state of the system and
  /// the desired joint accelerations.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint acceleration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] a The joint acceleration vector (dim model.nv).
  /// \param[in] dt The time step.
  /// \param[in] constraint_models The list of contact models.
  /// \param[in] constraint_datas The list of constraint_datas.
  /// \param[in] constraint_correction vector representing the constraint correction.
  /// \param[in] _lambda_sol initial guess for the contact forces
  /// \param[in] settings The settings for the proximal algorithm
  /// \param[in] solve_ncp whether to solve the NCP (true) or CCP (false).
  ///
  /// \return The desired joint torques stored in data.tau.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    class PointContactConstraintModelVector,
    class PointContactConstraintDataVector,
    typename VectorLikeGamma,
    typename VectorLikeLam>
  bool contactInverseDynamics(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const Scalar dt,
    const PointContactConstraintModelVector & constraint_models,
    PointContactConstraintDataVector & constraint_datas,
    const Eigen::MatrixBase<VectorLikeGamma> & constraint_correction,
    const Eigen::MatrixBase<VectorLikeLam> & _lambda_sol,
    ProximalSettingsTpl<Scalar> & settings,
    bool solve_ncp = true);
} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/contact-inverse-dynamics.hxx"
// IWYU pragma: end_exports
