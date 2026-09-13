//
// Copyright (c) 2024-2026 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/delassus-operator.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/delassus-operator.hpp"
#endif // PINOCCHIO_LSP

#include "pinocchio/math.hpp"

namespace pinocchio
{

  template<
    typename _Scalar,
    int _Options,
    template<typename, int> class JointCollectionTpl,
    class _ConstraintModel,
    template<typename T> class StorageHolder>
  struct traits<DelassusOperatorRigidBodySystemsTpl<
    _Scalar,
    _Options,
    JointCollectionTpl,
    _ConstraintModel,
    StorageHolder>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    static constexpr int RowsAtCompileTime = Eigen::Dynamic;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;
    typedef MatrixXs Matrix;

    typedef internal::EigenStorageTpl<VectorXs> EigenStorageVector;
    typedef internal::BlockDiagonalMatrixTpl<Scalar, Options> BlockDiagonalMatrix;

    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef typename Model::Data Data;

    typedef _ConstraintModel ConstraintModel;
    typedef typename internal::helper::remove_holder<ConstraintModel>::type InnerConstraintModel;
    typedef
      typename internal::helper::remove_holder<ConstraintModel>::ref_type ConstraintModelReference;
    static constexpr bool ConstraintModelIsConst =
      internal::helper::remove_holder<ConstraintModel>::is_const;

    typedef typename InnerConstraintModel::ConstraintData InnerConstraintData;
    typedef typename internal::helper::remove_holder<ConstraintModel>::template rebind<
      typename std::
        conditional<ConstraintModelIsConst, const InnerConstraintData, InnerConstraintData>::type>
      ConstraintData;

    typedef std::vector<ConstraintModel> ConstraintModelVector;
    typedef std::vector<ConstraintData> ConstraintDataVector;

    typedef BlockDiagonalMatrix DampingType;
    typedef const DampingType & getDampingReturnType;
    typedef typename EigenStorageVector::ConstMapType getComplianceReturnType;
  };

  /// \brief Unsafe version of DelassusOperatorRigidBodySystemsTpl.
  /// Allows to access protected members.
  /// Meant to be used by expert users.
  template<
    typename _Scalar,
    int _Options,
    template<typename, int> class _JointCollectionTpl,
    class _ConstraintModel,
    template<typename T> class _StorageHolder>
  struct Unsafe<DelassusOperatorRigidBodySystemsTpl<
    _Scalar,
    _Options,
    _JointCollectionTpl,
    _ConstraintModel,
    _StorageHolder>>
  : Unsafe<DelassusOperatorBase<DelassusOperatorRigidBodySystemsTpl<
      _Scalar,
      _Options,
      _JointCollectionTpl,
      _ConstraintModel,
      _StorageHolder>>>
  {
    typedef DelassusOperatorRigidBodySystemsTpl<
      _Scalar,
      _Options,
      _JointCollectionTpl,
      _ConstraintModel,
      _StorageHolder>
      SafeSelf;
    typedef Unsafe<DelassusOperatorBase<SafeSelf>> Base;
    typedef typename traits<SafeSelf>::DampingType DampingType;

    using Base::self;

    explicit Unsafe(SafeSelf & self)
    : Base(self)
    {
    }

    void makeDirtyImpl()
    {
      self.updateSumComplianceDamping();
      self.m_solve_in_place_dirty = true;
    }

    DampingType & dampingImpl()
    {
      return self.m_damping;
    }
  };

  /// \brief Operator for a delassus' low-complexity representation.
  /// \copydoc DelassusOperatorBase.
  template<
    typename _Scalar,
    int _Options,
    template<typename, int> class _JointCollectionTpl,
    class _ConstraintModel,
    template<typename T> class StorageHolder>
  struct DelassusOperatorRigidBodySystemsTpl
  : DelassusOperatorBase<DelassusOperatorRigidBodySystemsTpl<
      _Scalar,
      _Options,
      _JointCollectionTpl,
      _ConstraintModel,
      StorageHolder>>
  {

    typedef DelassusOperatorRigidBodySystemsTpl Self;
    typedef DelassusOperatorBase<Self> Base;

    typedef typename traits<Self>::Scalar Scalar;
    static constexpr int Options = traits<Self>::Options;

    typedef typename traits<Self>::VectorXs VectorXs;
    typedef typename traits<Self>::MatrixXs MatrixXs;
    typedef typename traits<Self>::EigenStorageVector EigenStorageVector;
    typedef typename traits<Self>::BlockDiagonalMatrix BlockDiagonalMatrix;
    typedef typename traits<Self>::getDampingReturnType getDampingReturnType;
    typedef typename traits<Self>::getComplianceReturnType getComplianceReturnType;

    typedef typename traits<Self>::Model Model;
    typedef StorageHolder<const Model> ModelHolder;
    typedef typename traits<Self>::Data Data;
    typedef StorageHolder<Data> DataHolder;

    typedef typename Data::Force Force;
    typedef std::vector<Force> ForceVector;

    typedef typename traits<Self>::ConstraintModel ConstraintModel;
    typedef typename traits<Self>::InnerConstraintModel InnerConstraintModel;
    typedef typename traits<Self>::ConstraintModelVector ConstraintModelVector;
    typedef StorageHolder<const ConstraintModelVector> ConstraintModelVectorHolder;

    typedef typename traits<Self>::ConstraintData ConstraintData;
    typedef typename traits<Self>::InnerConstraintData InnerConstraintData;
    typedef typename traits<Self>::ConstraintDataVector ConstraintDataVector;
    typedef StorageHolder<const ConstraintDataVector> ConstraintDataVectorHolder;

    using Base::size;

    /// \brief Default constructor from model, data, constraint_models and constraint_datas.
    DelassusOperatorRigidBodySystemsTpl(
      const ModelHolder & model_ref,
      const DataHolder & data_ref,
      const ConstraintModelVectorHolder & constraint_models_ref,
      const ConstraintDataVectorHolder & constraint_datas_ref,
      const Scalar min_damping_value = 0)
    : Base()
    , m_size(residualSize(internal::helper::get_ref(constraint_models_ref)))
    , m_min_damping_value(min_damping_value)
    , m_model_ref(model_ref)
    , m_data_ref(data_ref)
    , m_constraint_models_ref(constraint_models_ref)
    , m_constraint_datas_ref(constraint_datas_ref)
    , m_internal_data(internal::helper::get_ref(model_ref))
    , m_solve_in_place_dirty(true)
    , m_damping(VectorXs::Constant(m_size, min_damping_value).asDiagonal())
    , m_compliance_storage(m_size)
    , m_compliance(m_compliance_storage.map())
    , m_sum_compliance_damping(VectorXs::Constant(m_size, min_damping_value).asDiagonal())
    , m_sum_compliance_damping_inverse(VectorXs::Constant(m_size, min_damping_value).asDiagonal())
    {
      assert(model().check(data()) && "data is not consistent with model.");
      PINOCCHIO_CHECK_ARGUMENT_SIZE(
        constraint_models().size(), constraint_datas().size(),
        "The sizes of contact vector models and contact vector datas are not the same.");
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        min_damping_value >= Scalar(0) && "The damping value should be positive.");

      rebuild(model_ref, data_ref, constraint_models_ref, constraint_datas_ref);
    }

    DelassusOperatorRigidBodySystemsTpl(const DelassusOperatorRigidBodySystemsTpl & other)
    : DelassusOperatorRigidBodySystemsTpl(
        other.m_model_ref,
        other.m_data_ref,
        other.m_constraint_models_ref,
        other.m_constraint_datas_ref,
        other.m_min_damping_value)
    {
      m_internal_data = other.m_internal_data;
      m_solve_in_place_dirty = other.m_solve_in_place_dirty;
      m_damping = other.m_damping;
      m_compliance_storage = other.m_compliance_storage;
      m_sum_compliance_damping = other.m_sum_compliance_damping;
      m_sum_compliance_damping_inverse = other.m_sum_compliance_damping_inverse;
    }

    DelassusOperatorRigidBodySystemsTpl &
    operator=(const DelassusOperatorRigidBodySystemsTpl & other)
    {
      if (this != &other)
      {
        this->~DelassusOperatorRigidBodySystemsTpl();
        new (this) DelassusOperatorRigidBodySystemsTpl(other);
      }
      return *this;
    }

    /// \brief Update the constraint model and data vectors, and resize the internal quantities.
    ///
    /// \param[in] constraint_models_ref Vector of constraint models
    /// \param[in] constraint_datas_ref Vector of constraint datas
    ///
    void rebuild(
      const ModelHolder & model_ref,
      const DataHolder & data_ref,
      const ConstraintModelVectorHolder & constraint_models_ref,
      const ConstraintDataVectorHolder & constraint_datas_ref);

    /// \brief Update the intermediate computations before calling solveInPlace or operator*
    ///
    /// \param[in] apply_on_the_right If true, this will update the quantities involved in the
    /// applyOnTheRight method
    /// \param[in] solve_in_place If true, this will update the quantities involved in the
    /// solveInPlace method
    ///
    /// \remarks By activating or deactivating apply_on_the_right and solve_in_place, this enables
    /// to lower the quantities updated to the minimum, helping to save time overall.
    /// This method assumes the fields data.oMi, data.lMi and data.J have been computed.
    /// This is typically done by calling `computeJointJacobians` or `aba` in `Convention::WORLD`.
    ///
    void compute(bool apply_on_the_right = true, bool solve_in_place = true)
    {
      compute_or_update_decomposition(apply_on_the_right, solve_in_place);
    }

    /// \brief Returns the current memory footprint of this object in bytes.
    /// \details Sums up the sizes of all internal data members.
    std::size_t sizeInBytes() const
    {
      return m_damping.sizeInBytes() + m_compliance_storage.sizeInBytes()
             + m_sum_compliance_damping.sizeInBytes()
             + m_sum_compliance_damping_inverse.sizeInBytes() + m_internal_data.sizeInBytes();
    }

    /// \brief Const getter for model.
    const Model & model() const
    {
      return internal::helper::get_ref(m_model_ref);
    }

    /// \brief Getter for data.
    Data & data()
    {
      return internal::helper::get_ref(m_data_ref);
    }
    ///
    /// \brief Const getter for data.
    const Data & data() const
    {
      return internal::helper::get_ref(m_data_ref);
    }

    /// \brief Const getter of constraint models.
    const ConstraintModelVector & constraint_models() const
    {
      return internal::helper::get_ref(m_constraint_models_ref);
    }

    /// \brief Const getter of constraint datas.
    const ConstraintDataVector & constraint_datas() const
    {
      return internal::helper::get_ref(m_constraint_datas_ref);
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
      bool with_damping = true) const;

    template<typename VectorLike>
    void updateDampingImpl(const Eigen::MatrixBase<VectorLike> & damping_vector)
    {
      m_damping = damping_vector.asDiagonal();
      updateSumComplianceDamping();
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      const internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &
        block_diagonal_damping_matrix)
    {
      if (&block_diagonal_damping_matrix == &m_damping)
        return;

      m_damping = block_diagonal_damping_matrix;
      updateSumComplianceDamping();
    }

    template<int OtherOptions, std::size_t OtherAlignment>
    void updateDampingImpl(
      internal::BlockDiagonalMatrixTpl<Scalar, OtherOptions, OtherAlignment> &&
        block_diagonal_damping_matrix)
    {
      if (&block_diagonal_damping_matrix == &m_damping)
        return;

      m_damping = std::move(block_diagonal_damping_matrix);
      updateSumComplianceDamping();
    }

    template<typename VectorLike>
    void updateComplianceImpl(const Eigen::MatrixBase<VectorLike> & compliance_vector)
    {
      m_compliance = compliance_vector;
      updateSumComplianceDamping();
    }

    void updateDecompositionImpl()
    {
      compute_or_update_decomposition(false, true);
    }

    bool isDirtyImpl() const
    {
      return m_solve_in_place_dirty;
    }

    template<typename MatrixType>
    void matrixImpl(
      const Eigen::MatrixBase<MatrixType> & res,
      bool enforce_symmetry = false,
      bool with_damping = true) const
    {
      MatrixType & res_ = res.const_cast_derived();
      typedef Eigen::Map<VectorXs> MapVectorXs;
      MapVectorXs x = MapVectorXs(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, this->size(), 1));

      for (Eigen::Index i = 0; i < this->size(); ++i)
      {
        x = VectorXs::Unit(this->size(), i);
        this->applyOnTheRight(x, res_.col(i), with_damping);
      }
      if (enforce_symmetry)
      {
        res_ = 0.5 * (res_ + res_.transpose());
      }
    }

    template<typename MatrixLike>
    void solveInPlaceImpl(const Eigen::MatrixBase<MatrixLike> & mat) const;

    Eigen::Index sizeImpl() const
    {
      return m_size;
    }

    /// \brief Returns the number of rows of the Delassus.
    Eigen::Index rowsImpl() const
    {
      return m_size;
    }

    /// \brief Returns the number of cols of the Delassus.
    Eigen::Index colsImpl() const
    {
      return m_size;
    }

    getDampingReturnType getDampingImpl() const
    {
      return m_damping;
    }

    getComplianceReturnType getComplianceImpl() const
    {
      return m_compliance_storage.const_map();
    }

  public:
    /// \brief Internal data needed for the various passes of the delassus operator.
    struct InternalData
    {
      typedef typename Data::Motion Motion;
      typedef typename Data::Force Force;

      typedef typename std::vector<Motion> MotionVector;
      typedef typename std::vector<Force> ForceVector;

      InternalData(const Model & model)
      : a(std::size_t(model.njoints), Motion::Zero())
      , oa_augmented(std::size_t(model.njoints), Motion::Zero())
      , u_storage(model.nv)
      , u(u_storage.map())
      , ddq_storage(model.nv)
      , ddq(ddq_storage.map())
      , f(std::size_t(model.njoints))
      , of_augmented(std::size_t(model.njoints))
      {
      }

      InternalData(const InternalData & other)
      : a(other.a)
      , oa_augmented(other.oa_augmented)
      , u_storage(other.u_storage)
      , u(u_storage.map())
      , ddq_storage(other.ddq_storage)
      , ddq(ddq_storage.map())
      , f(other.f)
      , of_augmented(other.of_augmented)
      {
      }

      InternalData & operator=(const InternalData & other)
      {
        if (this != &other)
        {
          a = other.a;
          oa_augmented = other.oa_augmented;
          u_storage = other.u_storage;
          ddq_storage = other.ddq_storage;
          f = other.f;
          of_augmented = other.of_augmented;
        }
        return *this;
      }

      /// \brief Rebuild from a given model.
      void rebuild(const Model & model)
      {
        a.resize(std::size_t(model.njoints));
        oa_augmented.resize(std::size_t(model.njoints));
        u_storage.resize(model.nv);
        ddq_storage.resize(model.nv);
        f.resize(std::size_t(model.njoints));
        of_augmented.resize(std::size_t(model.njoints));

        assert(u.size() == model.nv);
        assert(ddq.size() == model.nv);
      }

      MotionVector a, oa_augmented;
      EigenStorageVector u_storage;
      typename EigenStorageVector::RefMapType u;
      EigenStorageVector ddq_storage;
      typename EigenStorageVector::RefMapType ddq;
      ForceVector f, of_augmented;

      /// \brief Returns the current memory footprint of this object in bytes.
      /// \details Sums up the sizes of all internal data members.
      std::size_t sizeInBytes() const
      {
        return pinocchio::internal::sizeInBytes(a) + pinocchio::internal::sizeInBytes(oa_augmented)
               + pinocchio::internal::sizeInBytes(u) + pinocchio::internal::sizeInBytes(ddq)
               + pinocchio::internal::sizeInBytes(f)
               + pinocchio::internal::sizeInBytes(of_augmented);
      }
    };

    /// \brief Const getter for internal data.
    const InternalData & getInternalData() const
    {
      return m_internal_data;
    }

    /// \brief Getter for internal data.
    InternalData & getInternalData()
    {
      return m_internal_data;
    }

    /// \brief AugmentedMassMatrixOperator needed for solveInPlace.
    struct AugmentedMassMatrixOperator
    {
      AugmentedMassMatrixOperator(const DelassusOperatorRigidBodySystemsTpl & delassus_operator)
      : m_self(delassus_operator)
      {
      }

      template<typename MatrixLike>
      void solveInPlace(
        const Eigen::MatrixBase<MatrixLike> & mat, bool reset_joint_force_vector = true) const;

    protected:
      const DelassusOperatorRigidBodySystemsTpl & m_self;
    };

    /// \brief Getter for the AugmentedMassMatrixOperator.
    AugmentedMassMatrixOperator getAugmentedMassMatrixOperator() const
    {
      return AugmentedMassMatrixOperator(*this);
    }

  protected:
    /// \brief Update the sum compliance + damping
    void updateSumComplianceDamping()
    {
      m_sum_compliance_damping = m_damping + m_compliance.asDiagonal();
      m_solve_in_place_dirty = true;
    }

    /// \brief Update the intermediate computations before calling solveInPlace or operator*.
    void compute_or_update_decomposition(bool apply_on_the_right, bool solve_in_place);

    // Holders
    Eigen::Index m_size;
    const Scalar m_min_damping_value;
    ModelHolder m_model_ref;
    DataHolder m_data_ref;
    ConstraintModelVectorHolder m_constraint_models_ref;
    ConstraintDataVectorHolder m_constraint_datas_ref;

    mutable InternalData m_internal_data;
    mutable bool m_solve_in_place_dirty;

    BlockDiagonalMatrix m_damping;
    EigenStorageVector m_compliance_storage;
    typename EigenStorageVector::RefMapType m_compliance;
    BlockDiagonalMatrix m_sum_compliance_damping;
    BlockDiagonalMatrix m_sum_compliance_damping_inverse;
  };

} // namespace pinocchio

namespace pinocchio
{

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    template<typename T> class StorageHolder>
  void DelassusOperatorRigidBodySystemsTpl<
    Scalar,
    Options,
    JointCollectionTpl,
    ConstraintModel,
    StorageHolder>::
    rebuild(
      const ModelHolder & model_ref,
      const DataHolder & data_ref,
      const ConstraintModelVectorHolder & constraint_models_ref,
      const ConstraintDataVectorHolder & constraint_datas_ref)
  {
    // Rebuild quantities related to model
    m_model_ref = model_ref;
    m_data_ref = data_ref;
    m_internal_data.rebuild(model());

    // Rebuild quantities related to constraints
    m_constraint_models_ref = constraint_models_ref;
    m_constraint_datas_ref = constraint_datas_ref;

    m_size = residualSize(internal::helper::get_ref(constraint_models_ref));

    // resize quantities
    m_damping = VectorXs::Constant(m_size, m_min_damping_value).asDiagonal();
    m_compliance_storage.resize(m_size);
    m_compliance.setZero();
    m_sum_compliance_damping = VectorXs::Constant(m_size, m_min_damping_value).asDiagonal();
    m_sum_compliance_damping_inverse = VectorXs::Zero(m_size).asDiagonal();

    assert(m_damping.rows() == m_size);
    assert(m_damping.cols() == m_size);
    assert(m_compliance.size() == m_size);
    assert(m_sum_compliance_damping.rows() == m_size);
    assert(m_sum_compliance_damping_inverse.rows() == m_size);

    retrieveConstraintCompliance(internal::helper::get_ref(constraint_models_ref), m_compliance);

    computeJointMinimalOrdering(model(), data(), internal::helper::get_ref(constraint_models_ref));
    updateSumComplianceDamping();
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    template<typename T> class StorageHolder>
  void DelassusOperatorRigidBodySystemsTpl<
    Scalar,
    Options,
    JointCollectionTpl,
    ConstraintModel,
    StorageHolder>::compute_or_update_decomposition(bool apply_on_the_right, bool solve_in_place)
  {
    typedef typename Data::Inertia Inertia;
    using Matrix6 = typename Inertia::Matrix6;

    const Model & model_ref = model();
    Data & data_ref = data();
    const ConstraintModelVector & constraint_models_ref = constraint_models();
    const ConstraintDataVector & constraint_datas_ref = constraint_datas();

    for (JointIndex i = 1; i < JointIndex(model_ref.njoints); ++i)
    {
      const auto & joint_inertia = model_ref.inertias[i];
      if (apply_on_the_right)
        data_ref.Yaba[i] = joint_inertia.matrix();
      if (solve_in_place)
      {
        const Inertia oinertia = data_ref.oMi[i].act(joint_inertia);
        data_ref.oYaba_augmented[i] = oinertia.matrix();
      }
    }

    if (solve_in_place)
    {
      for (JointIndex joint_id = 1; joint_id < JointIndex(model_ref.njoints); ++joint_id)
      {
        const auto joint_nv = model_ref.nvs[joint_id];
        const auto joint_idx_v = model_ref.idx_vs[joint_id];

        data_ref.joint_apparent_inertia[joint_id] =
          model_ref.armature.segment(joint_idx_v, joint_nv).asDiagonal();
      }

      data_ref.joint_cross_coupling.apply([](Matrix6 & v) { v.setZero(); });

      // Append constraint inertia to oYaba_augmented
      {
        PINOCCHIO_TRACY_ZONE_SCOPED_N("Inverse compliance and damping");
        m_sum_compliance_damping_inverse = m_sum_compliance_damping.inverse();
        assert(!m_sum_compliance_damping_inverse.hasNaN());
      }

      {
        PINOCCHIO_TRACY_ZONE_SCOPED_N("appendCouplingConstraintInertias");
        const auto & blocks = m_sum_compliance_damping_inverse.blocks();
        PINOCCHIO_THROW_PRETTY_IF(
          getSumOfBlockSizes(blocks)
            != residualSize(internal::helper::get_ref(constraint_models_ref)),
          std::runtime_error,
          "The sum of sizes of the blocks should be the same as the total residual size of the "
          "constraints vector.");
        if (blocks.size() == 1 && blocks[0].type() == internal::MatrixBlockType::Diagonal)
        {
          // we assume we have a single diagonal block to dispatch on all the contraints
          typedef typename BlockDiagonalMatrix::ConstVectorMap ConstVectorMap;

          const auto & diagonal_block = blocks[0];
          const auto & compliance_damping_inverse_vector =
            remap<ConstVectorMap>(diagonal_block.container());

          assert(residualSize(internal::helper::get_ref(constraint_models_ref)) == m_size);
          assert(compliance_damping_inverse_vector.size() == m_size);

          Eigen::Index row_id = 0;
          for (std::size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
               ++constraint_id)
          {
            const auto & cmodel = internal::helper::get_ref(constraint_models_ref[constraint_id]);
            const auto & cdata = internal::helper::get_ref(constraint_datas_ref[constraint_id]);

            const auto constraint_size = cmodel.residualSize();
            const auto constraint_diagonal_inertia =
              compliance_damping_inverse_vector.segment(row_id, constraint_size);

            cmodel.appendCouplingConstraintInertias(
              model_ref, data_ref, cdata, constraint_diagonal_inertia, WorldFrameTag());
            row_id += constraint_size;
          }
          assert(row_id == size());
        }
        else
        {
          // One block per outer constraint (atomic or pool as NestedBlockDiagonal).
          PINOCCHIO_THROW_PRETTY_IF(
            blocks.size() != constraint_models_ref.size(), std::runtime_error,
            "The number of blocks should equal the number of constraints. "
            "Pools must be represented as NestedBlockDiagonal blocks.");

          for (std::size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
               ++constraint_id)
          {
            const auto & cmodel = internal::helper::get_ref(constraint_models_ref[constraint_id]);
            const auto & cdata = internal::helper::get_ref(constraint_datas_ref[constraint_id]);
            cmodel.appendCouplingConstraintInertias(
              model_ref, data_ref, cdata, blocks[constraint_id], WorldFrameTag());
          }
        }
      }
    }

#define DO_PASS(apply_on_the_right_v, solve_in_place_v)                                            \
  {                                                                                                \
    typedef DelassusOperatorRigidBodySystemsComputeBackwardPass<                                   \
      DelassusOperatorRigidBodySystemsTpl, apply_on_the_right_v, solve_in_place_v>                 \
      Pass2;                                                                                       \
    for (const JointIndex i : data_ref.joint_elimination_order)                                    \
    {                                                                                              \
      typename Pass2::ArgsType args(model_ref, data_ref);                                          \
      Pass2::run(model_ref.joints[i], data_ref.joints[i], args);                                   \
    }                                                                                              \
  }

    {
      PINOCCHIO_TRACY_ZONE_SCOPED_N("Backward pass");
      if (apply_on_the_right)
      {
        if (solve_in_place)
        {
          DO_PASS(true, true);
        }
        else
        {
          DO_PASS(true, false);
        }
      }
      else
      {
        if (solve_in_place)
        {
          DO_PASS(false, true);
        }
        else
        {
          DO_PASS(false, false);
        }
      }
    }
#undef DO_PASS

    if (solve_in_place)
      m_solve_in_place_dirty = false;
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    template<typename T> class StorageHolder>
  template<typename MatrixIn, typename MatrixOut>
  void DelassusOperatorRigidBodySystemsTpl<
    Scalar,
    Options,
    JointCollectionTpl,
    ConstraintModel,
    StorageHolder>::
    applyOnTheRightImpl(
      const Eigen::MatrixBase<MatrixIn> & rhs,
      const Eigen::MatrixBase<MatrixOut> & res_,
      bool with_damping) const
  {
    MatrixOut & res = res_.const_cast_derived();
    PINOCCHIO_CHECK_SAME_MATRIX_SIZE(rhs, res);

    const Model & model_ref = model();
    const Data & data_ref = data();
    const ConstraintModelVector & constraint_models_ref = constraint_models();
    const ConstraintDataVector & constraint_datas_ref = constraint_datas();
    auto & internal_data = this->m_internal_data;
    auto & u = internal_data.u;

    // Make a pass over the whole set of constraints to add the contributions of constraint forces
    // u and internal_data.f are reset by mapConstraintForcesToJointSpace
    mapConstraintForcesToJointSpace(
      model_ref, data_ref, constraint_models_ref, constraint_datas_ref, rhs, m_internal_data.f, u,
      LocalFrameTag());
    // TODO(jcarpent): extend the code to operator on matrices

    //    typedef Eigen::Map<VectorXs,EIGEN_DEFAULT_ALIGN_BYTES> MapVectorXs;
    //    MapVectorXs u = MapVectorXs(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, model_ref.nv, 1));
    //    {
    //      auto & u = internal_data.u;
    //      u.setZero();
    //      Eigen::Index row_id = 0;
    //      for (size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
    //      ++constraint_id)
    //      {
    //        const auto & cmodel =
    //          internal::helper::get_ref(constraint_models_ref[constraint_id]);
    //        const auto & cdata =
    //          internal::helper::get_ref(constraint_datas_ref[constraint_id]);
    //        const auto csize = cmodel.size();
    //        const auto rhs_rows = rhs.middleRows(row_id, csize);
    //
    //        cmodel.jacobianTransposeMatrixProduct(model_ref, data_ref, cdata, rhs_rows, u,
    //        AddTo());
    //
    //        row_id += csize;
    //      }
    //    }

    // Backward sweep: propagate joint force contributions
    {
      //      for (auto & f : m_internal_data.f)
      //        f.setZero();
      //      auto & u = internal_data.u;
      //      u.setZero();

      typedef DelassusOperatorRigidBodySystemsTplApplyOnTheRightBackwardPass<
        DelassusOperatorRigidBodySystemsTpl>
        Pass1;
      typename Pass1::ArgsType args1(model_ref, data_ref, internal_data);
      for (JointIndex i = JointIndex(model_ref.njoints - 1); i > 0; --i)
      {
        Pass1::run(model_ref.joints[i], data_ref.joints[i], args1);
      }
    }

    // Forward sweep: compute joint accelerations
    {
      typedef DelassusOperatorRigidBodySystemsTplApplyOnTheRightForwardPass<
        DelassusOperatorRigidBodySystemsTpl>
        Pass2;
      for (auto & motion : internal_data.a)
        motion.setZero();
      typename Pass2::ArgsType args2(model_ref, data_ref, internal_data);
      for (JointIndex i = 1; i < JointIndex(model_ref.njoints); ++i)
      {
        Pass2::run(model_ref.joints[i], data_ref.joints[i], args2);
      }
    }

    // Make a pass over the whole set of constraints to project back the accelerations onto the
    // joint
    mapJointSpaceToConstraintMotions(
      model_ref, data_ref, constraint_models_ref, constraint_datas_ref, internal_data.a,
      internal_data.ddq, res, LocalFrameTag());

    // TODO(jcarpent): extend the code to operator on matrices
    //    {
    //      const auto & ddq = internal_data.ddq;
    //      Eigen::Index row_id = 0;
    //      for (size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
    //      ++constraint_id)
    //      {
    //        const auto & cmodel =
    //          internal::helper::get_ref(constraint_models_ref[constraint_id]);
    //        const auto & cdata =
    //          internal::helper::get_ref(constraint_datas_ref[constraint_id]);
    //        const auto csize = cmodel.size();
    //
    //        cmodel.jacobianMatrixProduct(
    //          model_ref, data_ref, cdata, ddq, res.middleRows(row_id, csize));
    //
    //        row_id += csize;
    //      }
    //    }

    // Add damping contribution
    if (with_damping)
    {
      // res.array() += m_sum_compliance_damping.array() * rhs.array();
      m_sum_compliance_damping.template applyOnTheRight<::pinocchio::internal::add_assign_op>(
        rhs, res);
    }
    else
    {
      // take only compliance into account
      res.array() += m_compliance.array() * rhs.array();
    }
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    template<typename T> class StorageHolder>
  template<typename MatrixLike>
  void DelassusOperatorRigidBodySystemsTpl<
    Scalar,
    Options,
    JointCollectionTpl,
    ConstraintModel,
    StorageHolder>::solveInPlaceImpl(const Eigen::MatrixBase<MatrixLike> & mat_) const
  {
    MatrixLike & mat = mat_.const_cast_derived();
    PINOCCHIO_CHECK_ARGUMENT_SIZE(
      mat.rows(), size(), "The input matrix does not match the size of the Delassus.");

    PINOCCHIO_THROW_IF(
      m_solve_in_place_dirty, std::logic_error,
      "The DelassusOperator has dirty quantities. Please call updateDecomposition() first.");

    const Model & model_ref = model();
    const Data & data_ref = data();
    const ConstraintModelVector & constraint_models_ref = constraint_models();
    const ConstraintDataVector & constraint_datas_ref = constraint_datas();
    auto & internal_data = this->m_internal_data;

    typedef Eigen::Map<VectorXs, EIGEN_DEFAULT_ALIGN_BYTES> MapVectorXs;
    MapVectorXs mat_tmp = MapVectorXs(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, size(), 1));

    // mat.array() *= m_sum_compliance_damping_inverse.array();
    m_sum_compliance_damping_inverse.template applyOnTheRight<::pinocchio::internal::assign_op>(
      mat, mat_tmp);
    mat = mat_tmp;

    // Make a pass over the whole set of constraints to add the contributions of constraint

    typedef Eigen::Map<VectorXs, EIGEN_DEFAULT_ALIGN_BYTES> MapVectorXs;
    MapVectorXs u = MapVectorXs(_PINOCCHIO_EIGEN_MAP_ALLOCA(Scalar, model_ref.nv, 1));
    // u and internal_data.of_augmented are reset by mapConstraintForcesToJointSpace
    mapConstraintForcesToJointSpace(
      model_ref, data_ref, constraint_models_ref, constraint_datas_ref, mat,
      internal_data.of_augmented, u, WorldFrameTag());

    //    {
    //      u.setZero();
    //      Eigen::Index row_id = 0;
    //      for (size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
    //      ++constraint_id)
    //      {
    //        const auto & cmodel =
    //          internal::helper::get_ref(constraint_models_ref[constraint_id]);
    //        const auto & cdata =
    //          internal::helper::get_ref(constraint_datas_ref[constraint_id]);
    //        const auto csize = cmodel.size();
    //        const auto mat_rows = mat.middleRows(row_id, csize);
    //
    //        cmodel.jacobianTransposeMatrixProduct(model_ref, data_ref, cdata, mat_rows, u,
    //        AddTo());
    //
    //        row_id += csize;
    //      }
    //    }

    const auto & augmented_mass_matrix_operator = this->getAugmentedMassMatrixOperator();
    augmented_mass_matrix_operator.solveInPlace(u, false);

    //    {
    //      Eigen::Index row_id = 0;
    //      for (size_t constraint_id = 0; constraint_id < constraint_models_ref.size();
    //      ++constraint_id)
    //      {
    //        const auto & cmodel =
    //          internal::helper::get_ref(constraint_models_ref[constraint_id]);
    //        const auto & cdata =
    //          internal::helper::get_ref(constraint_datas_ref[constraint_id]);
    //        const auto csize = cmodel.size();
    //
    //        cmodel.jacobianMatrixProduct(
    //          model_ref, data_ref, cdata, u, mat_tmp.middleRows(row_id, csize));
    //
    //        row_id += csize;
    //      }
    //    }

    // Make a pass over the whole set of constraints to project back the joint accelerations onto
    // the constraints
    mapJointSpaceToConstraintMotions(
      model_ref, data_ref, constraint_models_ref, constraint_datas_ref, internal_data.oa_augmented,
      u, mat_tmp, WorldFrameTag());

    // mat.noalias() -= m_sum_compliance_damping_inverse.asDiagonal() * mat_tmp;
    // m_sum_compliance_damping_inverse.template
    // applyOnTheRight<::pinocchio::internal::sub_assign_op>(mat_tmp,mat.noalias());
    m_sum_compliance_damping_inverse.template applyOnTheRight<::pinocchio::internal::sub_assign_op>(
      mat_tmp, mat); // TODO(jcarpent): fix me with proper noalias handling in applyOnTheRight
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    template<typename T> class StorageHolder>
  template<typename MatrixLike>
  void DelassusOperatorRigidBodySystemsTpl<
    Scalar,
    Options,
    JointCollectionTpl,
    ConstraintModel,
    StorageHolder>::AugmentedMassMatrixOperator::
    solveInPlace(const Eigen::MatrixBase<MatrixLike> & mat_, bool reset_joint_force_vector) const
  {
    MatrixLike & mat = mat_.const_cast_derived();
    const auto & model_ref = m_self.model();
    const auto & data_ref = m_self.data();
    auto & internal_data =
      const_cast<DelassusOperatorRigidBodySystemsTpl &>(m_self).getInternalData();
    const auto & joint_elimination_order = data_ref.joint_elimination_order;

    if (reset_joint_force_vector)
    {
      for (auto & of_augmented : internal_data.of_augmented)
        of_augmented.setZero();
    }

    // Backward sweep: propagate joint force contributions
    {
      internal_data.u = mat;
      typedef AugmentedMassMatrixOperatorSolveInPlaceBackwardPass<
        DelassusOperatorRigidBodySystemsTpl>
        Pass1;
      typename Pass1::ArgsType args1(model_ref, data_ref, internal_data);
      for (const JointIndex i : joint_elimination_order)
      {
        Pass1::run(model_ref.joints[i], data_ref.joints_augmented[i], args1);
      }
    }

    // Forward sweep: compute joint accelerations
    {
      typedef AugmentedMassMatrixOperatorSolveInPlaceForwardPass<
        DelassusOperatorRigidBodySystemsTpl>
        Pass2;
      internal_data.oa_augmented[0].setZero();
      typename Pass2::ArgsType args2(model_ref, data_ref, internal_data);
      for (int it = int(joint_elimination_order.size()) - 1; it >= 0; it--)
      {
        const JointIndex i = joint_elimination_order[size_t(it)];
        Pass2::run(model_ref.joints[i], data_ref.joints_augmented[i], args2);
      }
    }

    mat = internal_data.ddq;
  }

} // namespace pinocchio
