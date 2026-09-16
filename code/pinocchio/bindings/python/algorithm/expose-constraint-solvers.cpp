//
// Copyright (c) 2022-2024 INRIA
//

#include "pinocchio/bindings/python/algorithm/algorithms.hpp"

namespace pinocchio
{
  namespace python
  {

    // Forward declarations
    void exposePGSConstraintSolver();
    void exposeADMMConstraintSolver();

    void exposeConstraintSolvers()
    {
      exposePGSConstraintSolver();
      exposeADMMConstraintSolver();
    }

  } // namespace python
} // namespace pinocchio
