//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <Eigen/SparseCholesky>

#include "pinocchio/macros.hpp"
#include "pinocchio/context.hpp"
#include "pinocchio/multibody/fwd.hpp"

#include "pinocchio/utils/check.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/fwd.hxx"
#include "pinocchio/src/algorithm/proximal.hxx"
// IWYU pragma: end_exports
