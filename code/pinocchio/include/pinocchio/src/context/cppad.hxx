//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/context.hpp"

#define PINOCCHIO_SKIP_ALGORITHM_MODEL
#define PINOCCHIO_SKIP_ALGORITHM_GEOMETRY
#define PINOCCHIO_SKIP_MULTIBODY_SAMPLE_MODELS
#define PINOCCHIO_SKIP_ALGORITHM_SOLVERS
#define PINOCCHIO_TEMPLATE_INSTANTIATION_HEADER "pinocchio/autodiff/cppad.hpp"

#include <cppad/cppad.hpp>
#define PINOCCHIO_SCALAR_TYPE ::CppAD::AD<double>
#include "pinocchio/src/context/generic.hxx"
#undef PINOCCHIO_SCALAR_TYPE
