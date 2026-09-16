//
// Copyright (c) 2020 INRIA
//

#pragma once

#include <boost/python.hpp>

namespace pinocchio
{
  namespace python
  {

    namespace bp = boost::python;

    ///
    /// \brief Add the Python method == and != to allow a comparison of this.
    ///
    template<class C, bool has_comparison_operators = true>
    struct ComparableVisitor
    : public bp::def_visitor<ComparableVisitor<C, has_comparison_operators>>
    {
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(bp::self == bp::self).def(bp::self != bp::self);
      }
    };

    template<class C>
    struct ComparableVisitor<C, false> : public bp::def_visitor<ComparableVisitor<C, false>>
    {
      template<class PyClass>
      void visit(PyClass &) const
      {
      }
    };
  } // namespace python
} // namespace pinocchio
