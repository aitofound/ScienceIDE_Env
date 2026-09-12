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
      const pinocchio::Symmetric3Tpl<Scalar, Options> & S,
      const unsigned int /*version*/)
    {
      ar & make_nvp("data", make_array(S.data().data(), 6));
    }

    template<class Archive, typename Scalar, int Options>
    void load(
      Archive & ar, pinocchio::Symmetric3Tpl<Scalar, Options> & S, const unsigned int /*version*/)
    {
      ar >> make_nvp("data", make_array(S.data().data(), 6));
    }

    template<class Archive, typename Scalar, int Options>
    void serialize(
      Archive & ar, pinocchio::Symmetric3Tpl<Scalar, Options> & S, const unsigned int version)
    {
      split_free(ar, S, version);
    }

  } // namespace serialization
} // namespace boost
