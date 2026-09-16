//
// Copyright (c) 2016-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/copy.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/copy.hpp"
#endif // PINOCCHIO_LSP

/// \internal
namespace pinocchio
{
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  inline void copy(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const DataTpl<Scalar, Options, JointCollectionTpl> & origin,
    DataTpl<Scalar, Options, JointCollectionTpl> & dest,
    KinematicLevel kinematic_level)
  {
    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef typename Model::JointIndex JointIndex;

    PINOCCHIO_CHECK_INPUT_ARGUMENT(kinematic_level >= POSITION);

    for (JointIndex jid = 1; jid < (JointIndex)model.njoints; ++jid)
    {
      dest.oMi[jid] = origin.oMi[jid];
      if (kinematic_level >= VELOCITY)
      {
        dest.v[jid] = origin.v[jid];
      }
      if (kinematic_level >= ACCELERATION)
      {
        dest.a[jid] = origin.a[jid];
        dest.a_gf[jid] = origin.a_gf[jid];
        dest.f[jid] = origin.f[jid];
      }
    }
  }

} // namespace pinocchio
/// \endinternal
