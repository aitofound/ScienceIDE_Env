//
// Copyright (c) 2022-2025 INRIA
//

#pragma once

#include <eigenpy/copyable.hpp>

#include "pinocchio/constraints.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/set-base.hpp"
#include "pinocchio/bindings/python/utils/cast.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<typename CoulombFrictionCone>
    struct CoulombFrictionConePythonVisitor
    : public boost::python::def_visitor<CoulombFrictionConePythonVisitor<CoulombFrictionCone>>
    {
      typedef typename CoulombFrictionCone::Scalar Scalar;
      typedef CoulombFrictionCone Self;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(
            bp::init<const Scalar &>(
              bp::args("self", "mu"), "Constructor from a given friction coefficient"))
          .def(bp::init<const Self &>(bp::args("self", "other"), "Copy constructor"))
          .add_property(
            "mu", bp::make_function(+[](Self & self) -> Scalar { return self.mu; }),
            "Friction coefficient.")
          .def(
            "weightedProject",
            &Self::template weightedProject<context::Vector3s, context::Vector3s>,
            bp::args("self", "f", "R"), "Weighted projection of a vector f onto the cone.")
          .def(
            "computeNormalCorrection", &Self::template computeNormalCorrection<context::Vector3s>,
            bp::args("self", "v"),
            "Compute the complementary shift associted to the Coulomb friction cone for "
            "complementarity satisfaction in complementary problems.")
          .def(
            "computeRadialProjection", &Self::template computeRadialProjection<context::Vector3s>,
            bp::args("self", "f"),
            "Compute the radial projection associted to the Coulomb friction cone.");
      }

      static void expose()
      {
        bp::class_<CoulombFrictionCone>(
          "CoulombFrictionCone", "3d Coulomb friction cone.\n", bp::no_init)
          .def(SetPythonVisitor<CoulombFrictionCone, context::Vector3s>())
          .def(ConeSetPythonVisitor<CoulombFrictionCone>())
          .def(CoulombFrictionConePythonVisitor())
          // .def(CastVisitor<CoulombFrictionCone>())
          // .def(ExposeConstructorByCastVisitor<CoulombFrictionCone,::pinocchio::CoulombFrictionCone>())
          .def(::eigenpy::CopyableVisitor<CoulombFrictionCone>());
      }
    };

    template<typename DualCoulombFrictionCone>
    struct DualCoulombFrictionConePythonVisitor
    : public boost::python::def_visitor<
        DualCoulombFrictionConePythonVisitor<DualCoulombFrictionCone>>
    {
      typedef typename DualCoulombFrictionCone::Scalar Scalar;
      typedef DualCoulombFrictionCone Self;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(
            bp::init<const Scalar &>(
              bp::args("self", "mu"), "Constructor from a given friction coefficient"))
          .def(bp::init<const Self &>(bp::args("self", "other"), "Copy constructor"))
          .add_property(
            "mu", bp::make_function(+[](Self & self) -> Scalar { return self.mu; }),
            "Friction coefficient.");
      }

      static void expose()
      {
        bp::class_<DualCoulombFrictionCone>(
          "DualCoulombFrictionCone", "Dual cone of the 3d Coulomb friction cone.\n", bp::no_init)
          .def(SetPythonVisitor<DualCoulombFrictionCone, context::Vector3s>())
          .def(ConeSetPythonVisitor<DualCoulombFrictionCone>())
          .def(DualCoulombFrictionConePythonVisitor())
          // .def(CastVisitor<DualCoulombFrictionCone>())
          // .def(ExposeConstructorByCastVisitor<DualCoulombFrictionCone,::pinocchio::CoulombFrictionCone>())
          .def(::eigenpy::CopyableVisitor<DualCoulombFrictionCone>());
      }
    };
  } // namespace python
} // namespace pinocchio
