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

    template<typename Archive>
    void serialize(Archive &, ::boost::blank &, const unsigned int)
    {
      // do nothing
    }
  } // namespace serialization
} // namespace boost
