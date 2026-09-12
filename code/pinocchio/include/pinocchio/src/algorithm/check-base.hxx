//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/check-base.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/check-base.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  /// CRTP class describing the API of the checkers
  template<typename AlgorithmCheckerDerived>
  struct AlgorithmCheckerBase
  {
    AlgorithmCheckerDerived & derived()
    {
      return *static_cast<AlgorithmCheckerDerived *>(this);
    }

    const AlgorithmCheckerDerived & derived() const
    {
      return *static_cast<const AlgorithmCheckerDerived *>(this);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    bool checkModel(const ModelTpl<Scalar, Options, JointCollectionTpl> & model) const
    {
      return derived().checkModel_impl(model);
    }
  };

} // namespace pinocchio
