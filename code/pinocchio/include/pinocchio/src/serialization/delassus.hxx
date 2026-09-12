//
// Copyright (c) 2025-2026 INRIA
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

    template<class Archive, typename Derived>
    void serialize(
      Archive & ar,
      ::pinocchio::DelassusOperatorBase<Derived> & delassus,
      const unsigned int version)
    {
      // Nothing to do yet
      PINOCCHIO_UNUSED_VARIABLE(ar);
      PINOCCHIO_UNUSED_VARIABLE(delassus);
      PINOCCHIO_UNUSED_VARIABLE(version);
    }

    namespace internal
    {

      template<typename Scalar, int Options>
      struct DelassusOperatorDenseAccessor
      : public ::pinocchio::DelassusOperatorDenseTpl<Scalar, Options>
      {
        typedef ::pinocchio::DelassusOperatorDenseTpl<Scalar, Options> Base;
        using Base::m_cholesky_decomposition_data;
        using Base::m_cholesky_decomposition_data_storage;
        using Base::m_cholesky_decomposition_dirty;
        using Base::m_compliance;
        using Base::m_compliance_storage;
        using Base::m_damping;
        using Base::m_delassus_matrix;
        using Base::m_delassus_matrix_storage;
      };

    } // namespace internal

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      ::pinocchio::DelassusOperatorDenseTpl<Scalar, Options> & delassus,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::DelassusOperatorDenseTpl<Scalar, Options> Self;
      typedef typename Self::Base Base;

      ar & make_nvp("base", boost::serialization::base_object<Base>(delassus));

      typedef internal::DelassusOperatorDenseAccessor<Scalar, Options> Accessor;
      auto & delassus_ = reinterpret_cast<Accessor &>(delassus);
      ar & make_nvp("delassus_matrix_storage", delassus_.m_delassus_matrix_storage);
      ar & make_nvp(
        "cholesky_decomposition_data_storage", delassus_.m_cholesky_decomposition_data_storage);
      ar & make_nvp("cholesky_decomposition_dirty", delassus_.m_cholesky_decomposition_dirty);
      ar & make_nvp("damping", delassus_.m_damping);
      ar & make_nvp("compliance_storage", delassus_.m_compliance_storage);
    }

  } // namespace serialization
} // namespace boost
