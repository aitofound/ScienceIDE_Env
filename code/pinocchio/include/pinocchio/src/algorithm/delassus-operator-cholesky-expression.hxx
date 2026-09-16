//
// Copyright (c) 2024-2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/delassus-operator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/delassus-operator.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  // Forward declaration of Unsafe specialization.
  template<typename _ConstraintCholeskyDecomposition>
  struct Unsafe<DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition>>;

  template<typename ConstraintCholeskyDecomposition>
  struct traits<DelassusOperatorCholeskyExpressionTpl<ConstraintCholeskyDecomposition>>
  {
    static constexpr int RowsAtCompileTime = Eigen::Dynamic;
    typedef typename ConstraintCholeskyDecomposition::Scalar Scalar;
    typedef typename ConstraintCholeskyDecomposition::Matrix MatrixXs;
    typedef MatrixXs Matrix;
    typedef typename ConstraintCholeskyDecomposition::Vector VectorXs;

    typedef typename ConstraintCholeskyDecomposition::DampingType DampingType;
    typedef const DampingType & getDampingReturnType;

    typedef typename ConstraintCholeskyDecomposition::EigenStorageVector EigenStorageVector;
    typedef const typename EigenStorageVector::ConstMapType getComplianceReturnType;
  };

  /// \brief Unsafe version of DelassusOperatorCholeskyExpressionTpl.
  /// Allows direct access to protected members for expert users.
  template<typename _ConstraintCholeskyDecomposition>
  struct Unsafe<DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition>>
  : Unsafe<
      DelassusOperatorBase<DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition>>>
  {
    typedef DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition> SafeSelf;
    typedef Unsafe<DelassusOperatorBase<SafeSelf>> Base;
    typedef typename traits<SafeSelf>::DampingType DampingType;

    using Base::self;

    explicit Unsafe(SafeSelf & self)
    : Base(self)
    {
    }

    void makeDirtyImpl()
    {
      self.self.updateSumComplianceDamping();
    }

    DampingType & dampingImpl()
    {
      return self.self.m_damping;
    }
  };

  // TODO(jcarpent): change const_cast usage.
  /// \brief Operator for a delassus' cholesky expression representation.
  /// \copydoc DelassusOperatorBase.
  template<typename _ConstraintCholeskyDecomposition>
  struct DelassusOperatorCholeskyExpressionTpl
  : DelassusOperatorBase<DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition>>
  {
    typedef _ConstraintCholeskyDecomposition ConstraintCholeskyDecomposition;
    typedef typename ConstraintCholeskyDecomposition::Scalar Scalar;
    typedef typename ConstraintCholeskyDecomposition::Vector Vector;
    typedef typename ConstraintCholeskyDecomposition::Matrix Matrix;
    typedef typename ConstraintCholeskyDecomposition::RowMatrix RowMatrix;
    typedef DelassusOperatorCholeskyExpressionTpl<_ConstraintCholeskyDecomposition> Self;
    typedef DelassusOperatorBase<Self> Base;
    typedef typename ConstraintCholeskyDecomposition::EigenStorageVector EigenStorageVector;
    typedef typename ConstraintCholeskyDecomposition::DampingType DampingType;
    static constexpr int Options = ConstraintCholeskyDecomposition::Options;
    typedef DelassusOperatorDenseTpl<Scalar, Options> DelassusOperatorDense;
    typedef typename traits<Self>::getDampingReturnType getDampingReturnType;
    typedef typename traits<Self>::getComplianceReturnType getComplianceReturnType;

    typedef
      typename SizeDepType<Eigen::Dynamic>::template BlockReturn<RowMatrix>::Type RowMatrixBlockXpr;
    typedef typename SizeDepType<Eigen::Dynamic>::template BlockReturn<RowMatrix>::ConstType
      RowMatrixConstBlockXpr;

    static constexpr int RowsAtCompileTime =
      traits<DelassusOperatorCholeskyExpressionTpl>::RowsAtCompileTime;

    using Base::getDamping;
    using Base::size;

    /// \brief Default constructor from a cholesky decomposition.
    explicit DelassusOperatorCholeskyExpressionTpl(ConstraintCholeskyDecomposition & self)
    : Base()
    , self(self)
    {
    }

    /// \brief Returns the Constraint Cholesky decomposition associated to this
    /// DelassusOperatorCholeskyExpression.
    const ConstraintCholeskyDecomposition & cholesky() const
    {
      return self;
    }

    /// \brief Returns the Constraint Cholesky decomposition associated to this
    /// DelassusOperatorCholeskyExpression.
    ConstraintCholeskyDecomposition & cholesky()
    {
      return self;
    }

    /// \brief Returns the inverse of the damped delassus matrix.
    Matrix inverse() const
    {
      return self.getOperationalSpaceInertiaMatrix();
    }

    ///
    /// \brief Returns the corresponding dense delassus operator.
    ///
    DelassusOperatorDense dense(bool enforce_symmetry = false) const
    {
      return DelassusOperatorDense(*this, enforce_symmetry);
    }

    /// \brief Returns the current memory footprint of this object in bytes.
    /// \details Sums up the sizes of all internal data members.
    std::size_t sizeInBytes() const
    {
      return self.sizeInBytes();
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    Unsafe<Self> unsafeImpl()
    {
      return Unsafe<Self>(*this);
    }
    friend struct Unsafe<Self>;

    template<typename MatrixIn, typename MatrixOut>
    void applyOnTheRightImpl(
      const Eigen::MatrixBase<MatrixIn> & x,
      const Eigen::MatrixBase<MatrixOut> & res,
      bool with_damping = true) const
    {

      PINOCCHIO_CHECK_ARGUMENT_SIZE(x.rows(), self.constraintDim());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.rows(), self.constraintDim());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.cols(), x.cols());

      res.const_cast_derived().noalias() = self.delassus_block * x;
      if (with_damping)
      {
        self.m_sum_compliance_damping.template applyOnTheRight<pinocchio::internal::add_assign_op>(
          x, res.const_cast_derived());
      }
      else
      {
        // take only compliance into account
        res.const_cast_derived().noalias() += self.compliance.asDiagonal() * x;
      }

      // const auto U1 = self.U.topLeftCorner(self.constraintDim(), self.constraintDim());
      // {
      //   typedef Eigen::Map<RowMatrix> MapType;
      //   MapType tmp_mat = MapType(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, x.rows(), x.cols()));
      //   //            tmp_mat.noalias() = U1.adjoint() * x;
      //   triangularMatrixMatrixProduct<Eigen::UnitLower>(U1.adjoint(), x.derived(), tmp_mat);

      //   // The following commented lines produced some memory allocation.
      //   // Should be replaced by a manual loop
      //   //          tmp_mat.array().colwise() *= -self.D.head(self.constraintDim()).array();
      //   for (Eigen::Index i = 0; i < x.cols(); ++i)
      //     tmp_mat.col(i).array() *= -self.D.head(self.constraintDim()).array();

      //   //            res.const_cast_derived().noalias() = U1 * tmp_mat;
      //   triangularMatrixMatrixProduct<Eigen::UnitUpper>(U1, tmp_mat, res.const_cast_derived());
      // }
    }

    void updateDecompositionImpl()
    {
      self.computeDelassusCholeskyDecomposition();
    }

    bool isDirtyImpl() const
    {
      return self.isDirty();
    }

    template<typename MatrixDerived>
    void solveInPlaceImpl(const Eigen::MatrixBase<MatrixDerived> & x) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(x.rows(), self.constraintDim());

      PINOCCHIO_THROW_IF(
        self.isDirty(), std::logic_error,
        "The DelassusOperator has dirty quantities. Please call updateDecomposition() first.");

      const auto U1 = self.U.topLeftCorner(self.constraintDim(), self.constraintDim())
                        .template triangularView<Eigen::UnitUpper>();

      U1.solveInPlace(x.const_cast_derived());

      // The following commented lines produced some memory allocation.
      // Should be replaced by a manual loop
      //        x.const_cast_derived().array().colwise() *=
      //        -self.Dinv.head(self.constraintDim()).array();
      for (Eigen::Index i = 0; i < x.cols(); ++i)
        x.const_cast_derived().col(i).array() *= -self.Dinv.head(self.constraintDim()).array();

      U1.adjoint().solveInPlace(x);
    }

    template<typename MatrixType>
    void matrixImpl(
      const Eigen::MatrixBase<MatrixType> & mat,
      bool enforce_symmetry = false,
      bool with_damping = true) const
    {
      self.getInverseOperationalSpaceInertiaMatrix(mat.const_cast_derived(), enforce_symmetry);
      if (!with_damping)
      {
        getDamping().subTo(mat.const_cast_derived());
      }
    }

    getComplianceReturnType getComplianceImpl() const
    {
      return self.getCompliance();
    }

    getDampingReturnType getDampingImpl() const
    {
      return self.getDamping();
    }

    template<typename VectorLike>
    void updateComplianceImpl(const Eigen::MatrixBase<VectorLike> & compliances)
    {
      const_cast<ConstraintCholeskyDecomposition &>(self).updateCompliance(compliances);
    }

    template<typename VectorLike>
    void updateDampingImpl(const Eigen::MatrixBase<VectorLike> & mus)
    {
      const_cast<ConstraintCholeskyDecomposition &>(self).updateDamping(mus);
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      const internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> & block_damping)
    {
      const_cast<ConstraintCholeskyDecomposition &>(self).updateDamping(block_damping);
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> && block_damping)
    {
      const_cast<ConstraintCholeskyDecomposition &>(self).updateDamping(std::move(block_damping));
    }

    Eigen::Index sizeImpl() const
    {
      return self.constraintDim();
    }

    Eigen::Index rowsImpl() const
    {
      return size();
    }

    Eigen::Index colsImpl() const
    {
      return size();
    }

  protected:
    ConstraintCholeskyDecomposition & self;
  }; // DelassusOperatorCholeskyExpression

} // namespace pinocchio
