//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/autodiff/casadi.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/autodiff/casadi.hpp"
#endif // PINOCCHIO_LSP

// Overloading of min/max operator
namespace pinocchio
{
  namespace math
  {
    namespace internal
    {
      template<typename Scalar>
      struct return_type_min<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar, typename T>
      struct return_type_min<::casadi::Matrix<Scalar>, T>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar, typename T>
      struct return_type_min<T, ::casadi::Matrix<Scalar>>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar>
      struct call_min<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>>
      {
        static inline ::casadi::Matrix<Scalar>
        run(const ::casadi::Matrix<Scalar> & a, const ::casadi::Matrix<Scalar> & b)
        {
          return fmin(a, b);
        }
      };

      template<typename S1, typename S2>
      struct call_min<::casadi::Matrix<S1>, S2>
      {
        typedef ::casadi::Matrix<S1> CasadiType;
        static inline ::casadi::Matrix<S1> run(const ::casadi::Matrix<S1> & a, const S2 & b)
        {
          return fmin(a, static_cast<CasadiType>(b));
        }
      };

      template<typename S1, typename S2>
      struct call_min<S1, ::casadi::Matrix<S2>>
      {
        typedef ::casadi::Matrix<S2> CasadiType;
        static inline ::casadi::Matrix<S2> run(const S1 & a, const ::casadi::Matrix<S2> & b)
        {
          return fmin(static_cast<CasadiType>(a), b);
        }
      };

      template<typename Scalar>
      struct return_type_max<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar, typename T>
      struct return_type_max<::casadi::Matrix<Scalar>, T>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar, typename T>
      struct return_type_max<T, ::casadi::Matrix<Scalar>>
      {
        typedef ::casadi::Matrix<Scalar> type;
      };

      template<typename Scalar>
      struct call_max<::casadi::Matrix<Scalar>, ::casadi::Matrix<Scalar>>
      {
        static inline ::casadi::Matrix<Scalar>
        run(const ::casadi::Matrix<Scalar> & a, const ::casadi::Matrix<Scalar> & b)
        {
          return fmax(a, b);
        }
      };

      template<typename S1, typename S2>
      struct call_max<::casadi::Matrix<S1>, S2>
      {
        typedef ::casadi::Matrix<S1> CasadiType;
        static inline ::casadi::Matrix<S1> run(const ::casadi::Matrix<S1> & a, const S2 & b)
        {
          return fmax(a, static_cast<CasadiType>(b));
        }
      };

      template<typename S1, typename S2>
      struct call_max<S1, ::casadi::Matrix<S2>>
      {
        typedef ::casadi::Matrix<S2> CasadiType;
        static inline ::casadi::Matrix<S2> run(const S1 & a, const ::casadi::Matrix<S2> & b)
        {
          return fmax(static_cast<CasadiType>(a), b);
        }
      };
    } // namespace internal

  } // namespace math

} // namespace pinocchio
