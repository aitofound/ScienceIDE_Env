//
// Copyright (c) 2024 INRIA
//

#pragma once

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/math.hpp"

#include <eigenpy/eigenpy.hpp>
#include <eigenpy/memory.hpp>

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<typename TridiagonalSymmetricMatrix>
    struct TridiagonalSymmetricMatrixPythonVisitor
    : public boost::python::def_visitor<
        TridiagonalSymmetricMatrixPythonVisitor<TridiagonalSymmetricMatrix>>
    {
      typedef typename TridiagonalSymmetricMatrix::Scalar Scalar;
      typedef typename TridiagonalSymmetricMatrix::CoeffVectorType CoeffVectorType;
      typedef typename TridiagonalSymmetricMatrix::PlainMatrixType PlainMatrixType;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        static const Scalar dummy_precision = Eigen::NumTraits<Scalar>::dummy_precision();

        cl.def(
            bp::init<Eigen::Index>(
              (bp::arg("self"), bp::arg("size")), "Default constructor from a given size."))
#ifndef PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
          .def(bp::self == bp::self)
          .def(bp::self != bp::self)
#endif
          .def(
            "diagonal",
            (CoeffVectorType & (TridiagonalSymmetricMatrix::*)())
              & TridiagonalSymmetricMatrix::diagonal,
            bp::arg("self"),
            "Reference of the diagonal elements of the symmetric tridiagonal matrix.",
            bp::return_internal_reference<>())
          .def(
            "subDiagonal",
            (CoeffVectorType & (TridiagonalSymmetricMatrix::*)())
              & TridiagonalSymmetricMatrix::subDiagonal,
            bp::arg("self"),
            "Reference of the sub diagonal elements of the symmetric tridiagonal matrix.",
            bp::return_internal_reference<>())

          .def(
            "setIdentity", &TridiagonalSymmetricMatrix::setIdentity, bp::arg("self"),
            "Set the current tridiagonal matrix to identity.")
          .def(
            "setZero", &TridiagonalSymmetricMatrix::setZero, bp::arg("self"),
            "Set the current tridiagonal matrix to zero.")
          .def(
            "setDiagonal", &TridiagonalSymmetricMatrix::template setDiagonal<CoeffVectorType>,
            bp::args("self", "diagonal"),
            "Set the current tridiagonal matrix to a diagonal matrix given by the entry vector "
            "diagonal.")
          .def(
            "setRandom", &TridiagonalSymmetricMatrix::setRandom, bp::arg("self"),
            "Set the current tridiagonal matrix to random.")
#ifndef PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
          .def(
            "isIdentity", &TridiagonalSymmetricMatrix::isIdentity,
            (bp::arg("self"), bp::arg("prec") = dummy_precision),
            "Returns true if *this is approximately equal to the identity matrix, within the "
            "precision given by prec.")
          .def(
            "isZero", &TridiagonalSymmetricMatrix::isZero,
            (bp::arg("self"), bp::arg("prec") = dummy_precision),
            "Returns true if *this is approximately equal to the zero matrix, within the "
            "precision given by prec.")
          .def(
            "isDiagonal", &TridiagonalSymmetricMatrix::isDiagonal,
            (bp::arg("self"), bp::arg("prec") = dummy_precision),
            "Returns true if *this is approximately equal to the a diagonal matrix, within the "
            "precision given by prec.")
#endif // PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
          .def("rows", &TridiagonalSymmetricMatrix::rows, bp::arg("self"))
          .def("cols", &TridiagonalSymmetricMatrix::cols, bp::arg("self"))
          .def("matrix", &TridiagonalSymmetricMatrix::matrix, bp::arg("self"))

          .def(
            "computeEigenvalue", &TridiagonalSymmetricMatrix::computeEigenvalue,
            (bp::arg("self"), bp::arg("eigenvalue_index"), bp::arg("eps") = 1e-8),
            "Computes the kth eigenvalue associated with the input tridiagonal matrix up to "
            "precision eps.")

          .def(
            "computeSpectrum", &TridiagonalSymmetricMatrix::computeSpectrum,
            (bp::arg("self"), bp::arg("eps") = 1e-8),
            "Computes the full spectrum associated with the input tridiagonal matrix up to "
            "precision eps.")

          .def(bp::self * PlainMatrixType())
          .def(PlainMatrixType() * bp::self);
      }

      static void expose()
      {
        typedef ::boost::python::detail::not_specified HolderType;
        bp::class_<TridiagonalSymmetricMatrix, HolderType>(
          "TridiagonalSymmetricMatrix", "Tridiagonal symmetric matrix.", bp::no_init)
          .def(TridiagonalSymmetricMatrixPythonVisitor());
      }
    };

  } // namespace python
} // namespace pinocchio
