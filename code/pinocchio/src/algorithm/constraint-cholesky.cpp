//
// Copyright (c) 2022-2026 INRIA
//

#include "pinocchio/src/context/template-instantiation.hxx"
#ifndef PINOCCHIO_SKIP_ALGORITHM_CONTACT_CHOLESKY

  #include "pinocchio/algorithm/constraint-cholesky.hpp"

namespace pinocchio
{
  // TODO Remove when API is stabilized
  PINOCCHIO_COMPILER_DIAGNOSTIC_PUSH
  PINOCCHIO_COMPILER_DIAGNOSTIC_IGNORED_DEPRECECATED_DECLARATIONS
  namespace details
  {
    template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI context::VectorXs &
    inverseAlgo<context::Scalar, context::Options, context::VectorXs>(
      const ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options> &,
      const Eigen::Index,
      const Eigen::MatrixBase<context::VectorXs> &);
  }
  template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::rebuild<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    const Data &,
    const RigidConstraintModelVector &,
    const RigidConstraintDataVector &);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
    getInverseOperationalSpaceInertiaMatrix<context::MatrixXs>(
      const Eigen::MatrixBase<context::MatrixXs> &, bool) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
    getOperationalSpaceInertiaMatrix<context::MatrixXs>(
      const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::getInverseMassMatrix<
    context::MatrixXs>(const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::compute<
    context::Scalar,
    context::Options,
    JointCollectionDefaultTpl,
    RigidConstraintModel,
    typename RigidConstraintModelVector::allocator_type,
    RigidConstraintData,
    typename RigidConstraintDataVector::allocator_type>(
    const Model &,
    Data &,
    const RigidConstraintModelVector &,
    const RigidConstraintDataVector &,
    const context::Scalar);

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::solveInPlace<
    context::MatrixXs>(const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Matrix
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::solve<context::MatrixXs>(
      const Eigen::MatrixBase<
        ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Matrix> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::
      getMassMatrixChoeslkyDecomposition<
        context::Scalar,
        context::Options,
        JointCollectionDefaultTpl>(const Model &, const Data &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Uv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Utv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Uiv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::Utiv<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::matrix<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;

  template PINOCCHIO_EXPLICIT_INSTANTIATION_DEFINITION_DLLAPI void
  ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>::inverse<context::MatrixXs>(
    const Eigen::MatrixBase<context::MatrixXs> &) const;
  PINOCCHIO_COMPILER_DIAGNOSTIC_POP

} // namespace pinocchio

#endif // PINOCCHIO_SKIP_ALGORITHM_CONTACT_CHOLESKY
