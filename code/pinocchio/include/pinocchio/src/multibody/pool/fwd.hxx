//
// Copyright (c) 2021-2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/pool/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/pool/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<
    typename Scalar,
    int Options = 0,
    template<typename, int> class JointCollectionTpl = JointCollectionDefaultTpl>
  class ModelPoolTpl;
  typedef ModelPoolTpl<context::Scalar> ModelPool;

  template<
    typename Scalar,
    int Options = 0,
    template<typename, int> class JointCollectionTpl = JointCollectionDefaultTpl>
  class GeometryPoolTpl;
  typedef GeometryPoolTpl<context::Scalar> GeometryPool;

} // namespace pinocchio
