//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <cstddef>

#include <omp.h>

#include <Eigen/Core>

#include "pinocchio/macros.hpp"

#include "pinocchio/utils/openmp.hpp"
#include "pinocchio/multibody/pool.hpp"
#include "pinocchio/algorithm/rnea.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief The Recursive Newton-Euler algorithm. It computes the inverse dynamics, aka the joint
  /// torques according to the current state of the system and the desired joint accelerations.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorPool Matrix type of the joint configuration vector.
  /// \tparam TangentVectorPool1 Matrix type of the joint velocity vector.
  /// \tparam TangentVectorPool2 Matrix type of the joint acceleration vector.
  /// \tparam TangentVectorPool3 Matrix type of the joint torque vector.
  ///
  /// \param[in] pool Pool containing model and data for parallel computations.
  /// \param[in] num_threads Number of threads used for parallel computations.
  /// \param[in] q The joint configuration vector (dim model.nq x batch_size).
  /// \param[in] v The joint velocity vector (dim model.nv x batch_size).
  /// \param[in] a The joint acceleration vector (dim model.nv x batch_size).
  /// \param[out] tau The joint torque vector (dim model.nv x batch_size).
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorPool,
    typename TangentVectorPool1,
    typename TangentVectorPool2,
    typename TangentVectorPool3>
  void rneaInParallel(
    const size_t num_threads,
    ModelPoolTpl<Scalar, Options, JointCollectionTpl> & pool,
    const Eigen::MatrixBase<ConfigVectorPool> & q,
    const Eigen::MatrixBase<TangentVectorPool1> & v,
    const Eigen::MatrixBase<TangentVectorPool2> & a,
    const Eigen::MatrixBase<TangentVectorPool3> & tau);
} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/parallel/rnea.hxx"
// IWYU pragma: end_exports
