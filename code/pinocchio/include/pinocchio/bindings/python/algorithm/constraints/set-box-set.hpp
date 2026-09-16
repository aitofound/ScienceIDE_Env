//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

#include <eigenpy/eigenpy.hpp>
#include <eigenpy/copyable.hpp>

#include "pinocchio/constraints.hpp"

#include "pinocchio/bindings/python/algorithm/constraints/set-base.hpp"
#include "pinocchio/bindings/python/utils/cast.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<typename BoxSet>
    struct BoxSetPythonVisitor : public boost::python::def_visitor<BoxSetPythonVisitor<BoxSet>>
    {
      typedef typename BoxSet::Scalar Scalar;
      typedef typename BoxSet::Vector Vector;
      typedef BoxSet Self;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(bp::init<const Self &>(bp::args("self", "other"), "Copy constructor."))
          .def(
            bp::init<context::VectorXs, context::VectorXs>(
              bp::args("self", "lb", "ub"), "Constructor from lower and upper bounds."))
          .add_property(
            "lb", bp::make_function(+[](Self & self) { return self.lb; }),
            "Returns a copy of the vector of lower bounds")
          .add_property(
            "ub", bp::make_function(+[](Self & self) { return self.ub; }),
            "Returns a copy of the vector of upper bounds");
      }

      static void expose()
      {
        bp::class_<BoxSet>(
          "BoxSet", "Box set defined by a lower and an upper bounds [lb;ub].\n", bp::no_init)
          .def(SetPythonVisitor<BoxSet, context::VectorXs>())
          .def(BoxSetPythonVisitor())
          // .def(CastVisitor<BoxSet>())
          // .def(ExposeConstructorByCastVisitor<BoxSet,::pinocchio::BoxSet>())
          .def(::eigenpy::CopyableVisitor<BoxSet>());
      }
    };

  } // namespace python
} // namespace pinocchio
