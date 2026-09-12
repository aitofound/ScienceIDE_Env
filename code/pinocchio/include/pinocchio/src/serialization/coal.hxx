//
// Copyright (c) 2015-2024 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/serialization.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/serialization.hpp"
#endif // PINOCCHIO_LSP

namespace boost
{
  namespace serialization
  {

#ifndef PINOCCHIO_WITH_COLLISION

    template<class Archive>
    void serialize(
      Archive & /*ar*/,
      pinocchio::coal::FakeCollisionGeometry & /*fake_collision_geometry*/,
      const unsigned int /*version*/)
    {
    }

#endif // PINOCCHIO_WITH_COLLISION

  } // namespace serialization
} // namespace boost
