//
// Copyright (c) 2023-2024 Inria
// Copyright (c) 2023 KU Leuven
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <algorithm>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/utils/check.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/fwd.hpp"
#include "pinocchio/algorithm/check.hpp"
#include "pinocchio/algorithm/aba.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  ///
  /// \brief Init the data according to the contact information contained in contact_models.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  /// \tparam Allocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] contact_models Vector of contact information related to the problem.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator>
  inline void initPvSolver(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & contact_models);

  ///
  /// \brief The Popov-Vereshchagin algorithm. It computes constrained forward dynamics, aka the
  /// joint accelerations and constraint forces given the current state, actuation and the
  /// constraints on the system. All the quantities are expressed in the LOCAL coordinate systems of
  /// the joint frames.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  /// \tparam Allocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] tau The joint torque vector (dim model.nv).
  /// \param[in] contact_models Vector of contact models.
  /// \param[in] contact_datas Vector of contact data.
  /// \param[in] settings Proximal settings (mu, accuracy and maximal number of iterations).
  ///
  /// \note This also overwrites data.f, possibly leaving it in an inconsistent state.
  ///
  /// \return A reference to the joint acceleration stored in data.ddq. data.lambdaA[0] stores the
  /// constraint forces.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  inline const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & pv(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & tau,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & contact_models,
    std::vector<ConstraintData, ConstraintDataAllocator> & contact_datas,
    ProximalSettingsTpl<Scalar> & settings);

  ///
  /// \brief The constrained Articulated Body Algorithm (constrainedABA). It computes constrained
  /// forward dynamics, aka the joint accelerations and constraint forces given the current state,
  /// actuation and the constraints on the system. All the quantities are expressed in the LOCAL
  /// coordinate systems of the joint frames.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  /// \tparam Allocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] tau The joint torque vector (dim model.nv).
  /// \param[in] contact_models Vector of contact models.
  /// \param[in] contact_datas Vector of contact data.
  /// \param[in] settings Proximal settings (mu, accuracy and maximal number of iterations).
  ///
  /// \note A hint: a typical value of mu in proximal settings is 1e-6, and should always be
  /// positive. This also overwrites data.f, possibly leaving it in an inconsistent state.
  ///
  /// \return A reference to the joint acceleration stored in data.ddq. data.lambdaA[0] stores the
  /// constraint forces.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  inline const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType &
  constrainedABA(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & tau,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & contact_models,
    std::vector<ConstraintData, ConstraintDataAllocator> & contact_datas,
    ProximalSettingsTpl<Scalar> & settings);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/pv.hxx"
// IWYU pragma: end_exports
