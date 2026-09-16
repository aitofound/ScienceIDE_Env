//
// Copyright (c) INRIA 2026
//
#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/SVD>
#include <unsupported/Eigen/CXX11/Tensor>

#include <cassert>
#include <cmath>
#include <limits>
#include <cstddef>
#include <type_traits>
#include <vector>

#include <boost/type_traits.hpp>
#include <boost/math/constants/constants.hpp>

#include "pinocchio/context.hpp"
#include "pinocchio/multibody/fwd.hpp"
// TODO This is done for pinocchio 3 back compatibility
#include <pinocchio/config.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/container/matrix-stack.hpp"

#include "pinocchio/utils/alloca.hpp"
#include "pinocchio/utils/size-in-bytes.hpp"
#include "pinocchio/utils/eigen-helpers.hpp"
#include "pinocchio/utils/static-if.hpp"
#include "pinocchio/utils/check.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/math/fwd.hxx"
#include "pinocchio/src/math/eigen-helpers.hxx"
#include "pinocchio/src/math/comparison-operators.hxx"
#include "pinocchio/src/math/assign-operators.hxx"
#include "pinocchio/src/math/alias.hxx"
#include "pinocchio/src/math/taylor-expansion.hxx"
#include "pinocchio/src/math/matrix.hxx"
#include "pinocchio/src/math/sincos.hxx"
#include "pinocchio/src/math/rpy.hxx"
#include "pinocchio/src/math/rotation.hxx"
#include "pinocchio/src/math/quaternion.hxx"
#include "pinocchio/src/math/matrix-info.hxx"
#include "pinocchio/src/math/triangular-matrix.hxx"
#include "pinocchio/src/math/matrix-block.hxx"
#include "pinocchio/src/math/tensor.hxx"
#include "pinocchio/src/math/eigenvalues.hxx"
#include "pinocchio/src/math/gram-schmidt-orthonormalisation.hxx"
#include "pinocchio/src/math/sign.hxx"
#include "pinocchio/src/math/eigenvalues-tridiagonal-matrix.hxx"
#include "pinocchio/src/math/tridiagonal-matrix.hxx"
#include "pinocchio/src/math/lanczos-decomposition.hxx"
#include "pinocchio/src/math/arithmetic-operators.hxx"

#include "pinocchio/src/math/matrix-inverse-code-generated.hxx"
#include "pinocchio/src/math/details/matrix-inverse-1x1.hxx"
#include "pinocchio/src/math/details/matrix-inverse-2x2.hxx"
#include "pinocchio/src/math/details/matrix-inverse-3x3.hxx"
#include "pinocchio/src/math/details/matrix-inverse-4x4.hxx"
#include "pinocchio/src/math/details/matrix-inverse-5x5.hxx"
#include "pinocchio/src/math/details/matrix-inverse-6x6.hxx"
#include "pinocchio/src/math/details/matrix-inverse-7x7.hxx"
#include "pinocchio/src/math/details/matrix-inverse-8x8.hxx"
#include "pinocchio/src/math/details/matrix-inverse-9x9.hxx"
#include "pinocchio/src/math/details/matrix-inverse-10x10.hxx"
#include "pinocchio/src/math/details/matrix-inverse-11x11.hxx"
#include "pinocchio/src/math/details/matrix-inverse-12x12.hxx"
#include "pinocchio/src/math/matrix-inverse.hxx"
#include "pinocchio/src/math/matrix-product.hxx"

#include "pinocchio/src/math/matrix-block-type.hxx"
#include "pinocchio/src/math/matrix-block-element-operation.hxx"
#include "pinocchio/src/math/matrix-block-element-operations.hxx"
#include "pinocchio/src/math/matrix-block-element-base.hxx"
#include "pinocchio/src/math/matrix-block-element-plain.hxx"
#include "pinocchio/src/math/matrix-block-element.hxx"

#include "pinocchio/src/math/block-diagonal-matrix-base.hxx"
#include "pinocchio/src/math/block-diagonal-matrix-expression.hxx"
#include "pinocchio/src/math/block-diagonal-matrix-sum.hxx"
#include "pinocchio/src/math/block-diagonal-matrix-inverse.hxx"
#include "pinocchio/src/math/block-diagonal-matrix.hxx"
// IWYU pragma: end_exports
