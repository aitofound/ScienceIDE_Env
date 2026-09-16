//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <iosfwd>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>

#include <boost/optional.hpp>
#include <boost/none.hpp>

#include <Eigen/Core>

#include <urdf_model/model.h>
#include <urdf_model/pose.h>

#include "pinocchio/macros.hpp"

// clang-format off
#define PINOCCHIO_URDFDOM_HEADERS_VERSION_AT_LEAST(major, minor, patch)      \
  (PINOCCHIO_URDFDOM_HEADERS_MAJOR_VERSION > (major)                         \
   || (PINOCCHIO_URDFDOM_HEADERS_MAJOR_VERSION == (major)                    \
       && PINOCCHIO_URDFDOM_HEADERS_MINOR_VERSION > (minor))                 \
   || (PINOCCHIO_URDFDOM_HEADERS_MAJOR_VERSION == (major)                    \
       && PINOCCHIO_URDFDOM_HEADERS_MINOR_VERSION == (minor)                 \
       && PINOCCHIO_URDFDOM_HEADERS_PATCH_VERSION >= (patch)))
// clang-format on

#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/parsers/config.hpp"
#include "pinocchio/src/parsers/scalar-model.hxx"
#include "pinocchio/src/parsers/meshloader-fwd.hxx"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
namespace pinocchio
{
  namespace urdf
  {
    namespace details
    {
      typedef double urdf_scalar_type;
    }
  } // namespace urdf
} // namespace pinocchio
#include "pinocchio/src/parsers/urdf/urdf.hxx"
#include "pinocchio/src/parsers/urdf/utils.hxx"
#include "pinocchio/src/parsers/urdf/model.hxx"
#include "pinocchio/src/parsers/urdf/geometry.hxx"
// IWYU pragma: end_exports
