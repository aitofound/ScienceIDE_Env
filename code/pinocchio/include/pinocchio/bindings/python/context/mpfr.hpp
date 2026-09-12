//
// Copyright (c) 2021-2022 INRIA
//

#pragma once

#include "pinocchio/math/multiprecision-mpfr.hpp"

#define PINOCCHIO_PYTHON_SCALAR_TYPE                                                               \
  ::boost::multiprecision::number<                                                                 \
    ::boost::multiprecision::mpfr_float_backend<0>, ::boost::multiprecision::et_off>
#include "pinocchio/bindings/python/context/generic.hpp"

#define PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
#define PINOCCHIO_PYTHON_NO_SERIALIZATION
#define PINOCCHIO_PYTHON_SKIP_REACHABLE_WORKSPACE
