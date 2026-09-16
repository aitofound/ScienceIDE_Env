//
// Copyright (c) 2017-2019 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"
#include "pinocchio/fwd.hpp"

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
  /// \brief Computes the partial derivative of the generalized gravity contribution
  ///        with respect to the joint configuration.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam ReturnMatrixType Type of the matrix containing the partial derivative of the gravity
  /// vector with respect to the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[out] gravity_partial_dq Partial derivative of the generalized gravity vector with
  /// respect to the joint configuration.
  ///
  /// \remarks gravity_partial_dq must be first initialized with zeros (gravity_partial_dq.setZero).
  ///
  /// \sa pinocchio::computeGeneralizedGravity
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename ReturnMatrixType>
  void computeGeneralizedGravityDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<ReturnMatrixType> & gravity_partial_dq);

  ///
  /// \brief Computes the partial derivative of the generalized gravity and external forces
  /// contributions (a.k.a static torque vector)
  ///        with respect to the joint configuration.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam ReturnMatrixType Type of the matrix containing the partial derivative of the gravity
  /// vector with respect to the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] fext External forces expressed in the local frame of the joints (dim
  /// model.njoints). \param[out] static_torque_partial_dq Partial derivative of the static torque
  /// vector with respect to the joint configuration.
  ///
  /// \remarks gravity_partial_dq must be first initialized with zeros (gravity_partial_dq.setZero).
  ///
  /// \sa pinocchio::computeGeneralizedTorque
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename ReturnMatrixType>
  void computeStaticTorqueDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const std::vector<ForceTpl<Scalar, Options>> & fext,
    const Eigen::MatrixBase<ReturnMatrixType> & static_torque_partial_dq);

  ///
  /// \brief Computes the partial derivatives of the Recursive Newton Euler Algorithms
  ///        with respect to the joint configuration, the joint velocity and the joint acceleration.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint acceleration vector.
  /// \tparam MatrixType1 Type of the matrix containing the partial derivative with respect to the
  /// joint configuration vector. \tparam MatrixType2 Type of the matrix containing the partial
  /// derivative with respect to the joint velocity vector. \tparam MatrixType3 Type of the matrix
  /// containing the partial derivative with respect to the joint acceleration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] a The joint acceleration vector (dim model.nv).
  /// \param[out] rnea_partial_dq Partial derivative of the generalized torque vector with respect
  /// to the joint configuration. \param[out] rnea_partial_dv Partial derivative of the generalized
  /// torque vector with respect to the joint velocity. \param[out] rnea_partial_da Partial
  /// derivative of the generalized torque vector with respect to the joint acceleration.
  ///
  /// \remarks rnea_partial_dq, rnea_partial_dv and rnea_partial_da must be first initialized with
  /// zeros (rnea_partial_dq.setZero(),etc).
  ///         As for pinocchio::crba, only the upper triangular part of rnea_partial_da is filled.
  ///
  /// \sa pinocchio::rnea
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    typename MatrixType1,
    typename MatrixType2,
    typename MatrixType3>
  void computeRNEADerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const Eigen::MatrixBase<MatrixType1> & rnea_partial_dq,
    const Eigen::MatrixBase<MatrixType2> & rnea_partial_dv,
    const Eigen::MatrixBase<MatrixType3> & rnea_partial_da);

  ///
  /// \brief Computes the derivatives of the Recursive Newton Euler Algorithms
  ///        with respect to the joint configuration, the joint velocity and the joint acceleration.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint acceleration vector.
  /// \tparam MatrixType1 Type of the matrix containing the partial derivative with respect to the
  /// joint configuration vector. \tparam MatrixType2 Type of the matrix containing the partial
  /// derivative with respect to the joint velocity vector. \tparam MatrixType3 Type of the matrix
  /// containing the partial derivative with respect to the joint acceleration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] a The joint acceleration vector (dim model.nv).
  /// \param[in] fext External forces expressed in the local frame of the joints (dim
  /// model.njoints). \param[out] rnea_partial_dq Partial derivative of the generalized torque
  /// vector with respect to the joint configuration. \param[out] rnea_partial_dv Partial derivative
  /// of the generalized torque vector with respect to the joint velocity. \param[out]
  /// rnea_partial_da Partial derivative of the generalized torque vector with respect to the joint
  /// acceleration.
  ///
  /// \remarks rnea_partial_dq, rnea_partial_dv and rnea_partial_da must be first initialized with
  /// zeros (rnea_partial_dq.setZero(),etc).
  ///         As for pinocchio::crba, only the upper triangular part of rnea_partial_da is filled.
  ///
  /// \sa pinocchio::rnea
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    typename MatrixType1,
    typename MatrixType2,
    typename MatrixType3>
  void computeRNEADerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const std::vector<ForceTpl<Scalar, Options>> & fext,
    const Eigen::MatrixBase<MatrixType1> & rnea_partial_dq,
    const Eigen::MatrixBase<MatrixType2> & rnea_partial_dv,
    const Eigen::MatrixBase<MatrixType3> & rnea_partial_da);

  ///
  /// \brief Computes the derivatives of the Recursive Newton Euler Algorithms
  ///        with respect to the joint configuration, the joint velocity and the joint acceleration.
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
  ///
  /// \note The results are stored in data.dtau_dq, data.dtau_dv and data.M which respectively
  /// correspond
  ///          to the partial derivatives of the joint torque vector with respect to the joint
  ///          configuration, velocity and acceleration. As for pinocchio::crba, only the upper
  ///          triangular part of data.M is filled.
  ///
  /// \sa pinocchio::rnea, pinocchio::crba, pinocchio::decompose
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2>
  void computeRNEADerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a);

  ///
  /// \brief Computes the derivatives of the Recursive Newton Euler Algorithms
  ///        with respect to the joint configuration, the joint velocity and the joint acceleration.
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
  /// \param[in] fext External forces expressed in the local frame of the joints (dim
  /// model.njoints).
  ///
  /// \note The results are stored in data.dtau_dq, data.dtau_dv and data.M which respectively
  /// correspond
  ///          to the partial derivatives of the joint torque vector with respect to the joint
  ///          configuration, velocity and acceleration. As for pinocchio::crba, only the upper
  ///          triangular part of data.M is filled.
  ///
  /// \sa pinocchio::rnea, pinocchio::crba, pinocchio::decompose
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2>
  void computeRNEADerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const std::vector<ForceTpl<Scalar, Options>> & fext);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/rnea-derivatives.hxx"
// IWYU pragma: end_exports
