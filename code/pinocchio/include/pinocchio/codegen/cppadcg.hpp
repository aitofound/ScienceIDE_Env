//
// Copyright (c) 2018-2023 CNRS INRIA
//

#pragma once

// IWYU pragma: always_keep

#define PINOCCHIO_WITH_CPPADCG_SUPPORT

// IWYU pragma: begin_keep
#include <cmath>
#include <type_traits>

#include <Eigen/Core>

#include <boost/version.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/mpl/int.hpp>

#include <cppad/cg.hpp>
#include <cppad/cg/support/cppadcg_eigen.hpp>

#include "pinocchio/math.hpp"
#include "pinocchio/autodiff/cppad.hpp"
// IWYU pragma: end_keep

namespace boost
{
  namespace math
  {
    namespace constants
    {
      namespace detail
      {
        template<typename Scalar>
        struct constant_pi<CppAD::cg::CG<Scalar>> : constant_pi<Scalar>
        {
          typedef CppAD::cg::CG<Scalar> CGScalar;

          template<int N>
          static inline CGScalar get(const mpl::int_<N> & n)
          {
            return CGScalar(constant_pi<Scalar>::get(n));
          }

#if BOOST_VERSION >= 107700
          template<class T, T value>
          static inline CGScalar get(const std::integral_constant<T, value> & n)
          {
            return CGScalar(constant_pi<Scalar>::get(n));
          }
#else
          template<class T, T value>
          static inline CGScalar get(const boost::integral_constant<T, value> & n)
          {
            return CGScalar(constant_pi<Scalar>::get(n));
          }
#endif
        };
      } // namespace detail
    } // namespace constants
  } // namespace math
} // namespace boost

namespace Eigen
{
  namespace internal
  {
    // Specialization of Eigen::internal::cast_impl for CppAD input types
    template<typename Scalar>
    struct cast_impl<CppAD::cg::CG<Scalar>, Scalar>
    {
      EIGEN_DEVICE_FUNC
      static inline Scalar run(const CppAD::cg::CG<Scalar> & x)
      {
        return x.getValue();
      }
    };

    // Specialization of Eigen::internal::cast_impl for CppAD input types
    template<typename Scalar>
    struct cast_impl<CppAD::AD<CppAD::cg::CG<Scalar>>, Scalar>
    {
      EIGEN_DEVICE_FUNC
      static inline Scalar run(const CppAD::AD<CppAD::cg::CG<Scalar>> & x)
      {
        return CppAD::Value(x).getValue();
      }
    };

  } // namespace internal
} // namespace Eigen

namespace CppAD
{
  namespace cg
  {
    template<class Scalar>
    bool isfinite(const CG<Scalar> & x)
    {
      return std::isfinite(x.getValue());
    }
  } // namespace cg
} // namespace CppAD

// IWYU pragma: begin_exports
#include "pinocchio/src/codegen/cppadcg/math/cast.hxx"
#include "pinocchio/src/codegen/cppadcg/math/taylor-series-expansion.hxx"
// IWYU pragma: end_exports
