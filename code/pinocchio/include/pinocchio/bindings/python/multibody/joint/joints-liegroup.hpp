//
// Copyright (c) 2015-2021 CNRS INRIA
//

#pragma once

#include <boost/python.hpp>

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/multibody/joint.hpp"
#include "pinocchio/multibody/liegroup.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<class JointModelDerived>
    struct JointModelLieGroupPythonVisitor
    : public boost::python::def_visitor<JointModelLieGroupPythonVisitor<JointModelDerived>>
    {
    public:
      typedef
        typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
          LieGroupOperation;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("lieGroup", &lieGroup);
      }

      static LieGroupOperation lieGroup(const JointModelDerived & self)
      {
        return LieGroupOperation(self.template lieGroup<LieGroupMap>());
      }
    };

    template<>
    struct JointModelLieGroupPythonVisitor<context::JointModelComposite>
    : public boost::python::def_visitor<
        JointModelLieGroupPythonVisitor<context::JointModelComposite>>
    {
    public:
      typedef context::JointModelComposite Self;
      typedef
        typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
          LieGroupOperation;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("lieGroup", &lieGroup);
      }

      static LieGroupOperation lieGroup(const Self & self)
      {
        return self.template lieGroup<LieGroupMap>();
      }
    };

    template<>
    struct JointModelLieGroupPythonVisitor<context::JointModel>
    : public boost::python::def_visitor<JointModelLieGroupPythonVisitor<context::JointModel>>
    {
    public:
      typedef context::JointModel Self;
      typedef
        typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
          LieGroupOperation;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("lieGroup", &lieGroup);
      }

      static LieGroupOperation lieGroup(const Self & self)
      {
        return self.template lieGroup<LieGroupMap>();
      }
    };

    template<>
    struct JointModelLieGroupPythonVisitor<context::JointModelMimic>
    : public boost::python::def_visitor<JointModelLieGroupPythonVisitor<context::JointModelMimic>>
    {
    public:
      typedef context::JointModelMimic Self;
      typedef
        typename LieGroupMap::template operationProduct<context::Scalar, context::Options>::type
          LieGroupOperation;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("lieGroup", &lieGroup);
      }

      static LieGroupOperation lieGroup(const Self & self)
      {
        return self.template lieGroup<LieGroupMap>();
      }
    };

  } // namespace python
} // namespace pinocchio
