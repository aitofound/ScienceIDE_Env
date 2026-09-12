//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cstddef>

#include <boost/variant/get.hpp>

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

  /// Check the validity of data wrt to model, in particular if model has been modified.
  ///
  /// \param[in] model reference model
  /// \param[in] data corresponding data
  ///
  /// \returns True if data is valid wrt model.
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  bool checkData(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const DataTpl<Scalar, Options, JointCollectionTpl> & data);

} // namespace pinocchio

/* --- Details -------------------------------------------------------------------- */
// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/check-data.hxx"
// IWYU pragma: end_exports
