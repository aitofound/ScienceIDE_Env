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

    template<typename MatrixOrMap, typename _LhsType, typename _RhsType>
    struct add_op_tpl<MatrixBlockElementTpl<MatrixOrMap>, _LhsType, _RhsType> : binary_op
    {
      typedef MatrixBlockElementTpl<MatrixOrMap> To;

      template<typename DiagonalVectorTypeLhs, typename DiagonalVectorTypeRhs>
      static void run(
        const Eigen::DiagonalWrapper<DiagonalVectorTypeLhs> & diagonal_matrix_lhs,
        const Eigen::DiagonalWrapper<DiagonalVectorTypeRhs> & diagonal_matrix_rhs,
        To & to)
      {
        constexpr MatrixBlockType res_valid_block_types = static_cast<MatrixBlockType>(
          static_cast<unsigned char>(MatrixBlockType::Diagonal)
          | static_cast<unsigned char>(MatrixBlockType::Plain));
        PINOCCHIO_ONLY_USED_FOR_DEBUG(res_valid_block_types);

        assert(hasFlag(to.type(), res_valid_block_types) && "res block type is invalid");
        switch (to.type())
        {
        case MatrixBlockType::Diagonal: {
          auto & to_diagonal = to.container();
          to_diagonal = diagonal_matrix_lhs.diagonal() + diagonal_matrix_rhs.diagonal();
          break;
        }
        case MatrixBlockType::Plain: {
          auto & plain_mat = to.container();
          plain_mat = diagonal_matrix_lhs + diagonal_matrix_rhs;
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      template<typename MatrixDerived, typename DiagonalVectorTypeRhs>
      static void run(
        const Eigen::MatrixBase<MatrixDerived> & matrix_lhs,
        const Eigen::DiagonalWrapper<DiagonalVectorTypeRhs> & diagonal_matrix_rhs,
        To & to)
      {

        assert(hasFlag(to.type(), MatrixBlockType::Plain) && "res block type is invalid");
        switch (to.type())
        {
        case MatrixBlockType::Plain: {
          auto & plain_mat = to.container();
          plain_mat = matrix_lhs;
          plain_mat += diagonal_matrix_rhs;
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }
    }; // struct add_op_tpl

    template<typename MatrixOrMap, typename _LhsType, typename _RhsType>
    struct sub_op_tpl<MatrixBlockElementTpl<MatrixOrMap>, _LhsType, _RhsType> : binary_op
    {
      typedef MatrixBlockElementTpl<MatrixOrMap> To;

      template<typename DiagonalVectorTypeLhs, typename DiagonalVectorTypeRhs>
      static void run(
        const Eigen::DiagonalWrapper<DiagonalVectorTypeLhs> & diagonal_matrix_lhs,
        const Eigen::DiagonalWrapper<DiagonalVectorTypeRhs> & diagonal_matrix_rhs,
        To & to)
      {
        constexpr MatrixBlockType res_valid_block_types = static_cast<MatrixBlockType>(
          static_cast<unsigned char>(MatrixBlockType::Diagonal)
          | static_cast<unsigned char>(MatrixBlockType::Plain));
        PINOCCHIO_ONLY_USED_FOR_DEBUG(res_valid_block_types);

        assert(hasFlag(to.type(), res_valid_block_types) && "res block type is invalid");
        switch (to.type())
        {
        case MatrixBlockType::Diagonal: {
          auto & to_diagonal = to.container();
          to_diagonal = diagonal_matrix_lhs.diagonal() - diagonal_matrix_rhs.diagonal();
          break;
        }
        case MatrixBlockType::Plain: {
          auto & plain_mat = to.container();
          plain_mat = diagonal_matrix_lhs - diagonal_matrix_rhs;
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      template<typename MatrixDerived, typename DiagonalVectorTypeRhs>
      static void run(
        const Eigen::MatrixBase<MatrixDerived> & matrix_lhs,
        const Eigen::DiagonalWrapper<DiagonalVectorTypeRhs> & diagonal_matrix_rhs,
        To & to)
      {

        assert(hasFlag(to.type(), MatrixBlockType::Plain) && "res block type is invalid");
        switch (to.type())
        {
        case MatrixBlockType::Plain: {
          auto & plain_mat = to.container();
          plain_mat = matrix_lhs;
          plain_mat -= diagonal_matrix_rhs;
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }
    }; // struct sub_op_tpl

    template<typename BinaryOp, typename MatrixOrMap, typename Enable, typename DiagonalVectorType>
    struct traits<BinaryOperator<
      BinaryOp,
      MatrixBlockElementTpl<MatrixOrMap, Enable>,
      Eigen::DiagonalWrapper<DiagonalVectorType>>>
    {
      typedef MatrixBlockElementTpl<MatrixOrMap, Enable> LhsType;
      typedef typename traits<LhsType>::Matrix Matrix;
    };

    template<typename BinaryOp, typename MatrixOrMap, typename Enable, typename DiagonalVectorType>
    struct BinaryOperator<
      BinaryOp,
      MatrixBlockElementTpl<MatrixOrMap, Enable>,
      Eigen::DiagonalWrapper<DiagonalVectorType>>
    : MatrixBlockElementOperation<BinaryOperator<
        BinaryOp,
        MatrixBlockElementTpl<MatrixOrMap, Enable>,
        Eigen::DiagonalWrapper<DiagonalVectorType>>>
    {
      static_assert(
        std::is_base_of_v<pinocchio::internal::binary_op, BinaryOp>,
        "BinaryOp should be a binary operator.");

      typedef MatrixBlockElementTpl<MatrixOrMap, Enable> LhsType;
      typedef Eigen::DiagonalWrapper<DiagonalVectorType> RhsType;

      BinaryOperator(const LhsType & lhs, const RhsType & rhs)
      : m_lhs(lhs)
      , m_rhs(rhs)
      {
      }

      const LhsType & lhs() const
      {
        return m_lhs;
      }
      const RhsType & rhs() const
      {
        return m_rhs;
      }

      template<typename ResType>
      void evalTo(ResType & res) const
      {
        run(lhs(), rhs(), res);
      }

      template<typename ResType>
      static void
      run(const LhsType & lhs_block_elt, const RhsType & diagonal_matrix, ResType & res_block_elt)
      {

        typedef typename BinaryOp::template op_tpl<ResType> Op;

        constexpr MatrixBlockType res_valid_block_types = static_cast<MatrixBlockType>(
          static_cast<unsigned char>(MatrixBlockType::Diagonal)
          | static_cast<unsigned char>(MatrixBlockType::Plain));
        assert(hasFlag(res_block_elt.type(), res_valid_block_types) && "res block type is invalid");
        PINOCCHIO_ONLY_USED_FOR_DEBUG(res_valid_block_types);

        // typedef typename LhsType::Matrix LhsMatrix;
        typedef typename LhsType::Vector LhsVector;

        const auto lhs_size = lhs_block_elt.size();
        switch (lhs_block_elt.type())
        {
        case MatrixBlockType::Identity: {
          const auto lhs_matrix = LhsVector::Ones(lhs_size).asDiagonal();
          // const auto lhs_matrix = LhsMatrix::Identity(lhs_size,lhs_size);
          Op::run(lhs_matrix, diagonal_matrix, res_block_elt);
          break;
        }
        case MatrixBlockType::Zero: {
          const auto lhs_matrix = LhsVector::Zero(lhs_size).asDiagonal();
          Op::run(lhs_matrix, diagonal_matrix, res_block_elt);
          break;
        }
        case MatrixBlockType::ScalarIdentity: {
          const auto & mat = lhs_block_elt.container();
          const auto & scalar = mat(0, 0);
          const auto lhs_matrix = LhsVector::Constant(lhs_size, scalar).asDiagonal();
          Op::run(lhs_matrix, diagonal_matrix, res_block_elt);
          break;
        }
        case MatrixBlockType::Diagonal: {
          const auto & diagonal_elt = lhs_block_elt.container();
          const auto lhs_matrix = diagonal_elt.asDiagonal();
          Op::run(lhs_matrix, diagonal_matrix, res_block_elt);
          break;
        }
        case MatrixBlockType::Plain: {
          const auto & plain_mat = lhs_block_elt.container();
          Op::run(plain_mat, diagonal_matrix, res_block_elt);
          break;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

    protected:
      const LhsType & m_lhs;
      const RhsType m_rhs; // rhs is a DiagonalWrapper, copying it is fine.
    };

  } // namespace internal

} // namespace pinocchio
