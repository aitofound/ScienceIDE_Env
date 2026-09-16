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
      template<typename MatrixLike>
      struct EigenStorageAccessor : public ::pinocchio::internal::EigenStorageTpl<MatrixLike>
      {
        typedef ::pinocchio::internal::EigenStorageTpl<MatrixLike> Base;
        using Base::m_map;
        using Base::m_storage;
      };
    } // namespace internal

    template<typename Archive, typename MatrixLike>
    void serialize(
      Archive & ar,
      ::pinocchio::internal::EigenStorageTpl<MatrixLike> & storage,
      const unsigned int /*version*/)
    {
      Eigen::Index rows = storage.rows();
      Eigen::Index cols = storage.cols();

      ar & make_nvp("rows", rows);
      ar & make_nvp("cols", cols);

      if (Archive::is_loading::value)
      {
        // make internal maps point to correct data
        storage.resize(rows, cols);
      }

      typedef internal::EigenStorageAccessor<MatrixLike> Accessor;
      Accessor & storage_ = static_cast<Accessor &>(storage);
      // only serialize the "active" part of the storage
      ar & make_nvp("data", storage_.m_map);
    }

  } // namespace serialization
} // namespace boost
