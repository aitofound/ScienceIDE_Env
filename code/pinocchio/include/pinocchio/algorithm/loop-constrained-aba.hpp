//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <vector>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <Eigen/Core>

#include <boost/fusion/container/vector/vector.hpp>

#include "pinocchio/utils/alloca.hpp"
#include "pinocchio/utils/check.hpp"

#include "pinocchio/macros.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/algorithm/check.hpp"
#include "pinocchio/algorithm/proximal.hpp"
#include "pinocchio/constraints.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  ///
  /// \brief The closed-loop constrained Articulated Body Algorithm (CLconstrainedABA). It computes
  /// constrained forward dynamics, aka the joint accelerations and constraint forces given the
  /// current state, actuation and the constraints on the system for mechanisms potentially with
  /// internal closed kinematic loops.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint torque vector.
  /// \tparam ConstraintModelAllocator Allocator class for the std::vector.
  /// \tparam ConstraintDataAllocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] tau The joint torque vector (dim model.nv).
  /// \param[in] constraint_models Vector of contact models.
  /// \param[in] constraint_datas Vector of contact data.
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
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & lcaba(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & tau,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    ProximalSettingsTpl<Scalar> & settings);

} // namespace pinocchio

/* --- Details -------------------------------------------------------------------- */
// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/loop-constrained-aba.hxx"
// IWYU pragma: end_exports
