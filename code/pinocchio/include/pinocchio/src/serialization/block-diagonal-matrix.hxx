//
// Copyright (c) 2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/serialization.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/serialization.hpp"
#endif // PINOCCHIO_LSP

namespace boost
{
  namespace serialization
  {

    namespace internal
    {
      template<typename Scalar, int Options, std::size_t Alignment>
      struct BlockDiagonalMatrixAccessor
      : public ::pinocchio::internal::BlockDiagonalMatrixTpl<Scalar, Options, Alignment>
      {
        typedef ::pinocchio::internal::BlockDiagonalMatrixTpl<Scalar, Options, Alignment> Base;
        using Base::m_cols;
        using Base::m_matrix_block_elements;
        using Base::m_matrix_stack;
        using Base::m_rows;
      };
    } // namespace internal

    template<typename Archive, typename Scalar, int Options, std::size_t Alignment>
    void serialize(
      Archive & ar,
      ::pinocchio::internal::BlockDiagonalMatrixTpl<Scalar, Options, Alignment> & matrix,
      const unsigned int /*version*/)
    {
      typedef internal::BlockDiagonalMatrixAccessor<Scalar, Options, Alignment> Accessor;
      Accessor & matrix_ = static_cast<Accessor &>(matrix);
      ar & make_nvp("m_rows", matrix_.m_rows);
      ar & make_nvp("m_cols", matrix_.m_cols);
      ar & make_nvp("m_matrix_stack", matrix_.m_matrix_stack);

      auto & m_matrix_block_elements = matrix_.m_matrix_block_elements;
      ar & make_nvp("m_matrix_block_elements", m_matrix_block_elements);
      std::vector<std::size_t> index_map;
      index_map.reserve(m_matrix_block_elements.size());
      if (Archive::is_loading::value)
      {
        ar & make_nvp("index_map", index_map);
        std::size_t idx = 0;
        for (auto & block : m_matrix_block_elements)
        {
          if (block.type() == ::pinocchio::internal::MatrixBlockType::NestedBlockDiagonal)
          {
            // Remap each sub-block; the outer NestedBlockDiagonal block itself has no data.
            for (auto & sub : block.nested_blocks())
            {
              const auto index_value = index_map[idx++];
              if (index_value != std::numeric_limits<std::size_t>::max())
                sub.remap(matrix_.m_matrix_stack[index_value]);
            }
          }
          else
          {
            const auto index_value = index_map[idx++];
            if (index_value != std::numeric_limits<std::size_t>::max())
              block.remap(matrix_.m_matrix_stack[index_value]);
          }
        }
      }
      else
      {
        const auto & m_matrix_stack = matrix_.m_matrix_stack;

        const auto push_data_index = [&](const void * data_ptr) {
          if (data_ptr != nullptr)
          {
            const auto it = std::find_if(
              m_matrix_stack.begin(), m_matrix_stack.end(),
              [&data_ptr](const auto & stack_elt) { return data_ptr == stack_elt.data(); });
            assert(it != m_matrix_stack.end() && "must_never happened");
            index_map.push_back(std::size_t(std::distance(m_matrix_stack.begin(), it)));
          }
          else
            index_map.push_back(std::numeric_limits<std::size_t>::max());
        };

        for (const auto & block : m_matrix_block_elements)
        {
          if (block.type() == ::pinocchio::internal::MatrixBlockType::NestedBlockDiagonal)
          {
            // The outer block has no data; push one entry per sub-block.
            for (const auto & sub : block.nested_blocks())
              push_data_index(sub.data());
          }
          else
          {
            push_data_index(block.data());
          }
        }

        ar & make_nvp("index_map", index_map);
      }
    }

  } // namespace serialization
} // namespace boost
