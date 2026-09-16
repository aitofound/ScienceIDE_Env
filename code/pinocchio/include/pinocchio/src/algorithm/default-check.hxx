//
// Copyright (c) 2016-2018 CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/default-check.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/default-check.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  /// Default checker-list, used as the default argument in Model::check().
  inline AlgorithmCheckerList<ParentChecker, CRBAChecker, ABAChecker> makeDefaultCheckerList()
  {
    return makeAlgoCheckerList(ParentChecker(), CRBAChecker(), ABAChecker());
  }

} // namespace pinocchio
