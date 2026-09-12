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
      template<typename StdVectorLike>
      struct DoubleEntryContainerAccessor
      : public ::pinocchio::internal::DoubleEntryContainer<StdVectorLike>
      {
        typedef ::pinocchio::internal::DoubleEntryContainer<StdVectorLike> Base;
        using Base::m_keys;
        using Base::m_values;
      };
    } // namespace internal

    template<typename Archive, typename StdVectorLike>
    void serialize(
      Archive & ar,
      ::pinocchio::internal::DoubleEntryContainer<StdVectorLike> & container,
      const unsigned int /*version*/)
    {
      typedef internal::DoubleEntryContainerAccessor<StdVectorLike> Accessor;
      Accessor & container_ = static_cast<Accessor &>(container);
      ar & make_nvp("m_keys", container_.m_keys);
      ar & make_nvp("m_values", container_.m_values);
    }

  } // namespace serialization
} // namespace boost
