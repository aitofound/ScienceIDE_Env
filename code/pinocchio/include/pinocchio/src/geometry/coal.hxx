//
// Copyright (c) 2015-2023 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/geometry.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/geometry.hpp"
#endif // PINOCCHIO_LSP

#ifdef PINOCCHIO_WITH_COLLISION

  #ifdef _WIN32
    // It appears that std::snprintf is missing for Windows.
    #if !(                                                                                         \
      (defined(_MSC_VER) && _MSC_VER < 1900)                                                       \
      || (defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)))
      #include <cstdio>
      #include <stdarg.h>
namespace std
{
  inline int _snprintf(char * buffer, std::size_t buf_size, const char * format, ...)
  {
    int res;

    va_list args;
    va_start(args, format);
    res = vsnprintf(buffer, buf_size, format, args);
    va_end(args);

    return res;
  }
} // namespace std
    #endif
  #endif

  #include <coal/collision_object.h>
  #include <coal/collision.h>
  #include <coal/contact_patch.h>
  #include <coal/distance.h>
  #include <coal/shape/geometric_shapes.h>
  #include "pinocchio/collision/coal-pinocchio-conversions.hpp"
#endif

namespace pinocchio
{

#ifndef PINOCCHIO_WITH_COLLISION

  namespace coal
  {

    struct FakeCollisionGeometry
    {
      FakeCollisionGeometry() {};

      bool operator==(const FakeCollisionGeometry &) const
      {
        return true;
      }
    };

    struct AABB
    {
      AABB()
      : min_(0)
      , max_(1) {};

      int min_;
      int max_;
    };

    typedef FakeCollisionGeometry CollisionGeometry;

  } // namespace coal

#else

  inline bool operator==(const ::coal::CollisionObject & lhs, const ::coal::CollisionObject & rhs)
  {
    return lhs.collisionGeometry() == rhs.collisionGeometry()
           && lhs.getAABB().min_ == rhs.getAABB().min_ && lhs.getAABB().max_ == rhs.getAABB().max_;
  }

#endif // PINOCCHIO_WITH_COLLISION

} // namespace pinocchio
