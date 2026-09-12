//
// Copyright (c) 2025-2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/utils/promote-static-eval.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/utils/promote-static-eval.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace internal
  {
    template<int MinRowsAtCompileTime, int MinColsAtCompileTime, typename PlainMatrix>
    struct make_static_matrix
    {
      static constexpr int RowsAtCompileTime = PlainMatrix::RowsAtCompileTime == Eigen::Dynamic
                                                 ? MinRowsAtCompileTime
                                                 : PlainMatrix::RowsAtCompileTime;
      static constexpr int ColsAtCompileTime = PlainMatrix::ColsAtCompileTime == Eigen::Dynamic
                                                 ? MinColsAtCompileTime
                                                 : PlainMatrix::ColsAtCompileTime;

      static constexpr int MaxRowsAtCompileTime =
        PlainMatrix::MaxRowsAtCompileTime == Eigen::Dynamic ? MinRowsAtCompileTime
                                                            : PlainMatrix::MaxRowsAtCompileTime;
      static constexpr int MaxColsAtCompileTime =
        PlainMatrix::MaxColsAtCompileTime == Eigen::Dynamic ? MinColsAtCompileTime
                                                            : PlainMatrix::MaxColsAtCompileTime;

      typedef Eigen::Matrix<
        typename PlainMatrix::Scalar,
        RowsAtCompileTime,
        ColsAtCompileTime,
        ((ColsAtCompileTime == 1 && RowsAtCompileTime == 1) || ColsAtCompileTime == 1)
          ? static_cast<int>(Eigen::ColMajor)
        : RowsAtCompileTime == 1 ? static_cast<int>(Eigen::RowMajor)
                                 : PlainMatrix::Options,
        MaxRowsAtCompileTime,
        MaxColsAtCompileTime>
        type;
    };

    typedef Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic> DynamicStride;

    template<typename MatrixLike, typename MatrixDerived>
    // static Eigen::Map<MatrixLike, 0, DynamicStride>
    static Eigen::Map<MatrixLike> make_eigen_map(Eigen::MatrixBase<MatrixDerived> & _plain_object)
    {
      auto & plain_object = _plain_object.const_cast_derived();
      // const DynamicStride stride = {plain_object.outerStride(), plain_object.innerStride()};
      // return {plain_object.data(), plain_object.rows(), plain_object.cols(), stride};
      return {plain_object.data(), plain_object.rows(), plain_object.cols()};
    }

    template<typename MatrixLike, typename MatrixDerived>
    // static Eigen::Map<MatrixLike, 0, DynamicStride>
    static const Eigen::Map<const MatrixLike>
    make_eigen_map(const Eigen::MatrixBase<MatrixDerived> & _plain_object)
    {
      auto & plain_object = _plain_object.const_cast_derived();
      // const DynamicStride stride = {plain_object.outerStride(), plain_object.innerStride()};
      // return {plain_object.data(), plain_object.rows(), plain_object.cols(), stride};
      return {plain_object.data(), plain_object.rows(), plain_object.cols()};
    }

    template<typename MatrixLike, typename MatrixDerived, template<typename> class _StorageBase>
    // static Eigen::Map<MatrixLike, 0, DynamicStride>
    static Eigen::Map<MatrixLike>
    make_eigen_map(Eigen::NoAlias<MatrixDerived, _StorageBase> & _plain_object_noalias)
    {
      return make_eigen_map<MatrixLike>(_plain_object_noalias.expression().const_cast_derived());
    }

    template<typename MatrixLike, typename MatrixDerived, template<typename> class _StorageBase>
    // static Eigen::Map<MatrixLike, 0, DynamicStride>
    static const Eigen::Map<const MatrixLike>
    make_eigen_map(const Eigen::NoAlias<MatrixDerived, _StorageBase> & _plain_object_noalias)
    {
      return make_eigen_map<MatrixLike>(_plain_object_noalias.expression().const_cast_derived());
    }

    template<
      template<typename, typename> class Op,
      typename Dst,
      template<typename> class _StorageBase,
      typename Src>
    inline constexpr void
    call_eigen_assignment(Eigen::NoAlias<Dst, _StorageBase> & dst, const Src & src)
    {
      typedef typename Dst::Scalar S1;
      typedef typename Src::Scalar S2;

      Eigen::internal::call_assignment_no_alias(dst.expression(), src, Op<S1, S2>());
    }

    template<template<typename, typename> class Op, typename Dst, typename Src>
    inline constexpr void call_eigen_assignment(const Eigen::MatrixBase<Dst> & dst, const Src & src)
    {
      typedef typename Dst::Scalar S1;
      typedef typename Src::Scalar S2;

      Eigen::internal::call_assignment(dst.const_cast_derived(), src, Op<S1, S2>());
    }

    template<
      template<typename, typename> class EigenOp,
      typename PlainResult,
      typename PlainLhs,
      typename PlainRhs,
      typename Dst,
      typename Src>
    inline constexpr void call_eigen_static_map_assignment(Dst & dst, const Src & src)
    {
      const auto & lhs = src.lhs();
      const auto & rhs = src.rhs();

      // typedef typename PINOCCHIO_DECLTYPE(rhs) Rhs;

      auto get_lhs = [&]() -> decltype(auto) {
        typedef typename PINOCCHIO_DECLTYPE(lhs) Lhs;
        if constexpr (helper::has_fixed_size_v<Lhs>)
          return (lhs); // reference
        else
          return make_eigen_map<PlainLhs>(lhs);
      };

      auto get_rhs = [&]() -> decltype(auto) {
        typedef typename PINOCCHIO_DECLTYPE(rhs) Rhs;
        if constexpr (helper::has_fixed_size_v<Rhs>)
          return (rhs); // reference
        else
          return make_eigen_map<PlainRhs>(rhs);
      };

      const auto & lhs_map = get_lhs();
      const auto & rhs_map = get_rhs();
      const auto matrix_map_product = lhs_map * rhs_map;

      using ExpressionType = typename helper::remove_eigen_noalias<Dst>::type;
      if constexpr (helper::has_fixed_size_v<ExpressionType>)
      {
        call_eigen_assignment<EigenOp>(dst, matrix_map_product);
      }
      else
      {
        auto result_matrix_map = make_eigen_map<PlainResult>(dst);
        if constexpr (helper::is_eigen_noalias_v<Dst>)
        {
          auto result_matrix_map_noalias = result_matrix_map.noalias();
          call_eigen_assignment<EigenOp>(result_matrix_map_noalias, matrix_map_product);
        }
        else
        {
          call_eigen_assignment<EigenOp>(result_matrix_map, matrix_map_product);
        }
      }
    }

    template<typename Result, typename Lhs, typename Rhs>
    struct MatrixProductDimensions
    {
      static constexpr int RowsAtCompileTime = Lhs::RowsAtCompileTime != Eigen::Dynamic
                                                 ? static_cast<int>(Lhs::RowsAtCompileTime)
                                                 : static_cast<int>(Result::RowsAtCompileTime);
      static constexpr int ColsAtCompileTime = Rhs::ColsAtCompileTime != Eigen::Dynamic
                                                 ? static_cast<int>(Rhs::ColsAtCompileTime)
                                                 : static_cast<int>(Result::ColsAtCompileTime);
      static constexpr int InnerDimensionAtCompileTime =
        Lhs::ColsAtCompileTime != Eigen::Dynamic ? static_cast<int>(Lhs::ColsAtCompileTime)
                                                 : static_cast<int>(Rhs::RowsAtCompileTime);

      static constexpr bool is_static_size_product()
      {
        return RowsAtCompileTime != Eigen::Dynamic && ColsAtCompileTime != Eigen::Dynamic
               && InnerDimensionAtCompileTime != Eigen::Dynamic;
      }

      static constexpr bool is_partial_static_size_product()
      {
        return (RowsAtCompileTime != Eigen::Dynamic && ColsAtCompileTime != Eigen::Dynamic)
               || (ColsAtCompileTime != Eigen::Dynamic && InnerDimensionAtCompileTime != Eigen::Dynamic)
               || (InnerDimensionAtCompileTime != Eigen::Dynamic && RowsAtCompileTime != Eigen::Dynamic);
      }

      static Eigen::Index
      dynamic_size(const Eigen::MatrixBase<Lhs> & lhs, const Eigen::MatrixBase<Rhs> & rhs)
      {
        if constexpr (is_static_size_product())
          return -1;

        if constexpr (RowsAtCompileTime != Eigen::Dynamic && ColsAtCompileTime != Eigen::Dynamic)
          return lhs.cols();
        else if constexpr (
          ColsAtCompileTime != Eigen::Dynamic && InnerDimensionAtCompileTime != Eigen::Dynamic)
          return lhs.rows();
        else /*if constexpr (InnerDimensionAtCompileTime != Eigen::Dynamic && RowsAtCompileTime !=
                Eigen::Dynamic)*/
          return rhs.cols();
      }

    private:
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Lhs) LhsPlain_;
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Rhs) RhsPlain_;
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Result) ResultPlain_;

    public:
      typedef Eigen::Matrix<
        typename Lhs::Scalar,
        RowsAtCompileTime,
        InnerDimensionAtCompileTime,
        ColsAtCompileTime == 1 ? static_cast<int>(Eigen::ColMajor) : LhsPlain_::Options>
        PlainLhs;
      typedef Eigen::Matrix<
        typename Rhs::Scalar,
        InnerDimensionAtCompileTime,
        ColsAtCompileTime,
        ColsAtCompileTime == 1 ? static_cast<int>(Eigen::ColMajor) : RhsPlain_::Options>
        PlainRhs;
      typedef Eigen::Matrix<
        typename Result::Scalar,
        RowsAtCompileTime,
        ColsAtCompileTime,
        ColsAtCompileTime == 1 ? static_cast<int>(Eigen::ColMajor) : ResultPlain_::Options>
        PlainResult;
    };

    template<int N>
    struct partial_static_dispatch_impl
    {
      template<
        template<typename, typename> class EigenOp,
        typename Dst,
        typename Lhs,
        typename Rhs,
        int Option>
      static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void run(
        const Eigen::Index leading_size,
        Dst & result,
        const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        typedef typename helper::remove_eigen_noalias<Dst>::type Result;
        typedef MatrixProductDimensions<Result, Lhs, Rhs> Dims;
        if (leading_size == N)
        {
          using LhsPlainStatic = typename make_static_matrix<N, N, typename Dims::PlainLhs>::type;
          using RhsPlainStatic = typename make_static_matrix<N, N, typename Dims::PlainRhs>::type;
          using PlainResultStatic =
            typename make_static_matrix<N, N, typename Dims::PlainResult>::type;

          call_eigen_static_map_assignment<
            EigenOp, PlainResultStatic, LhsPlainStatic, RhsPlainStatic>(result, matrix_product);
        }
        else
        {
          partial_static_dispatch_impl<N - 1>::template run<EigenOp>(
            leading_size, result, matrix_product);
        }
      }
    };

    template<>
    struct partial_static_dispatch_impl<0>
    {
      template<
        template<typename, typename> class EigenOp,
        typename Dst,
        typename Lhs,
        typename Rhs,
        int Option>
      static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void run(
        const Eigen::Index leading_size,
        Dst & result,
        const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        PINOCCHIO_UNUSED_VARIABLE(leading_size);
        PINOCCHIO_UNUSED_VARIABLE(result);
        PINOCCHIO_UNUSED_VARIABLE(matrix_product);
      }
    };

    enum class DispatchType
    {
      STATIC,
      PARTIAL_STATIC,
      DYNAMIC
    };

    template<int _MaxStaticUnfolding, typename ExpressionType, template<typename> class StorageBase>
    class PromoteStaticEval
    {
    public:
      static constexpr int MaxStaticUnfolding = _MaxStaticUnfolding;

      typedef typename ExpressionType::Scalar Scalar;

      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(
        typename helper::remove_eigen_noalias<ExpressionType>::type) PlainExpression;

      EIGEN_DEVICE_FUNC PromoteStaticEval(ExpressionType & expression)
      : m_expression(expression)
      {
      }
      EIGEN_DEVICE_FUNC PromoteStaticEval(ExpressionType && expression)
      : m_expression(expression)
      {
      }

      template<typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ExpressionType &
      operator=(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        return dispatch<Eigen::internal::assign_op>(matrix_product);
      }

      template<typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ExpressionType &
      operator+=(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        return dispatch<Eigen::internal::add_assign_op>(matrix_product);
      }

      template<typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ExpressionType &
      operator-=(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        return dispatch<Eigen::internal::sub_assign_op>(matrix_product);
      }

      ExpressionType & expression()
      {
        return m_expression;
      }

      template<typename Lhs, typename Rhs, int Option>
      DispatchType dispatch_type(const Eigen::Product<Lhs, Rhs, Option> & matrix_product) const
      {
        PINOCCHIO_UNUSED_VARIABLE(matrix_product);
        typedef MatrixProductDimensions<PlainExpression, Lhs, Rhs> Dims;
        if constexpr (Dims::is_static_size_product())
          return DispatchType::STATIC;
        else if constexpr (Dims::is_partial_static_size_product())
          return DispatchType::PARTIAL_STATIC;
        else
          return DispatchType::DYNAMIC;
      }

    protected:
      std::
        conditional_t<helper::is_eigen_noalias_v<ExpressionType>, ExpressionType, ExpressionType &>
          m_expression;

      template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ExpressionType &
      dispatch(const Eigen::Product<Lhs, Rhs, Option> & mat_prod)
      {
        const auto & lhs = mat_prod.lhs();
        const auto & rhs = mat_prod.rhs();

        typedef MatrixProductDimensions<PlainExpression, Lhs, Rhs> Dims;
        if constexpr (Dims::is_static_size_product())
        {
          static_dispatch<EigenOp>(mat_prod);
        }
        else if constexpr (Dims::is_partial_static_size_product())
        {
          partial_static_dispatch<MaxStaticUnfolding, EigenOp>(mat_prod);
        }
        else
        {
          const auto max_size = std::max(lhs.rows(), std::max(lhs.cols(), rhs.cols()));
          if (max_size <= 6)
          {
            matrix_product<EigenOp>(
              lhs, rhs, helper::remove_eigen_noalias<ExpressionType>::get(expression()));
          }
          else
            dynamic_dispatch<EigenOp>(mat_prod);
        }
        return m_expression;
      }

      template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void
      dynamic_dispatch(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        call_eigen_assignment<EigenOp>(expression(), matrix_product);
      }

      template<
        int N,
        template<typename, typename> class EigenOp,
        typename Lhs,
        typename Rhs,
        int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void
      partial_static_dispatch(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        // call_eigen_assignment<EigenOp>(expression(), matrix_product);
        const auto & lhs = matrix_product.lhs();
        const auto & rhs = matrix_product.rhs();

        typedef typename helper::remove_eigen_noalias<ExpressionType>::type Result;
        typedef MatrixProductDimensions<Result, Lhs, Rhs> Dims;
        const auto leading_size = Dims::dynamic_size(lhs, rhs);
        if (leading_size > N)
          dynamic_dispatch<EigenOp>(matrix_product);
        else
          partial_static_dispatch_impl<N>::template run<EigenOp>(
            leading_size, expression(), matrix_product);
      }

      template<template<typename, typename> class EigenOp, typename Lhs, typename Rhs, int Option>
      EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void
      static_dispatch(const Eigen::Product<Lhs, Rhs, Option> & matrix_product)
      {
        // static_dispatch<EigenOp>(matrix_product);
        // expression() = matrix_product;
        typedef MatrixProductDimensions<PlainExpression, Lhs, Rhs> Dims;

        typedef typename Dims::PlainLhs PlainLhs;
        typedef typename Dims::PlainRhs PlainRhs;
        typedef typename Dims::PlainResult PlainResult;

        call_eigen_static_map_assignment<EigenOp, PlainResult, PlainLhs, PlainRhs>(
          expression(), matrix_product);
      }

    }; // struct PromoteStaticEval

    template<int MaxStaticUnfolding, typename MatrixExpression>
    PromoteStaticEval<MaxStaticUnfolding, MatrixExpression, Eigen::MatrixBase>
    promote_static_eval(const Eigen::MatrixBase<MatrixExpression> & matrix_expression)
    {
      return {matrix_expression.const_cast_derived()};
    }

    template<typename MatrixExpression>
    PromoteStaticEval<0, MatrixExpression, Eigen::MatrixBase>
    promote_static_eval(const Eigen::MatrixBase<MatrixExpression> & matrix_expression)
    {
      return {matrix_expression.const_cast_derived()};
    }

    template<
      int MaxStaticUnfolding,
      typename MatrixExpression,
      template<typename> class StorageBase>
    PromoteStaticEval<
      MaxStaticUnfolding,
      Eigen::NoAlias<MatrixExpression, StorageBase>,
      Eigen::MatrixBase>
    promote_static_eval(Eigen::NoAlias<MatrixExpression, StorageBase> && matrix_expression)
    {
      return {std::forward<Eigen::NoAlias<MatrixExpression, StorageBase>>(matrix_expression)};
    }

    template<typename MatrixExpression, template<typename> class StorageBase>
    PromoteStaticEval<0, Eigen::NoAlias<MatrixExpression, StorageBase>, Eigen::MatrixBase>
    promote_static_eval(Eigen::NoAlias<MatrixExpression, StorageBase> && matrix_expression)
    {
      return {std::forward<Eigen::NoAlias<MatrixExpression, StorageBase>>(matrix_expression)};
    }

  } // namespace internal
} // namespace pinocchio
