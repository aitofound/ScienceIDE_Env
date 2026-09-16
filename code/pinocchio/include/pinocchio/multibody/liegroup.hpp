//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <cmath>
#include <cassert>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>
#include <sstream>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <boost/blank.hpp>
#include <boost/variant.hpp>
#include <boost/core/ref.hpp>
#include <boost/preprocessor.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/integer/static_min_max.hpp>

#include "pinocchio/context.hpp"
#include "pinocchio/fwd.hpp"
#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/utils/check.hpp"
#include "pinocchio/utils/static-if.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody/joint.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/multibody/liegroup/fwd.hxx"

#include "pinocchio/src/multibody/liegroup/liegroup-base.hxx"
#include "pinocchio/src/multibody/liegroup/liegroup-algo.hxx"

#include "pinocchio/src/multibody/liegroup/vector-space.hxx"
#include "pinocchio/src/multibody/liegroup/cartesian-product.hxx"
#include "pinocchio/src/multibody/liegroup/special-orthogonal.hxx"
#include "pinocchio/src/multibody/liegroup/special-euclidean.hxx"

#include "pinocchio/src/multibody/liegroup/liegroup-collection.hxx"

#include "pinocchio/src/multibody/liegroup/cartesian-product-variant-fwd.hxx"
#include "pinocchio/src/multibody/liegroup/liegroup-variant-visitors.hxx"
#include "pinocchio/src/multibody/liegroup/cartesian-product-variant.hxx"

#include "pinocchio/src/multibody/liegroup/liegroup-generic.hxx"
#include "pinocchio/src/multibody/liegroup/liegroup-map.hxx"
#include "pinocchio/src/multibody/liegroup/liegroup-joint.hxx"
// IWYU pragma: end_exports
