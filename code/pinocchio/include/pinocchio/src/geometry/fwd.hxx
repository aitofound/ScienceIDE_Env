//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/geometry.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/geometry/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename T>
  struct InstanceFilterBase;

  struct GeometryNoMaterial;
  struct GeometryPhongMaterial;
  struct FrictionCoefficientMatrix;
  struct PhysicsMaterial;
  struct GeometryObject;
  struct ComputeCollision;
  struct ComputeContactPatch;
  struct ComputeDistance;

  struct CollisionPair;
  struct GeometryModel;
  struct GeometryData;

  struct GeometryObjectFilterBase;
  struct GeometryObjectFilterNothing;
  struct GeometryObjectFilterSelectByJoint;
} // namespace pinocchio
