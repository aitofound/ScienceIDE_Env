//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <sstream>
#include <limits>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <stdexcept>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <boost/none.hpp>
#include <boost/none_t.hpp>
#include <boost/optional.hpp>

#include <sdf/sdf.hh>
#include <gz/math.hh>

#include "pinocchio/unsupported.hpp"

#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"
#include "pinocchio/multibody.hpp"

#include "pinocchio/utils/check.hpp"

#include "pinocchio/constraints.hpp"

#include "pinocchio/parsers/urdf.hpp"
#include "pinocchio/parsers/config.hpp"
#include "pinocchio/src/parsers/meshloader-fwd.hxx"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/parsers/sdf/sdf.hxx"
#include "pinocchio/src/parsers/sdf/model.hxx"
#include "pinocchio/src/parsers/sdf/geometry.hxx"
// IWYU pragma: end_exports
