//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/next_prior.hpp>
#include <boost/variant.hpp>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Cholesky>

#include "pinocchio/context.hpp"
#include "pinocchio/deprecated.hpp"
#include "pinocchio/macros.hpp"
#include "pinocchio/config.hpp"

#include "pinocchio/common.hpp"

#include "pinocchio/container/double-entry-container.hpp"
#include "pinocchio/container/matrix-stack.hpp"

#include "pinocchio/src/fwd.hxx"

#include "pinocchio/utils/static-if.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/liegroup.hpp"
#include "pinocchio/algorithm/fwd.hpp"
#include "pinocchio/algorithm/constraint-cholesky-decl.hpp"

#include "pinocchio/serialization/serializable.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/multibody/fwd.hxx"
#include "pinocchio/src/multibody/force-set.hxx"
#include "pinocchio/src/multibody/model-item.hxx"
#include "pinocchio/src/multibody/frame.hxx"
#include "pinocchio/src/multibody/model.hxx"
#include "pinocchio/src/multibody/data.hxx"
// IWYU pragma: end_exports

// IWYU pragma: begin_keep
#include "pinocchio/algorithm/joint-configuration.hpp"
// IWYU pragma: end_keep
