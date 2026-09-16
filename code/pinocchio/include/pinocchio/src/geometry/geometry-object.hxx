//
// Copyright (c) 2015-2023 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/geometry.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/geometry.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  enum GeometryType
  {
    VISUAL,
    COLLISION
  };

  /// No material associated to a geometry.
  struct GeometryNoMaterial
  {
    bool operator==(const GeometryNoMaterial &) const
    {
      return true;
    }
  };

  /// Mesh material based on the Phong lighting model.
  /// Diffuse color is stored in \p GeometryObject::meshColor.
  struct GeometryPhongMaterial
  {
    GeometryPhongMaterial() = default;
    GeometryPhongMaterial(
      const Eigen::Vector4d & meshEmissionColor,
      const Eigen::Vector4d & meshSpecularColor,
      double meshShininess)
    : meshEmissionColor(meshEmissionColor)
    , meshSpecularColor(meshSpecularColor)
    , meshShininess(meshShininess)
    {
    }

    bool operator==(const GeometryPhongMaterial & other) const
    {
      return meshEmissionColor == other.meshEmissionColor
             && meshSpecularColor == other.meshSpecularColor
             && meshShininess == other.meshShininess;
    }

    /// \brief RGBA emission (ambient) color value of the GeometryObject::geometry object.
    Eigen::Vector4d meshEmissionColor{Eigen::Vector4d(0., 0., 0., 1.)};

    /// \brief RGBA specular color value of the GeometryObject::geometry object.
    Eigen::Vector4d meshSpecularColor{Eigen::Vector4d(0., 0., 0., 1.)};

    /// \brief Shininess associated to the specular lighting model.
    ///
    /// This value must normalized between 0 and 1.
    double meshShininess{0.};
  };

  typedef boost::variant<GeometryNoMaterial, GeometryPhongMaterial> GeometryMaterial;

  /// Type of physics material.
  /// When two objects collide, the physics material of the two objects is notably used
  /// to compute the coefficient of friction of the collision pair.
  enum PhysicsMaterialType
  {
    ICE,
    METAL,
    CONCRETE,
    PLASTIC,
    WOOD,
    PHYSICS_MATERIAL_COUNT
  };

  struct FrictionCoefficientMatrix
  {
    typedef Eigen::Matrix<double, PHYSICS_MATERIAL_COUNT, PHYSICS_MATERIAL_COUNT> Matrix;
    typedef Eigen::Index Index;

    Matrix friction_coefficient_matrix;

    FrictionCoefficientMatrix();

    double getFrictionFromMaterialPair(PhysicsMaterialType type1, PhysicsMaterialType type2) const
    {
      return friction_coefficient_matrix(type1, type2);
    }
  };

  inline FrictionCoefficientMatrix & getFrictionCoefficientMatrix()
  {
    static FrictionCoefficientMatrix table;
    return table;
  }

  /// Physics material associated to a geometry.
  struct PhysicsMaterial
  {
    PhysicsMaterialType materialType;
    double compliance;
    double elasticity;

    // Default constructor
    explicit PhysicsMaterial(
      PhysicsMaterialType materialType = PLASTIC, double compliance = 0.0, double elasticity = 0.)
    : materialType(materialType)
    , compliance(compliance)
    , elasticity(elasticity)
    {
    }

    bool operator==(const PhysicsMaterial & other) const
    {
      return materialType == other.materialType && compliance == other.compliance
             && elasticity == other.elasticity;
    }
  };

  template<>
  struct traits<GeometryObject>
  {
    typedef double Scalar;
    static constexpr int Options = 0;
  };

  struct GeometryObject
  : public ModelItem<GeometryObject>
  , serialization::Serializable<GeometryObject>
  {
    typedef ModelItem<GeometryObject> Base;
    typedef typename traits<GeometryObject>::Scalar Scalar;
    static constexpr int Options = traits<GeometryObject>::Options;

    typedef SE3Tpl<Scalar, Options> SE3;

    typedef std::shared_ptr<coal::CollisionGeometry> CollisionGeometryPtr;

    using Base::name;
    using Base::parentFrame;
    using Base::parentJoint;
    using Base::placement;

    /// \brief The coal CollisionGeometry (might be a Mesh, a Geometry Primitive, etc.)
    CollisionGeometryPtr geometry;

    /// \brief Absolute path to the mesh file (if the geometry pointee is also a Mesh)
    std::string meshPath;

    /// \brief Scaling vector applied to the GeometryObject::geometry object.
    Eigen::Vector3d meshScale;

    /// \brief Decide whether to override the Material.
    bool overrideMaterial;

    /// \brief RGBA color value of the GeometryObject::geometry object.
    Eigen::Vector4d meshColor;

    /// \brief Material associated to the mesh.
    /// This material should be used only if overrideMaterial is set to true.
    /// In other case, the mesh default material must be used.
    GeometryMaterial meshMaterial;

    /// \brief Absolute path to the mesh texture file.
    std::string meshTexturePath;

    /// \brief If true, no collision or distance check will be done between the Geometry and any
    /// other geometry
    bool disableCollision;

    /// \brief The physics property of the object.
    PhysicsMaterial physicsMaterial;

    ///
    /// \brief Full constructor.
    ///
    /// \param[in] name  Name of the geometry object.
    /// \param[in] parent_joint  Index of the parent joint (that supports the geometry).
    /// \param[in] parent_frame  Index of the parent frame.
    /// \param[in] placement Placement of the geometry with respect to the joint frame.
    /// \param[in] collision_geometry The coal collision geometry object.
    /// \param[in] meshPath Path of the mesh (may be needed extarnally to load the mesh inside a
    /// viewer for instance) [if applicable]. \param[in] meshScale Scale of the mesh [if
    /// applicable]. \param[in] overrideMaterial If true, this option allows to overrite the
    /// material [if applicable]. \param[in] meshColor Color of the mesh [if applicable]. \param[in]
    /// meshTexturePath Path to the file containing the texture information [if applicable].
    /// \param[in] meshMaterial Material of the mesh [if applicable].
    ///
    GeometryObject(
      const std::string & name,
      const JointIndex parent_joint,
      const FrameIndex parent_frame,
      const SE3 & placement,
      const CollisionGeometryPtr & collision_geometry,
      const std::string & meshPath = "",
      const Eigen::Vector3d & meshScale = Eigen::Vector3d::Ones(),
      const bool overrideMaterial = false,
      const Eigen::Vector4d & meshColor = Eigen::Vector4d(0, 0, 0, 1),
      const std::string & meshTexturePath = "",
      const GeometryMaterial & meshMaterial = GeometryNoMaterial(),
      const PhysicsMaterial & physicsMaterial = PhysicsMaterial())
    : Base(name, parent_joint, parent_frame, placement)
    , geometry(collision_geometry)
    , meshPath(meshPath)
    , meshScale(meshScale)
    , overrideMaterial(overrideMaterial)
    , meshColor(meshColor)
    , meshMaterial(meshMaterial)
    , meshTexturePath(meshTexturePath)
    , disableCollision(false)
    , physicsMaterial(physicsMaterial)
    {
    }

    ///
    /// \brief Full constructor.
    ///
    /// \param[in] name  Name of the geometry object.
    /// \param[in] parent_frame  Index of the parent frame.
    /// \param[in] parent_joint  Index of the parent joint (that supports the geometry).
    /// \param[in] collision_geometry The coal collision geometry object.
    /// \param[in] placement Placement of the geometry with respect to the joint frame.
    /// \param[in] meshPath Path of the mesh (may be needed extarnally to load the mesh inside a
    /// viewer for instance) [if applicable]. \param[in] meshScale Scale of the mesh [if
    /// applicable]. \param[in] overrideMaterial If true, this option allows to overrite the
    /// material [if applicable]. \param[in] meshColor Color of the mesh [if applicable]. \param[in]
    /// meshTexturePath Path to the file containing the texture information [if applicable].
    /// \param[in] meshMaterial Material of the mesh [if applicable].
    ///
    /// \deprecated This constructor is now deprecated, and its argument order has been changed.
    ///
    PINOCCHIO_DEPRECATED GeometryObject(
      const std::string & name,
      const FrameIndex parent_frame,
      const JointIndex parent_joint,
      const CollisionGeometryPtr & collision_geometry,
      const SE3 & placement,
      const std::string & meshPath = "",
      const Eigen::Vector3d & meshScale = Eigen::Vector3d::Ones(),
      const bool overrideMaterial = false,
      const Eigen::Vector4d & meshColor = Eigen::Vector4d(0, 0, 0, 1),
      const std::string & meshTexturePath = "",
      const GeometryMaterial & meshMaterial = GeometryNoMaterial(),
      const PhysicsMaterial & physicsMaterial = PhysicsMaterial())
    : Base(name, parent_joint, parent_frame, placement)
    , geometry(collision_geometry)
    , meshPath(meshPath)
    , meshScale(meshScale)
    , overrideMaterial(overrideMaterial)
    , meshColor(meshColor)
    , meshMaterial(meshMaterial)
    , meshTexturePath(meshTexturePath)
    , disableCollision(false)
    , physicsMaterial(physicsMaterial)
    {
    }

    ///
    /// \brief Reduced constructor.
    /// \remarks Compared to the other constructor, this one assumes that there is no parentFrame
    /// associated to the geometry.
    ///
    /// \param[in] name  Name of the geometry object.
    /// \param[in] parent_joint  Index of the parent joint (that supports the geometry).
    /// \param[in] placement Placement of the geometry with respect to the joint frame.
    /// \param[in] collision_geometry The coal collision geometry object.
    /// \param[in] meshPath Path of the mesh (may be needed extarnally to load the mesh inside a
    /// viewer for instance) [if applicable]. \param[in] meshScale Scale of the mesh [if
    /// applicable]. \param[in] overrideMaterial If true, this option allows to overrite the
    /// material [if applicable]. \param[in] meshColor Color of the mesh [if applicable]. \param[in]
    /// meshTexturePath Path to the file containing the texture information [if applicable].
    /// \param[in] meshMaterial Material of the mesh [if applicable].
    ///
    GeometryObject(
      const std::string & name,
      const JointIndex parent_joint,
      const SE3 & placement,
      const CollisionGeometryPtr & collision_geometry,
      const std::string & meshPath = "",
      const Eigen::Vector3d & meshScale = Eigen::Vector3d::Ones(),
      const bool overrideMaterial = false,
      const Eigen::Vector4d & meshColor = Eigen::Vector4d(0, 0, 0, 1),
      const std::string & meshTexturePath = "",
      const GeometryMaterial & meshMaterial = GeometryNoMaterial(),
      const PhysicsMaterial & physicsMaterial = PhysicsMaterial())
    : Base(name, parent_joint, std::numeric_limits<FrameIndex>::max(), placement)
    , geometry(collision_geometry)
    , meshPath(meshPath)
    , meshScale(meshScale)
    , overrideMaterial(overrideMaterial)
    , meshColor(meshColor)
    , meshMaterial(meshMaterial)
    , meshTexturePath(meshTexturePath)
    , disableCollision(false)
    , physicsMaterial(physicsMaterial)
    {
    }

    ///
    /// \brief Reduced constructor.
    /// \remarks Compared to the other constructor, this one assumes that there is no parentFrame
    /// associated to the geometry.
    ///
    /// \param[in] name  Name of the geometry object.
    /// \param[in] parent_joint  Index of the parent joint (that supports the geometry).
    /// \param[in] collision_geometry The coal collision geometry object.
    /// \param[in] placement Placement of the geometry with respect to the joint frame.
    /// \param[in] meshPath Path of the mesh (may be needed extarnally to load the mesh inside a
    /// viewer for instance) [if applicable]. \param[in] meshScale Scale of the mesh [if
    /// applicable]. \param[in] overrideMaterial If true, this option allows to overrite the
    /// material [if applicable]. \param[in] meshColor Color of the mesh [if applicable]. \param[in]
    /// meshTexturePath Path to the file containing the texture information [if applicable].
    /// \param[in] meshMaterial Material of the mesh [if applicable].
    ///
    /// \deprecated This constructor is now deprecated, and its argument order has been changed.
    ///
    PINOCCHIO_DEPRECATED GeometryObject(
      const std::string & name,
      const JointIndex parent_joint,
      const CollisionGeometryPtr & collision_geometry,
      const SE3 & placement,
      const std::string & meshPath = "",
      const Eigen::Vector3d & meshScale = Eigen::Vector3d::Ones(),
      const bool overrideMaterial = false,
      const Eigen::Vector4d & meshColor = Eigen::Vector4d(0, 0, 0, 1),
      const std::string & meshTexturePath = "",
      const GeometryMaterial & meshMaterial = GeometryNoMaterial(),
      const PhysicsMaterial & physicsMaterial = PhysicsMaterial())
    : Base(name, parent_joint, std::numeric_limits<FrameIndex>::max(), placement)
    , geometry(collision_geometry)
    , meshPath(meshPath)
    , meshScale(meshScale)
    , overrideMaterial(overrideMaterial)
    , meshColor(meshColor)
    , meshMaterial(meshMaterial)
    , meshTexturePath(meshTexturePath)
    , disableCollision(false)
    , physicsMaterial(physicsMaterial)
    {
    }

    GeometryObject(const GeometryObject & other)
    : Base(other)
    {
      *this = other;
    }

    GeometryObject & operator=(const GeometryObject & other)
    {
      if (&other == this)
        return *this;

      name = other.name;
      parentFrame = other.parentFrame;
      parentJoint = other.parentJoint;
      geometry = other.geometry;
      placement = other.placement;
      meshPath = other.meshPath;
      meshScale = other.meshScale;
      overrideMaterial = other.overrideMaterial;
      meshColor = other.meshColor;
      meshMaterial = other.meshMaterial;
      meshTexturePath = other.meshTexturePath;
      disableCollision = other.disableCollision;
      physicsMaterial = other.physicsMaterial;
      return *this;
    }

    ///
    /// \brief Perform a deep copy of this. It will create a copy of the underlying coal geometry.
    ///
    GeometryObject clone() const
    {
      GeometryObject res(*this);

#ifdef PINOCCHIO_WITH_COLLISION
      if (geometry)
        res.geometry = CollisionGeometryPtr(geometry->clone());
#endif

      return res;
    }

    bool operator==(const GeometryObject & other) const
    {
      if (this == &other)
        return true;
      return name == other.name && parentFrame == other.parentFrame
             && parentJoint == other.parentJoint && placement == other.placement
             && meshPath == other.meshPath && meshScale == other.meshScale
             && overrideMaterial == other.overrideMaterial && meshColor == other.meshColor
             && meshMaterial == other.meshMaterial && meshTexturePath == other.meshTexturePath
             && disableCollision == other.disableCollision
             && physicsMaterial == other.physicsMaterial
             && compare_shared_ptr(geometry, other.geometry);
    }

    bool operator!=(const GeometryObject & other) const
    {
      return !(*this == other);
    }

    friend std::ostream & operator<<(std::ostream & os, const GeometryObject & geomObject);
  };

#ifdef PINOCCHIO_WITH_COLLISION

  struct CollisionObject : ::coal::CollisionObject
  {
    typedef ::coal::CollisionObject Base;
    typedef SE3Tpl<double> SE3;

    CollisionObject()
    : Base(nullptr, false)
    , geometryObjectIndex((std::numeric_limits<size_t>::max)())
    {
    }

    explicit CollisionObject(
      const std::shared_ptr<::coal::CollisionGeometry> & collision_geometry,
      const size_t geometryObjectIndex = (std::numeric_limits<size_t>::max)(),
      bool compute_local_aabb = true)
    : Base(collision_geometry, compute_local_aabb)
    , geometryObjectIndex(geometryObjectIndex)
    {
    }

    CollisionObject(
      const std::shared_ptr<::coal::CollisionGeometry> & collision_geometry,
      const SE3 & transform,
      const size_t geometryObjectIndex = (std::numeric_limits<size_t>::max)(),
      bool compute_local_aabb = true)
    : Base(collision_geometry, toCoalTransform3s(transform), compute_local_aabb)
    , geometryObjectIndex(geometryObjectIndex)
    {
    }

    bool operator==(const CollisionObject & other) const
    {
      return Base::operator==(other) && geometryObjectIndex == other.geometryObjectIndex;
    }

    bool operator!=(const CollisionObject & other) const
    {
      return !(*this == other);
    }

    /// @brief Geometry object index related to the current collision object.
    size_t geometryObjectIndex;
  };

  struct ComputeCollision : ::coal::ComputeCollision
  {
    typedef ::coal::ComputeCollision Base;

    ComputeCollision(const GeometryObject & go1, const GeometryObject & go2)
    : Base(go1.geometry.get(), go2.geometry.get())
    , go1_ptr(&go1)
    , go2_ptr(&go2)
    {
    }

    virtual ~ComputeCollision() {};

    virtual std::size_t run(
      const coal::Transform3s & tf1,
      const coal::Transform3s & tf2,
      const coal::CollisionRequest & request,
      coal::CollisionResult & result) const
    {
      typedef ::coal::CollisionGeometry const * Pointer;
      const_cast<Pointer &>(Base::o1) = go1_ptr->geometry.get();
      const_cast<Pointer &>(Base::o2) = go2_ptr->geometry.get();
      return Base::run(tf1, tf2, request, result);
    }

    bool operator==(const ComputeCollision & other) const
    {
      return Base::operator==(other) && go1_ptr == other.go1_ptr
             && go2_ptr == other.go2_ptr; // Maybe, it would be better to just check *go2_ptr ==
                                          // *other.go2_ptr
    }

    bool operator!=(const ComputeCollision & other) const
    {
      return !(*this == other);
    }

    const GeometryObject & getGeometryObject1() const
    {
      return *go1_ptr;
    }
    const GeometryObject & getGeometryObject2() const
    {
      return *go2_ptr;
    }

  protected:
    const GeometryObject * go1_ptr;
    const GeometryObject * go2_ptr;
  };

  struct ComputeContactPatch : ::coal::ComputeContactPatch
  {
    typedef ::coal::ComputeContactPatch Base;

    ComputeContactPatch(const GeometryObject & go1, const GeometryObject & go2)
    : Base(go1.geometry.get(), go2.geometry.get())
    , go1_ptr(&go1)
    , go2_ptr(&go2)
    {
    }

    virtual ~ComputeContactPatch() {};

    void run(
      const coal::Transform3s & tf1,
      const coal::Transform3s & tf2,
      const coal::CollisionResult & collision_result,
      const coal::ContactPatchRequest & request,
      coal::ContactPatchResult & result) const override
    {
      typedef ::coal::CollisionGeometry const * Pointer;
      const_cast<Pointer &>(Base::o1) = go1_ptr->geometry.get();
      const_cast<Pointer &>(Base::o2) = go2_ptr->geometry.get();
      return Base::run(tf1, tf2, collision_result, request, result);
    }

    bool operator==(const ComputeContactPatch & other) const
    {
      return Base::operator==(other) && go1_ptr == other.go1_ptr
             && go2_ptr == other.go2_ptr; // Maybe, it would be better to just check *go2_ptr ==
                                          // *other.go2_ptr
    }

    bool operator!=(const ComputeContactPatch & other) const
    {
      return !(*this == other);
    }

    const GeometryObject & getGeometryObject1() const
    {
      return *go1_ptr;
    }
    const GeometryObject & getGeometryObject2() const
    {
      return *go2_ptr;
    }

  protected:
    const GeometryObject * go1_ptr;
    const GeometryObject * go2_ptr;
  };

  struct ComputeDistance : ::coal::ComputeDistance
  {
    typedef ::coal::ComputeDistance Base;

    ComputeDistance(const GeometryObject & go1, const GeometryObject & go2)
    : Base(go1.geometry.get(), go2.geometry.get())
    , go1_ptr(&go1)
    , go2_ptr(&go2)
    {
    }

    virtual ~ComputeDistance() {};

    virtual coal::CoalScalar run(
      const coal::Transform3s & tf1,
      const coal::Transform3s & tf2,
      const coal::DistanceRequest & request,
      coal::DistanceResult & result) const
    {
      typedef ::coal::CollisionGeometry const * Pointer;
      const_cast<Pointer &>(Base::o1) = go1_ptr->geometry.get();
      const_cast<Pointer &>(Base::o2) = go2_ptr->geometry.get();
      return Base::run(tf1, tf2, request, result);
    }

    bool operator==(const ComputeDistance & other) const
    {
      return Base::operator==(other) && go1_ptr == other.go1_ptr && go2_ptr == other.go2_ptr;
    }

    bool operator!=(const ComputeDistance & other) const
    {
      return !(*this == other);
    }

    const GeometryObject & getGeometryObject1() const
    {
      return *go1_ptr;
    }
    const GeometryObject & getGeometryObject2() const
    {
      return *go2_ptr;
    }

  protected:
    const GeometryObject * go1_ptr;
    const GeometryObject * go2_ptr;
  };

#endif

} // namespace pinocchio

/* --- Details -------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------- */
/* --- Details -------------------------------------------------------------- */
namespace pinocchio
{

  inline std::ostream & operator<<(std::ostream & os, const GeometryObject & geom_object)
  {
    os << "Name: \t \n"
       << geom_object.name << "\n"
       << "Parent frame ID: \t \n"
       << geom_object.parentFrame << "\n"
       << "Parent joint ID: \t \n"
       << geom_object.parentJoint << "\n"
       << "Position in parent frame: \t \n"
       << geom_object.placement << "\n"
       << "Absolute path to mesh file: \t \n"
       << geom_object.meshPath << "\n"
       << "Scale for transformation of the mesh: \t \n"
       << geom_object.meshScale.transpose() << "\n"
       << "Disable collision: \t \n"
       << geom_object.disableCollision << "\n"
       << std::endl;
    return os;
  }

  inline FrictionCoefficientMatrix::FrictionCoefficientMatrix()
  {
    // Initialize the matrix with zeros, in case we forget to set some coefficients.
    for (Index i = 0; i < friction_coefficient_matrix.rows(); ++i)
    {
      for (Index j = 0; j < friction_coefficient_matrix.cols(); ++j)
      {
        friction_coefficient_matrix.coeffRef(i, j) = 0.0;
      }
    }

    // Sources: https://en.wikipedia.org/wiki/Friction
    //          https://www.engineeringtoolbox.com/friction-coefficients-d_778.html
    // These are really rough estimates, and should be replaced by more accurate values if
    // available.
    friction_coefficient_matrix.coeffRef(METAL, METAL) = 0.75;
    friction_coefficient_matrix.coeffRef(METAL, WOOD) = 0.5;
    friction_coefficient_matrix.coeffRef(METAL, PLASTIC) = 0.2;
    friction_coefficient_matrix.coeffRef(METAL, ICE) = 0.03;
    friction_coefficient_matrix.coeffRef(METAL, CONCRETE) = 0.85;

    friction_coefficient_matrix.coeffRef(WOOD, WOOD) = 0.4;
    friction_coefficient_matrix.coeffRef(WOOD, PLASTIC) = 0.3;
    friction_coefficient_matrix.coeffRef(WOOD, ICE) = 0.03;
    friction_coefficient_matrix.coeffRef(WOOD, CONCRETE) = 0.65;

    friction_coefficient_matrix.coeffRef(PLASTIC, PLASTIC) = 0.2;
    friction_coefficient_matrix.coeffRef(PLASTIC, ICE) = 0.02;
    friction_coefficient_matrix.coeffRef(PLASTIC, CONCRETE) = 0.55;

    friction_coefficient_matrix.coeffRef(ICE, ICE) = 0.01;
    friction_coefficient_matrix.coeffRef(ICE, CONCRETE) = 0.25;

    friction_coefficient_matrix.coeffRef(CONCRETE, CONCRETE) = 1.0;

    // Symmetrize the matrix
    friction_coefficient_matrix.triangularView<Eigen::StrictlyLower>() =
      friction_coefficient_matrix.transpose();
  }

} // namespace pinocchio
