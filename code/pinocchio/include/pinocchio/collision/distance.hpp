//
// Copyright (c) 2015-2021 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <coal/collision_data.h>
#include <coal/math/transform.h>

#include "pinocchio/macros.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/geometry.hpp"

#include "pinocchio/collision/config.hpp"
#include "pinocchio/collision/coal-pinocchio-conversions.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  ///
  /// Update the geometry placements and
  /// calls computeDistance for every active pairs of GeometryData.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  ///
  /// \param[in] model: robot model (const)
  /// \param[in] data: corresponding data (nonconst) where FK results are stored
  /// \param[in] geom_model: geometry model (const)
  /// \param[out] geom_data: corresponding geometry data (nonconst) where distances are computed
  ///
  /// \note A similar function is available without model, data and q, not recomputing the FK.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  std::size_t computeDistances(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const GeometryModel & geom_model,
    GeometryData & geom_data);

  ///
  /// Compute the forward kinematics, update the geometry placements and
  /// calls computeDistance for every active pairs of GeometryData.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  ///
  /// \param[in] model: robot model (const)
  /// \param[in] data: corresponding data (nonconst) where FK results are stored
  /// \param[in] geom_model: geometry model (const)
  /// \param[out] geom_data: corresponding geometry data (nonconst) where distances are computed
  /// \param[in] q: robot configuration.
  ///
  /// \note A similar function is available without model, data and q, not recomputing the FK.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  std::size_t computeDistances(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const GeometryModel & geom_model,
    GeometryData & geom_data,
    const Eigen::MatrixBase<ConfigVectorType> & q);

  ///
  /// \brief Compute the minimal distance between collision objects of a *SINGLE* collison pair
  ///
  /// \param[in] geom_model the geometry model (const)
  /// \param[out] geom_data the corresponding geometry data, where computations are done.
  /// \param[in] pair_id The index of the collision pair in geom model.
  ///
  /// \return A reference on coal struct containing the distance result, referring an element
  /// of vector geom_data::distanceResults.
  /// \note The complete distance result is also available in geom_data.distanceResults[pair_id]
  ///
  coal::DistanceResult & computeDistance(
    const GeometryModel & geom_model, GeometryData & geom_data, const PairIndex pair_id);

  ///
  /// \brief Compute the minimal distance between collision objects of a *ALL* collison pair
  ///
  /// \param[in] geom_model the geometry model (const)
  /// \param[out] geom_data the corresponding geometry data, where computations are done.
  /// \return Index of the minimal pair distance in geom_data.DistanceResult
  ///
  /// \note The complete distance result is available by pair in geom_data.distanceResults
  ///
  std::size_t computeDistances(const GeometryModel & geom_model, GeometryData & geom_data);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/distance.hxx"
// IWYU pragma: end_exports
