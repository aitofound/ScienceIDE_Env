//
// Copyright (c) 2024 CNRS INRIA
//

#pragma once

#include "pinocchio/bindings/python/fwd.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

#if defined(PINOCCHIO_WITH_EXTRA_SUPPORT)
    void exposeReachableWorkspace();
#endif // defined(PINOCCHIO_WITH_EXTRA_SUPPORT)

    void exposeExtras();

  } // namespace python
} // namespace pinocchio
