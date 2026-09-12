//
// Copyright (c) 2015-2021 CNRS INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/spatial/motion.hpp"
#include "pinocchio/bindings/python/spatial/classic-acceleration.hpp"
#include "pinocchio/bindings/python/serialization/serialization.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"

namespace pinocchio
{
  namespace python
  {

    void exposeMotion()
    {
      exposeClassicAcceleration();
      MotionPythonVisitor<context::Motion>::expose();
      StdVectorPythonVisitor<std::vector<context::Motion>>::expose("StdVec_Motion");
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
      serialize<std::vector<context::Motion>>();
#endif
    }

  } // namespace python
} // namespace pinocchio
