//
// Copyright (c) 2016-2018 CNRS
// Copyright (c) 2018-2024 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <vector>

#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/visitor.hpp"

#include "pinocchio/algorithm/check.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief The Articulated-Body algorithm. It computes the forward dynamics, aka the joint
  /// accelerations given the current state and actuation of the model.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] tau The joint torque vector (dim model.nv).
  /// \param[in] convention Convention to use.
  ///
  /// \note When running in LOCAL convention data.f can be leaved in an inconsistent state.
  ///
  /// \return The current joint acceleration stored in data.ddq.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & aba(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & tau,
    const Convention convention = Convention::LOCAL);

  ///
  /// \brief The Articulated-Body algorithm. It computes the forward dynamics, aka the joint
  /// accelerations given the current state and actuation of the model and the external forces.
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  /// \tparam ForceDerived Type of the external forces.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] tau The joint torque vector (dim model.nv).
  /// \param[in] fext Vector of external forces expressed in the local frame of the joints (dim
  /// model.njoints)
  /// \param[in] convention Convention to use.
  ///
  /// \note When running in LOCAL convention data.f can be leaved in an inconsistent state.
  ///
  /// \return The current joint acceleration stored in data.ddq.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    typename SpatialForce,
    typename SpatialForceAllocator>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & aba(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & tau,
    const std::vector<SpatialForce, SpatialForceAllocator> & fext,
    const Convention convention = Convention::LOCAL);

  ///
  /// \brief Computes the inverse of the joint space inertia matrix using Articulated Body
  /// formulation. \remarks Only the upper triangular part of the matrix is filled.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  ///
  /// \return The inverse of the joint space inertia matrix stored in data.Minv.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::RowMatrixXs & computeMinverse(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q);

  ///
  /// \brief Computes the inverse of the joint space inertia matrix using Articulated Body
  /// formulation.
  ///        Compared to the complete signature computeMinverse<Scalar,Options,ConfigVectorType>,
  ///        this version assumes that ABA has been called first.
  ///
  /// \tparam JointCollection Collection of Joint types.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  ///
  /// \return The inverse of the joint space inertia matrix stored in data.ddq.
  ///
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::RowMatrixXs & computeMinverse(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data);

  PINOCCHIO_DEFINE_ALGO_CHECKER(ABA);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/aba.hxx"
// IWYU pragma: end_exports
