//
// Copyright (c) 2016-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include "pinocchio/macros.hpp"
//
#include "pinocchio/multibody.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief Copy part of the data from \c origin to \c dest. Template parameter can be
  /// used to select at which differential level the copy should occur.
  ///
  /// \tparam JointCollection Collection of Joint types.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] origin  Data from which the values are copied.
  /// \param[out] dest  Data to which the values are copied
  /// \param[in] kinematic_level if =0, copy oMi. If =1, also copy v. If =2, also copy a, a_gf and
  /// f.
  ///
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  inline void copy(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const DataTpl<Scalar, Options, JointCollectionTpl> & origin,
    DataTpl<Scalar, Options, JointCollectionTpl> & dest,
    KinematicLevel kinematic_level);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/copy.hxx"
// IWYU pragma: end_exports
