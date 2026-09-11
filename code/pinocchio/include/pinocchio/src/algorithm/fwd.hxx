//
// Copyright (c) 2016-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/fwd.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/fwd.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename Scalar>
  struct ProximalSettingsTpl;
  typedef ProximalSettingsTpl<context::Scalar> ProximalSettings;

  template<typename Scalar, int Options>
  struct ConstraintCholeskyDecompositionTpl;
  typedef ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>
    ConstraintCholeskyDecomposition;

  template<typename Scalar, int Options>
  using ContactCholeskyDecompositionTpl PINOCCHIO_DEPRECATED_MESSAGE(
    "ContactCholeskyDecompositionTpl is deprecated, please use "
    "ConstraintCholeskyDecompositionTpl") = ConstraintCholeskyDecompositionTpl<Scalar, Options>;
  using ContactCholeskyDecomposition PINOCCHIO_DEPRECATED_MESSAGE(
    "ContactCholeskyDecomposition is deprecated, please use "
    "ConstraintCholeskyDecomposition") =
    ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>;

  template<typename Scalar, int Options>
  struct RigidConstraintModelTpl;
  template<typename Scalar, int Options>
  struct RigidConstraintDataTpl;

  typedef RigidConstraintModelTpl<context::Scalar, context::Options> RigidConstraintModel;
  typedef RigidConstraintDataTpl<context::Scalar, context::Options> RigidConstraintData;
  typedef std::vector<RigidConstraintModel> RigidConstraintModelVector;
  typedef std::vector<RigidConstraintData> RigidConstraintDataVector;

  template<typename DelassusOperatorDerived>
  struct DelassusOperatorBase;

  template<typename DelassusOperatorDerived, typename MatrixDerived>
  struct DelassusOperatorApplyOnTheRightReturnType;

  template<
    typename Scalar,
    int Options = 0,
    template<typename, auto...> class CholeskyDecompositionTpl = Eigen::LLT>
  struct DelassusOperatorDenseTpl;
  typedef DelassusOperatorDenseTpl<context::Scalar, context::Options> DelassusOperatorDense;

  template<
    typename Scalar,
    int Options = 0,
    class SparseCholeskyDecomposition = Eigen::SimplicialLLT<Eigen::SparseMatrix<Scalar, Options>>>
  struct DelassusOperatorSparseTpl;
  typedef DelassusOperatorSparseTpl<context::Scalar, context::Options> DelassusOperatorSparse;

  /// \tparam _ConstraintModel The element type stored in the constraint model vector.
  ///   May itself be a holder type (e.g. std::reference_wrapper<const ConstraintModel>)
  ///   when the vector holds non-owning references to externally-managed constraints.
  /// \tparam StorageHolder Controls how the operator stores its own external references
  ///   (model, data, and constraint vectors). Default: std::reference_wrapper (non-owning).
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConstraintModel,
    template<typename T> class StorageHolder = std::reference_wrapper>
  struct DelassusOperatorRigidBodySystemsTpl;

  template<typename DelassusOperator, typename PreconditionerType>
  struct DelassusOperatorPreconditionedTpl;

  template<typename ConstraintCholeskyDecomposition>
  struct DelassusOperatorCholeskyExpressionTpl;
  typedef DelassusOperatorCholeskyExpressionTpl<ConstraintCholeskyDecomposition>
    DelassusOperatorCholeskyExpression;

  template<typename ConstraintCholeskyDecomposition>
  using DelassusCholeskyExpressionTpl PINOCCHIO_DEPRECATED_MESSAGE(
    "DelassusCholeskyExpressionTpl is deprecated, please use "
    "DelassusOperatorCholeskyExpressionTpl") =
    DelassusOperatorCholeskyExpressionTpl<ConstraintCholeskyDecomposition>;

  using DelassusCholeskyExpression PINOCCHIO_DEPRECATED_MESSAGE(
    "DelassusCholeskyExpression is deprecated, please use DelassusOperatorCholeskyExpression") =
    DelassusOperatorCholeskyExpression;

  template<class... D>
  struct AlgorithmCheckerList;

  struct ParentChecker;
  struct CRBAChecker;
  struct ABAChecker;

  AlgorithmCheckerList<ParentChecker, CRBAChecker, ABAChecker> makeDefaultCheckerList();

  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  bool checkData(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const DataTpl<Scalar, Options, JointCollectionTpl> & data);

  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options>
  neutral(const ModelTpl<Scalar, Options, JointCollectionTpl> & model);
} // namespace pinocchio
