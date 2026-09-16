//
// Copyright (c) 2017-2024 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/eigen-common.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include <Eigen/Core>
#endif //  PINOCCHIO_LSP

namespace pinocchio
{
  // Read and write
  template<typename Derived>
  Eigen::Ref<typename Derived::PlainObject> make_ref(const Eigen::MatrixBase<Derived> & x)
  {
    return Eigen::Ref<typename Derived::PlainObject>(x.const_cast_derived());
  }

  // Read-only
  template<typename M>
  auto make_const_ref(Eigen::MatrixBase<M> const & m) -> Eigen::Ref<typename M::PlainObject const>
  {
    return m;
  }
} // namespace pinocchio
