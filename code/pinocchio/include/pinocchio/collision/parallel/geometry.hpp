
//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <omp.h>
#include <cstddef>
#include <vector>

#include "pinocchio/eigen-common.hpp"
#include "pinocchio/macros.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/pool.hpp"

#include "pinocchio/algorithm/geometry.hpp"

#include "pinocchio/utils/openmp.hpp"

#include "pinocchio/collision/collision.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  inline bool computeCollisionsInParallel(
    const size_t num_threads,
    const GeometryModel & geom_model,
    GeometryData & geom_data,
    const bool stopAtFirstCollision = false);

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  inline bool computeCollisionsInParallel(
    const size_t num_threads,
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const GeometryModel & geom_model,
    GeometryData & geom_data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const bool stopAtFirstCollision = false);

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorPool,
    typename CollisionVectorResult>
  inline void computeCollisionsInParallel(
    const size_t num_threads,
    GeometryPoolTpl<Scalar, Options, JointCollectionTpl> & pool,
    const Eigen::MatrixBase<ConfigVectorPool> & q,
    const Eigen::MatrixBase<CollisionVectorResult> & res,
    const bool stopAtFirstCollisionInConfiguration = false,
    const bool stopAtFirstCollisionInBatch = false);
} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/parallel/geometry.hxx"
// IWYU pragma: end_exports
