//
// Copyright (c) 2026 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/collision/broadphase.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/collision/broadphase.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename BroadPhaseManagerDerived>
  bool computeCollisions(
    BroadPhaseManagerBase<BroadPhaseManagerDerived> & broadphase_manager,
    CollisionCallBackBase * callback)
  {
    PINOCCHIO_CHECK_INPUT_ARGUMENT(broadphase_manager.check(callback));
    broadphase_manager.collide(callback);
    callback->done();
    return callback->collision;
  }

  template<typename BroadPhaseManagerDerived>
  bool computeCollisions(
    BroadPhaseManagerBase<BroadPhaseManagerDerived> & broadphase_manager,
    const bool stopAtFirstCollision)
  {
    CollisionCallBackDefault callback(
      broadphase_manager.getGeometryModel(), broadphase_manager.getGeometryData(),
      stopAtFirstCollision);

    return computeCollisions(broadphase_manager, &callback);
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename BroadPhaseManagerDerived,
    typename ConfigVectorType>
  inline bool computeCollisions(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    BroadPhaseManagerBase<BroadPhaseManagerDerived> & broadphase_manager,
    CollisionCallBackBase * callback,
    const Eigen::MatrixBase<ConfigVectorType> & q)
  {
    updateGeometryPlacements(
      model, data, broadphase_manager.getGeometryModel(), broadphase_manager.getGeometryData(), q);

    broadphase_manager.update(false);
    return computeCollisions(broadphase_manager, callback);
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename BroadPhaseManagerDerived,
    typename ConfigVectorType>
  inline bool computeCollisions(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    BroadPhaseManagerBase<BroadPhaseManagerDerived> & broadphase_manager,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const bool stopAtFirstCollision)
  {
    updateGeometryPlacements(
      model, data, broadphase_manager.getGeometryModel(), broadphase_manager.getGeometryData(), q);

    broadphase_manager.update(false);

    CollisionCallBackDefault callback(
      broadphase_manager.getGeometryModel(), broadphase_manager.getGeometryData(),
      stopAtFirstCollision);
    return computeCollisions(broadphase_manager, &callback);
  }
} // namespace pinocchio
