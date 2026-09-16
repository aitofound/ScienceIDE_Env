//
// Copyright (c) 2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/collision/tree-broadphase-manager.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/collision/tree-broadphase-manager.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename _Manager>
  struct TreeBroadPhaseManagerTpl : public BroadPhaseManagerBase<TreeBroadPhaseManagerTpl<_Manager>>
  {

    typedef _Manager Manager;
    typedef BroadPhaseManagerBase<TreeBroadPhaseManagerTpl<_Manager>> Base;
    typedef BroadPhaseManagerTpl<Manager> BroadPhaseManager;

    typedef std::vector<coal::CollisionObject *> CollisionObjectPointerVector;
    typedef std::vector<BroadPhaseManager> BroadPhaseManagerVector;

    typedef typename BroadPhaseManager::Model Model;
    typedef typename BroadPhaseManager::GeometryModel GeometryModel;
    typedef typename BroadPhaseManager::GeometryData GeometryData;

    /// @brief Default constructor.
    TreeBroadPhaseManagerTpl() // for std::vector
    : Base()
    {
    }

    /// @brief Constructor from a given geometry model and data.
    ///
    /// \param[in] model_ptr pointer to the model of the kinematic tree.
    /// \param[in] geometry_model_ptr pointer to the geometry model.
    /// \param[in] geometry_data_ptr pointer to the geometry data.
    ///
    TreeBroadPhaseManagerTpl(
      const Model * model_ptr,
      const GeometryModel * geometry_model_ptr,
      GeometryData * geometry_data_ptr)
    : Base(model_ptr, geometry_model_ptr, geometry_data_ptr)
    {
      init(static_cast<size_t>(model_ptr->njoints));
    }

    /// @brief Copy contructor.
    ///
    /// \param[in] other manager to copy.
    ///
    TreeBroadPhaseManagerTpl(const TreeBroadPhaseManagerTpl & other)
    : Base(other)
    {
      init(other.managers.size());
    }

    using Base::getGeometryData;
    using Base::getGeometryModel;
    using Base::getModel;

    ///
    /// @brief Update the manager from the current geometry positions and update the underlying coal
    /// broad phase manager.
    ///
    /// @param[in] compute_local_aabb whether to recompute the local AABB of the collision
    /// geometries which have changed.
    ///
    void update(bool compute_local_aabb = false);

    ///
    /// @brief Update the manager with a new geometry data.
    ///
    /// \param[in] geom_data_ptr_new pointer to the new geometry data.
    ///
    void update(GeometryData * geom_data_ptr_new);

    ~TreeBroadPhaseManagerTpl()
    {
    }

    /// @brief Check whether the base broad phase manager is aligned with the current
    /// collision_objects.
    bool check() const;

    /// @brief Check whether the callback is inline with *this
    bool check(CollisionCallBackBase * callback) const;

    /// @brief Performs collision test between one object and all the objects belonging to the
    /// manager.
    bool collide(CollisionObject & obj, CollisionCallBackBase * callback) const;

    /// @brief Performs collision test for the objects belonging to the manager.
    bool collide(CollisionCallBackBase * callback) const;

    /// @brief Performs collision test with objects belonging to another manager.
    bool collide(TreeBroadPhaseManagerTpl & other_manager, CollisionCallBackBase * callback) const;

    //  /// @brief Performs distance computation between one object and all the objects belonging to
    //  the manager void distance(CollisionObject* obj, DistanceCallBackBase * callback) const;

    //  /// @brief Performs distance test for the objects belonging to the manager (i.e., N^2 self
    //  distance) void distance(DistanceCallBackBase * callback) const;

    //  /// @brief Performs distance test with objects belonging to another manager
    //  void distance(TreeBroadPhaseManagerTpl* other_manager, DistanceCallBackBase * callback)
    //  const;

    /// @brief Returns internal broad phase managers.
    const BroadPhaseManagerVector & getBroadPhaseManagers() const
    {
      return managers;
    }

    /// @brief Returns internal broad phase managers.
    BroadPhaseManagerVector & getBroadPhaseManagers()
    {
      return managers;
    }

  protected:
    /// @brief the vector of collision objects.
    BroadPhaseManagerVector managers;

    /// @brief Initialialisation
    void init(const size_t njoints);

  }; // struct BroadPhaseManagerTpl<BroadPhaseManagerDerived>

} // namespace pinocchio

/* --- Details -------------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------------- */

namespace pinocchio
{

  template<typename Manager>
  void TreeBroadPhaseManagerTpl<Manager>::update(bool compute_local_aabb)
  {
    for (auto && manager : managers)
    {
      manager.update(compute_local_aabb);
    }
  }

  template<typename Manager>
  void TreeBroadPhaseManagerTpl<Manager>::update(GeometryData * geom_data_ptr_new)
  {
    for (auto && manager : managers)
    {
      manager.update(geom_data_ptr_new);
    }
  }

  template<typename Manager>
  bool TreeBroadPhaseManagerTpl<Manager>::check() const
  {
    for (auto && manager : managers)
    {
      if (!manager.check())
        return false;
    }

    return true;
  }

  template<typename Manager>
  bool TreeBroadPhaseManagerTpl<Manager>::check(CollisionCallBackBase * callback) const
  {
    for (auto && manager : managers)
    {
      if (!manager.check(callback))
        return false;
    }

    return true;
  }

  template<typename Manager>
  void TreeBroadPhaseManagerTpl<Manager>::init(const size_t njoints)
  {
    managers.reserve(njoints);
    for (size_t joint_id = 0; joint_id < njoints; ++joint_id)
    {
      GeometryObjectFilterSelectByJoint filter(joint_id);
      managers.push_back(
        BroadPhaseManager(&getModel(), &getGeometryModel(), &getGeometryData(), filter));
    }
  }

  template<typename Manager>
  bool TreeBroadPhaseManagerTpl<Manager>::collide(CollisionCallBackBase * callback) const
  {
    const size_t num_joints = managers.size();

    callback->init();
    const bool accumulate_save_value = callback->accumulate;
    callback->accumulate = true;

    for (size_t i = 0; i < num_joints; ++i)
    {
      const BroadPhaseManager & manager_outer = managers[i];
      bool should_stop = false;
      for (size_t j = i + 1; j < num_joints; ++j)
      {
        BroadPhaseManager & manager_inner = const_cast<BroadPhaseManager &>(managers[j]);
        manager_outer.collide(manager_inner, callback);
        should_stop = callback->stop();

        if (should_stop)
          break;
      }

      if (should_stop)
        break;
    }

    callback->accumulate = accumulate_save_value; // restore initial value

    callback->done();
    return callback->collision;
  }

  template<typename Manager>
  bool TreeBroadPhaseManagerTpl<Manager>::collide(
    CollisionObject & collision_object, CollisionCallBackBase * callback) const
  {
    const size_t num_joints = managers.size();

    callback->init();
    const bool accumulate_save_value = callback->accumulate;
    callback->accumulate = true;

    for (size_t i = 0; i < num_joints; ++i)
    {
      const BroadPhaseManager & manager = managers[i];
      manager.collide(collision_object, callback);
      if (callback->stop())
        break;
    }

    callback->accumulate = accumulate_save_value; // restore initial value

    callback->done();
    return callback->collision;
  }

  template<typename Manager>
  bool TreeBroadPhaseManagerTpl<Manager>::collide(
    TreeBroadPhaseManagerTpl & other_manager, CollisionCallBackBase * callback) const
  {
    const size_t num_joints = managers.size();

    callback->init();
    const bool accumulate_save_value = callback->accumulate;
    callback->accumulate = true;

    for (size_t i = 0; i < num_joints; ++i)
    {
      const BroadPhaseManager & manager_outer = managers[i];
      bool should_stop = false;
      for (auto && manager_inner : other_manager.managers)
      {
        manager_outer.collide(manager_inner, callback);
        should_stop = callback->stop();

        if (should_stop)
          break;
      }
      if (should_stop)
        break;
    }

    callback->accumulate = accumulate_save_value; // restore initial value

    callback->done();
    return callback->collision;
  }

} // namespace pinocchio
