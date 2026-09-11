//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep

#include <cstddef>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <iostream>

#include <Eigen/Core>

#include <boost/math/constants/constants.hpp>
#include <boost/none.hpp>
#include <boost/none_t.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/variant.hpp>
#include <boost/logic/tribool.hpp>

#include "pinocchio/unsupported.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"

#include "pinocchio/geometry.hpp"

#include "pinocchio/constraints.hpp"

#include "pinocchio/parsers/urdf.hpp"
#include "pinocchio/src/parsers/meshloader-fwd.hxx"
#include "pinocchio/parsers/config.hpp"
#include "pinocchio/src/parsers/scalar-model.hxx"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/parsers/mjcf/mjcf.hxx"
#include "pinocchio/src/parsers/mjcf/mjcf-graph.hxx"
#include "pinocchio/src/parsers/mjcf/model.hxx"
#include "pinocchio/src/parsers/mjcf/geometry.hxx"
// IWYU pragma: end_exports
