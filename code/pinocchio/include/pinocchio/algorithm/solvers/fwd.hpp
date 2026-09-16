//
// Copyright (c) 2025 INRIA
//

#pragma once

#include "pinocchio/algorithm/fwd.hpp"

namespace pinocchio
{
  // -------------------------
  // PGS constraint solver
  template<typename Scalar, int Options>
  struct PGSConstraintSolverTpl;
  typedef PGSConstraintSolverTpl<context::Scalar, context::Options> PGSConstraintSolver;

  template<typename Scalar>
  struct PGSSolverSettingsTpl;
  typedef PGSSolverSettingsTpl<context::Scalar> PGSSolverSettings;

  template<typename Scalar, int Options>
  struct PGSSolverResultTpl;
  typedef PGSSolverResultTpl<context::Scalar, context::Options> PGSSolverResult;

  template<typename Scalar>
  struct PGSSolverStatsTpl;
  typedef PGSSolverStatsTpl<context::Scalar> PGSSolverStats;

  // -------------------------
  // ADMM constraint solver
  template<typename Scalar, int Options>
  struct ADMMConstraintSolverTpl;
  typedef ADMMConstraintSolverTpl<context::Scalar, context::Options> ADMMConstraintSolver;

  template<typename Scalar>
  struct ADMMSolverSettingsTpl;
  typedef ADMMSolverSettingsTpl<context::Scalar> ADMMSolverSettings;

  template<typename Scalar, int Options>
  struct ADMMSolverResultTpl;
  typedef ADMMSolverResultTpl<context::Scalar, context::Options> ADMMSolverResult;

  template<typename Scalar>
  struct ADMMSolverStatsTpl;
  typedef ADMMSolverStatsTpl<context::Scalar> ADMMSolverStats;

} // namespace pinocchio
