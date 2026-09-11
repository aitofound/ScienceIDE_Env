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

  namespace internal
  {

    template<typename Derived>
    struct SimplicialCholeskyWrapper : public Derived
    {
      typedef Eigen::SimplicialCholeskyBase<Derived> Base;

      using Base::derived;
      using Base::m_diag;
      using Base::m_info;
      using Base::m_matrix;
      using Base::m_P;
      using Base::m_Pinv;

      template<typename Rhs, typename Dest>
      void _solve_impl(const Eigen::MatrixBase<Rhs> & b, Eigen::MatrixBase<Dest> & dest) const
      {
        //    eigen_assert(m_factorizationIsOk && "The decomposition is not in a valid state for
        //    solving, you must first call either compute() or symbolic()/numeric()");
        //    eigen_assert(m_matrix.rows()==b.rows());

        if (m_info != Eigen::Success)
          return;

        typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(Rhs) PlainMatrix;
        typedef typename PlainMatrix::Scalar Scalar;

        typedef Eigen::Map<PlainMatrix, EIGEN_DEFAULT_ALIGN_BYTES> MapPlainMatrix;
        MapPlainMatrix tmp =
          MapPlainMatrix(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, b.rows(), b.cols()));
        if (m_P.size() > 0)
          tmp.noalias() = m_P * b;
        else
          tmp = b;

        if (m_matrix.nonZeros() > 0) // otherwise L==I
          derived().matrixL().solveInPlace(tmp);

        if (m_diag.size() > 0)
          tmp = m_diag.asDiagonal().inverse() * tmp;

        if (m_matrix.nonZeros() > 0) // otherwise U==I
          derived().matrixU().solveInPlace(tmp);

        if (m_P.size() > 0)
          dest.noalias() = m_Pinv * tmp;
      }

    }; // SimplicialCholeskyWrapper

    template<typename SparseCholeskySolver>
    struct getSparseCholeskySolverBase;

    template<typename SparseCholeskySolver> //, typename Base = typename SparseCholeskySolver::Base>
    struct SparseSolveInPlaceMethod;

#ifdef PINOCCHIO_WITH_ACCELERATE_SUPPORT
    template<typename MatrixType, int UpLo, SparseFactorization_t Solver, bool EnforceSquare>
    struct SparseSolveInPlaceMethod<Eigen::AccelerateImpl<MatrixType, UpLo, Solver, EnforceSquare>>
    {
      typedef Eigen::AccelerateImpl<MatrixType, UpLo, Solver, EnforceSquare> SparseCholeskySolver;

      template<typename Rhs, typename Dest>
      static void run(
        const SparseCholeskySolver & solver,
        const Eigen::MatrixBase<Rhs> & mat,
        const Eigen::MatrixBase<Dest> & dest)
      {
        dest.const_cast_derived() = solver.solve(mat.derived());
      }
    };
#endif

    template<typename SparseCholeskySolver>
    struct SparseSolveInPlaceMethod
    {
      template<typename Rhs, typename Dest>
      static void run(
        const SparseCholeskySolver & solver,
        const Eigen::MatrixBase<Rhs> & mat,
        const Eigen::MatrixBase<Dest> & dest)
      {
        static_assert(
          std::is_base_of_v<
            Eigen::SimplicialCholeskyBase<SparseCholeskySolver>, SparseCholeskySolver>,
          "The solver is not a base of SimplicialCholeskyBase.");
        typedef SimplicialCholeskyWrapper<SparseCholeskySolver> CholeskyWrapper;

        const CholeskyWrapper & wrapper = reinterpret_cast<const CholeskyWrapper &>(solver);
        wrapper._solve_impl(mat, dest.const_cast_derived());
      }
    };

  } // namespace internal

  template<typename _Scalar, int _Options, class _SparseCholeskyDecomposition>
  struct traits<DelassusOperatorSparseTpl<_Scalar, _Options, _SparseCholeskyDecomposition>>
  {
    typedef _SparseCholeskyDecomposition CholeskyDecomposition;
    typedef typename CholeskyDecomposition::MatrixType SparseMatrix;
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    static constexpr int RowsAtCompileTime = Eigen::Dynamic;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;
    typedef MatrixXs Matrix; // for eigen lazy expression

    typedef const VectorXs & getDampingReturnType;
    typedef const VectorXs & getComplianceReturnType;
  };

  /// \brief Operator for a delassus' sparse representation.
  /// \copydoc DelassusOperatorBase.
  template<typename _Scalar, int _Options, class SparseCholeskyDecomposition>
  struct DelassusOperatorSparseTpl
  : DelassusOperatorBase<DelassusOperatorSparseTpl<_Scalar, _Options, SparseCholeskyDecomposition>>
  {

    typedef DelassusOperatorSparseTpl Self;
    typedef typename traits<Self>::Scalar Scalar;
    static constexpr int Options = traits<Self>::Options;
    static constexpr int RowsAtCompileTime = traits<Self>::RowsAtCompileTime;

    typedef typename traits<Self>::SparseMatrix SparseMatrix;
    typedef typename traits<Self>::VectorXs VectorXs;
    typedef typename traits<Self>::MatrixXs MatrixXs;
    typedef SparseCholeskyDecomposition CholeskyDecomposition;
    typedef DelassusOperatorBase<Self> Base;

    typedef typename traits<Self>::getDampingReturnType getDampingReturnType;
    typedef typename traits<Self>::getComplianceReturnType getComplianceReturnType;

    using Base::isDirty;
    using Base::size;

    /// \brief Default constructor from a given sparse matrix.
    template<typename MatrixDerived>
    explicit DelassusOperatorSparseTpl(const Eigen::SparseMatrixBase<MatrixDerived> & mat)
    : Base()
    , m_delassus_matrix(mat)
    , m_damped_delassus_matrix(mat)
    , m_cholsky_decomposition(mat)
    , m_cholesky_decomposition_dirty(true)
    , m_damping(VectorXs::Zero(mat.rows()))
    , m_compliance(VectorXs::Zero(mat.rows()))
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.rows(), mat.cols());
    }

    /// \brief Returns the sparse matrix representation of the delassus operator, including the
    /// compliance and damping.
    ///
    /// \param with_damping Whether to include the damping in the returned matrix. Default is true.
    SparseMatrix sparseMatrix(bool with_damping = true) const
    {
      m_damped_delassus_matrix = m_delassus_matrix;
      m_damped_delassus_matrix += m_compliance.asDiagonal();
      if (with_damping)
        m_damped_delassus_matrix += m_damping.asDiagonal();
      return m_damped_delassus_matrix;
    }

    /// \brief Returns the sparse inverse of the damped delassus matrix.
    SparseMatrix inverse() const
    {
      SparseMatrix identity_matrix(size(), size());
      identity_matrix.setIdentity();
      SparseMatrix res = m_cholsky_decomposition.solve(identity_matrix);
      return res;
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    template<typename VectorLike>
    void updateComplianceImpl(const Eigen::MatrixBase<VectorLike> & compliance_vector)
    {
      for (Eigen::Index k = 0; k < size(); ++k)
      {
        m_damped_delassus_matrix.coeffRef(k, k) += -m_compliance[k] + compliance_vector[k];
      }
      m_compliance = compliance_vector;
      m_cholesky_decomposition_dirty = true;
    }

    template<typename VectorLike>
    void updateDampingImpl(const Eigen::MatrixBase<VectorLike> & damping_vector)
    {
      for (Eigen::Index k = 0; k < size(); ++k)
      {
        m_damped_delassus_matrix.coeffRef(k, k) += -m_damping[k] + damping_vector[k];
      }
      m_damping = damping_vector;
      m_cholesky_decomposition_dirty = true;
    }

    void updateDecompositionImpl()
    {
      m_cholsky_decomposition.factorize(m_damped_delassus_matrix);
      m_cholesky_decomposition_dirty = false;
    }

    bool isDirtyImpl() const
    {
      return m_cholesky_decomposition_dirty;
    }

    template<typename MatrixType>
    void matrixImpl(
      const Eigen::MatrixBase<MatrixType> & mat,
      bool enforce_symmetry = false,
      bool with_damping = true) const
    {
      MatrixType & mat_ = mat.const_cast_derived();
      mat_ = m_delassus_matrix;
      mat_ += m_compliance.asDiagonal();
      if (with_damping)
        mat_ += m_damping.asDiagonal();
      if (enforce_symmetry)
        enforceSymmetry(mat_);
    }

    template<typename MatrixLike>
    void solveInPlaceImpl(const Eigen::MatrixBase<MatrixLike> & mat) const
    {
      PINOCCHIO_THROW_IF(
        isDirty(), std::logic_error,
        "The DelassusOperator has dirty quantities. Please call updateDecomposition() first.");
      internal::SparseSolveInPlaceMethod<CholeskyDecomposition>::run(
        m_cholsky_decomposition, mat.derived(), mat.const_cast_derived());
    }

    template<typename MatrixIn, typename MatrixOut>
    void applyOnTheRightImpl(
      const Eigen::MatrixBase<MatrixIn> & x,
      const Eigen::MatrixBase<MatrixOut> & res_,
      bool with_damping = true) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(x.rows(), size());
      MatrixOut & res = res_.const_cast_derived();
      res.noalias() = m_delassus_matrix * x;
      res.array() += m_compliance.array() * x.array();
      if (with_damping)
      {
        res.array() += m_damping.array() * x.array();
      }
    }

    Eigen::Index sizeImpl() const
    {
      return m_delassus_matrix.rows();
    }
    Eigen::Index rowsImpl() const
    {
      return m_delassus_matrix.rows();
    }
    Eigen::Index colsImpl() const
    {
      return m_delassus_matrix.cols();
    }

    getComplianceReturnType getComplianceImpl() const
    {
      return m_compliance;
    }

    getDampingReturnType getDampingImpl() const
    {
      return m_damping;
    }

  protected:
    SparseMatrix m_delassus_matrix;
    mutable SparseMatrix m_damped_delassus_matrix;
    CholeskyDecomposition m_cholsky_decomposition;
    bool m_cholesky_decomposition_dirty;
    VectorXs m_damping;
    VectorXs m_compliance;

  }; // struct DelassusOperatorSparseTpl

} // namespace pinocchio
