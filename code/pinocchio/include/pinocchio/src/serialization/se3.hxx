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
    void
    save(Archive & ar, const pinocchio::SE3Tpl<Scalar, Options> & M, const unsigned int /*version*/)
    {
      ar & make_nvp("translation", make_array(M.translation().data(), 3));
      ar & make_nvp("rotation", make_array(M.rotation().data(), 9));
    }

    template<class Archive, typename Scalar, int Options>
    void load(Archive & ar, pinocchio::SE3Tpl<Scalar, Options> & M, const unsigned int /*version*/)
    {
      ar >> make_nvp("translation", make_array(M.translation().data(), 3));
      ar >> make_nvp("rotation", make_array(M.rotation().data(), 9));
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(Archive & ar, pinocchio::SE3Tpl<Scalar, Options> & M, const unsigned int version)
    {
      split_free(ar, M, version);
    }

  } // namespace serialization
} // namespace boost
