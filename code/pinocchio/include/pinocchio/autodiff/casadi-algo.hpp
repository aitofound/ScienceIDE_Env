//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <casadi/casadi.hpp>

#include "pinocchio/autodiff/casadi.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/aba-derivatives.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/joint-configuration.hpp"
#include "pinocchio/algorithm/constrained-dynamics-derivatives.hpp"
#include "pinocchio/algorithm/constrained-dynamics.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/autodiff/casadi/algorithm/algo.hxx"
// IWYU pragma: end_exports
