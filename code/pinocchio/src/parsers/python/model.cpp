//
// Copyright (c) 2016-2023 CNRS INRIA
//

#include "pinocchio/parsers/python.hpp"

#include <boost/version.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/python.hpp>

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    Model buildModel(const std::string & filename, const std::string & model_name)
    {
      Py_Initialize();

      bp::object main_module = bp::import("__main__");
      // Get a dict for the global namespace to exec further python code with
      bp::dict globals = bp::extract<bp::dict>(main_module.attr("__dict__"));

      // That's it, you can exec your python script, starting with a model you
      // can update as you want.
      try
      {
        bp::exec_file((bp::str)filename, globals);
      }
      catch (bp::error_already_set & e)
      {
        PyErr_PrintEx(0);
      }

      Model model;
      try
      {
        bp::object obj_model = globals[model_name];
        model = bp::extract<Model>(obj_model);
      }
      catch (bp::error_already_set & e)
      {
        PyErr_PrintEx(0);
      }

      // close the interpreter
      // cf. https://github.com/numpy/numpy/issues/8097
      PyObject * poMainModule = PyImport_AddModule("__main__");
      PyObject * poAttrList = PyObject_Dir(poMainModule);
      PyObject * poAttrIter = PyObject_GetIter(poAttrList);
      PyObject * poAttrName;

      while ((poAttrName = PyIter_Next(poAttrIter)) != NULL)
      {
        std::string oAttrName((bp::extract<char const *>(poAttrName)));

        // Make sure we don't delete any private objects.
        if (!boost::starts_with(oAttrName, "__") || !boost::ends_with(oAttrName, "__"))
        {
          PyObject * poAttr = PyObject_GetAttr(poMainModule, poAttrName);

          // Make sure we don't delete any module objects.
          if (poAttr && poAttr->ob_type != poMainModule->ob_type)
            PyObject_SetAttr(poMainModule, poAttrName, NULL);
          Py_DecRef(poAttr);
        }
        Py_DecRef(poAttrName);
      }
      Py_DecRef(poAttrIter);
      Py_DecRef(poAttrList);
      return model;
    }
  } // namespace python
} // namespace pinocchio
