//
// Copyright (c) 2018 CNRS
//

#pragma once

#include <boost/python.hpp>
#include <boost/python/scope.hpp>

namespace boost
{
  namespace python
  {

    ///
    /// \brief Define a constant given its value and a name within the current Boost Python scope.
    ///
    /// \tparam T Type of the constant.
    ///
    /// \param[in] name Name of the constant.
    /// \param[in] value Value of the constant.
    ///
    template<typename T>
    void def_constant(const char * name, const T & value)
    {
      namespace bp = boost::python;
      bp::scope().attr(name) = value;
    }

  } // namespace python
} // namespace boost
