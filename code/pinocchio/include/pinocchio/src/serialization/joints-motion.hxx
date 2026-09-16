//
// Copyright (c) 2019 INRIA
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

    template<class Archive, typename Scalar, int Options, int axis>
    void serialize(
      Archive & ar,
      pinocchio::MotionRevoluteTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("w", m.angularRate());
    }

    template<class Archive, typename Scalar, int Options, int axis>
    void serialize(
      Archive & ar,
      pinocchio::MotionPrismaticTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("v", m.linearRate());
    }

    template<class Archive, typename Scalar, int Options, int axis>
    void serialize(
      Archive & ar,
      pinocchio::MotionHelicalTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("w", m.angularRate());
      ar & make_nvp("v", m.linearRate());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::MotionSphericalTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("angular", m.angular());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::MotionTranslationTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("linear", m.linear());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar, pinocchio::MotionPlanarTpl<Scalar, Options> & m, const unsigned int /*version*/)
    {
      ar & make_nvp("data", m.data());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::MotionRevoluteUnalignedTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("axis", m.axis());
      ar & make_nvp("w", m.angularRate());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::MotionPrismaticUnalignedTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("axis", m.axis());
      ar & make_nvp("v", m.linearRate());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::MotionHelicalUnalignedTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("axis", m.axis());
      ar & make_nvp("w", m.angularRate());
      ar & make_nvp("v", m.linearRate());
    }
  } // namespace serialization
} // namespace boost
