//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep

#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/foreach.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/fusion/container/vector.hpp>

#include <Eigen/Core>

#include "pinocchio/macros.hpp"

#include "pinocchio/math.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/geometry.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/parsers/srdf/srdf.hxx"
// IWYU pragma: end_exports
