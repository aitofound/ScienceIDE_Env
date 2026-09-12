//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <stddef.h>
#include <omp.h>
#include <vector>
#include <iterator>
#include <algorithm>

#include "pinocchio/macros.hpp"

#include "pinocchio/multibody/pool.hpp"
#include "pinocchio/collision/broadphase-manager.hpp"
#include "pinocchio/collision/tree-broadphase-manager.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/collision/pool/fwd.hxx"
#include "pinocchio/src/collision/pool/broadphase-manager.hxx"
// IWYU pragma: end_exports
