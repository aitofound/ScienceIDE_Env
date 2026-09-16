//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <boost/optional.hpp>
#include <boost/none.hpp>

#include <stdexcept>
#include <functional>
#include <utility>
#include <vector>

#include "pinocchio/src/context.hxx"

#include "pinocchio/macros.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/geometry.hpp"

#include "pinocchio/visualizers/config.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  namespace visualizers
  {
#define PINOCCHIO_VISUALIZER_TYPEDEF_GENERIC(Scalar, Options)                                      \
  typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;                              \
  typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;                 \
  typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor | Options>         \
    RowMatrixXs;                                                                                   \
  typedef Eigen::Matrix<Scalar, 3, 1, Options> Vector3;                                            \
                                                                                                   \
  typedef ModelTpl<Scalar, Options> Model;                                                         \
  typedef DataTpl<Scalar, Options> Data;                                                           \
                                                                                                   \
  typedef SE3Tpl<Scalar, Options> SE3;
  } // namespace visualizers

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/visualizers/base-visualizer.hxx"
// IWYU pragma: end_exports
