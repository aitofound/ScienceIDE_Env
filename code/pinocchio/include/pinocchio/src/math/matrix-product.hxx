//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  namespace internal
  {

    template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, typename Res>
    void matrix_product_generic(
      const Eigen::MatrixBase<Lhs> & lhs,
      const Eigen::MatrixBase<Rhs> & rhs,
      const Eigen::MatrixBase<Res> & res);

    template<
      template<typename, typename> class EigenOp,
      typename LhsType,
      typename RhsType,
      typename ResType>
    void matrix_product_small_size(
      const Eigen::MatrixBase<LhsType> & lhs,
      const Eigen::MatrixBase<RhsType> & rhs,
      const Eigen::MatrixBase<ResType> & res);

    template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, typename Res>
    void matrix_product(
      const Eigen::MatrixBase<Lhs> & lhs,
      const Eigen::MatrixBase<Rhs> & rhs,
      const Eigen::MatrixBase<Res> & res)
    {
      const auto max_size = std::max(lhs.rows(), std::max(lhs.cols(), rhs.cols()));
      if (max_size <= 0)
        matrix_product_small_size<EigenOp>(lhs.derived(), rhs.derived(), res.const_cast_derived());
      else
        matrix_product_generic<EigenOp>(lhs.derived(), rhs.derived(), res.const_cast_derived());
    };

    template<
      template<typename, typename> class EigenOp,
      typename LhsType,
      typename RhsType,
      typename ResType>
    void matrix_product_small_size(
      const Eigen::MatrixBase<LhsType> & lhs,
      const Eigen::MatrixBase<RhsType> & rhs,
      const Eigen::MatrixBase<ResType> & res)
    {
      typedef typename ResType::Scalar S1;
      typedef typename LhsType::Scalar S2;

      typedef EigenOp<S1, S2> Op;

      typedef Eigen::Matrix<typename LhsType::Scalar, 6, 6> LhsStatic6x6;
      typedef Eigen::Matrix<typename RhsType::Scalar, 6, 6> RhsStatic6x6;
      typedef Eigen::Matrix<typename ResType::Scalar, 6, 6> ResStatic6x6;

      LhsStatic6x6 lhs_static = LhsStatic6x6::Zero();
      lhs_static.topLeftCorner(lhs.rows(), lhs.cols()) = lhs;
      RhsStatic6x6 rhs_static = RhsStatic6x6::Zero();
      rhs_static.topLeftCorner(rhs.rows(), rhs.cols()) = rhs;
      ResStatic6x6 res_static = ResStatic6x6::Zero();
      res_static.topLeftCorner(res.rows(), res.cols()) = res;

      const auto matrix_product = lhs_static * rhs_static;

      if constexpr (is_specialization_of_v<Op, Eigen::internal::assign_op>)
      {
        res_static.noalias() = matrix_product;
      }
      else if constexpr (is_specialization_of_v<Op, Eigen::internal::add_assign_op>)
      {
        res_static.noalias() += matrix_product;
      }
      else if constexpr (is_specialization_of_v<Op, Eigen::internal::sub_assign_op>)
      {
        res_static.noalias() -= matrix_product;
      }

      res.const_cast_derived() = res_static.topLeftCorner(res.rows(), res.cols());
    }

    template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, typename Res>
    void matrix_product_generic(
      const Eigen::MatrixBase<Lhs> & lhs,
      const Eigen::MatrixBase<Rhs> & rhs,
      const Eigen::MatrixBase<Res> & res)
    {
      using Scalar = typename Rhs::Scalar;

      const auto rows = lhs.rows();
      const auto cols = rhs.cols();
      const auto inner_dim = lhs.cols();

      const auto * lhs_data = lhs.derived().data();
      const auto * rhs_data = rhs.derived().data();
      auto * res_data = res.const_cast_derived().data();

      auto lhs_index = [&](const Eigen::Index i, const Eigen::Index j) -> Eigen::Index {
        if constexpr (Lhs::IsRowMajor)
          return j + i * inner_dim;
        else // lhs(i,k) = lhs_data[k*rows + i]
          return j * rows + i;
      };

      auto rhs_index = [&](const Eigen::Index i, const Eigen::Index j) -> Eigen::Index {
        if constexpr (Rhs::IsRowMajor)
          return j + i * cols;
        else // rhs(k,j) = rhs_data[j*inner_dim + k]
          return j * inner_dim + i;
      };

      auto res_index = [&](const Eigen::Index i, const Eigen::Index j) -> Eigen::Index {
        if constexpr (Res::IsRowMajor)
          return j + i * cols;
        else // res(i,j) = res_data[j*rows + i]
          return j * rows + i;
      };

      // TODO @jcarpent check why you write this: #pragma omp simd
      for (Eigen::Index j = 0; j < cols; ++j) // loop over columns of result
      {
        for (Eigen::Index i = 0; i < rows; ++i) // loop over rows of result
        {
          Scalar sum = Scalar(0);
          for (Eigen::Index k = 0; k < inner_dim; ++k)
          {
            sum += lhs_data[lhs_index(i, k)] * rhs_data[rhs_index(k, j)];
          }
          typedef EigenOp<typename Res::Scalar, typename Lhs::Scalar> Op;
          if constexpr (is_specialization_of_v<Op, Eigen::internal::assign_op>)
          {
            res_data[res_index(i, j)] = sum;
          }
          else if constexpr (is_specialization_of_v<Op, Eigen::internal::add_assign_op>)
          {
            res_data[res_index(i, j)] += sum;
          }
          else if constexpr (is_specialization_of_v<Op, Eigen::internal::sub_assign_op>)
          {
            res_data[res_index(i, j)] -= sum;
          }
        }
      }
    }
  } // namespace internal
} // namespace pinocchio
