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
      pinocchio::TransformRevoluteTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("sin", m.sin());
      ar & make_nvp("cos", m.cos());
    }

    template<class Archive, typename Scalar, int Options, int axis>
    void serialize(
      Archive & ar,
      pinocchio::TransformPrismaticTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("displacement", m.displacement());
    }

    template<class Archive, typename Scalar, int Options, int axis>
    void serialize(
      Archive & ar,
      pinocchio::TransformHelicalTpl<Scalar, Options, axis> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("sin", m.sin());
      ar & make_nvp("cos", m.cos());
      ar & make_nvp("displacement", m.displacement());
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar,
      pinocchio::TransformTranslationTpl<Scalar, Options> & m,
      const unsigned int /*version*/)
    {
      ar & make_nvp("translation", m.translation());
    }

  } // namespace serialization
} // namespace boost
