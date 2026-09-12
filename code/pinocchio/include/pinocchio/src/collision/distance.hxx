//
// Copyright (c) 2015-2021 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/collision/distance.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/collision/distance.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  inline std::size_t computeDistances(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const GeometryModel & geom_model,
    GeometryData & geom_data)
  {
    assert(model.check(data) && "data is not consistent with model.");
    updateGeometryPlacements(model, data, geom_model, geom_data);
    return computeDistances(geom_model, geom_data);
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  inline std::size_t computeDistances(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const GeometryModel & geom_model,
    GeometryData & geom_data,
    const Eigen::MatrixBase<ConfigVectorType> & q)
  {
    assert(model.check(data) && "data is not consistent with model.");
    updateGeometryPlacements(model, data, geom_model, geom_data, q);
    return computeDistances(geom_model, geom_data);
  }

  inline coal::DistanceResult & computeDistance(
    const GeometryModel & geom_model, GeometryData & geom_data, const PairIndex pair_id)
  {
    PINOCCHIO_CHECK_INPUT_ARGUMENT(pair_id < geom_model.collisionPairs.size());
    PINOCCHIO_CHECK_INPUT_ARGUMENT(
      geom_model.collisionPairs.size() == geom_data.collisionResults.size());
    const CollisionPair & pair = geom_model.collisionPairs[pair_id];

    PINOCCHIO_CHECK_INPUT_ARGUMENT(pair.first < geom_model.ngeoms);
    PINOCCHIO_CHECK_INPUT_ARGUMENT(pair.second < geom_model.ngeoms);

    coal::DistanceRequest & distance_request = geom_data.distanceRequests[pair_id];
    coal::DistanceResult & distance_result = geom_data.distanceResults[pair_id];
    distance_result.clear();

    coal::Transform3s oM1(toCoalTransform3s(geom_data.oMg[pair.first])),
      oM2(toCoalTransform3s(geom_data.oMg[pair.second]));

    try
    {
      GeometryData::ComputeDistance & do_computations = geom_data.distance_functors[pair_id];
      do_computations(oM1, oM2, distance_request, distance_result);
    }
    catch (std::invalid_argument & e)
    {
      std::stringstream ss;
      ss << "Problem when trying to compute the distance of collision pair #" << pair_id << " ("
         << pair.first << "," << pair.second << ")" << std::endl;
      ss << "coal original error:\n" << e.what() << std::endl;
      throw std::invalid_argument(ss.str());
    }

    return geom_data.distanceResults[pair_id];
  }

  inline std::size_t computeDistances(const GeometryModel & geom_model, GeometryData & geom_data)
  {
    std::size_t min_index = geom_model.collisionPairs.size();
    double min_dist = std::numeric_limits<double>::infinity();

    for (std::size_t cp_index = 0; cp_index < geom_model.collisionPairs.size(); ++cp_index)
    {
      const CollisionPair & cp = geom_model.collisionPairs[cp_index];

      if (
        geom_data.activeCollisionPairs[cp_index]
        && !(
          geom_model.geometryObjects[cp.first].disableCollision
          || geom_model.geometryObjects[cp.second].disableCollision))
      {
        computeDistance(geom_model, geom_data, cp_index);
        if (geom_data.distanceResults[cp_index].min_distance < min_dist)
        {
          min_index = cp_index;
          min_dist = geom_data.distanceResults[cp_index].min_distance;
        }
      }
    }

    return min_index;
  }

} // namespace pinocchio

#ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION

namespace pinocchio
{

  extern template PINOCCHIO_COLLISION_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI std::size_t
  computeDistances<context::Scalar, context::Options, JointCollectionDefaultTpl, context::VectorXs>(
    const Model &,
    Data &,
    const GeometryModel &,
    GeometryData &,
    const Eigen::MatrixBase<context::VectorXs> &);

}
#endif // ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION
