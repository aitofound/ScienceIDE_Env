//
// Copyright (c) 2019-2021 INRIA
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
    void
    serialize(Archive & ar, pinocchio::FrameTpl<Scalar, Options> & f, const unsigned int version)
    {
      ar & make_nvp("name", f.name);
      ar & make_nvp("parent", f.parentJoint);
      ar & make_nvp("parentFrame", f.parentFrame);
      ar & make_nvp("placement", f.placement);
      ar & make_nvp("type", f.type);

      if (version > 0)
        ar & make_nvp("inertia", f.inertia);
    }

    template<typename Scalar, int Options>
    struct version<pinocchio::FrameTpl<Scalar, Options>>
    {
      enum
      {
        value = 1
      };
    };

  } // namespace serialization
} // namespace boost
