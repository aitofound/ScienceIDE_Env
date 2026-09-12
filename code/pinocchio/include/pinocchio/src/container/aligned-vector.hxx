//
// Copyright (c) -2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/container/aligned-vector.hpp"
#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/container/aligned-vector.hpp"
#endif // PINOCCHIO_LSP

// This macro is deprecated
#define PINOCCHIO_ALIGNED_STD_VECTOR(Type) ::pinocchio::container::aligned_vector<Type>

#define PINOCCHIO_STD_VECTOR_WITH_EIGEN_ALLOCATOR(T) PINOCCHIO_ALIGNED_STD_VECTOR(T)

namespace pinocchio
{
  namespace container
  {

    template<typename T>
    using aligned_vector PINOCCHIO_DEPRECATED_MESSAGE(
      "aligned_vector is deprecated, please use std::vector") = std::vector<T>;

  } // namespace container

} // namespace pinocchio
