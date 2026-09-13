//
// Copyright (c) 2017-2024 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/eigen-common.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/eigen-common.hpp"
#endif // PINOCCHIO_LSP

/// \brief Macro giving access to the equivalent plain type of D
#define PINOCCHIO_EIGEN_PLAIN_TYPE(D)                                                              \
  Eigen::internal::plain_matrix_type<typename pinocchio::helper::argument_type<void(D)>::type>::type
#define PINOCCHIO_EIGEN_PLAIN_TYPE_NO_PARENS(D)                                                    \
  Eigen::internal::plain_matrix_type<typename pinocchio::helper::argument_type<void D>::type>::type

/// \brief Similar to macro PINOCCHIO_EIGEN_PLAIN_TYPE but with guaranty to provite a column major
/// type
#define PINOCCHIO_EIGEN_PLAIN_COLUMN_MAJOR_TYPE(D)                                                 \
  pinocchio::helper::handle_return_type_without_typename<                                          \
    D, Eigen::internal::plain_matrix_type_column_major>::type

/// \brief Similar to macro PINOCCHIO_EIGEN_PLAIN_TYPE but with guaranty to provite a row major type
#define PINOCCHIO_EIGEN_PLAIN_ROW_MAJOR_TYPE(D)                                                    \
  pinocchio::helper::handle_return_type_without_typename<                                          \
    D, ::pinocchio::fix::Eigen::internal::plain_matrix_type_row_major>::type

/// \brief Macro giving access to the reference type of D
#define PINOCCHIO_EIGEN_REF_CONST_TYPE(D) Eigen::internal::ref_selector<D>::type
#define PINOCCHIO_EIGEN_REF_TYPE(D) Eigen::internal::ref_selector<D>::non_const_type

/// \brief Macro giving access to the return type of the dot product operation
#define PINOCCHIO_EIGEN_DOT_PRODUCT_RETURN_TYPE(D1, D2)                                            \
  Eigen::ScalarBinaryOpTraits<                                                                     \
    typename Eigen::internal::traits<D1>::Scalar,                                                  \
    typename Eigen::internal::traits<D2>::Scalar>::ReturnType

/// \brief Macro for an automatic const_cast
#define PINOCCHIO_EIGEN_CONST_CAST(TYPE, OBJ) const_cast<TYPE &>(OBJ.derived())

/// All Eigen malloc management macro are deactivated
#define PINOCCHIO_EIGEN_MALLOC(allowed)                                                            \
  PINOCCHIO_PRAGMA("WARNING: \"PINOCCHIO_EIGEN_MALLOC is deprecated and will be removed\"")
#define PINOCCHIO_EIGEN_MALLOC_ALLOWED()                                                           \
  PINOCCHIO_PRAGMA("WARNING: \"PINOCCHIO_EIGEN_MALLOC_ALLOWED is deprecated and will be removed\"")
#define PINOCCHIO_EIGEN_MALLOC_NOT_ALLOWED()                                                       \
  PINOCCHIO_PRAGMA(                                                                                \
    "WARNING: \"PINOCCHIO_EIGEN_MALLOC_NOT_ALLOWED is deprecated and will be removed\"")
#define PINOCCHIO_EIGEN_MALLOC_SAVE_STATUS()                                                       \
  PINOCCHIO_PRAGMA(                                                                                \
    "WARNING: \"PINOCCHIO_EIGEN_MALLOC_SAVE_STATUS is deprecated and will be removed\"")
#define PINOCCHIO_EIGEN_MALLOC_RESTORE_STATUS()                                                    \
  PINOCCHIO_PRAGMA(                                                                                \
    "WARNING: \"PINOCCHIO_EIGEN_MALLOC_RESTORE_STATUSE is deprecated and will be removed\"")

/// Check if a vector have a compile time size or is dynamic
#define PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(TYPE, SIZE)                  \
  static_assert(                                                                                   \
    TYPE::IsVectorAtCompileTime                                                                    \
    && (TYPE::SizeAtCompileTime == Eigen::Dynamic || TYPE::SizeAtCompileTime == SIZE))

/// Check if a matrix have a compile time size or is dynamic
#define PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(TYPE, ROWS, COLS)            \
  static_assert(                                                                                   \
    (TYPE::RowsAtCompileTime == Eigen::Dynamic || TYPE::RowsAtCompileTime == ROWS)                 \
    && (TYPE::ColsAtCompileTime == Eigen::Dynamic || TYPE::ColsAtCompileTime == COLS))
