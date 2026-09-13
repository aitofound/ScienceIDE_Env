//
// Copyright (c) 2017-2019 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/serialization/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/serialization/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename T>
  struct Serialize
  {
    template<typename Archive>
    static void run(Archive & ar, T & object);
  };
} // namespace pinocchio
