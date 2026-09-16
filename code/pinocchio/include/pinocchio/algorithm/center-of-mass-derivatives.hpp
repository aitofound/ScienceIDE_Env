//
// Copyright (c) 2019 CNRS
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <vector>

#include <boost/fusion/container/vector.hpp>
//
#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"
//
#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/visitor.hpp"

#include "pinocchio/algorithm/check.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief Computes the partial derivatie of the center-of-mass velocity with respect to
  ///        the joint configuration q.
  ///        You must first call computeAllTerms(model,data,q,v) or
  ///        computeCenterOfMass(model,data,q,v) before calling this function.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam Matrix3xOut Matrix3x containing the partial derivatives of the CoM velocity with
  /// respect to the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[out] vcom_partial_dq Partial derivative of the CoM velocity w.r.t. \f$ q \f$.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename Matrix3xOut>
  void getCenterOfMassVelocityDerivatives(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<Matrix3xOut> & vcom_partial_dq);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/center-of-mass-derivatives.hxx"
// IWYU pragma: end_exports
