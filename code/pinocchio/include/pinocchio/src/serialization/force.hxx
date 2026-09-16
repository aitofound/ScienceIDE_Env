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
      Archive & ar, const pinocchio::ForceTpl<Scalar, Options> & f, const unsigned int /*version*/)
    {
      ar & make_nvp("linear", make_array(f.linear().data(), 3));
      ar & make_nvp("angular", make_array(f.angular().data(), 3));
    }

    template<class Archive, typename Scalar, int Options>
    void
    load(Archive & ar, pinocchio::ForceTpl<Scalar, Options> & f, const unsigned int /*version*/)
    {
      ar >> make_nvp("linear", make_array(f.linear().data(), 3));
      ar >> make_nvp("angular", make_array(f.angular().data(), 3));
    }

    template<class Archive, typename Scalar, int Options>
    void
    serialize(Archive & ar, pinocchio::ForceTpl<Scalar, Options> & f, const unsigned int version)
    {
      split_free(ar, f, version);
    }

  } // namespace serialization
} // namespace boost
