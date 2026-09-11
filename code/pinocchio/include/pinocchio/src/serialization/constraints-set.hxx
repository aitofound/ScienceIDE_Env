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

    template<typename Archive, typename Derived>
    void serialize(Archive & ar, ::pinocchio::SetBase<Derived> & set, const unsigned int version)
    {
      // Nothing to do
      PINOCCHIO_UNUSED_VARIABLE(ar);
      PINOCCHIO_UNUSED_VARIABLE(set);
      PINOCCHIO_UNUSED_VARIABLE(version);
    }

    template<typename Archive, typename Derived>
    void
    serialize(Archive & ar, ::pinocchio::ConeBase<Derived> & set, const unsigned int /*version*/)
    {
      typedef ::pinocchio::ConeBase<Derived> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));
    }

    namespace internal
    {
      template<typename Scalar, int Options>
      struct BoxSetAccessor : public ::pinocchio::BoxSetTpl<Scalar, Options>
      {
        typedef ::pinocchio::BoxSetTpl<Scalar, Options> Base;
        using Base::m_lb;
        using Base::m_ub;
      };
    } // namespace internal

    template<typename Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar, ::pinocchio::BoxSetTpl<Scalar, Options> & set, const unsigned int /*version*/)
    {
      typedef ::pinocchio::BoxSetTpl<Scalar, Options> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      typedef internal::BoxSetAccessor<Scalar, Options> Accessor;
      auto & set_ = reinterpret_cast<Accessor &>(set);
      ar & make_nvp("m_lb", set_.m_lb);
      ar & make_nvp("m_ub", set_.m_ub);
    }

    namespace internal
    {
      template<typename Scalar, int Options>
      struct FullSpaceConeAccessor : public ::pinocchio::FullSpaceConeTpl<Scalar, Options>
      {
        typedef ::pinocchio::FullSpaceConeTpl<Scalar, Options> Base;
        using Base::m_size;
      };
    } // namespace internal

    template<typename Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      ::pinocchio::FullSpaceConeTpl<Scalar, Options> & set,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::FullSpaceConeTpl<Scalar, Options> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      typedef internal::FullSpaceConeAccessor<Scalar, Options> Accessor;
      auto & set_ = reinterpret_cast<Accessor &>(set);
      ar & make_nvp("m_size", set_.m_size);
    }

    namespace internal
    {
      template<typename Scalar, int Options>
      struct ZeroConeAccessor : public ::pinocchio::ZeroConeTpl<Scalar, Options>
      {
        typedef ::pinocchio::ZeroConeTpl<Scalar, Options> Base;
        using Base::m_size;
      };
    } // namespace internal

    template<typename Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar, ::pinocchio::ZeroConeTpl<Scalar, Options> & set, const unsigned int /*version*/)
    {
      typedef ::pinocchio::ZeroConeTpl<Scalar, Options> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      typedef internal::ZeroConeAccessor<Scalar, Options> Accessor;
      auto & set_ = reinterpret_cast<Accessor &>(set);
      ar & make_nvp("m_size", set_.m_size);
    }

    template<typename Archive, typename Scalar>
    void serialize(
      Archive & ar,
      ::pinocchio::CoulombFrictionConeTpl<Scalar> & set,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::CoulombFrictionConeTpl<Scalar> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      ar & make_nvp("mu", set.mu);
    }

    template<typename Archive, typename Scalar>
    void serialize(
      Archive & ar,
      ::pinocchio::DualCoulombFrictionConeTpl<Scalar> & set,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::DualCoulombFrictionConeTpl<Scalar> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      ar & make_nvp("mu", set.mu);
    }

    namespace internal
    {
      template<typename Scalar>
      struct NonNegativeOrthantConeAccessor : public ::pinocchio::NonNegativeOrthantConeTpl<Scalar>
      {
        typedef ::pinocchio::NonNegativeOrthantConeTpl<Scalar> Base;
        using Base::m_size;
      };
    } // namespace internal

    template<typename Archive, typename Scalar>
    void serialize(
      Archive & ar,
      ::pinocchio::NonNegativeOrthantConeTpl<Scalar> & set,
      const unsigned int /*version*/)
    {
      typedef ::pinocchio::NonNegativeOrthantConeTpl<Scalar> Self;
      typedef typename Self::Base Base;
      ar & make_nvp("base", boost::serialization::base_object<Base>(set));

      typedef internal::NonNegativeOrthantConeAccessor<Scalar> Accessor;
      auto & set_ = reinterpret_cast<Accessor &>(set);
      ar & make_nvp("m_size", set_.m_size);
    }

  } // namespace serialization
} // namespace boost
