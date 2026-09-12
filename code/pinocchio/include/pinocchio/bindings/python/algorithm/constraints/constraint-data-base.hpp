//
// Copyright (c) 2025 INRIA
//

#pragma once

#include <boost/python.hpp>
#include <eigenpy/exception.hpp>
#include <eigenpy/eigen-to-python.hpp>

#include "pinocchio/constraints.hpp"
#include "pinocchio/bindings/python/fwd.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<class ConstraintDataDerived>
    struct ConstraintDataBasePythonVisitor
    : public bp::def_visitor<ConstraintDataBasePythonVisitor<ConstraintDataDerived>>
    {
      typedef ConstraintDataDerived Self;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("classname", &Self::classname)
          .staticmethod("classname")
          .def("shortname", &Self::shortname, "Short name of the class.")
#ifndef PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
          .def(bp::self == bp::self)
          .def(bp::self != bp::self)
#endif
          ;
      }
    };
  } // namespace python
} // namespace pinocchio
