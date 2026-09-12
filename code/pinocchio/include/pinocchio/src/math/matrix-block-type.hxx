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
    /// @brief Enumeration of block types for structured or generic matrix blocks.
    enum class MatrixBlockType : unsigned char
    {
      /// Block equal to the identity matrix.
      Identity = 1,

      /// Block filled with zeros.
      Zero = 2,

      /// The block is a scalar multiple of the identity matrix (s*I).
      ScalarIdentity = 4,

      /// Diagonal block with arbitrary diagonal coefficients.
      Diagonal = 8,

      /// Generic, dense block with arbitrary values.
      Plain = 16,

      /// A block that is itself block-diagonal, composed of a list of sub-blocks.
      /// Used to represent a pool of constraints as a single outer block.
      NestedBlockDiagonal = 32,

      /// Undefined type.
      Undefined = 64
    }; // enum class MatrixBlockType

    ///  @brief Block type tags
    template<MatrixBlockType val>
    struct MatrixBlockTypeTag
    {
    }; // struct MatrixBlockTypeTag

    constexpr bool hasFlag(MatrixBlockType value, MatrixBlockType flag)
    {
      using T = std::underlying_type_t<MatrixBlockType>;
      return (static_cast<T>(value) & static_cast<T>(flag)) != 0;
    }

    /// @brief Helper constexpr to test whether a block type implies structural sparsity.
    constexpr bool isStructuredBlock(MatrixBlockType type)
    {
      constexpr MatrixBlockType structured_block_types = static_cast<MatrixBlockType>(
        static_cast<unsigned char>(MatrixBlockType::Identity)
        | static_cast<unsigned char>(MatrixBlockType::Zero)
        | static_cast<unsigned char>(MatrixBlockType::Diagonal));
      return hasFlag(type, structured_block_types);
    }

    /// @brief Helper constexpr to test whether a block type implies raw non-trivial data.
    /// Example: Identity and Zero don't need to be associated to any data.
    /// On the contrary, ScalarIdentity, Diagonal and Plain are typically associated to some data.
    /// NestedBlockDiagonal stores data only in its sub-blocks, not in the outer block itself.
    constexpr bool isDataBlock(MatrixBlockType type)
    {
      constexpr MatrixBlockType data_block_types = static_cast<MatrixBlockType>(
        static_cast<unsigned char>(MatrixBlockType::ScalarIdentity)
        | static_cast<unsigned char>(MatrixBlockType::Diagonal)
        | static_cast<unsigned char>(MatrixBlockType::Plain));
      return hasFlag(type, data_block_types);
    }

    /// @brief Helper constexpr to test whether a block is a nested block-diagonal.
    constexpr bool isNestedBlock(MatrixBlockType type)
    {
      return type == MatrixBlockType::NestedBlockDiagonal;
    }

  } // namespace internal

} // namespace pinocchio
