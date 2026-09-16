//
// Copyright (c) 2017-2018 CNRS
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/eigen-common.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/eigen-common.hpp"
#endif // PINOCCHIO_LSP

#if EIGEN_VERSION_AT_LEAST(3, 4, 90)
  #define EIGEN_EMPTY_STRUCT_CTOR(x)
#endif

namespace pinocchio
{
  namespace fix
  {
    namespace Eigen
    {
      namespace internal
      {

        /* plain_matrix_type_row_major : same as plain_matrix_type but guaranteed to be row-major
         */
        template<typename T>
        struct plain_matrix_type_row_major
        {
          enum
          {
            Rows = ::Eigen::internal::traits<T>::RowsAtCompileTime,
            Cols = ::Eigen::internal::traits<T>::ColsAtCompileTime,
            MaxRows = ::Eigen::internal::traits<T>::MaxRowsAtCompileTime,
            MaxCols = ::Eigen::internal::traits<T>::MaxColsAtCompileTime
          };
          typedef ::Eigen::Matrix<
            typename ::Eigen::internal::traits<T>::Scalar,
            Rows,
            Cols,
            (MaxCols == 1 && MaxRows != 1) ? ::Eigen::ColMajor : ::Eigen::RowMajor,
            MaxRows,
            MaxCols>
            type;
        };

      } // namespace internal
    } // namespace Eigen
  } // namespace fix
} // namespace pinocchio
