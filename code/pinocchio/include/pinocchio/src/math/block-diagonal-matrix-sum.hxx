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

    template<typename Scalar, int Options, std::size_t Alignment, typename DiagonalVectorType>
    struct Sum<
      BlockDiagonalMatrixTpl<Scalar, Options, Alignment>,
      Eigen::DiagonalWrapper<DiagonalVectorType>>
    : BlockDiagonalMatrixExpression<Sum<
        BlockDiagonalMatrixTpl<Scalar, Options, Alignment>,
        Eigen::DiagonalWrapper<DiagonalVectorType>>>
    {
      typedef BlockDiagonalMatrixTpl<Scalar, Options, Alignment> LhsType;
      typedef Eigen::DiagonalWrapper<DiagonalVectorType> RhsType;

      Sum(const LhsType & lhs, const RhsType & rhs)
      : m_lhs(lhs)
      , m_rhs(rhs)
      {
      }

      /// @brief \copydoc Base::evalTo
      template<typename ResScalar, int ResOptions, std::size_t ResAlignment>
      void evalTo(BlockDiagonalMatrixTpl<ResScalar, ResOptions, ResAlignment> & res) const
      {
        typedef BlockDiagonalMatrixTpl<ResScalar, ResOptions, ResAlignment> ResType;

        const auto & diag = rhs().diagonal();
        PINOCCHIO_CHECK_ARGUMENT_SIZE(
          diag.size(), lhs().rows(),
          "The size of the diagonal expression does not match the number of rows of the block "
          "diagonal matrix.");

        // Check if rebuild is needed
        bool need_rebuild = false;
        if (
          res.rows() != lhs().rows() || res.cols() != lhs().cols()
          || res.blocks().size() != lhs().blocks().size())
        {
          need_rebuild = true;
        }
        else
        {
          for (std::size_t i = 0; i < lhs().blocks().size(); ++i)
          {
            const auto current_type = lhs().blocks()[i].type();

            if (current_type == MatrixBlockType::NestedBlockDiagonal)
            {
              // Outer stays NestedBlockDiagonal; check sub-blocks too
              if (res.blocks()[i].type() != MatrixBlockType::NestedBlockDiagonal)
              {
                need_rebuild = true;
                break;
              }
              const auto & src_subs = lhs().blocks()[i].nested_blocks();
              const auto & res_subs = res.blocks()[i].nested_blocks();
              if (src_subs.size() != res_subs.size())
              {
                need_rebuild = true;
                break;
              }
              for (std::size_t j = 0; j < src_subs.size(); ++j)
              {
                const auto sub_type = src_subs[j].type();
                const auto sub_target =
                  (sub_type != MatrixBlockType::Diagonal && sub_type != MatrixBlockType::Plain)
                    ? MatrixBlockType::Diagonal
                    : sub_type;
                if (res_subs[j].type() != sub_target)
                {
                  need_rebuild = true;
                  break;
                }
              }
              if (need_rebuild)
                break;
              continue;
            }

            // Upgrading rule: anything not Diagonal/Plain becomes Diagonal
            const auto target_type =
              (current_type != MatrixBlockType::Diagonal && current_type != MatrixBlockType::Plain)
                ? MatrixBlockType::Diagonal
                : current_type;

            if (res.blocks()[i].type() != target_type)
            {
              need_rebuild = true;
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

          const std::size_t num_blocks = lhs().blocks().size();

          // Check if any block needs type upgrading (non-Diagonal/Plain → Diagonal, including
          // nested sub-blocks). If no upgrading is needed we can rebuild directly from the original
          // pattern, avoiding any extra temporary allocation.
          bool needs_upgrade = false;
          for (std::size_t i = 0; i < num_blocks && !needs_upgrade; ++i)
          {
            const auto & block = lhs().blocks()[i];
            if (
              block.type() != MatrixBlockType::Diagonal && block.type() != MatrixBlockType::Plain
              && block.type() != MatrixBlockType::NestedBlockDiagonal)
            {
              needs_upgrade = true;
            }
            else if (block.type() == MatrixBlockType::NestedBlockDiagonal)
            {
              for (const auto & sub : block.nested_blocks())
                if (sub.type() != MatrixBlockType::Diagonal && sub.type() != MatrixBlockType::Plain)
                {
                  needs_upgrade = true;
                  break;
                }
            }
          }

          if (!needs_upgrade)
          {
            // Pattern is already compatible: rebuild directly from existing blocks.
            if (&res == &lhs())
            {
              tmp_res.rebuild(lhs().blocks());
              res_ptr = &tmp_res;
            }
            else
            {
              res.rebuild(lhs().blocks());
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
              const auto & block = lhs().blocks()[i];
              if (block.type() == MatrixBlockType::NestedBlockDiagonal)
              {
                has_nested_in_pattern = true;
                std::vector<MatrixBlockElement> new_subs;
                new_subs.reserve(block.nested_blocks().size());
                for (const auto & sub : block.nested_blocks())
                {
                  MatrixBlockType sub_type = sub.type();
                  if (sub_type != MatrixBlockType::Diagonal && sub_type != MatrixBlockType::Plain)
                    sub_type = MatrixBlockType::Diagonal;
                  new_subs.emplace_back(sub_type, sub.size());
                }
                new (new_pattern + i)
                  MatrixBlockElement(MatrixBlockType::NestedBlockDiagonal, std::move(new_subs));
              }
              else
              {
                MatrixBlockType new_type = block.type();
                if (new_type != MatrixBlockType::Diagonal && new_type != MatrixBlockType::Plain)
                  new_type = MatrixBlockType::Diagonal;
                new (new_pattern + i) MatrixBlockElement(new_type, block.size());
              }
            }

            if (&res == &lhs())
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

        Eigen::Index row_id = 0;
        for (std::size_t i = 0; i < lhs().blocks().size(); ++i)
        {
          const auto & lhs_block = lhs().blocks()[i];
          auto & res_block = res_ptr->blocks()[i];
          const auto block_size = lhs_block.size();
          const auto diag_segment = diag.segment(row_id, block_size);

          if (lhs_block.type() == MatrixBlockType::NestedBlockDiagonal)
          {
            const auto & lhs_subs = lhs_block.nested_blocks();
            auto & res_subs = res_block.nested_blocks();
            Eigen::Index sub_offset = 0;
            for (std::size_t j = 0; j < lhs_subs.size(); ++j)
            {
              const auto & lhs_sub = lhs_subs[j];
              auto & res_sub = res_subs[j];
              const auto sub_size = lhs_sub.size();
              const auto sub_diag = diag_segment.segment(sub_offset, sub_size);
              if (!need_rebuild && &res == &lhs())
              {
                // In-place: res += rhs
                if (res_sub.type() == MatrixBlockType::Diagonal)
                  res_sub.container() += sub_diag;
                else
                  res_sub.container().diagonal() += sub_diag;
              }
              else
              {
                if (res_sub.type() == MatrixBlockType::Diagonal)
                {
                  lhs_sub.diagonal(res_sub.container());
                  res_sub.container() += sub_diag;
                }
                else
                {
                  assert(lhs_sub.type() == MatrixBlockType::Plain);
                  lhs_sub.evalTo(res_sub.container());
                  res_sub.container().diagonal() += sub_diag;
                }
              }
              sub_offset += sub_size;
            }
          }
          else if (!need_rebuild && &res == &lhs())
          {
            // In-place: res += rhs
            if (res_block.type() == MatrixBlockType::Diagonal)
            {
              res_block.container() += diag_segment;
            }
            else
            {
              res_block.container().diagonal() += diag_segment;
            }
          }
          else
          {
            // res = lhs + rhs
            if (res_block.type() == MatrixBlockType::Diagonal)
            {
              assert(lhs_block.type() != MatrixBlockType::Plain);
              lhs_block.diagonal(res_block.container());
              res_block.container() += diag_segment;
            }
            else
            {
              assert(lhs_block.type() == MatrixBlockType::Plain);
              lhs_block.evalTo(res_block.container());
              res_block.container().diagonal() += diag_segment;
            }
          }

          row_id += block_size;
        }

        if (need_rebuild && &res == &lhs())
        {
          res = std::move(tmp_res);
        }
      }

      const LhsType & lhs() const
      {
        return m_lhs;
      }

      const RhsType & rhs() const
      {
        return m_rhs;
      }

      Eigen::Index rows() const
      {
        return m_lhs.rows();
      }

      Eigen::Index cols() const
      {
        return m_lhs.cols();
      }

      Eigen::Index size() const
      {
        return m_lhs.size();
      }

    protected:
      const LhsType & m_lhs; // block diagonal matrix ref
      const RhsType m_rhs;   // diagonal expression
    };

  } // namespace internal
} // namespace pinocchio
