//
// Copyright (c) 2016-2024 CNRS INRIA
//

#pragma once

#include "pinocchio/bindings/python/fwd.hpp"

#include <eigenpy/std-vector.hpp>
#include <eigenpy/pickle-vector.hpp>

#include <boost/python/def_visitor.hpp>

#include <type_traits>

namespace eigenpy
{

  template<typename Derived>
  struct has_operator_equal<
    Derived,
    std::enable_if_t<std::is_base_of_v<::pinocchio::NumericalBase<Derived>, Derived>, Derived>>
  : has_operator_equal<typename ::pinocchio::NumericalBase<Derived>::Scalar>
  {
  };

  template<typename _Scalar, int _Rows, int _Cols, int _Options>
  struct has_operator_equal<Eigen::Matrix<_Scalar, _Rows, _Cols, _Options>>
  : has_operator_equal<typename Eigen::Matrix<_Scalar, _Rows, _Cols, _Options>::Scalar>
  {
  };

} // namespace eigenpy

namespace pinocchio
{
  namespace python
  {
    template<
      class vector_type,
      bool NoProxy = false,
      bool EnableFromPythonListConverter = true,
#ifdef PINOCCHIO_PYTHON_NO_SERIALIZATION
      bool pickable = false>
#else
      bool pickable = true>
#endif
    using StdVectorPythonVisitor = eigenpy::
      StdVectorPythonVisitor<vector_type, NoProxy, EnableFromPythonListConverter, pickable>;

    template<typename MatType>
    void exposeStdVectorEigenSpecificType(const char * name)
    {
      typedef std::vector<MatType> VecMatType;
      std::string full_name = "StdVec_";
      full_name += name;
      StdVectorPythonVisitor<VecMatType>::expose(
        full_name.c_str(), eigenpy::details::overload_base_get_item_for_std_vector<VecMatType>());
    }

    // Visitor to add default pickling method to an already registerd StdVector.
    // Since eigenpy::registration_class doesn't support adding attribute, we must set
    // __safe_for_unpickling__ and __getstate_manages_dict__ to true in the __init__.py.
    // This visitor is intended to be use as eigenpy::register_symbolic_link_to_registered_type
    // argument.
    template<typename vector_type>
    struct DefPickleStdVectorVisitor
    : public boost::python::def_visitor<DefPickleStdVectorVisitor<vector_type>>
    {
      template<class PyClass>
      void visit(PyClass & cl) const
      {
#ifndef PINOCCHIO_PYTHON_NO_SERIALIZATION
        typedef eigenpy::PickleVector<vector_type> PickleSuiteType;
        cl.def("__getinitargs__", &PickleSuiteType::getinitargs);
        cl.def("__getstate__", &PickleSuiteType::getstate);
        cl.def("__setstate__", &PickleSuiteType::setstate);
#else
        PINOCCHIO_UNUSED_VARIABLE(cl);
#endif
      }
    };
  } // namespace python
} // namespace pinocchio
