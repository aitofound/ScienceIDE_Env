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

    template<class Archive, typename Scalar, int Options>
    void save(
      Archive & ar, const pinocchio::MotionTpl<Scalar, Options> & m, const unsigned int /*version*/)
    {
      ar & make_nvp("linear", make_array(m.linear().data(), 3));
      ar & make_nvp("angular", make_array(m.angular().data(), 3));
    }

    template<class Archive, typename Scalar, int Options>
    void
    load(Archive & ar, pinocchio::MotionTpl<Scalar, Options> & m, const unsigned int /*version*/)
    {
      ar >> make_nvp("linear", make_array(m.linear().data(), 3));
      ar >> make_nvp("angular", make_array(m.angular().data(), 3));
    }

    template<class Archive, typename Scalar, int Options>
    void
    serialize(Archive & ar, pinocchio::MotionTpl<Scalar, Options> & m, const unsigned int version)
    {
      split_free(ar, m, version);
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & /*ar*/,
      pinocchio::MotionZeroTpl<Scalar, Options> & /*m*/,
      const unsigned int /*version*/)
    {
      // Nothing to do
    }

  } // namespace serialization
} // namespace boost
