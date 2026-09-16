//
// Copyright (c) 2025 INRIA
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
      template<typename MatrixLike, std::size_t Alignment>
      struct MatrixStackAccessor
      : public ::pinocchio::internal::MatrixStackTpl<MatrixLike, Alignment>
      {
        typedef ::pinocchio::internal::MatrixStackTpl<MatrixLike, Alignment> Base;
        using Base::m_data_ptr;
        using Base::m_matrix_maps;
        using Base::m_memory_capacity;
        using Base::m_offsets;

        using Base::free;
        using Base::incr_ptr;
        using Base::malloc;
        using Base::raw_size;
      };
    } // namespace internal

    template<typename Archive, typename MatrixLike, std::size_t Alignment>
    void serialize(
      Archive & ar,
      ::pinocchio::internal::MatrixStackTpl<MatrixLike, Alignment> & matrix_stack,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::internal::MatrixStackTpl<MatrixLike, Alignment> MatrixStack;
      typedef typename MatrixStack::MapType MapType;
      typedef typename MatrixStack::Scalar Scalar;
      typedef internal::MatrixStackAccessor<MatrixLike, Alignment> Accessor;
      Accessor & matrix_stack_ = static_cast<Accessor &>(matrix_stack);

      auto & offsets = matrix_stack_.m_offsets;
      auto & memory_capacity = matrix_stack_.m_memory_capacity;
      auto & matrix_maps = matrix_stack_.m_matrix_maps;

      if (Archive::is_loading::value)
        matrix_stack.clear();

      std::size_t real_memory_capacity = 0;
      ar & make_nvp("offsets", offsets);
      if (Archive::is_loading::value)
      {
        ar & make_nvp("memory_capacity", real_memory_capacity);
        memory_capacity = real_memory_capacity;
      }
      else
      {
        real_memory_capacity = matrix_stack_.raw_size();
        ar & make_nvp("memory_capacity", real_memory_capacity);
      }

      auto & data_ptr = matrix_stack_.m_data_ptr;
      if (Archive::is_loading::value)
      {
        Accessor::free(data_ptr);
        if (real_memory_capacity > 0)
        {
          data_ptr = Accessor::malloc(real_memory_capacity);
          assert(data_ptr != nullptr);
          assert(reinterpret_cast<std::size_t>(data_ptr) % Alignment == 0);
        }
        else
        {
          data_ptr = nullptr;
        }
      }
      ar & make_nvp(
        "data",
        make_array<char>(
          reinterpret_cast<char *>(data_ptr), real_memory_capacity)); // Raw copy of the data buffer

      if (Archive::is_loading::value)
      {
        for (std::size_t i = 0; i < offsets.size(); ++i)
        {
          const auto offset = offsets[i];
          Eigen::Index rows = -1, cols = -1;
          ar & make_nvp("rows", rows);
          ar & make_nvp("cols", cols);
          MapType matrix_map(
            static_cast<Scalar *>(Accessor::incr_ptr(data_ptr, offset)), rows, cols);
          matrix_maps.push_back(matrix_map);
        }
      }
      else // writting mode
      {
        for (std::size_t i = 0; i < matrix_stack.size(); ++i)
        {
          auto & matrix_map = matrix_stack[i];
          Eigen::Index rows = matrix_map.rows(), cols = matrix_map.cols();
          ar & make_nvp("rows", rows);
          ar & make_nvp("cols", cols);
        }
      }
    }

  } // namespace serialization
} // namespace boost
