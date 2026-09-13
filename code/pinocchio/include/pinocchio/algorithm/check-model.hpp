//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/generation/make_list.hpp>
#include <boost/fusion/algorithm/iteration/accumulate.hpp>

#include "pinocchio/multibody.hpp"
#include "pinocchio/algorithm/check-base.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{

#define PINOCCHIO_DEFINE_ALGO_CHECKER(NAME)                                                        \
  struct NAME##Checker : public AlgorithmCheckerBase<NAME##Checker>                                \
  {                                                                                                \
    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>       \
    bool checkModel_impl(const ModelTpl<Scalar, Options, JointCollectionTpl> &) const;             \
  }

  /// Simple model checker, that assert that model.parents is indeed a tree.
  PINOCCHIO_DEFINE_ALGO_CHECKER(Parent);
  /// Simple model checker, that assert that there is a mimic joint in the tree
  PINOCCHIO_DEFINE_ALGO_CHECKER(Mimic);

  template<class... D>
  struct AlgorithmCheckerList : AlgorithmCheckerBase<AlgorithmCheckerList<D...>>
  {
    typedef typename boost::fusion::list<D...> ArgType;

    AlgorithmCheckerList(const ArgType & checkerList)
    : checkerList(checkerList)
    {
    }

    // Calls model.check for each checker in the fusion::list.
    // Each list element is supposed to implement the AlgorithmCheckerBase API.
    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    bool checkModel_impl(const ModelTpl<Scalar, Options, JointCollectionTpl> & model) const;

    const ArgType & checkerList;
  };

  template<class... T>
  AlgorithmCheckerList<T...> makeAlgoCheckerList(const T &... args)
  {
    return AlgorithmCheckerList<T...>(boost::fusion::make_list(args...));
  }

} // namespace pinocchio

/* --- Details -------------------------------------------------------------------- */

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/check-model.hxx"
// IWYU pragma: end_exports
