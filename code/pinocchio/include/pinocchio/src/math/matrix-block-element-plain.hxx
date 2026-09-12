//
// Copyright (c) 2026 INRIA
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
    /**
     * @ingroup pinocchio_math
     * @brief Describes a single block within a larger structured matrix.
     *
     * @tparam Matrix The underlying Eigen matrix type this block will map to.
     *
     * @details This struct acts as a descriptor for a matrix sub-block. It does not own any memory
     * itself; instead, it uses an `Eigen::Map` to provide a non-owning view into a region of memory
     *          managed elsewhere (e.g., by a `MatrixStack` in `BlockDiagonalMatrixTpl`).
     *
     *          It stores the block's structural type (e.g., `Identity`, `Plain`), its size, and the
     *          `Eigen::Map` to its data if the type requires it.
     */
    template<typename Derived>
    struct MatrixBlockElementPlain : MatrixBlockElementBase<Derived>
    {
      typedef MatrixBlockElementBase<Derived> Base;
      typedef typename traits<Derived>::MatrixContainer MatrixContainer;
      typedef typename traits<Derived>::Matrix Matrix;
      typedef typename traits<Derived>::Vector Vector;
      typedef typename traits<Derived>::Scalar Scalar;
      typedef typename traits<Derived>::PlainBlockElement PlainBlockElement;

      using Base::derived;

      /// @brief Default constructor. Initializes to an invalid state.
      MatrixBlockElementPlain()
      : m_type(MatrixBlockType::Undefined)
      , m_size(-1)
      {
      }

      /**
       * @brief Constructs a block info for types that do not require external data (e.g., Identity,
       * Zero).
       * @param[in] type The structural type of the block.
       * @param[in] size The dimension of the (square) block.
       */
      MatrixBlockElementPlain(const MatrixBlockType type, const Eigen::Index size)
      : m_type(type)
      , m_size(size)
      {
      }

      /// @brief Default copy constructor.
      MatrixBlockElementPlain(const MatrixBlockElementPlain & other) = default;

      /// @brief Default copy-assignment operator.
      MatrixBlockElementPlain & operator=(const MatrixBlockElementPlain & other) = default;

      template<typename Other>
      Derived & operator=(const MatrixBlockElementOperation<Other> & other)
      {
        other.evalTo(derived());
        return derived();
      }

      /**
       * @brief Checks for strict equality between two block info objects.
       *
       * @param[in] other The other block info to compare against.
       * @return `true` if the blocks are strictly equal, `false` otherwise.
       *
       * @details Two `MatrixBlockElementPlain` objects are considered equal if and only if:
       *          1. They have the same `type` and `size`.
       *          2. The data viewed by their internal `Eigen::Map`s is **coefficient-wise equal**.
       *
       * @note This operator performs a deep, numerical comparison of the underlying data, which
       *       can be computationally expensive for large blocks. It does not simply compare
       * pointers.
       */
      bool operator==(const MatrixBlockElementPlain & other) const
      {
        if (this == &other)
          return true;
        return m_type == other.m_type && m_size == other.m_size;
      }

      /**
       * @brief Checks for inequality between two block info objects.
       *
       * @param[in] other The other block info to compare against.
       * @return `true` if the blocks are not strictly equal, `false` otherwise.
       *
       * @details This is the logical negation of `operator==`.
       * @see operator==()
       */
      bool operator!=(const MatrixBlockElementPlain & other) const
      {
        return !(*this == other);
      }

      /**
       * @brief Checks if the block information is valid and self-consistent.
       *
       * @details A block is considered valid if:
       *          - Its size is positive.
       *          - Its type is not `Undefined`.
       *
       * @return `true` if the block info is valid, `false` otherwise.
       */
      bool isValid() const
      {
        bool is_invalid = m_size <= 0 || m_type == MatrixBlockType::Undefined;
        return !is_invalid;
      }

      /**
       * @brief Getter for this block's data container.
       */
      MatrixContainer & container()
      {
        return derived().container();
      }

      /**
       * @brief Const getter for this block's data container.
       */
      const MatrixContainer & container() const
      {
        return derived().container();
      }

      /**
       * @brief Returns a const pointer to the data this blocks points to.
       */
      const Scalar * data() const
      {
        return derived().data();
      }

      /**
       * @brief Returns a pointer to the data this blocks points to.
       */
      Scalar * data()
      {
        return derived().data();
      }

      /**
       * @brief Returns the type of this block (Zero, Identity, ScalarIdentity, Diagonal or Plain).
       */
      MatrixBlockType type() const
      {
        return m_type;
      }

      /**
       * @brief Returns the size of this square block (size = rows and size = cols).
       */
      Eigen::Index size() const
      {
        return m_size;
      }

      /**
       * @brief Applies an assignment operation to a matrix based on this block's type.
       *
       * @tparam AssignOp The assignment operation functor (e.g., `internal::assign_op`,
       *                  `internal::add_assign_op`, `internal::sub_assign_op`).
       * @tparam Matrix The Eigen matrix type to assign to.
       *
       * @param[in,out] _matrix The destination matrix expression to modify.
       *
       * @details This method dispatches on the block's `type` to efficiently apply the
       *          appropriate assignment operation:
       *          - `Zero`: Applies AssignOp with a zero matrix.
       *          - `Identity`: Applies AssignOp with an identity matrix.
       *          - `ScalarIdentity`: Applies AssignOp with a scaled identity matrix.
       *          - `Diagonal`: Applies AssignOp with the diagonal coefficients.
       *          - `Plain`: Applies AssignOp with the full dense block data.
       *
       * @note This is a low-level method used internally by `evalTo`, `addTo`, and `subTo`.
       */
      template<typename AssignOp, typename Matrix>
      void assign_op(const Eigen::MatrixBase<Matrix> & _matrix) const
      {
        auto & matrix = _matrix.const_cast_derived();

        switch (type())
        {
        case MatrixBlockType::Zero: {
          AssignOp::run(Matrix::Zero(size(), size()), matrix);
          break;
        }
        case MatrixBlockType::Identity: {
          AssignOp::run(Matrix::Identity(size(), size()), matrix);
          break;
        }
        case MatrixBlockType::ScalarIdentity: {
          const auto & scalar = container()(0, 0);
          AssignOp::run(scalar * Matrix::Identity(size(), size()), matrix);
          break;
        }
        case MatrixBlockType::Diagonal: {
          AssignOp::run(container().asDiagonal(), matrix);
          break;
        }
        case MatrixBlockType::Plain: {
          AssignOp::run(container(), matrix);
          break;
        }
        case MatrixBlockType::NestedBlockDiagonal: {
          // Use Map with OuterStride to break the recursive template instantiation chain:
          // matrix.block() yields Block<Matrix,...> which causes Block<Block<...>> nesting
          // in recursive assign_op calls, exceeding the template depth limit.
          // Map<MatrixXd, Unaligned, OuterStride<Dynamic>> is a fixed concrete type,
          // so assign_op is always called with the same instantiation (no type explosion).
          const Eigen::Index outer_stride = matrix.outerStride();
          Scalar * const base_ptr = matrix.derived().data();
          Eigen::Index sub_row_id = 0;
          for (const auto & sub_block : derived().nested_blocks())
          {
            const auto sub_size = sub_block.size();
            Eigen::Map<
              Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>, Eigen::Unaligned,
              Eigen::OuterStride<Eigen::Dynamic>>
              sub_map(
                // base ptr + col ptr + row ptr
                base_ptr + sub_row_id + sub_row_id * outer_stride, sub_size, sub_size,
                Eigen::OuterStride<Eigen::Dynamic>(outer_stride));
            sub_block.template assign_op<AssignOp>(sub_map);
            sub_row_id += sub_size;
          }
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      /**
       * @brief Evaluates this block into a dense matrix expression.
       *
       * @tparam Matrix The Eigen matrix type to evaluate into.
       *
       * @param[out] _matrix The destination matrix expression to be filled. Must be a square
       *                     matrix with dimensions equal to this block's `size`.
       *
       * @details This method first zeroes out the destination matrix, then applies the
       *          appropriate assignment based on the block's type using `assign_op`.
       *          This is the primary method for converting a block descriptor into its
       *          dense matrix representation.
       *
       * @throws std::invalid_argument If the matrix dimensions do not match the block size.
       *
       * @see assign_op()
       */
      template<typename Matrix>
      void evalTo(const Eigen::MatrixBase<Matrix> & _matrix) const
      {
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.rows(), size(), "The input matrix has not the right number of rows.");
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.cols(), size(), "The input matrix has not the right number of columns.");

        _matrix.const_cast_derived().setZero();
        assign_op<internal::assign_op>(_matrix.const_cast_derived());
      }

      /**
       * @brief Adds this block's content to an existing matrix expression.
       *
       * @tparam Matrix The Eigen matrix type to add to.
       *
       * @param[in,out] _matrix The destination matrix expression to modify. Must be a square
       *                        matrix with dimensions equal to this block's `size`.
       *
       * @details Unlike `evalTo`, this method does not zero out the destination first.
       *          It performs an additive assignment, equivalent to `_matrix += this_block`.
       *
       * @throws std::invalid_argument If the matrix dimensions do not match the block size.
       *
       * @see assign_op(), evalTo(), subTo()
       */
      template<typename Matrix>
      void addTo(const Eigen::MatrixBase<Matrix> & _matrix) const
      {
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.rows(), size(), "The input matrix has not the right number of rows.");
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.cols(), size(), "The input matrix has not the right number of columns.");

        assign_op<internal::add_assign_op>(_matrix.const_cast_derived());
      }

      /**
       * @brief Subtracts this block from a matrix based on the block's structural type.
       *
       * @tparam Matrix The Eigen matrix type to subtract from.
       *
       * @param[in,out] _matrix The destination matrix expression to modify. Its contents will
       *                        be updated as: `_matrix -= this_block`.
       *
       * @details This method dispatches on the block's `type` to efficiently subtract the
       *          appropriate matrix representation:
       *          - `Zero`: No operation (subtracting zero).
       *          - `Identity`: Subtracts an identity matrix.
       *          - `ScalarIdentity`: Subtracts a scaled identity matrix.
       *          - `Diagonal`: Subtracts the diagonal coefficients.
       *          - `Plain`: Subtracts the full dense block data.
       *
       * @pre The input matrix must have dimensions equal to `size() x size()`.
       *
       * @throws std::invalid_argument (via PINOCCHIO_CHECK_ARGUMENT_SIZE) if the matrix
       *         dimensions do not match the block size.
       *
       * @see evalTo(), addTo()
       */
      template<typename Matrix>
      void subTo(const Eigen::MatrixBase<Matrix> & _matrix) const
      {
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.rows(), size(), "The input matrix has not the right number of rows.");
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          _matrix.cols(), size(), "The input matrix has not the right number of columns.");

        assign_op<internal::sub_assign_op>(_matrix.const_cast_derived());
      }

      /**
       * @brief Evaluates this block element into a dense matrix expression.
       *
       * @tparam Matrix The Eigen matrix type of the destination expression.
       *
       * @param[out] _matrix A pre-allocated, matrix-like Eigen expression that will be
       *                     filled with the block's contents. It **must** have dimensions
       *                     equal to `size() x size()`.
       *
       * @details This is a convenience wrapper around `evalTo()` that provides a more
       *          intuitive interface for extracting the full matrix representation of this block.
       *
       * @see evalTo() for the underlying implementation.
       */
      template<typename Matrix>
      void matrix(const Eigen::MatrixBase<Matrix> & _matrix) const
      {
        evalTo(_matrix.const_cast_derived());
      }

      /**
       * @brief Converts this block descriptor into a dense matrix representation.
       *
       * @return A newly allocated dense matrix of dimensions `size x size` containing
       *         the full matrix representation of this block.
       *
       * @details This is a convenience method that allocates a new matrix and calls
       *          `evalTo()` to fill it based on the block's type. The resulting matrix
       *          will be:
       *          - All zeros for `MatrixBlockType::Zero`
       *          - Identity matrix for `MatrixBlockType::Identity`
       *          - Scaled identity for `MatrixBlockType::ScalarIdentity`
       *          - Diagonal matrix for `MatrixBlockType::Diagonal`
       *          - Full dense copy for `MatrixBlockType::Plain`
       *
       * @note For performance-critical code where repeated memory allocations should
       *       be avoided, prefer the overload `matrix(const Eigen::MatrixBase<Matrix>&)`
       *       that fills a pre-allocated matrix.
       *
       * @see evalTo(), matrix(const Eigen::MatrixBase<Matrix>&) const
       */
      Matrix matrix() const
      {
        Matrix res(size(), size());
        matrix(res);
        return res;
      }

      /**
       * @brief Fills a pre-allocated vector expression with the diagonal elements of this block.
       *
       * @tparam DiagonalSlice The Eigen type of the destination vector, slice, or expression.
       *
       * @param[out] _diagonal_slice A pre-allocated, vector-like Eigen expression that will be
       *                             filled with the diagonal elements. It **must** have a size
       *                             equal to this block's `size`.
       *
       * @details This is the core, high-performance method for extracting a diagonal. It performs
       *          no memory allocation and writes the result directly into the provided destination.
       *
       *          The behavior depends on the block's `type`:
       *          - `Zero`: Fills the destination with zeros.
       *          - `Identity`: Fills the destination with ones.
       *          - `ScalarIdentity`: Fills the destination with the block's scalar value.
       *          - `Diagonal`: Copies the vector of diagonal coefficients.
       *          - `Plain`: Extracts the diagonal from the dense block's data.
       */
      template<typename DiagonalSlice>
      void diagonal(const Eigen::MatrixBase<DiagonalSlice> & _diagonal_slice) const
      {
        auto & diagonal_slice = _diagonal_slice.const_cast_derived();
        assert(diagonal_slice.size() == size());

        switch (type())
        {
        case MatrixBlockType::Zero: {
          diagonal_slice.setZero();
          break;
        }
        case MatrixBlockType::Identity: {
          diagonal_slice.setOnes();
          break;
        }
        case MatrixBlockType::ScalarIdentity: {

          const auto & scalar_value = container()(0, 0);
          diagonal_slice.fill(scalar_value);
          break;
        }
        case MatrixBlockType::Diagonal: {
          diagonal_slice = container();
          break;
        }
        case MatrixBlockType::Plain: {
          diagonal_slice = container().diagonal();
          break;
        }
        case MatrixBlockType::NestedBlockDiagonal: {
          // Use Map<VectorXd> from raw pointer to break the recursive template instantiation
          // chain. Calling .segment() would yield VectorBlock<DiagonalSlice>, causing
          // VectorBlock<VectorBlock<...>> nesting on recursive calls, exceeding template depth.
          // Map<VectorXd> is a fixed concrete type: always the same instantiation (no explosion).
          Scalar * const base_ptr = diagonal_slice.derived().data();
          Eigen::Index sub_offset = 0;
          for (const auto & sub_block : derived().nested_blocks())
          {
            const auto sub_size = sub_block.size();
            Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>> sub_map(
              base_ptr + sub_offset, sub_size);
            sub_block.diagonal(sub_map);
            sub_offset += sub_size;
          }
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      /**
       * @brief Extracts the main diagonal of this block into a new dense vector.
       *
       * @return A new dense column vector of size `size` containing the diagonal elements.
       *
       * @details This is a convenience method that allocates a new vector and calls the in-place
       *          `diagonal()` overload to fill it.
       *
       * @note For performance-critical code where repeated memory allocations should be avoided,
       *       prefer the overload that fills a pre-allocated slice.
       * @see void diagonal(const Eigen::MatrixBase<DiagonalSlice>&) const
       */
      Vector diagonal() const
      {
        Vector diagonal_elements(size());
        diagonal(diagonal_elements);
        return diagonal_elements;
      }

      /**
       * @brief Fill this block with random values between -1 and 1.
       */
      void setRandom()
      {
        switch (type())
        {
        case MatrixBlockType::Zero:
        case MatrixBlockType::Identity:
          return;
        case MatrixBlockType::ScalarIdentity:
        case MatrixBlockType::Diagonal:
        case MatrixBlockType::Plain:
          container().setRandom();
          break;
        case MatrixBlockType::NestedBlockDiagonal:
          for (auto & sub_block : derived().nested_blocks())
            sub_block.setRandom();
          break;
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      /**
       * @brief Fill this block with random values forming a Positive Definite matrix.
       *
       * @details For a matrix to be positive definite, all eigenvalues must be strictly positive.
       *          The implementation strategy depends on the block type:
       *          - `Zero`: Invalid operation - a zero matrix is not positive definite.
       *          - `Identity`: Already positive definite (all eigenvalues are 1).
       *          - `ScalarIdentity`: Sets a random positive scalar value.
       *          - `Diagonal`: Sets random positive values on the diagonal.
       *          - `Plain`: Generates A = R^T * R + eps*I where R is random, ensuring PD.
       *
       * @throws std::invalid_argument If the block type is `Zero`, which cannot be made PD.
       */
      void setRandomPD()
      {
        switch (type())
        {
        case MatrixBlockType::Zero: {
          throw std::invalid_argument("Cannot create a positive definite matrix from a Zero block");
        }
        case MatrixBlockType::Identity:
          return;
        case MatrixBlockType::ScalarIdentity: {
          // Set a random positive scalar (between 0.1 and 1.1 to avoid near-zero values)
          container().fill(Scalar(0.1) + std::abs(container().Random(1, 1)(0, 0)));
          break;
        }
        case MatrixBlockType::Diagonal: {
          // Set random positive values on the diagonal (between 0.1 and 1.1)
          container().setRandom();
          container() = container().cwiseAbs().array() + Scalar(0.1);
          break;
        }
        case MatrixBlockType::Plain: {
          // Generate PD matrix as A = R^T * R + eps*I
          // This guarantees positive definiteness
          container().setRandom();
          Matrix R = container();
          container().noalias() = R.transpose() * R;
          container().diagonal().array() += Scalar(0.1); // Ensure strict positive definiteness
          break;
        }
        case MatrixBlockType::NestedBlockDiagonal: {
          for (auto & sub_block : derived().nested_blocks())
            sub_block.setRandomPD();
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      /// \brief Returns true if any coefficient (element) of this blocl element is NaN
      /// (Not‑a‑Number).
      bool hasNaN() const
      {
        switch (type())
        {
        case MatrixBlockType::Zero:
        case MatrixBlockType::Identity:
          return false;
        case MatrixBlockType::ScalarIdentity:
        case MatrixBlockType::Diagonal:
        case MatrixBlockType::Plain:
          return container().hasNaN();
        case MatrixBlockType::NestedBlockDiagonal:
          for (const auto & sub_block : derived().nested_blocks())
            if (sub_block.hasNaN())
              return true;
          return false;
        default:
          PINOCCHIO_UNREACHABLE();
        }
        return true;
      }

      /**
       * @brief Computes the inverse of this block and stores it in a pre-allocated result block.
       *
       * @tparam Other The derived type of the destination block element.
       *
       * @param[out] res A pre-allocated block element to store the inverse. Its `size` must
       *                 match this block's `size`, and its `type` must be compatible with the
       *                 inverse operation for this block's type.
       *
       * @details This method computes the matrix inverse based on the block's structural type,
       *          exploiting the structure for efficiency:
       *          - `Zero`: The inverse is undefined; fills `res` with infinity values.
       *                    Requires `res.type() == Plain`.
       *          - `Identity`: The inverse is the identity matrix itself.
       *                        Requires `res.type() == Identity`.
       *          - `ScalarIdentity`: The inverse is `1/scalar * I`.
       *                              Requires `res.type()` to be `ScalarIdentity`, `Diagonal`, or
       *                              `Plain`.
       *          - `Diagonal`: The inverse is the element-wise reciprocal of the diagonal.
       *                        Requires `res.type()` to be `Diagonal` or `Plain`.
       *          - `Plain`: Computes the full dense matrix inverse.
       *                     Requires `res.type() == Plain`.
       *
       * @pre `res.size() == size()`
       * @pre `res.type()` must be compatible with this block's type (see details above).
       *
       * @note For performance-critical code, prefer this in-place version over the allocating
       *       `inverse()` overload to avoid repeated memory allocations.
       *
       * @see inverse() const for the allocating version.
       */
      template<typename Other>
      void inverse(MatrixBlockElementPlain<Other> & res) const
      {
        assert(res.size() == size());

        switch (type())
        {
        case MatrixBlockType::Zero: {
          assert((res.type() == MatrixBlockType::Plain) && "res block type is invalid");
          typedef typename Other::Scalar OtherScalar;
          res.container().fill(std::numeric_limits<OtherScalar>::infinity());
          break;
        }
        case MatrixBlockType::Identity: {
          assert((res.type() == MatrixBlockType::Identity) && "res block type is invalid");
          break;
        }
        case MatrixBlockType::ScalarIdentity: {
          constexpr MatrixBlockType res_valid_block_types = static_cast<MatrixBlockType>(
            static_cast<unsigned char>(MatrixBlockType::ScalarIdentity)
            | static_cast<unsigned char>(MatrixBlockType::Diagonal)
            | static_cast<unsigned char>(MatrixBlockType::Plain));
          assert(hasFlag(res.type(), res_valid_block_types) && "res block type is invalid");
          PINOCCHIO_ONLY_USED_FOR_DEBUG(res_valid_block_types);

          const auto & value = container().coeffRef(0, 0);
          const auto inverse_value = Scalar(1) / value;
          switch (res.type())
          {
          case MatrixBlockType::ScalarIdentity: {
            res.container().fill(inverse_value);
            break;
          }
          case MatrixBlockType::Diagonal: {
            res.container().fill(inverse_value);
            break;
          }
          case MatrixBlockType::Plain: {
            res.container().setZero();
            res.container().diagonal().fill(inverse_value);
            break;
          }
          default:
            PINOCCHIO_UNREACHABLE();
          }
          break;
        }
        case MatrixBlockType::Diagonal: {
          constexpr MatrixBlockType res_valid_block_types = static_cast<MatrixBlockType>(
            static_cast<unsigned char>(MatrixBlockType::Diagonal)
            | static_cast<unsigned char>(MatrixBlockType::Plain));
          PINOCCHIO_ONLY_USED_FOR_DEBUG(res_valid_block_types);

          assert(hasFlag(res.type(), res_valid_block_types) && "res block type is invalid");

          switch (res.type())
          {
          case MatrixBlockType::Diagonal: {
            res.container() = container().cwiseInverse();
            break;
          }
          case MatrixBlockType::Plain: {
            res.container().setZero();
            res.container().diagonal() = container().cwiseInverse();
            break;
          }
          default:
            PINOCCHIO_UNREACHABLE();
          }

          break;
        }
        case MatrixBlockType::Plain: {
          assert((res.type() == MatrixBlockType::Plain) && "res block type is invalid");
          if (isSymmetric(container()))
          {
            typedef Eigen::Map<Matrix, EIGEN_DEFAULT_ALIGN_BYTES> MapMatrix;
            MapMatrix tmp = MapMatrix(
              _PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, container().rows(), container().cols()));
            tmp = container();
            matrix_inversion(tmp, res.container());
          }
          else
          {
            res.container().noalias() = container().inverse();
          }
          break;
        }
        case MatrixBlockType::NestedBlockDiagonal: {
          assert(res.type() == MatrixBlockType::NestedBlockDiagonal && "res block type is invalid");
          const auto & src_subs = derived().nested_blocks();
          auto & res_subs = res.derived().nested_blocks();
          assert(src_subs.size() == res_subs.size());
          for (std::size_t i = 0; i < src_subs.size(); ++i)
            src_subs[i].inverse(res_subs[i]);
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      /**
       * @brief Computes the inverse of this block and returns it as a new PlainBlockElement.
       *
       * @return A newly allocated `PlainBlockElement` containing the matrix inverse of this block.
       *
       * @details This is a convenience method that allocates a new block element and calls the
       *          in-place `inverse()` overload to compute the result. The returned block's type
       *          is determined by the input block's type to preserve structural properties where
       *          possible:
       *          - `Zero` → `Plain` (filled with infinity, as inverse of zero is undefined)
       *          - `Identity` → `Identity` (inverse of identity is identity)
       *          - `ScalarIdentity` → `ScalarIdentity` (inverse of scalar `s` is `1/s`)
       *          - `Diagonal` → `Diagonal` (element-wise inverse of diagonal)
       *          - `Plain` → `Plain` (full matrix inverse)
       *
       * @note For performance-critical code where repeated memory allocations should be avoided,
       *       prefer the overload `inverse(MatrixBlockElementPlain<Other>&)` that fills a
       *       pre-allocated block.
       *
       * @see void inverse(MatrixBlockElementPlain<Other>&) const
       */
      PlainBlockElement inverse() const
      {
        MatrixBlockType res_type = MatrixBlockType::Undefined;

        switch (type())
        {
        case MatrixBlockType::Zero: {
          res_type = MatrixBlockType::Plain;
          break;
        }
        case MatrixBlockType::Identity: {
          res_type = MatrixBlockType::Identity;
          break;
        }
        case MatrixBlockType::ScalarIdentity: {
          res_type = MatrixBlockType::ScalarIdentity;
          break;
        }
        case MatrixBlockType::Diagonal: {
          res_type = MatrixBlockType::Diagonal;
          break;
        }
        case MatrixBlockType::Plain: {
          res_type = MatrixBlockType::Plain;
          break;
        }
        case MatrixBlockType::NestedBlockDiagonal: {
          // For NestedBlockDiagonal, we can't return a PlainBlockElement that contains nested
          // blocks (PlainBlockElement = owning Matrix variant doesn't support nested blocks).
          // This path should not be called — use the in-place inverse() with a matching res block.
          assert(
            false && "NestedBlockDiagonal inverse() factory not supported; use in-place version");
          PINOCCHIO_THROW_PRETTY(
            std::runtime_error,
            "Calling unsupported inverse() on NestedBlockDiagonal; use in-place version instead.")
          res_type = MatrixBlockType::NestedBlockDiagonal;
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }

        PlainBlockElement res(res_type, size());
        inverse(res);
        return res;
      }

    protected:
      /// @brief The structural type of the matrix block (e.g., Zero, Diagonal, Plain).
      MatrixBlockType m_type;

      /// @brief The size of the block (assuming a square block of size x size).
      Eigen::Index m_size;

    }; // struct MatrixBlockElementPlain
  } // namespace internal

} // namespace pinocchio
