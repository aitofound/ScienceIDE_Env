//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <fstream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/archive/basic_archive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/core/nvp.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/math/special_functions/nonfinite_num_facets.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/src/serialization/static-buffer.hxx"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/serialization/archive.hxx"
// IWYU pragma: end_exports
