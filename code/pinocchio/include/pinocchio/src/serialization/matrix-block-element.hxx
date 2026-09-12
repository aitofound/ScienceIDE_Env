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
      /// Accessor for the owning variant (Eigen::Matrix).
      template<typename MatrixType, typename = void>
      struct MatrixBlockElementTplAccessor
      : public ::pinocchio::internal::MatrixBlockElementTpl<MatrixType>
      {
        typedef ::pinocchio::internal::MatrixBlockElementTpl<MatrixType> Base;
        using Base::m_size;
        using Base::m_type;
      };

      /// Accessor for the non-owning Map variant — also exposes m_nested_blocks.
      template<typename MapType>
      struct MatrixBlockElementTplAccessor<
        MapType,
        std::enable_if_t<!pinocchio::internal::helper::is_eigen_matrix_v<MapType>>>
      : public ::pinocchio::internal::MatrixBlockElementTpl<MapType>
      {
        typedef ::pinocchio::internal::MatrixBlockElementTpl<MapType> Base;
        using Base::m_nested_blocks;
        using Base::m_size;
        using Base::m_type;
      };
    } // namespace internal

    template<typename Archive, typename Matrix>
    void serialize(
      Archive & ar,
      ::pinocchio::internal::MatrixBlockElementTpl<Matrix> & _matrix_block_element,
      const unsigned int /*version*/)
    {
      typedef internal::MatrixBlockElementTplAccessor<Matrix> Accessor;
      Accessor & matrix_block_element = static_cast<Accessor &>(_matrix_block_element);

      ar & make_nvp("type", matrix_block_element.m_type);
      ar & make_nvp("size", matrix_block_element.m_size);

      if constexpr (pinocchio::internal::helper::is_eigen_matrix_v<Matrix>)
      {
        auto & container = matrix_block_element.container();
        ar & make_nvp("container", container);
      }
      else
      {
        // Map variant: serialize nested block structure for NestedBlockDiagonal.
        if (
          matrix_block_element.m_type == pinocchio::internal::MatrixBlockType::NestedBlockDiagonal)
        {
          ar & make_nvp("nested_blocks", matrix_block_element.m_nested_blocks);
        }
      }
    }

  } // namespace serialization
} // namespace boost
