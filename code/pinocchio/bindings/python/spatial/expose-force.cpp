//
// Copyright (c) 2015-2021 CNRS INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/spatial/force.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeForce()
    {
      ForcePythonVisitor<context::Force>::expose();
      StdVectorPythonVisitor<std::vector<context::Force>>::expose("StdVec_Force");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::Force>>();
#endif
    }

  } // namespace python
} // namespace pinocchio
