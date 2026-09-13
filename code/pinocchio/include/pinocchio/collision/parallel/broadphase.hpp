//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>
#include <Eigen/Dense>

#include <cstdint>
#include <cstddef>
#include <vector>
#include <omp.h>

#include "pinocchio/fwd.hpp"
#include "pinocchio/eigen-common.hpp"
#include "pinocchio/macros.hpp"

#include "pinocchio/algorithm/geometry.hpp"

#include "pinocchio/utils/openmp.hpp"

#include "pinocchio/collision/pool/broadphase-manager.hpp"
#include "pinocchio/collision/broadphase.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  template<
    typename BroadPhaseManagerDerived,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorPool,
    typename CollisionVectorResult>
  void computeCollisionsInParallel(
    const size_t num_threads,
    BroadPhaseManagerPoolBase<BroadPhaseManagerDerived, Scalar, Options, JointCollectionTpl> & pool,
    const Eigen::MatrixBase<ConfigVectorPool> & q,
    const Eigen::MatrixBase<CollisionVectorResult> & res,
    const bool stopAtFirstCollisionInConfiguration = false,
    const bool stopAtFirstCollisionInBatch = false);

  ///
  /// \brief Evaluate the collision over a set of trajectories and return whether a trajectory
  /// contains a collision
  ///
  template<
    typename BroadPhaseManagerDerived,
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl>
  void computeCollisionsInParallel(
    const size_t num_threads,
    BroadPhaseManagerPoolBase<BroadPhaseManagerDerived, Scalar, Options, JointCollectionTpl> & pool,
    const std::vector<Eigen::MatrixXd> & trajectories,
    std::vector<VectorXb> & res,
    const bool stopAtFirstCollisionInTrajectory = false);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/parallel/broadphase.hxx"
// IWYU pragma: end_exports
