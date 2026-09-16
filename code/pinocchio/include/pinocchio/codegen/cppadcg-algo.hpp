//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <cppad/cg.hpp>
#include <cppad/cppad.hpp>

#include "pinocchio/eigen-common.hpp"

#include "pinocchio/codegen/cppadcg.hpp"

#include "pinocchio/multibody.hpp"

#include "pinocchio/algorithm/joint-configuration.hpp"
#include "pinocchio/algorithm/crba.hpp"
#include "pinocchio/algorithm/rnea.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/rnea-derivatives.hpp"
#include "pinocchio/algorithm/aba-derivatives.hpp"
#include "pinocchio/algorithm/constrained-dynamics-derivatives.hpp"
#include "pinocchio/algorithm/constrained-dynamics.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/codegen/cppadcg/algorithm/code-generator-base.hxx"
#include "pinocchio/src/codegen/cppadcg/algorithm/code-generator-algo.hxx"
// IWYU pragma: end_exports
