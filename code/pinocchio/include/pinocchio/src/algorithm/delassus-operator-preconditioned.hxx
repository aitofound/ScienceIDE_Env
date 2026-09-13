//
// Copyright (c) 2025 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/delassus-operator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/delassus-operator.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename DelassusOperator, typename PreconditionerType>
  struct traits<DelassusOperatorPreconditionedTpl<DelassusOperator, PreconditionerType>>
  : traits<DelassusOperator>
  {
  };

  template<typename DelassusOperator, typename PreconditionerType>
  struct DelassusOperatorPreconditionedTpl
  : DelassusOperatorBase<DelassusOperatorPreconditionedTpl<DelassusOperator, PreconditionerType>>
  {

    typedef DelassusOperatorPreconditionedTpl Self;
    typedef DelassusOperatorBase<Self> Base;

    typedef typename traits<Self>::MatrixXs MatrixXs;
    typedef typename traits<Self>::VectorXs VectorXs;
    typedef typename traits<Self>::Scalar Scalar;

    DelassusOperatorPreconditionedTpl(
      DelassusOperatorBase<DelassusOperator> & delassus, const PreconditionerType & preconditioner)
    : m_delassus(delassus.derived())
    , m_preconditioner(preconditioner)
    , m_tmp_vec(preconditioner.cols())
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(m_delassus.rows(), m_preconditioner.cols());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(m_delassus.cols(), m_preconditioner.rows());
    }

    DelassusOperator & ref()
    {
      return m_delassus;
    }
    const DelassusOperator & ref() const
    {
      return m_delassus;
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    template<typename VectorLike>
    void updateDampingImpl(const Eigen::MatrixBase<VectorLike> & vec)
    {
      // G_bar + mu * Id = P * (G + mu * P^{-2}) * P
      m_preconditioner.scaleSquare(vec, m_tmp_vec);
      ref().updateDamping(m_tmp_vec);
    }

    template<typename VectorLike>
    void updateComplianceImpl(const Eigen::MatrixBase<VectorLike> & compliance_vector)
    {
      // G_bar + mu * Id = P * (G + mu * P^{-2}) * P
      m_preconditioner.scaleSquare(compliance_vector, m_tmp_vec);
      ref().updateCompliance(m_tmp_vec);
    }

    bool isDirtyImpl() const
    {
      return ref().isDirty();
    }

    void updateDecompositionImpl()
    {
      ref().updateDecomposition();
    }

    template<typename MatrixLike>
    void solveInPlaceImpl(const Eigen::MatrixBase<MatrixLike> & mat) const
    {
      auto & mat_ = mat.const_cast_derived();
      m_preconditioner.scaleInPlace(mat_);
      ref().solveInPlace(mat_);
      m_preconditioner.scaleInPlace(mat_);
    }

    template<typename MatrixIn, typename MatrixOut>
    void applyOnTheRightImpl(
      const Eigen::MatrixBase<MatrixIn> & x,
      const Eigen::MatrixBase<MatrixOut> & res,
      bool with_damping = true) const
    {
      auto & res_ = res.const_cast_derived();
      m_preconditioner.unscale(x, res_);
      ref().applyOnTheRight(res_, m_tmp_vec, with_damping);
      m_preconditioner.unscale(m_tmp_vec, res_);
    }

    Eigen::Index sizeImpl() const
    {
      return ref().size();
    }
    Eigen::Index rowsImpl() const
    {
      return ref().rows();
    }
    Eigen::Index colsImpl() const
    {
      return ref().cols();
    }

    template<typename MatrixType>
    void matrixImpl(
      const Eigen::MatrixBase<MatrixType> & res,
      bool enforce_symmetry = false,
      bool with_damping = true) const
    {
      MatrixType & res_ = res.const_cast_derived();
      m_delassus.matrix(res_, enforce_symmetry, with_damping);
      res_.noalias() = m_preconditioner.getDiagonal().asDiagonal() * res_;
      res_.noalias() = res_ * m_preconditioner.getDiagonal().asDiagonal();
    }

    VectorXs getDampingImpl() const
    {
      m_preconditioner.unscaleSquare(ref().getDamping(), m_tmp_vec);
      return m_tmp_vec;
    }

    VectorXs getComplianceImpl() const
    {
      m_preconditioner.unscaleSquare(ref().getCompliance(), m_tmp_vec);
      return m_tmp_vec;
    }

    DelassusOperator & m_delassus;
    const PreconditionerType & m_preconditioner;
    VectorXs m_tmp_vec;

  }; // struct DelassusOperatorPreconditioned

} // namespace pinocchio
