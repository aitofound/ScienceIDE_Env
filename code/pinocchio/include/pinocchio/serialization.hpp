//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <new>
#include <cstddef>
#include <vector>
#include <fstream>
#include <locale>
#include <string>
#include <sstream>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <iterator>
#include <limits>

#include <boost/version.hpp>
#if BOOST_VERSION / 100 % 1000 == 78 && __APPLE__
// See https://github.com/qcscine/utilities/issues/5#issuecomment-1246897049 for further details

  #ifndef BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC
    #define DEFINE_BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC
    #define BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC
  #endif

  #include <boost/asio/streambuf.hpp>

  #ifdef DEFINE_BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC
    #undef BOOST_ASIO_DISABLE_STD_ALIGNED_ALLOC
  #endif

#else
  #include <boost/asio/streambuf.hpp>
#endif
#include <boost/archive/basic_archive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/array_wrapper.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/throw_exception.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/math/special_functions/nonfinite_num_facets.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/blank.hpp>

#include <Eigen/Core>

#ifdef PINOCCHIO_WITH_COLLISION
  #define COAL_SKIP_EIGEN_BOOST_SERIALIZATION
  #include <coal/serialization/collision_data.h>
  #undef COAL_SKIP_EIGEN_BOOST_SERIALIZATION
  #include <coal/serialization/geometric_shapes.h>
  #include <coal/serialization/hfield.h>
  #include <coal/serialization/octree.h>
  #include <coal/serialization/convex.h>
  #include <coal/serialization/BVH_model.h>
#endif // PINOCCHIO_WITH_COLLISION

#include "pinocchio/macros.hpp"
// TODO This is done for pinocchio 3 back compatibility
#include <pinocchio/config.hpp>

#include "pinocchio/container/double-entry-container.hpp"
#include "pinocchio/container/matrix-stack.hpp"
#include "pinocchio/src/container/eigen-storage.hxx"

#include "pinocchio/math.hpp"
#include "pinocchio/spatial.hpp"
#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/geometry.hpp"

#include "pinocchio/constraints.hpp"
#include "pinocchio/algorithm/fwd.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/serialization/fwd.hxx"
#include "pinocchio/src/serialization/common.hxx"
#include "pinocchio/src/serialization/static-buffer.hxx"
#include "pinocchio/src/serialization/boost-blank.hxx"
#include "pinocchio/src/serialization/archive.hxx"
#include "pinocchio/src/serialization/serializable.hxx"
#include "pinocchio/src/serialization/vector.hxx"
#include "pinocchio/src/serialization/coal.hxx"
#include "pinocchio/src/serialization/csv.hxx"
#include "pinocchio/src/serialization/eigen.hxx"
#include "pinocchio/src/serialization/matrix-stack.hxx"
#include "pinocchio/src/serialization/double-entry-container.hxx"
#include "pinocchio/src/serialization/eigen-storage.hxx"
#include "pinocchio/src/serialization/symmetric3.hxx"
#include "pinocchio/src/serialization/se3.hxx"
#include "pinocchio/src/serialization/motion.hxx"
#include "pinocchio/src/serialization/force.hxx"
#include "pinocchio/src/serialization/inertia.hxx"
#include "pinocchio/src/serialization/joints-motion-subspace.hxx"
#include "pinocchio/src/serialization/joints-motion.hxx"
#include "pinocchio/src/serialization/joints-transform.hxx"
#include "pinocchio/src/serialization/joints-data.hxx"
#include "pinocchio/src/serialization/joints-model.hxx"
#include "pinocchio/src/serialization/frame.hxx"
#include "pinocchio/src/serialization/model.hxx"
#include "pinocchio/src/serialization/data.hxx"
#include "pinocchio/src/serialization/matrix-block-element.hxx"
#include "pinocchio/src/serialization/block-diagonal-matrix.hxx"
#include "pinocchio/src/serialization/constraints-data.hxx"
#include "pinocchio/src/serialization/constraints-model.hxx"
#include "pinocchio/src/serialization/constraints-set.hxx"
#include "pinocchio/src/serialization/delassus.hxx"
#include "pinocchio/src/serialization/geometry.hxx"
// IWYU pragma: end_exports
