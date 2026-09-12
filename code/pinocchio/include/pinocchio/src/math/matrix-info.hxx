//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/math.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/math.hpp"
#endif // PINOCCHIO_LSP

/**
 * @file matrix-info.hpp
 * @brief Defines the pinocchio::internal::MatrixInfo struct, a utility for storing matrix
 * dimensions.
 */

namespace pinocchio
{
  namespace internal
  {

    /**
     * @ingroup pinocchio_math_linalg
     * @brief A structure to hold and query the dimensions (rows and columns) of a matrix.
     *
     * @details This utility struct is used to store and pass around the size information of a
     * matrix, particularly when the dimensions need to be queried from an algorithm or function.
     *
     *          A default-constructed instance is initialized to an invalid state (`rows = -1`,
     * `cols = -1`), which can be verified using the isValid() method.
     */
    struct MatrixInfo
    {
      /// @brief Default constructor. Initializes to an invalid state (-1, -1).
      MatrixInfo() = default;

      /**
       * @brief Constructs a MatrixInfo object with specified dimensions.
       * @param[in] rows The number of rows for the matrix.
       * @param[in] cols The number of columns for the matrix.
       */
      MatrixInfo(const Eigen::Index rows, const Eigen::Index cols)
      : m_rows(rows)
      , m_cols(cols)
      {
      }

      /**
       * @brief Checks if the stored matrix dimensions are valid.
       *
       * @details A MatrixInfo object is considered valid if and only if both its `rows` and `cols`
       *          attributes are non-negative.
       *
       * @return `true` if both rows and cols are non-negative, `false` otherwise.
       */
      bool isValid() const
      {
        return m_rows >= 0 && m_cols >= 0;
      }

      /// @brief Returns the number of rows.
      Eigen::Index rows() const
      {
        return m_rows;
      }

      /// @brief Returns the number of columns.
      Eigen::Index cols() const
      {
        return m_cols;
      }

      /// @brief Returns the total number of elements in the matrix (rows * cols).
      Eigen::Index size() const
      {
        return m_rows * m_cols;
      }

    protected:
      /// @brief The number of rows of the matrix. Initialized to -1.
      Eigen::Index m_rows = -1;

      /// @brief The number of columns of the matrix. Initialized to -1.
      Eigen::Index m_cols = -1;

    }; // struct MatrixInfo
  } // namespace internal

} // namespace pinocchio
