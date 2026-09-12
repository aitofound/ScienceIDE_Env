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

    template<typename TrivialCone>
    struct TrivialConePythonVisitor
    : public boost::python::def_visitor<TrivialConePythonVisitor<TrivialCone>>
    {

      static void expose(const std::string & class_name, const std::string & doc_string = "")
      {
        bp::class_<TrivialCone>(class_name.c_str(), doc_string.c_str(), bp::no_init)
          .def(SetPythonVisitor<TrivialCone, context::VectorXs>())
          .def(ConeSetPythonVisitor<TrivialCone>())
          .def(::eigenpy::CopyableVisitor<TrivialCone>());
      }
    };

  } // namespace python
} // namespace pinocchio
