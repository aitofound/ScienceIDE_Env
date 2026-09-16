//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/delassus-operator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/delassus-operator.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename DelassusOperatorDerived, typename MatrixDerived>
  struct traits<DelassusOperatorApplyOnTheRightReturnType<DelassusOperatorDerived, MatrixDerived>>
  {
    typedef typename traits<DelassusOperatorDerived>::Scalar Scalar;
    typedef typename traits<DelassusOperatorDerived>::MatrixXs Matrix;
  };

  template<typename DelassusOperatorDerived, typename MatrixDerived>
  struct MultiplicationOperatorReturnType<
    DelassusOperatorBase<DelassusOperatorDerived>,
    Eigen::MatrixBase<MatrixDerived>>
  {
    typedef DelassusOperatorApplyOnTheRightReturnType<DelassusOperatorDerived, MatrixDerived> type;
  };

} // namespace pinocchio

namespace Eigen
{
  namespace internal
  {

    // Required by Eigen::ReturnByValue: provides the concrete ReturnType that the lazy expression
    // evaluates into, and the expression Flags.
    template<typename DelassusOperatorDerived, typename MatrixDerived>
    struct traits<
      pinocchio::DelassusOperatorApplyOnTheRightReturnType<DelassusOperatorDerived, MatrixDerived>>
    {
      typedef typename ::pinocchio::traits<DelassusOperatorDerived>::Matrix ReturnType;
      static constexpr int Flags = 0;
    };

    // Dispatched by Eigen when assigning a lazy delassus * x expression to a dense matrix.
    // Resizes the destination if needed, then triggers evaluation via evalTo -> applyOnTheRight.
    template<
      typename DstXprType,
      typename DelassusOperatorDerived,
      typename MatrixDerived,
      typename Functor>
    struct Assignment<
      DstXprType,
      Eigen::ReturnByValue<pinocchio::DelassusOperatorApplyOnTheRightReturnType<
        DelassusOperatorDerived,
        MatrixDerived>>,
      Functor,
      Dense2Dense,
      void>
    {
      typedef Eigen::ReturnByValue<pinocchio::DelassusOperatorApplyOnTheRightReturnType<
        DelassusOperatorDerived,
        MatrixDerived>>
        SrcXprType;

      EIGEN_DEVICE_FUNC
      static EIGEN_STRONG_INLINE void
      run(DstXprType & dst, const SrcXprType & src, const Functor & /*func*/)
      {
        Index dstRows = src.rows();
        Index dstCols = src.cols();
        if ((dst.rows() != dstRows) || (dst.cols() != dstCols))
          dst.resize(dstRows, dstCols);

        eigen_assert(dst.rows() == src.rows() && dst.cols() == src.cols());
        src.evalTo(dst);
      }
    };

  } // namespace internal
} // namespace Eigen

namespace pinocchio
{

  template<typename DelassusOperatorDerived, typename MatrixDerived>
  struct DelassusOperatorApplyOnTheRightReturnType
  : public Eigen::ReturnByValue<
      DelassusOperatorApplyOnTheRightReturnType<DelassusOperatorDerived, MatrixDerived>>
  {
    typedef DelassusOperatorApplyOnTheRightReturnType Self;

    DelassusOperatorApplyOnTheRightReturnType(
      const DelassusOperatorDerived & lhs, const MatrixDerived & rhs)
    : m_lhs(lhs)
    , m_rhs(rhs)
    {
    }

    template<typename ResultType>
    inline void evalTo(ResultType & result) const
    {
      m_lhs.applyOnTheRight(m_rhs.derived(), result);
    }

    EIGEN_CONSTEXPR Eigen::Index rows() const PINOCCHIO_NOEXCEPT
    {
      return m_lhs.rows();
    }
    EIGEN_CONSTEXPR Eigen::Index cols() const PINOCCHIO_NOEXCEPT
    {
      return m_rhs.cols();
    }

  protected:
    const DelassusOperatorDerived & m_lhs;
    const MatrixDerived & m_rhs;
  };

  /// \brief Unsafe version of DelassusOperatorBase<Derived>.
  /// This struct provides unsafe access to the internal data of the Delassus operator, such
  /// as the numerical damping.
  /// This is meant to be used by Pinocchio's algorithms or expert users who know what they are
  /// doing.
  /// This base access struct provides the common interface and some common functionalities for all
  /// Unsafe<DelassusOperatorBase> specializations.
  template<typename Derived>
  struct Unsafe<DelassusOperatorBase<Derived>>
  {
    typedef Derived SafeDerived;
    typedef typename traits<Derived>::DampingType DampingType;

    /// \brief Constructor from a reference to the safe base.
    Unsafe(SafeDerived & self)
    : self(self)
    {
    }

    /// \brief Cast to unsafe Derived.
    Unsafe<Derived> unsafe_derived()
    {
      return Unsafe<Derived>(self);
    }

    /// \brief Signal the delassus that updateDecomposition() should be called.
    /// This is typically called after damping or compliance has been updated
    /// so updateSumComplianceDamping is called internally.
    void makeDirty()
    {
      unsafe_derived().makeDirtyImpl();
    }

    /// \brief Getter to the block diagonal damping.
    DampingType & damping()
    {
      return unsafe_derived().dampingImpl();
    }

    SafeDerived & self;
  };

  /// \brief CRTP base class for Delassus operators.
  /// The Delassus operator is the square linear operator that maps the constraint forces to the
  /// constraint accelerations. We often denote G and its dense representation is J^T Minv J, where
  /// J is the constraints jacobian, Minv is the inverse of the mass matrix of the system. The
  /// Delassus operator is used in the context of constraint-based dynamics algorithms, such as the
  /// non-smooth contact dynamics algorithms implemented in Pinocchio. The Delassus operator may
  /// also include numerical damping (for numerical solvers) and physical compliance (for soft
  /// constraints). This base class provides the common interface and some common functionalities
  /// for all Delassus operator implementations.
  template<typename Derived>
  struct DelassusOperatorBase
  {
    typedef typename traits<Derived>::Scalar Scalar;
    typedef typename traits<Derived>::VectorXs VectorXs;
    typedef typename traits<Derived>::MatrixXs MatrixXs;
    typedef typename traits<Derived>::getDampingReturnType getDampingReturnType;
    typedef typename traits<Derived>::getComplianceReturnType getComplianceReturnType;

    /// \brief Cast to Derived.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Cast to Unsafe<Derived>.
    Unsafe<DelassusOperatorBase<Derived>> unsafe()
    {
      return derived().unsafeImpl();
    }
    friend struct Unsafe<DelassusOperatorBase<Derived>>;

  protected:
    /// \brief Default constructor.
    /// Protected so that DelassusOperatorBase cannot be constructed.
    DelassusOperatorBase()
    {
    }

  public:
    /// \brief Stores the product delassus * x in res.
    ///
    /// \param[in] x The matrix to multiply on the right of the Delassus operator.
    /// \param[out] res The result of the product delassus * x.
    /// \param[in] with_damping If true, the numerical damping is taken into account.
    template<typename MatrixIn, typename MatrixOut>
    void applyOnTheRight(
      const Eigen::MatrixBase<MatrixIn> & x,
      const Eigen::MatrixBase<MatrixOut> & res,
      bool with_damping = true) const
    {
      derived().applyOnTheRightImpl(x.derived(), res.const_cast_derived(), with_damping);
    }

    /// \brief Lazy multiplication operator, returning an Eigen expression representing the product
    /// of the Delassus operator with a matrix on the right.
    /// \warning Numerical damping is taken into account. Use applyOnTheRight if you want to control
    /// this.
    template<typename MatrixDerived>
    typename MultiplicationOperatorReturnType<
      DelassusOperatorBase,
      Eigen::MatrixBase<MatrixDerived>>::type
    operator*(const Eigen::MatrixBase<MatrixDerived> & x) const
    {
      typedef typename MultiplicationOperatorReturnType<
        DelassusOperatorBase, Eigen::MatrixBase<MatrixDerived>>::type ReturnType;
      return ReturnType(derived(), x.derived());
    }

    /// \brief Update the numerical damping of the Delassus from a vector.
    template<typename VectorLike>
    void updateDamping(const Eigen::MatrixBase<VectorLike> & damping)
    {
      derived().updateDampingImpl(damping.derived());
    }

    /// \brief Update the numerical damping of the Delassus from a scalar.
    void updateDamping(const Scalar damping)
    {
      updateDamping(VectorXs::Constant(size(), damping));
    }

    /// \brief Update numerical damping by copying an input block diagonal matrix.
    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDamping(
      const internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &
        block_diagonal_damping_matrix)
    {
      derived().updateDampingImpl(block_diagonal_damping_matrix);
    }

    /// \brief Update numerical damping by moving an input block diagonal matrix.
    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDamping(
      internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &&
        block_diagonal_damping_matrix)
    {
      derived().updateDampingImpl(std::move(block_diagonal_damping_matrix));
    }

    /// \brief Update the physical compliance of the Delassus from a vector.
    template<typename VectorLike>
    void updateCompliance(const Eigen::MatrixBase<VectorLike> & compliance)
    {
      derived().updateComplianceImpl(compliance.derived());
    }

    /// \brief Update the physical compliance of the Delassus from a scalar.
    void updateCompliance(const Scalar compliance)
    {
      updateCompliance(VectorXs::Constant(size(), compliance));
    }

    /// \brief Update the decomposition, following for instance, a call to updateDamping or
    /// updateCompliance As a result isDirty() returns false after this call.
    void updateDecomposition()
    {
      derived().updateDecompositionImpl();
      PINOCCHIO_THROW_IF(
        isDirty(), std::logic_error, "updateDecomposition() should make isDirty false.");
    }

    /// \returns true if the Delassus is not valid, for instance, after a call to updateDamping or
    /// updateCompliance
    bool isDirty() const
    {
      return derived().isDirtyImpl();
    }

    /// \brief Fills input matrix with the dense representation of the Delassus.
    ///
    /// \param[in] res The matrix to fill with the dense representation of the Delassus.
    /// \param[in] enforce_symmetry If true, the output matrix is symmetrized (default false).
    /// \param[in] with_damping If true, the numerical damping is taken into account (default true).
    template<typename MatrixType>
    void matrix(
      const Eigen::MatrixBase<MatrixType> & res,
      bool enforce_symmetry = false,
      bool with_damping = true) const
    {
      derived().matrixImpl(res.derived(), enforce_symmetry, with_damping);
    }

    /// \brief Returns the dense representation of the Delassus.
    ///
    /// \param[in] enforce_symmetry If true, the output matrix is symmetrized (default false).
    /// \param[in] with_damping If true, the numerical damping is taken into account (default true).
    MatrixXs matrix(bool enforce_symmetry = false, bool with_damping = true) const
    {
      MatrixXs res(size(), size());
      matrix(res, enforce_symmetry, with_damping);
      return res;
    }

    /// \brief Fills input matrix with the dense representation of the Delassus.
    /// The numerical damping is NOT taken into account here.
    ///
    /// \param[in] res The matrix to fill with the dense representation of the Delassus.
    /// \param[in] enforce_symmetry If true, the output matrix is symmetrized (default false).
    template<typename MatrixType>
    void
    undampedMatrix(const Eigen::MatrixBase<MatrixType> & res, bool enforce_symmetry = false) const
    {
      matrix(res, enforce_symmetry, false /*no damping*/);
    }

    /// \brief Returns the dense representation of the Delassus.
    /// The numerical damping is NOT taken into account here.
    ///
    /// \param[in] enforce_symmetry If true, the output matrix is symmetrized (default false).
    MatrixXs undampedMatrix(bool enforce_symmetry = false) const
    {
      MatrixXs res(this->size(), this->size());
      matrix(res, enforce_symmetry, false /*no damping*/);
      return res;
    }

    /// \brief solveInPlace operation returning the results of the inverse of the Delassus operator
    /// times the input matrix mat
    ///
    /// \param[in,out] mat Input/output argument containing the right hand side and the result of
    /// the operation
    ///
    /// \warning The parameter is only marked 'const' to make the C++ compiler accept a temporary
    /// expression here. This function will const_cast it, so constness isn't honored here.
    /// This method will throw if updateDecomposition() or compute(true, true) has not been called.
    template<typename MatrixLike>
    void solveInPlace(const Eigen::MatrixBase<MatrixLike> & mat) const
    {
      PINOCCHIO_THROW_IF(
        isDirty(), std::logic_error,
        "The DelassusOperator has dirty quantities. Please call updateDecomposition() first.");
      derived().solveInPlaceImpl(mat.const_cast_derived());
    }

    /// \brief Same as \ref solveInPlace but returns the result.
    template<typename MatrixLike>
    typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike)
      solve(const Eigen::MatrixBase<MatrixLike> & mat) const
    {
      typename PINOCCHIO_EIGEN_PLAIN_TYPE(MatrixLike) res(mat);
      solveInPlace(res);
      return res;
    }

    /// \brief Same as \ref solveInPlace but takes the result as an output argument.
    template<typename MatrixDerivedIn, typename MatrixDerivedOut>
    void solve(
      const Eigen::MatrixBase<MatrixDerivedIn> & x,
      const Eigen::MatrixBase<MatrixDerivedOut> & res) const
    {
      res.const_cast_derived() = solve(x.derived());
    }

    /// \brief Returns the numerical damping of the Delassus.
    getDampingReturnType getDamping() const
    {
      return derived().getDampingImpl();
    }

    /// \brief Returns the physical compliance of the Delassus.
    getComplianceReturnType getCompliance() const
    {
      return derived().getComplianceImpl();
    }

    /// \brief Returns the number of size (rows/cols) of the Delassus.
    /// The Delassus operator represents a size() x size() linear operator.
    Eigen::Index size() const
    {
      return derived().sizeImpl();
    }

    /// \brief Returns the number of rows of the Delassus. Identical to size().
    Eigen::Index rows() const
    {
      assert(derived().rowsImpl() == size());
      return derived().rowsImpl();
    }

    /// \brief Returns the number of cols of the Delassus. Identical to size().
    Eigen::Index cols() const
    {
      assert(derived().colsImpl() == size());
      return derived().colsImpl();
    }

  }; // struct DelassusOperatorBase

} // namespace pinocchio
