//
// Copyright (c) INRIA 2026
//
#pragma once

/// Some consumers redefines pinocchio serialization and except
/// this header to no include the whole serialization module.

// IWYU pragma: begin_keep
#include <cstddef>
#include <stdexcept>
#include <array>

#include <boost/archive/basic_archive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/array_wrapper.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/throw_exception.hpp>

#include <Eigen/Core>
#include <unsupported/Eigen/CXX11/Tensor>
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/serialization/eigen.hxx"
// IWYU pragma: end_exports
