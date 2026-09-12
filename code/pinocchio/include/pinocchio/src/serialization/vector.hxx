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

    template<class T, class Allocator>
    inline const nvp<std::vector<T, Allocator>>
    make_nvp(const char * name, std::vector<T, Allocator> & t)
    {
      return nvp<std::vector<T, Allocator>>(name, t);
    }

  } // namespace serialization
} // namespace boost
