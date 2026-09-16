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

    template<typename Scalar, int Options, std::size_t Alignment>
    struct Inverse<BlockDiagonalMatrixTpl<Scalar, Options, Alignment>>
    : BlockDiagonalMatrixExpression<Inverse<BlockDiagonalMatrixTpl<Scalar, Options, Alignment>>>
    {
      typedef BlockDiagonalMatrixTpl<Scalar, Options, Alignment> Derived;
      typedef BlockDiagonalMatrixExpression<Derived> Base;

      Inverse(const Derived & matrix)
      : m_matrix(matrix)
      {
      }

      /// @brief \copydoc Base::evalTo
      template<typename ResScalar, int ResOptions, std::size_t ResAlignment>
      void evalTo(BlockDiagonalMatrixTpl<ResScalar, ResOptions, ResAlignment> & res) const
      {
        typedef BlockDiagonalMatrixTpl<ResScalar, ResOptions, ResAlignment> ResType;

        // Check if rebuild is needed
        bool need_rebuild = false;
        if (
          res.rows() != m_matrix.rows() || res.cols() != m_matrix.cols()
          || res.blocks().size() != m_matrix.blocks().size())
        {
          need_rebuild = true;
        }
        else
        {
          for (std::size_t i = 0; i < m_matrix.blocks().size(); ++i)
          {
            // Upgrading rule: inverse block keeps the same type except for Zero which becomes
            // Plain.
            const auto current_type = m_matrix.blocks()[i].type();
            auto target_type = current_type;
            if (current_type == MatrixBlockType::Zero)
              target_type = MatrixBlockType::Plain;

            if (res.blocks()[i].type() != target_type)
            {
              need_rebuild = true;
              break;
            }
            if (current_type == MatrixBlockType::NestedBlockDiagonal)
            {
              const auto & src_subs = m_matrix.blocks()[i].nested_blocks();
              const auto & res_subs = res.blocks()[i].nested_blocks();
              if (src_subs.size() != res_subs.size())
              {
                need_rebuild = true;
                break;
              }
              for (std::size_t j = 0; j < src_subs.size(); ++j)
              {
                auto sub_target = src_subs[j].type();
                if (sub_target == MatrixBlockType::Zero)
                  sub_target = MatrixBlockType::Plain;
                if (res_subs[j].type() != sub_target)
                {
                  need_rebuild = true;
                  break;
                }
              }
              if (need_rebuild)
                break;
            }
          }
        }

        ResType * res_ptr = &res;
        ResType tmp_res;

        if (need_rebuild)
        {
          typedef Eigen::Matrix<ResScalar, Eigen::Dynamic, Eigen::Dynamic, ResOptions> ResMatrix;
          typedef Eigen::Map<ResMatrix, to_eigen_alignment(ResAlignment)> ResMatrixMap;
          typedef MatrixBlockElementTpl<ResMatrixMap> ResMatrixBlockElement;

          typedef typename ResType::MatrixBlockElement MatrixBlockElement;
          static_assert(
            pinocchio::internal::is_same_type<MatrixBlockElement, ResMatrixBlockElement>::value,
            "MatrixBlockElement is not of type "
            "pinocchio::internal::MatrixBlockElementTpl<Eigen::Map>");

          const std::size_t num_blocks = m_matrix.blocks().size();

          // Check if any block needs type upgrading (Zero→Plain, including nested sub-blocks).
          // If no upgrading is needed we can rebuild directly from the original pattern,
          // avoiding any extra temporary allocation.
          bool needs_upgrade = false;
          for (std::size_t i = 0; i < num_blocks && !needs_upgrade; ++i)
          {
            const auto & block = m_matrix.blocks()[i];
            if (block.type() == MatrixBlockType::Zero)
            {
              needs_upgrade = true;
            }
            else if (block.type() == MatrixBlockType::NestedBlockDiagonal)
            {
              for (const auto & sub : block.nested_blocks())
                if (sub.type() == MatrixBlockType::Zero)
                {
                  needs_upgrade = true;
                  break;
                }
            }
          }

          if (!needs_upgrade)
          {
            // Pattern is already compatible: rebuild directly from existing blocks.
            if (&res == &m_matrix)
            {
              tmp_res.rebuild(m_matrix.blocks());
              res_ptr = &tmp_res;
            }
            else
            {
              res.rebuild(m_matrix.blocks());
            }
          }
          else
          {
            // Need to upgrade some block types. Use alloca for the outer pattern array.
            // For NestedBlockDiagonal entries, placement-new with a moved sub-vector and
            // call the destructor explicitly afterwards to avoid a memory leak.
            MatrixBlockElement * new_pattern = static_cast<MatrixBlockElement *>(
              _PINOCCHIO_ALLOCA(num_blocks * sizeof(MatrixBlockElement)));
            bool has_nested_in_pattern = false;
            for (std::size_t i = 0; i < num_blocks; ++i)
            {
              const auto & block = m_matrix.blocks()[i];
              if (block.type() == MatrixBlockType::NestedBlockDiagonal)
              {
                has_nested_in_pattern = true;
                std::vector<MatrixBlockElement> new_subs;
                new_subs.reserve(block.nested_blocks().size());
                for (const auto & sub : block.nested_blocks())
                {
                  MatrixBlockType sub_type = sub.type();
                  if (sub_type == MatrixBlockType::Zero)
                    sub_type = MatrixBlockType::Plain;
                  new_subs.emplace_back(sub_type, sub.size());
                }
                new (new_pattern + i)
                  MatrixBlockElement(MatrixBlockType::NestedBlockDiagonal, std::move(new_subs));
              }
              else
              {
                MatrixBlockType new_type = block.type();
                if (new_type == MatrixBlockType::Zero)
                  new_type = MatrixBlockType::Plain;
                new (new_pattern + i) MatrixBlockElement(new_type, block.size());
              }
            }

            if (&res == &m_matrix)
            {
              tmp_res.rebuild(new_pattern, num_blocks);
              res_ptr = &tmp_res;
            }
            else
            {
              res.rebuild(new_pattern, num_blocks);
            }

            // Explicitly destroy alloca'd entries to release any heap memory held by nested
            // sub-block std::vectors.
            if (has_nested_in_pattern)
              for (std::size_t i = 0; i < num_blocks; ++i)
                new_pattern[i].~MatrixBlockElement();
          }
        }

        const auto num_blocks = m_matrix.blocks().size();
        for (size_t block_id = 0; block_id < num_blocks; ++block_id)
        {
          const auto & input_block = m_matrix.blocks()[block_id];
          auto & res_block = res_ptr->blocks()[block_id];

          if (input_block.type() == MatrixBlockType::NestedBlockDiagonal)
          {
            const bool inplace_aliasing = (!need_rebuild && &res == &m_matrix);
            const auto & input_subs = input_block.nested_blocks();
            auto & res_subs = res_block.nested_blocks();
            for (std::size_t j = 0; j < input_subs.size(); ++j)
            {
              const auto & input_sub = input_subs[j];
              auto & res_sub = res_subs[j];
              if (inplace_aliasing && input_sub.type() == MatrixBlockType::Plain)
              {
                // Aliasing: copy sub-block before inverting.
                const auto sub_size = input_sub.size();
                typedef Eigen::Map<typename ResType::Matrix, to_eigen_alignment(ResAlignment)>
                  ResMatrixMap;
                typedef MatrixBlockElementTpl<ResMatrixMap> ResMatrixBlockElement;
                ResMatrixMap tmp_map(_PINOCCHIO_EIGEN_MAP_ALLOCA_ALIGNED(
                  Scalar, sub_size, sub_size, static_cast<intptr_t>(ResAlignment)));
                ResMatrixBlockElement temp_input(input_sub.type(), sub_size, tmp_map);
                temp_input.container() = input_sub.container();
                temp_input.inverse(res_sub);
              }
              else
              {
                input_sub.inverse(res_sub);
              }
            }
          }
          else if (
            !need_rebuild        //
            && &res == &m_matrix //
            && input_block.type() == MatrixBlockType::Plain)
          {
            // Be careful of aliasing on plain blocks, otherwise it's fine.
            // If there is aliasing, we copy the block to invert, then we invert it.
            const auto block_size = input_block.size();
            typedef Eigen::Map<typename ResType::Matrix, to_eigen_alignment(ResAlignment)>
              ResMatrixMap;
            typedef MatrixBlockElementTpl<ResMatrixMap> ResMatrixBlockElement;

            ResMatrixMap tmp_map(_PINOCCHIO_EIGEN_MAP_ALLOCA_ALIGNED(
              Scalar, block_size, block_size, static_cast<intptr_t>(ResAlignment)));
            ResMatrixBlockElement temp_input(input_block.type(), block_size, tmp_map);

            temp_input.container() = input_block.container();
            temp_input.inverse(res_block);
          }
          else
          {
            input_block.inverse(res_block);
          }
        }

        if (need_rebuild && &res == &m_matrix)
        {
          res = std::move(tmp_res);
        }
      }

      /// @brief Returns the total number of rows of the full matrix.
      Eigen::Index rows() const
      {
        return m_matrix.rows();
      }

      /// @brief Returns the total number of cols of the full matrix.
      Eigen::Index cols() const
      {
        return m_matrix.cols();
      }

      /// @brief Returns the total number of elements in the full matrix (rows * cols).
      Eigen::Index size() const
      {
        return m_matrix.size();
      }

    protected:
      const Derived & m_matrix;
    }; // struct BlockDiagonalMatrixInverse

  } // namespace internal
} // namespace pinocchio
