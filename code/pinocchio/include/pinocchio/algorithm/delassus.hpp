//
// Copyright (c) 2020-2024 INRIA
// Copyright (c) 2023 KU Leuven
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <algorithm>
#include <vector>
#include <cstddef>
#include <limits>

#include <Eigen/Core>
#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/unsupported.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/utils/check.hpp"

#include "pinocchio/algorithm/check.hpp"
#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/model.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class Allocator>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  inline void initPvDelassus(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, Allocator> & contact_models);

  ///
  /// \brief Computes the Delassus matrix associated to a set of given constraints.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam ModelAllocator Allocator class for the std::vector.
  /// \tparam DataAllocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration (size model.nq).
  /// \param[in] contact_models Vector of contact models.
  /// \param[in] contact_data Vector of contact data.
  /// \param[out] delassus The resulting Delassus matrix.
  /// \param[in] mu Optional damping factor used when computing the inverse of the Delassus matrix.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    class ModelAllocator,
    class DataAllocator,
    typename MatrixType>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  void computeDelassusMatrix(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ModelAllocator> & contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, DataAllocator> & contact_data,
    const Eigen::MatrixBase<MatrixType> & delassus,
    const Scalar mu = 0);

  ///
  /// \brief Computes the inverse of the Delassus matrix associated to a set of given constraints.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam ModelAllocator Allocator class for the std::vector.
  /// \tparam DataAllocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration (size model.nq).
  /// \param[in] contact_models Vector of contact models.
  /// \param[in] contact_data Vector of contact data.
  /// \param[out] damped_delassus_inverse The resulting damped Delassus matrix.
  /// \param[in] mu Damping factor well-posdnessed of the problem.
  /// \param[in] scaled If set to true, the solution is scaled my a factor \f$ \mu \f$ to avoid
  /// numerical rounding issues. \param[in] Pv If set to true, uses PV-OSIMr, otherwise uses EFPA.
  ///
  /// \note A hint: a typical value for mu is 1e-4 when two contact constraints or more are
  /// redundant.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    class ModelAllocator,
    class DataAllocator,
    typename MatrixType>
  PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
  void computeDampedDelassusMatrixInverse(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const std::vector<RigidConstraintModelTpl<Scalar, Options>, ModelAllocator> & contact_models,
    std::vector<RigidConstraintDataTpl<Scalar, Options>, DataAllocator> & contact_data,
    const Eigen::MatrixBase<MatrixType> & damped_delassus_inverse,
    const Scalar mu,
    const bool scaled = false,
    const bool Pv = true);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/delassus.hxx"
// IWYU pragma: end_exports
