//
// Copyright (c) 2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/geometry.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/geometry.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  struct GeometryObjectFilterBase : InstanceFilterBase<GeometryObject>
  {

  }; // struct GeometryObjectFilterBase

  struct GeometryObjectFilterNothing final : GeometryObjectFilterBase
  {
    bool operator()(const GeometryObject &) const
    {
      return true;
    }

  }; // struct GeometryObjectFilterNothing

  struct GeometryObjectFilterSelectByJoint final : GeometryObjectFilterBase
  {
    GeometryObjectFilterSelectByJoint(const size_t joint_id)
    : joint_id(joint_id)
    {
    }

    bool operator()(const GeometryObject & geometry_object) const
    {
      return geometry_object.parentJoint == joint_id;
    }

    const size_t joint_id;

  }; // struct GeometryObjectFilterSelectByJoint

} // namespace pinocchio
