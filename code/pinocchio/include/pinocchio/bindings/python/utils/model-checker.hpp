//
// Copyright (c) 2025-2026 INRIA
//

#pragma once

#include <Python.h>
#include <string>

#include "pinocchio/algorithm/check-model.hpp"
#include "pinocchio/bindings/python/fwd.hpp"

namespace pinocchio
{
  namespace python
  {

    // Checker Policy to make a safe version of python function, concerning joint mimic.
    // This will throw a runtime error in case algorithm does not allow joint mimic in the model.
    template<class Policy = boost::python::default_call_policies>
    struct mimic_not_supported_function : Policy
    {
      mimic_not_supported_function(size_t model_arg_list_idx_)
      : Policy()
      , model_arg_list_idx(model_arg_list_idx_)
      {
      }

      template<class ArgumentPackage>
      bool precall(ArgumentPackage const & args) const
      {
        // Convert the object to a tuple
        boost::python::tuple py_args = boost::python::extract<boost::python::tuple>(args);

        boost::python::extract<const context::Model &> get_model(py_args[model_arg_list_idx]);
        if (!get_model().check(MimicChecker()))
        {
          PyErr_SetString(PyExc_RuntimeError, m_error_message.c_str());
          return false;
        }
        else
          return static_cast<const Policy *>(this)->precall(args);
      }

    protected:
      static const std::string m_error_message;
      // index of the pinocchio in the function arguments. Need it to avoid a loop
      const size_t model_arg_list_idx;
    };

    template<class Policy>
    const std::string mimic_not_supported_function<Policy>::m_error_message =
      "This algorithm does not support Joint Mimic type in the model.";
  } // namespace python
} // namespace pinocchio
