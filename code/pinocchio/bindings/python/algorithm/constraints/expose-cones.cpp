//
// Copyright (c) 2022-2024 INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/set-coulomb-friction-cone.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/set-box-set.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/set-trivial-cones.hpp"

namespace pinocchio
{
  namespace python
  {
    void exposeCones()
    {
      CoulombFrictionConePythonVisitor<context::CoulombFrictionCone>::expose();
      DualCoulombFrictionConePythonVisitor<context::DualCoulombFrictionCone>::expose();
      BoxSetPythonVisitor<context::BoxSet>::expose();
      TrivialConePythonVisitor<context::ZeroCone>::expose(
        "ZeroCone", "Set reduce to 0 singleton in R^d.");
      TrivialConePythonVisitor<context::FullSpaceCone>::expose("FullSpaceCone", "Set R^d.");
      TrivialConePythonVisitor<context::NonNegativeOrthantCone>::expose(
        "NonNegativeOrthantCone", "Set R_+^d.");
    }
  } // namespace python
} // namespace pinocchio
