//
// Copyright (c) 2015-2021 CNRS INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/multibody/frame.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeFrame()
    {
      FramePythonVisitor<context::Frame>::expose();
      StdVectorPythonVisitor<std::vector<context::Frame>>::expose("StdVec_Frame");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::Frame>>();
#endif
    }

  } // namespace python
} // namespace pinocchio
