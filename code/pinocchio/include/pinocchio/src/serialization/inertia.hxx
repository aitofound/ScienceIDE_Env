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
      Archive & ar,
      const pinocchio::InertiaTpl<Scalar, Options> & I,
      const unsigned int /*version*/)
    {
      const Scalar mass = I.mass();
      ar & make_nvp("mass", mass);
      ar & make_nvp("lever", make_array(I.lever().data(), 3));
      ar & make_nvp("inertia", I.inertia());
    }

    template<class Archive, typename Scalar, int Options>
    void
    load(Archive & ar, pinocchio::InertiaTpl<Scalar, Options> & I, const unsigned int /*version*/)
    {
      ar >> make_nvp("mass", I.mass());
      ar >> make_nvp("lever", make_array(I.lever().data(), 3));
      ar >> make_nvp("inertia", I.inertia());
    }

    template<class Archive, typename Scalar, int Options>
    void
    serialize(Archive & ar, pinocchio::InertiaTpl<Scalar, Options> & I, const unsigned int version)
    {
      split_free(ar, I, version);
    }

  } // namespace serialization
} // namespace boost
