//
// Copyright (c) 2016-2018 CNRS
//

#pragma once

// IWYU pragma: begin_keep
#include <pinocchio/algorithm/check.hpp>
#include <pinocchio/algorithm/aba.hpp>
#include <pinocchio/algorithm/crba.hpp>
// IWYU pragma: end_keep

namespace pinocchio
{
  /// Default checker-list, used as the default argument in Model::check().
  inline AlgorithmCheckerList<ParentChecker, CRBAChecker, ABAChecker> makeDefaultCheckerList();

#define DEFAULT_CHECKERS makeDefaultCheckerList()

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/default-check.hxx"
// IWYU pragma: end_exports
