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

  template<
    typename _Scalar,
    int _Options,
    template<typename, auto...> class CholeskyDecompositionTpl>
  struct traits<DelassusOperatorDenseTpl<_Scalar, _Options, CholeskyDecompositionTpl>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    static constexpr int RowsAtCompileTime = Eigen::Dynamic;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;
    typedef MatrixXs Matrix; // for eigen lazy evaluation
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;

    typedef internal::BlockDiagonalMatrixTpl<Scalar, Options> BlockDiagonalMatrix;
    typedef BlockDiagonalMatrix DampingType;
    typedef const DampingType & getDampingReturnType;

    typedef internal::EigenStorageTpl<VectorXs> VectorStorage;
    typedef const typename VectorStorage::ConstMapType getComplianceReturnType;
  };

  /// \brief \copydoc Unsafe<DelassusOperatorBase>>
  template<
    typename _Scalar,
    int _Options,
    template<typename, auto...> class CholeskyDecompositionTpl>
  struct Unsafe<DelassusOperatorDenseTpl<_Scalar, _Options, CholeskyDecompositionTpl>>
  : Unsafe<
      DelassusOperatorBase<DelassusOperatorDenseTpl<_Scalar, _Options, CholeskyDecompositionTpl>>>
  {
    typedef DelassusOperatorDenseTpl<_Scalar, _Options, CholeskyDecompositionTpl> SafeSelf;
    typedef Unsafe<DelassusOperatorBase<SafeSelf>> Base;
    typedef typename traits<SafeSelf>::DampingType DampingType;

    using Base::self;

    explicit Unsafe(SafeSelf & self)
    : Base(self)
    {
    }

    void makeDirtyImpl()
    {
      self.m_cholesky_decomposition_dirty = true;
    }

    /// \brief Getter to the block diagonal damping.
    DampingType & dampingImpl()
    {
      return self.m_damping;
    }
  };

  /// \brief Operator for a delassus' dense representation.
  /// \copydoc DelassusOperatorBase.
  template<
    typename _Scalar,
    int _Options,
    template<typename, auto...> class CholeskyDecompositionTpl>
  struct DelassusOperatorDenseTpl
  : DelassusOperatorBase<DelassusOperatorDenseTpl<_Scalar, _Options>>
  {

    typedef _Scalar Scalar;
    typedef DelassusOperatorDenseTpl Self;
    static constexpr int Options = _Options;
    static constexpr int RowsAtCompileTime = traits<DelassusOperatorDenseTpl>::RowsAtCompileTime;

    typedef typename traits<Self>::MatrixXs MatrixXs;
    typedef typename traits<Self>::VectorXs VectorXs;
    typedef typename traits<Self>::getDampingReturnType getDampingReturnType;
    typedef typename traits<Self>::getComplianceReturnType getComplianceReturnType;
    typedef internal::EigenStorageTpl<MatrixXs> MatrixStorage;
    typedef internal::EigenStorageTpl<VectorXs> VectorStorage;
    typedef typename MatrixStorage::RefMapType MatrixStorageRefMapType;
    typedef typename VectorStorage::RefMapType VectorStorageRefMapType;
    typedef typename traits<Self>::DampingType DampingType;
    typedef CholeskyDecompositionTpl<Eigen::Ref<MatrixXs>> CholeskyDecomposition;
    typedef DelassusOperatorBase<Self> Base;

    using Base::isDirty;
    using Base::size;
    using Base::solveInPlace;

    /// \brief Default constructor.
    DelassusOperatorDenseTpl()
    : Base()
    , m_cholesky_decomposition(m_cholesky_decomposition_data)
    , m_cholesky_decomposition_dirty(true)
    {
    }

    /// \brief Copy constructor.
    DelassusOperatorDenseTpl(const DelassusOperatorDenseTpl & other)
    : DelassusOperatorDenseTpl()
    {
      *this = other;
    }

    /// \brief Copy assignment operator.
    DelassusOperatorDenseTpl & operator=(const DelassusOperatorDenseTpl & other)
    {
      if (this != &other)
      {
        Base::operator=(other);
        m_delassus_matrix_storage = other.m_delassus_matrix_storage;
        m_cholesky_decomposition_data_storage = other.m_cholesky_decomposition_data_storage;
        m_cholesky_decomposition.~CholeskyDecomposition();
        new (&m_cholesky_decomposition) CholeskyDecomposition(m_cholesky_decomposition_data);
        m_cholesky_decomposition_dirty = other.m_cholesky_decomposition_dirty;
        m_damping = other.m_damping;
        m_compliance_storage = other.m_compliance_storage;
      }
      return *this;
    }

    /// \brief Constructor from a given matrix.
    /// \note The constructor does not compute the cholesky decomposition of the delassus.
    template<typename MatrixDerived>
    explicit DelassusOperatorDenseTpl(const Eigen::MatrixBase<MatrixDerived> & mat)
    : DelassusOperatorDenseTpl()
    {
      rebuild(mat);
    }

    /// \brief Constructor from a DelassusOperatorCholeskyExpressionTpl.
    /// \note The constructor does not compute the cholesky decomposition of the delassus.
    template<typename ConstraintCholeskyDecomposition>
    explicit DelassusOperatorDenseTpl(
      const DelassusOperatorCholeskyExpressionTpl<ConstraintCholeskyDecomposition> &
        delassus_expression,
      const bool enforce_symmetry = false)
    : DelassusOperatorDenseTpl()
    {
      rebuild(delassus_expression, enforce_symmetry);
    }

    /// \brief Constructor from a DelassusOperatorRigidBodySystemsTpl.
    /// \note The constructor does not compute the cholesky decomposition of the delassus.
    template<
      template<typename, int> class JointCollectionTpl,
      typename ConstraintModel,
      template<typename T> class StorageHolder>
    explicit DelassusOperatorDenseTpl(
      const DelassusOperatorRigidBodySystemsTpl<
        Scalar,
        Options,
        JointCollectionTpl,
        ConstraintModel,
        StorageHolder> & delassus_rigid_body,
      const bool enforce_symmetry = false)
    : DelassusOperatorDenseTpl()
    {
      rebuild(delassus_rigid_body, enforce_symmetry);
    }

    /// \brief Rebuild the internal data structure from a given matrix.
    /// \note This resets the physical compliance and numerical damping.
    template<typename MatrixDerived>
    void rebuild(const Eigen::MatrixBase<MatrixDerived> & mat)
    {
      PINOCCHIO_THROW_IF(mat.rows() != mat.cols(), std::runtime_error, "Expected a square matrix.");

      // copy input matrix
      m_delassus_matrix_storage.resize(mat.rows(), mat.cols());
      m_delassus_matrix = mat;

      // resize cholesky decomposition data
      m_cholesky_decomposition_data_storage.resize(mat.rows(), mat.cols());
      m_cholesky_decomposition_data.setZero();

      // resize/reset damping and compliance
      m_damping = DampingType::Zero(mat.rows());
      m_compliance_storage.resize(mat.rows());
      m_compliance.setZero();

      // mark decomposition as dirty
      m_cholesky_decomposition_dirty = true;
    }

    /// \brief Rebuild the internal data structure from a DelassusOperatorCholeskyExpressionTpl.
    template<typename ConstraintCholeskyDecomposition>
    void rebuild(
      const DelassusOperatorCholeskyExpressionTpl<ConstraintCholeskyDecomposition> &
        delassus_expression,
      const bool enforce_symmetry = false)
    {
      assert(delassus_expression.rows() == delassus_expression.size());
      assert(delassus_expression.cols() == delassus_expression.size());

      // retrieve delassus matrix
      const auto size = delassus_expression.size();
      m_delassus_matrix_storage.resize(size, size);
      delassus_expression.undampedMatrix(m_delassus_matrix, enforce_symmetry);
      m_delassus_matrix.diagonal() -= delassus_expression.getCompliance();

      // resize cholesky decomposition data
      m_cholesky_decomposition_data_storage.resize(size, size);
      m_cholesky_decomposition_data.setZero();

      // resize/reset damping and compliance
      m_damping = delassus_expression.getDamping();
      m_compliance_storage.resize(size);
      m_compliance = delassus_expression.getCompliance();

      // mark decomposition as dirty
      m_cholesky_decomposition_dirty = true;
    }

    /// \brief Rebuild the internal data structure from a DelassusOperatorRigidBodySystemsTpl.
    template<
      template<typename, int> class JointCollectionTpl,
      typename ConstraintModel,
      template<typename T> class StorageHolder>
    void rebuild(
      const DelassusOperatorRigidBodySystemsTpl<
        Scalar,
        Options,
        JointCollectionTpl,
        ConstraintModel,
        StorageHolder> & delassus_rigid_body,
      const bool enforce_symmetry = false)
    {
      assert(delassus_rigid_body.rows() == delassus_rigid_body.size());
      assert(delassus_rigid_body.cols() == delassus_rigid_body.size());

      // retrieve delassus matrix
      const auto size = delassus_rigid_body.size();
      m_delassus_matrix_storage.resize(size, size);
      delassus_rigid_body.undampedMatrix(m_delassus_matrix, enforce_symmetry);
      m_delassus_matrix.diagonal() -= delassus_rigid_body.getCompliance();

      // resize cholesky decomposition data
      m_cholesky_decomposition_data_storage.resize(size, size);
      m_cholesky_decomposition_data.setZero();

      // resize/reset damping and compliance
      m_damping = delassus_rigid_body.getDamping();
      m_compliance_storage.resize(size);
      m_compliance = delassus_rigid_body.getCompliance();

      // mark decomposition as dirty
      m_cholesky_decomposition_dirty = true;
    }

    /// \brief Comparison operator.
    bool operator==(const Self & other) const
    {
      if (&other == this)
        return true;

      return m_delassus_matrix == other.m_delassus_matrix                              //
             && m_cholesky_decomposition_data == other.m_cholesky_decomposition_data   //
             && m_cholesky_decomposition_dirty == other.m_cholesky_decomposition_dirty //
             && m_damping == other.m_damping                                           //
             && m_compliance == other.m_compliance;
    }

    /// \brief Comparison operator.
    bool operator!=(const Self & other) const
    {
      return !(*this == other);
    }

    /// \brief Returns the inverse of the damped delassus matrix.
    MatrixXs inverse() const
    {
      MatrixXs res = MatrixXs::Identity(size(), size());
      solveInPlace(res);
      return res;
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
      const Eigen::MatrixBase<MatrixOut> & res_,
      bool with_damping = true) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(x.rows(), size());
      MatrixOut & res = res_.const_cast_derived();
      res.noalias() = m_delassus_matrix * x;
      res.noalias() += m_compliance.asDiagonal() * x;
      if (with_damping)
      {
        m_damping.template applyOnTheRight<pinocchio::internal::add_assign_op>(x, res);
      }
    }

    template<typename VectorLike>
    void updateComplianceImpl(const Eigen::MatrixBase<VectorLike> & compliance_vector)
    {
      m_compliance = compliance_vector;
      m_cholesky_decomposition_dirty = true;
    }

    template<typename VectorLike>
    void updateDampingImpl(const Eigen::MatrixBase<VectorLike> & damping_vector)
    {
      m_damping = damping_vector.asDiagonal();
      m_cholesky_decomposition_dirty = true;
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      const internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &
        block_diagonal_damping_matrix)
    {
      if (&block_diagonal_damping_matrix == &m_damping)
        return;

      m_damping = block_diagonal_damping_matrix;
      m_cholesky_decomposition_dirty = true;
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &&
        block_diagonal_damping_matrix)
    {
      if (&block_diagonal_damping_matrix == &m_damping)
        return;

      m_damping = std::move(block_diagonal_damping_matrix);
      m_cholesky_decomposition_dirty = true;
    }

    void updateDecompositionImpl()
    {
      if (m_cholesky_decomposition_dirty)
      {
        m_cholesky_decomposition_data = m_delassus_matrix;
        m_damping.addTo(m_cholesky_decomposition_data);
        m_cholesky_decomposition_data += m_compliance.asDiagonal();
        computeCholeskyDecomposition();
        m_cholesky_decomposition_dirty = false;
      }
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
      if (with_damping)
      {
        m_damping.addTo(mat_);
      }
      mat_ += m_compliance.asDiagonal();
      if (enforce_symmetry)
      {
        enforceSymmetry(mat_);
      }
    }

    template<typename MatrixLike>
    void solveInPlaceImpl(const Eigen::MatrixBase<MatrixLike> & mat) const
    {
      PINOCCHIO_THROW_IF(
        isDirty(), std::logic_error,
        "The DelassusOperator has dirty quantities. Please call updateDecomposition() first.");
      m_cholesky_decomposition.solveInPlace(mat.const_cast_derived());
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
      return m_compliance_storage.const_map();
    }

    getDampingReturnType getDampingImpl() const
    {
      return m_damping;
    }

    /// \brief Compute the cholesky decomposition of the matrix contained
    /// in m_cholesky_decomposition_data and stores it in place.
    void computeCholeskyDecomposition()
    {
      if (
        m_cholesky_decomposition.cols() != m_cholesky_decomposition_data.cols()
        || m_cholesky_decomposition.rows() != m_cholesky_decomposition_data.rows())
      {
        // if the decomposition does not point to the data with the right size,
        // we recreate it.
        m_cholesky_decomposition.~CholeskyDecomposition();
        new (&m_cholesky_decomposition) CholeskyDecomposition(m_cholesky_decomposition_data);
      }
      else
      {
        // otherwise we run the decomposition algorithm on the internal decomposition data.
        m_cholesky_decomposition.compute(m_cholesky_decomposition_data);
      }
    }

  protected:
    /// \brief Storage for the delassus matrix.
    MatrixStorage m_delassus_matrix_storage;
    MatrixStorageRefMapType m_delassus_matrix = m_delassus_matrix_storage.map();

    /// \brief Data where the cholesky decomposition is stored.
    MatrixStorage m_cholesky_decomposition_data_storage;
    MatrixStorageRefMapType m_cholesky_decomposition_data =
      m_cholesky_decomposition_data_storage.map();

    /// \brief Cholesky decomposition algorithm.
    CholeskyDecomposition m_cholesky_decomposition;

    /// \brief Boolean to signal wether or not updateDecomposition() should be called.
    bool m_cholesky_decomposition_dirty;

    /// \brief Block diagonal numerical damping.
    DampingType m_damping;

    /// \brief Physical compliance.
    VectorStorage m_compliance_storage;
    VectorStorageRefMapType m_compliance = m_compliance_storage.map();
  }; // struct DelassusOperatorDenseTpl

} // namespace pinocchio
