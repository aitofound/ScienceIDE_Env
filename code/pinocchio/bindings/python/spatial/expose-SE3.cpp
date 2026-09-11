//
// Copyright (c) 2015-2021 CNRS INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/spatial/se3.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeSE3()
    {
      SE3PythonVisitor<context::SE3>::expose();
      StdVectorPythonVisitor<std::vector<context::SE3>>::expose("StdVec_SE3");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::SE3>>();
#endif
    }

  } // namespace python
} // namespace pinocchio
