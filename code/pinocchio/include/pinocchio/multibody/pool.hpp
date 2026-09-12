//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <algorithm>
#include <iterator>
#include <vector>
#include <cstddef>

#include <omp.h>

#include "pinocchio/context.hpp"
#include "pinocchio/macros.hpp"

#include "pinocchio/utils/openmp.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/multibody/pool/fwd.hxx"
#include "pinocchio/src/multibody/pool/model.hxx"
#include "pinocchio/src/multibody/pool/geometry.hxx"
// IWYU pragma: end_exports
