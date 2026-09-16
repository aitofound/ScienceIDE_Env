//
// Copyright (c) 2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/collision/broadphase-manager.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/collision/broadphase-manager.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename _Manager>
  struct BroadPhaseManagerTpl : public BroadPhaseManagerBase<BroadPhaseManagerTpl<_Manager>>
  {

    typedef BroadPhaseManagerBase<BroadPhaseManagerTpl<_Manager>> Base;
    typedef std::vector<CollisionObject> CollisionObjectVector;
    typedef Eigen::VectorXd VectorXs;
    typedef _Manager Manager;

    typedef ::pinocchio::Model Model;
    typedef ::pinocchio::GeometryModel GeometryModel;
    typedef ::pinocchio::GeometryData GeometryData;

    /// @brief Default constructor.
    BroadPhaseManagerTpl() // for std::vector
    : Base()
    {
    }

    /// @brief Constructor from a given geometry model and data.
    ///
    /// \param[in] model_ptr pointer to the model.
    /// \param[in] geometry_model_ptr pointer to the geometry model.
    /// \param[in] geometry_data_ptr pointer to the geometry data.
    ///
    BroadPhaseManagerTpl(
      const Model * model_ptr,
      const GeometryModel * geometry_model_ptr,
      GeometryData * geometry_data_ptr,
      const GeometryObjectFilterBase & filter = GeometryObjectFilterNothing())
    : Base(model_ptr, geometry_model_ptr, geometry_data_ptr)
    {
      const GeometryModel & geom_model = *geometry_model_ptr;
      selected_geometry_objects = filter.apply(geom_model.geometryObjects);

      geometry_to_collision_index.resize(
        geometry_model_ptr->geometryObjects.size(), (std::numeric_limits<size_t>::max)());
      collision_object_is_active.resize(selected_geometry_objects.size(), true);
      for (size_t k = 0; k < selected_geometry_objects.size(); ++k)
      {
        const size_t geometry_id = selected_geometry_objects[k];
        geometry_to_collision_index[geometry_id] = k;
        collision_object_is_active[k] = !geom_model.geometryObjects[geometry_id].disableCollision;
      }

      selected_collision_pairs.reserve(geom_model.collisionPairs.size());
      for (size_t k = 0; k < geom_model.collisionPairs.size(); ++k)
      {
        const CollisionPair & pair = geom_model.collisionPairs[k];
        if (
          geometry_to_collision_index[pair.first] != (std::numeric_limits<size_t>::max)()
          && geometry_to_collision_index[pair.second] != (std::numeric_limits<size_t>::max)())
        {
          selected_collision_pairs.push_back(k);
        }

        selected_collision_pairs.resize(selected_collision_pairs.size());
      }

      collision_object_inflation.resize(
        static_cast<Eigen::Index>(selected_geometry_objects.size()));

      init();
    }

    /// @brief Copy contructor.
    ///
    /// \param[in] other manager to copy.
    ///
    BroadPhaseManagerTpl(const BroadPhaseManagerTpl & other)
    : Base(other)
    , collision_object_inflation(other.collision_object_inflation.size())
    , selected_geometry_objects(other.selected_geometry_objects)
    , geometry_to_collision_index(other.geometry_to_collision_index)
    , selected_collision_pairs(other.selected_collision_pairs)
    , collision_object_is_active(other.collision_object_is_active)
    {
      init();
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

    ~BroadPhaseManagerTpl();

    /// @brief Check whether the base broad phase manager is aligned with the current
    /// collision_objects.
    bool check() const;

    /// @brief Check whether the callback is inline with *this
    bool check(CollisionCallBackBase * callback) const;

    /// @brief Returns the vector of collision objects associated to the manager.
    const CollisionObjectVector & getCollisionObjects() const
    {
      return collision_objects;
    }
    /// @brief Returns the vector of collision objects associated to the manager.
    CollisionObjectVector & getCollisionObjects()
    {
      return collision_objects;
    }

    /// @brief Returns the inflation value related to each collision object.
    const VectorXs & getCollisionObjectInflation()
    {
      return collision_object_inflation;
    }

    /// @brief Performs collision test between one object and all the objects belonging to the
    /// manager.
    bool collide(CollisionObject & obj, CollisionCallBackBase * callback) const;

    /// @brief Performs collision test for the objects belonging to the manager.
    bool collide(CollisionCallBackBase * callback) const;

    /// @brief Performs collision test with objects belonging to another manager.
    bool collide(BroadPhaseManagerTpl & other_manager, CollisionCallBackBase * callback) const;

    //  /// @brief Performs distance computation between one object and all the objects belonging to
    //  the manager void distance(CollisionObject* obj, DistanceCallBackBase * callback) const;

    //  /// @brief Performs distance test for the objects belonging to the manager (i.e., N^2 self
    //  distance) void distance(DistanceCallBackBase * callback) const;

    //  /// @brief Performs distance test with objects belonging to another manager
    //  void distance(BroadPhaseCollisionManager* other_manager, DistanceCallBackBase * callback)
    //  const;

    /// @brief Returns internal manager.
    Manager & getManager()
    {
      return manager;
    }

    /// @brief Returns internal manager.
    const Manager & getManager() const
    {
      return manager;
    }

    /// @brief Returns the status of the collision object.
    const std::vector<bool> & getCollisionObjectStatus() const
    {
      return collision_object_is_active;
    }

  protected:
    /// @brief internal manager
    Manager manager;

    /// @brief the vector of collision objects.
    CollisionObjectVector collision_objects;

    /// @brief the inflation value related to each collision object.
    VectorXs collision_object_inflation;

    /// @brief Selected geometry objects in the original geometry_model.
    std::vector<size_t> selected_geometry_objects;

    /// @brief Mapping between a given geometry index and a collision index.
    std::vector<size_t> geometry_to_collision_index;

    /// @brief Selected  collision pairs in the original geometry_model.
    std::vector<size_t> selected_collision_pairs;

    /// @brief Disable status related to each collision objects.
    std::vector<bool> collision_object_is_active;

    /// @brief Initialialisation of BroadPhaseManagerTpl
    void init();

  }; // struct BroadPhaseManagerTpl<BroadPhaseManagerDerived>

} // namespace pinocchio

/* --- Details -------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------- */

namespace pinocchio
{

  template<typename Manager>
  void BroadPhaseManagerTpl<Manager>::update(bool compute_local_aabb)
  {
    const GeometryModel & geom_model = getGeometryModel();
    assert(selected_geometry_objects.size() == size_t(collision_object_inflation.size()));

    GeometryData & geom_data = getGeometryData();

    // Pass 1: check the new active geometries and list the new deactive geometries
    std::vector<size_t> new_active, new_inactive;
    for (size_t k = 0; k < selected_geometry_objects.size(); ++k)
    {
      const size_t geometry_object_id = selected_geometry_objects[k];
      const GeometryObject & geom_object = geom_model.geometryObjects[geometry_object_id];

      if (geom_object.geometry->aabb_local.volume() <= 0.) // degenerated geometry, we should not
                                                           // consider it as an active object.
      {
        if (collision_object_is_active[k])
        {
          collision_object_is_active[k] = false;
          new_inactive.push_back(k);
        }

        continue; // don't go further and checks the next objects
      }

      if (collision_object_is_active[k] != !geom_object.disableCollision) // change state
      {
        collision_object_is_active[k] = !geom_object.disableCollision;
        if (geom_object.disableCollision)
          new_inactive.push_back(k);
        else
          new_active.push_back(k);
      }
    }

    // The pass should be done over all the geometry objects composing the collision tree.
    collision_object_inflation.setZero();
    for (size_t pair_id = 0; pair_id < geom_model.collisionPairs.size(); ++pair_id)
    {
      // TODO(jcarpent): enhance the performances by collecting only the collision pairs related to
      // the selected_geometry_objects
      const CollisionPair & pair = geom_model.collisionPairs[pair_id];
      const GeomIndex geom1_id = pair.first;
      const GeomIndex geom2_id = pair.second;

      const bool geom1_is_selected =
        geometry_to_collision_index[geom1_id] != (std::numeric_limits<size_t>::max)();
      const bool geom2_is_selected =
        geometry_to_collision_index[geom2_id] != (std::numeric_limits<size_t>::max)();
      if (!(geom1_is_selected || geom2_is_selected))
        continue;

      const bool check_collision = geom_data.activeCollisionPairs[pair_id]
                                   && !(
                                     geom_model.geometryObjects[geom1_id].disableCollision
                                     || geom_model.geometryObjects[geom2_id].disableCollision);

      if (!check_collision)
        continue;

      const ::coal::CollisionRequest & cr = geom_data.collisionRequests[pair_id];
      const double inflation = (cr.break_distance + cr.security_margin) * 0.5;

      if (geom1_is_selected)
      {
        const Eigen::Index geom1_id_local =
          static_cast<Eigen::Index>(geometry_to_collision_index[geom1_id]);
        collision_object_inflation[geom1_id_local] =
          (std::max)(inflation, collision_object_inflation[geom1_id_local]);
      }

      if (geom2_is_selected)
      {
        const Eigen::Index geom2_id_local =
          static_cast<Eigen::Index>(geometry_to_collision_index[geom2_id]);
        collision_object_inflation[geom2_id_local] =
          (std::max)(inflation, collision_object_inflation[geom2_id_local]);
      }
    }

    for (size_t k = 0; k < selected_geometry_objects.size(); ++k)
    {
      if (!collision_object_is_active[k])
        continue;

      const size_t geometry_object_id = selected_geometry_objects[k];

      const GeometryObject & geom_obj = geom_model.geometryObjects[geometry_object_id];
      coal::CollisionGeometryPtr_t new_geometry = geom_obj.geometry;

      CollisionObject & collision_obj = collision_objects[k];
      coal::CollisionGeometryPtr_t geometry = collision_obj.collisionGeometry();

      collision_obj.setTransform(toCoalTransform3s(geom_data.oMg[geometry_object_id]));

      if (new_geometry.get() != geometry.get())
        collision_obj.setCollisionGeometry(new_geometry, compute_local_aabb);

      collision_obj.computeAABB();
      collision_obj.getAABB().expand(collision_object_inflation[static_cast<Eigen::Index>(k)]);
    }

    // Remove deactivated collision objects
    for (size_t k : new_inactive)
      manager.unregisterObject(&collision_objects[k]);

    // Add deactivated collision objects
    for (size_t k : new_active)
      manager.registerObject(&collision_objects[k]);

    manager.update(); // because the position has changed.
    assert(check() && "The status of the BroadPhaseManager is not valid");
  }

  template<typename Manager>
  void BroadPhaseManagerTpl<Manager>::update(GeometryData * geom_data_ptr_new)
  {
    Base::geometry_data_ptr = geom_data_ptr_new;
    update(false);
  }

  template<typename Manager>
  BroadPhaseManagerTpl<Manager>::~BroadPhaseManagerTpl()
  {
  }

  template<typename Manager>
  bool BroadPhaseManagerTpl<Manager>::check() const
  {
    std::vector<coal::CollisionObject *> collision_objects_ptr = manager.getObjects();
    if (collision_objects_ptr.size() > collision_objects.size())
      return false;

    size_t count_active_objects = 0;
    for (auto active : collision_object_is_active)
      count_active_objects += active;

    if (count_active_objects != collision_objects_ptr.size())
      return false;

    const GeometryModel & geom_model = getGeometryModel();
    for (size_t k = 0; k < selected_geometry_objects.size(); ++k)
    {
      const size_t geometry_id = selected_geometry_objects[k];

      const coal::CollisionObject & collision_obj = collision_objects[k];
      coal::CollisionGeometryConstPtr_t geometry = collision_obj.collisionGeometry();

      if (collision_object_is_active[k]) // The collision object is active
      {
        if (
          std::find(collision_objects_ptr.begin(), collision_objects_ptr.end(), &collision_obj)
          == collision_objects_ptr.end())
          return false;

        if (
          geometry.get()->aabb_local.volume()
          == -(std::numeric_limits<coal::CoalScalar>::infinity)())
          return false;

        const GeometryObject & geom_obj = geom_model.geometryObjects[geometry_id];
        coal::CollisionGeometryConstPtr_t geometry_of_geom_obj = geom_obj.geometry;

        if (geometry.get() != geometry_of_geom_obj.get())
          return false;
      }
      else
      {
        if (
          std::find(collision_objects_ptr.begin(), collision_objects_ptr.end(), &collision_obj)
          != collision_objects_ptr.end())
          return false;
      }
    }

    return true;
  }

  template<typename Manager>
  bool BroadPhaseManagerTpl<Manager>::check(CollisionCallBackBase * callback) const
  {
    return &callback->getGeometryModel() == &getGeometryModel()
           && &callback->getGeometryData() == &getGeometryData();
  }

  template<typename Manager>
  void BroadPhaseManagerTpl<Manager>::init()
  {
    const GeometryModel & geom_model = getGeometryModel();
    collision_objects.reserve(selected_geometry_objects.size());
    for (size_t k = 0; k < selected_geometry_objects.size(); ++k)
    {
      const size_t geometry_id = selected_geometry_objects[k];
      GeometryObject & geom_obj =
        const_cast<GeometryObject &>(geom_model.geometryObjects[geometry_id]);
      collision_objects.push_back(CollisionObject(geom_obj.geometry, geometry_id));

      // Feed the base broadphase manager
      if (collision_object_is_active[k])
        manager.registerObject(&collision_objects.back());
    }
  }

  template<typename Manager>
  bool BroadPhaseManagerTpl<Manager>::collide(
    CollisionObject & obj, CollisionCallBackBase * callback) const
  {
    manager.collide(&obj, callback);
    return callback->collision;
  }

  template<typename Manager>
  bool BroadPhaseManagerTpl<Manager>::collide(CollisionCallBackBase * callback) const
  {
    manager.collide(callback);
    return callback->collision;
  }

  template<typename Manager>
  bool BroadPhaseManagerTpl<Manager>::collide(
    BroadPhaseManagerTpl & other_manager, CollisionCallBackBase * callback) const
  {
    manager.collide(&other_manager.manager, callback);
    return callback->collision;
  }

} // namespace pinocchio
