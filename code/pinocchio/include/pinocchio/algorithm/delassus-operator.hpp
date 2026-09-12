//
// Copyright (c) 2024 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <new>
#include <functional>
#include <utility>
#include <vector>
#include <type_traits>

#include <boost/fusion/container/vector.hpp>
#include <boost/container/detail/std_fwd.hpp>
#include <boost/variant.hpp>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#ifdef PINOCCHIO_WITH_ACCELERATE_SUPPORT
  #include <Eigen/AccelerateSupport>
#endif // ifdef PINOCCHIO_WITH_ACCELERATE_SUPPORT

#include "pinocchio/fwd.hpp"
#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/utils/alloca.hpp"
#include "pinocchio/utils/promote-static-eval.hpp"
#include "pinocchio/utils/reference.hpp"
#include "pinocchio/utils/size-in-bytes.hpp"

#include "pinocchio/container/eigen-storage.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/algorithm/fwd.hpp"
#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/diagonal-preconditioner.hpp"
#include "pinocchio/algorithm/aba.hpp"
#include "pinocchio/algorithm/constraint-cholesky.hpp"

#include "pinocchio/tracy.hpp"

// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/delassus-operator-base.hxx"
#include "pinocchio/src/algorithm/delassus-operator-sparse.hxx"
#include "pinocchio/src/algorithm/delassus-operator-rigid-body-visitors.hxx"
#include "pinocchio/src/algorithm/delassus-operator-rigid-body.hxx"
#include "pinocchio/src/algorithm/delassus-operator-dense.hxx"
#include "pinocchio/src/algorithm/delassus-operator-preconditioned.hxx"
#include "pinocchio/src/algorithm/delassus-operator-cholesky-expression.hxx"
// IWYU pragma: end_exports
