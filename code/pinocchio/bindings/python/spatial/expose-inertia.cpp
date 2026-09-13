//
// Copyright (c) 2015-2021 CNRS INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/spatial/inertia.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeInertia()
    {
      InertiaPythonVisitor<context::Inertia>::expose();
      PseudoInertiaPythonVisitor<context::PseudoInertia>::expose();
      LogCholeskyParametersPythonVisitor<context::LogCholeskyParameters>::expose();
      StdVectorPythonVisitor<std::vector<context::Inertia>>::expose("StdVec_Inertia");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::Inertia>>();
#endif
    }

  } // namespace python
} // namespace pinocchio
