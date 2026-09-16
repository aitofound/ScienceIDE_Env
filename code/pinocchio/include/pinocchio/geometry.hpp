//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <cstddef>
#include <limits>
#include <memory>
#include <ostream>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <map>

#include <Eigen/Core>

#include <boost/bind/bind.hpp>
#include <boost/variant.hpp>
#include <boost/foreach.hpp>

#include "pinocchio/src/fwd.hxx"
#include "pinocchio/deprecated.hpp"

#include "pinocchio/utils/shared-ptr.hpp"

#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody.hpp"

#include "pinocchio/serialization/serializable.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/geometry/fwd.hxx"
#include "pinocchio/src/geometry/coal.hxx"
#include "pinocchio/src/multibody/model-item.hxx"
#include "pinocchio/src/geometry/instance-filter.hxx"
#include "pinocchio/src/geometry/geometry-object.hxx"
#include "pinocchio/src/geometry/geometry.hxx"
#include "pinocchio/src/geometry/geometry-object-filter.hxx"
// IWYU pragma: end_exports
