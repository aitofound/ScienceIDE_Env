//
// Copyright (c) 2022 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <cstddef>
#include <limits>
#include <ostream>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <vector>

#include <coal/collision_data.h>
#include <coal/BV/AABB.h>
#include <coal/collision_object.h>
#include <coal/data_types.h>
#include <coal/fwd.hh>

#include "pinocchio/macros.hpp"

#include "pinocchio/multibody.hpp"

#include "pinocchio/collision/broadphase-manager-base.hpp"
#include "pinocchio/collision/broadphase-callbacks.hpp"
#include "pinocchio/collision/coal-pinocchio-conversions.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/broadphase-manager.hxx"
// IWYU pragma: end_exports
