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
#include <vector>

#include <coal/collision_data.h>
#include <coal/broadphase/broadphase_callbacks.h>

#include "pinocchio/macros.hpp"

#include "pinocchio/multibody.hpp"

#include "pinocchio/collision/collision.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/broadphase-callbacks.hxx"
// IWYU pragma: end_exports
