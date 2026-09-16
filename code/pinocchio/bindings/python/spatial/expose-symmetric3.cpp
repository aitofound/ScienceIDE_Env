//
// Copyright (c) 2022 INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/spatial/symmetric3.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeSymmetric3()
    {
      Symmetric3PythonVisitor<context::Symmetric3>::expose();
      StdVectorPythonVisitor<std::vector<context::Symmetric3>>::expose("StdVec_Symmetric3");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::Symmetric3>>();
#endif // ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
    }

  } // namespace python
} // namespace pinocchio
