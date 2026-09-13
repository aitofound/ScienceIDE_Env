//
// Copyright (c) 2024-2026 INRIA
//

#pragma once

#include <type_traits>

#include <eigenpy/memory.hpp>

#include "pinocchio/algorithm/delassus-operator.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    namespace details
    {
      /// Check if matrix method take one or two parameters
      template<typename, typename = void>
      constexpr bool is_matrix_bool_bool_method = false;

      template<typename T>
      constexpr bool
        is_matrix_bool_bool_method<T, std::void_t<decltype(std::declval<T>().matrix(true, true))>> =
          true;
    } // namespace details

    template<typename DelassusOperator>
    struct DelassusOperatorBasePythonVisitor
    : public boost::python::def_visitor<DelassusOperatorBasePythonVisitor<DelassusOperator>>
    {
      typedef DelassusOperator Self;
      typedef typename DelassusOperator::Scalar Scalar;
      typedef context::MatrixXs MatrixXs;
      typedef context::VectorXs VectorXs;
      typedef typename traits<DelassusOperator>::getDampingReturnType getDampingReturnType;

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(bp::self * bp::other<MatrixXs>())
          .def(
            "__matmul__",
            +[](const DelassusOperator & self, const MatrixXs & other) -> MatrixXs {
              return MatrixXs(self * other);
            },
            bp::args("self", "other"),
            "Matrix multiplication between self and another matrix. Returns the result of Delassus "
            "* matrix.")

          .def(
            "solve", &DelassusOperator::template solve<MatrixXs>, bp::args("self", "mat"),
            "Returns the solution x of Delassus * x = mat using the current decomposition of the "
            "Delassus matrix.")

          .def(
            "updateCompliance",
            (void (DelassusOperator::*)(const Scalar))&DelassusOperator::updateCompliance,
            bp::args("self", "mu"),
            "Add a compliance term to the diagonal of the Delassus matrix. The compliance term "
            "should be "
            "positive.")
          .def(
            "updateCompliance", &DelassusOperator::template updateCompliance<VectorXs>,
            bp::args("self", "mus"),
            "Add a compliance term to the diagonal of the Delassus matrix. The compliance terms "
            "should "
            "be all positive.")

          .def(
            "getCompliance",
            +[](const DelassusOperator & self) -> VectorXs { return self.getCompliance(); },
            bp::arg("self"),
            "Returns the value of the compliance terms contained in the Delassus operator")

          .def(
            "updateDamping",
            (void (DelassusOperator::*)(const Scalar))&DelassusOperator::updateDamping,
            bp::args("self", "mu"),
            "Add a damping term to the diagonal of the Delassus matrix. The damping term should be "
            "positive.")
          .def(
            "updateDamping", &DelassusOperator::template updateDamping<VectorXs>,
            bp::args("self", "mus"),
            "Add a damping term to the diagonal of the Delassus matrix. The damping terms should "
            "be all positive.")

          .def(
            "getDamping",
            +[](const DelassusOperator & self) -> std::remove_reference_t<getDampingReturnType> {
              return self.getDamping();
            },
            bp::arg("self"),
            "Returns the value of the damping terms contained in the Delassus operator")
          .def(
            "inverse", &DelassusOperator::inverse, bp::arg("self"),
            "Returns the inverse of the Delassus expression as a dense matrix.")

          .def(
            "size", &DelassusOperator::size, bp::arg("self"),
            "Returns the size of the decomposition.")
          .def("rows", &DelassusOperator::rows, bp::arg("self"), "Returns the number of rows.")
          .def("cols", &DelassusOperator::cols, bp::arg("self"), "Returns the number of columns.");
        if constexpr (details::is_matrix_bool_bool_method<DelassusOperator>)
        {
          cl.def(
            "matrix",
            static_cast<MatrixXs (DelassusOperator::*)(bool, bool) const>(
              &DelassusOperator::matrix),
            (bp::arg("self"), bp::arg("enforce_symmetry") = false, bp::arg("with_damping") = true),
            "Returns the Delassus expression as a dense matrix.");
        }
      }
    };

  } // namespace python
} // namespace pinocchio
