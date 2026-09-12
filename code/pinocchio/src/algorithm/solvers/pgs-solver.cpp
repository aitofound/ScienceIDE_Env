//
// Copyright (c) 2026 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_SOLVERS

  #include "pinocchio/algorithm/solvers/pgs-solver.hpp"

namespace pinocchio
{

  // -------------------------------------------------------------------------
  // Struct instantiations
  // -------------------------------------------------------------------------

  template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    PGSSolverSettingsTpl<context::Scalar>;

  template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    PGSSolverStatsTpl<context::Scalar>;

  template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    PGSSolverResultTpl<context::Scalar, context::Options>;

  namespace internal
  {
    template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
      PGSSolverWorkspaceTpl<context::Scalar, context::Options>;
  } // namespace internal

  template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    PGSConstraintSolverTpl<context::Scalar, context::Options>;

  // -------------------------------------------------------------------------
  // solveImpl() with DelassusOperatorCholeskyExpression + default constraint collection
  // -------------------------------------------------------------------------

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI bool
  PGSConstraintSolverTpl<context::Scalar, context::Options>::solveImpl<
    DelassusOperatorCholeskyExpressionTpl<
      ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>>,
    context::VectorXs,
    ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
    std::allocator<
      ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>,
    ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
    std::allocator<
      ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>>(
    DelassusOperatorBase<DelassusOperatorCholeskyExpressionTpl<
      ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>>> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const std::vector<
      ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
      std::allocator<
        ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>> &,
    const std::vector<
      ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
      std::allocator<
        ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>> &,
    const PGSSolverSettingsTpl<context::Scalar> &,
    PGSSolverResultTpl<context::Scalar, context::Options> &);

} // namespace pinocchio

#endif // ifndef PINOCCHIO_SKIP_ALGORITHM_SOLVERS
