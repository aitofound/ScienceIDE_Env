//
// Copyright (c) 2019-2025 INRIA CNRS
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  // --------------------------------------------------------------
  // Declaration
  // --------------------------------------------------------------
  template<typename Derived>
  struct FrameConstraintModelBase;

  // --------------------------------------------------------------
  // Helpers
  // --------------------------------------------------------------
  template<typename Derived>
  using enable_if_frame_model_t =
    std::enable_if_t<std::is_base_of_v<FrameConstraintModelBase<Derived>, Derived>>;

  // --------------------------------------------------------------
  // Traits
  // --------------------------------------------------------------
  template<typename Derived>
  struct traits<FrameConstraintModelBase<Derived>>
  {
    static constexpr ConstraintSizeType constraint_size_type = ConstraintSizeType::STATIC;
    static constexpr int Size = 6;

    // Template to generate type
    template<typename InputMatrix>
    struct JacobianMatrixProductReturnType
    {
      typedef typename InputMatrix::Scalar Scalar;
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(InputMatrix) InputMatrixPlain;
      typedef Eigen::
        Matrix<Scalar, Size, InputMatrixPlain::ColsAtCompileTime, InputMatrixPlain::Options>
          type;
    };

    template<typename InputMatrix>
    struct JacobianTransposeMatrixProductReturnType
    {
      typedef typename InputMatrix::Scalar Scalar;
      typedef typename PINOCCHIO_EIGEN_PLAIN_TYPE(InputMatrix) InputMatrixPlain;
      typedef Eigen::Matrix<
        Scalar,
        Eigen::Dynamic,
        InputMatrixPlain::ColsAtCompileTime,
        InputMatrixPlain::Options>
        type;
    };
  };

  // --------------------------------------------------------------
  // Struct
  // --------------------------------------------------------------
  template<typename Derived>
  struct FrameConstraintModelBase : BinaryKinematicsConstraintModelBase<Derived>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef BinaryKinematicsConstraintModelBase<Derived> Base;
    typedef typename Base::BaseCommonParameters BaseCommonParameters;
    typedef ConstraintModelBase<Derived> RootBase;

    // Retrieving traits --------------------------------------------
    typedef typename traits<Derived>::ConstraintModel ConstraintModel;
    typedef typename traits<Derived>::ConstraintData ConstraintData;

    typedef typename traits<Derived>::Scalar Scalar;
    static constexpr int Options = traits<Derived>::Options;

    static constexpr int Size = traits<Derived>::Size;

    typedef typename traits<Derived>::ResidualVectorType ResidualVectorType;
    typedef typename traits<Derived>::JacobianMatrixType JacobianMatrixType;

    // Friendship ---------------------------------------------------
    template<typename OtherDerived>
    friend struct FrameConstraintModelBase;

    // Base usage ---------------------------------------------------
    using Base::getA1;
    using Base::getA2;
    using Base::joint1_id;
    using Base::joint2_id;
    using RootBase::jacobianMatrixProduct;
    using RootBase::jacobianTransposeMatrixProduct;
    using RootBase::residualSize;

    // Useful types ------------------------------------------------
    typedef SE3Tpl<Scalar, Options> SE3;
    typedef MotionTpl<Scalar, Options> Motion;
    typedef ForceTpl<Scalar, Options> Force;
    typedef Eigen::Matrix<Scalar, 3, 1, Options> Vector3;
    typedef Eigen::Matrix<Scalar, 6, 1, Options> Vector6;
    typedef Eigen::Matrix<Scalar, 6, 6, Options> Matrix6;
    typedef Eigen::Matrix<Scalar, Size, 6, Options> MatrixSize6;
    typedef Eigen::Matrix<Scalar, 6, 6, Options> Matrix66;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    // CRTP related ------------------

    /// \brief Cast to Base
    Base & base()
    {
      return static_cast<Base &>(*this);
    }

    /// \brief Const cast to Base
    const Base & base() const
    {
      return static_cast<const Base &>(*this);
    }

    /// \brief Cast to BaseCommonParameters.
    BaseCommonParameters & base_common_parameters()
    {
      return static_cast<BaseCommonParameters &>(*this);
    }

    /// \brief Const cast to BaseCommonParameters.
    const BaseCommonParameters & base_common_parameters() const
    {
      return static_cast<const BaseCommonParameters &>(*this);
    }

    // Constructors ------------------

  protected:
    ///
    /// \brief Default constructor
    ///
    FrameConstraintModelBase()
    : Base()
    {
    }

    ///
    /// \brief Constructor from model.
    ///
    /// \param[in] model Kinematic tree.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameConstraintModelBase(const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model)
    : Base(model)
    {
    }

    ///
    /// \brief Constructor from joint indexes and placements.
    ///
    /// \param[in] type Type of the contact.
    /// \param[in] model Model associated to the constraint.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// \param[in] joint2_placement Placement of the constraint w.r.t the frame of joint2.
    /// \param[in] reference_frame Reference frame in which the constraints quantities are
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameConstraintModelBase(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement,
      const JointIndex joint2_id,
      const SE3 & joint2_placement)
    : Base(model, joint1_id, joint1_placement, joint2_id, joint2_placement)
    {
    }

    ///
    /// \brief Constructor from joint1_id.
    ///
    /// \param[in] model Kinematic tree.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    ///
    /// \remarks The second joint id (joint2_id) is set to be 0 (corresponding to the index of the
    /// universe).
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameConstraintModelBase(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model, const JointIndex joint1_id)
    : Base(model, joint1_id)
    {
    }

    ///
    /// \brief Constructor from joint1_id and placement.
    ///
    /// \param[in] model Kinematic tree.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameConstraintModelBase(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement)
    : Base(model, joint1_id, joint1_placement)
    {
    }

    ///
    /// \brief Constructor from joint ids.
    ///
    /// \param[in] model Kinematic tree.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameConstraintModelBase(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const JointIndex joint2_id)
    : Base(model, joint1_id, joint2_id)
    {
    }

    // Operators ---------------------

  public:
    /// \returns An expression of *this with the Scalar type casted to NewScalar.
    template<typename NewScalar, typename OtherDerived>
    void cast(FrameConstraintModelBase<OtherDerived> & res) const
    {
      Base::template cast<NewScalar>(res);
    }

    ///
    /// \brief Comparison operator
    ///
    /// \param[in] other Other FrameConstraintModelBase to compare with.
    ///
    /// \returns true if the two *this is equal to other (type, joint1_id and placement attributes
    /// must be the same).
    ///
    bool operator==(const FrameConstraintModelBase & other) const
    {
      return Base::operator==(other);
    }

    ///
    /// \brief Opposite of the comparison operator.
    ///
    /// \param[in] other Other FrameConstraintModelBase to compare with.
    ///
    /// \returns false if the two *this is not equal to other (at least type, joint1_id or placement
    /// attributes is different).
    ///
    bool operator!=(const FrameConstraintModelBase & other) const
    {
      return Base::operator!=(other);
    }

    // Methods for rigid body --------

    template<
      typename Matrix6LikeOut1,
      typename Matrix6LikeOut2,
      typename Matrix6LikeOut3,
      ReferenceFrame rf>
    void computeConstraintInertias(
      const ConstraintData & cdata,
      const Scalar & constraint_inertia_value,
      const Eigen::MatrixBase<Matrix6LikeOut1> & I11,
      const Eigen::MatrixBase<Matrix6LikeOut2> & I12,
      const Eigen::MatrixBase<Matrix6LikeOut3> & I22,
      const ReferenceFrameTag<rf> reference_frame) const
    {
      const auto cinertia = Vector6::Constant(constraint_inertia_value);
      computeConstraintInertias(
        cdata, cinertia.asDiagonal(), I11.const_cast_derived(), I12.const_cast_derived(),
        I22.const_cast_derived(), reference_frame);
    }

    template<
      typename Matrix6LikeIn,
      typename Matrix6LikeOut1,
      typename Matrix6LikeOut2,
      typename Matrix6LikeOut3,
      ReferenceFrame rf>
    void computeConstraintInertias(
      const ConstraintData & cdata,
      const Eigen::EigenBase<Matrix6LikeIn> & constraint_inertia,
      const Eigen::MatrixBase<Matrix6LikeOut1> & I11,
      const Eigen::MatrixBase<Matrix6LikeOut2> & I12,
      const Eigen::MatrixBase<Matrix6LikeOut3> & I22,
      const ReferenceFrameTag<rf> reference_frame) const
    {
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Matrix6LikeIn, Matrix6);
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Matrix6LikeOut1, Matrix6);
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Matrix6LikeOut2, Matrix6);
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Matrix6LikeOut3, Matrix6);

      PINOCCHIO_UNUSED_VARIABLE(reference_frame);
      //      assert((check_expression_if_real<Scalar,
      //      true>(diagonal_constraint_inertia.isZero(Scalar(0)))));

      const auto & A1 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A1_world : cdata.A1_local;
      const auto & A2 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A2_world : cdata.A2_local;

      Matrix6 constraint_inertia_time_A;
      if (this->joint1_id > 0)
      {
        constraint_inertia_time_A = constraint_inertia.derived() * A1;
        I11.const_cast_derived().noalias() = A1.transpose() * constraint_inertia_time_A;
      }
      else
        I11.const_cast_derived().setZero();

      if (this->joint2_id > 0)
      {
        constraint_inertia_time_A = constraint_inertia.derived() * A2;
        I22.const_cast_derived().noalias() = A2.transpose() * constraint_inertia_time_A;
      }
      else
        I22.const_cast_derived().setZero();

      // Compute the cross coupling term
      if (this->joint1_id > 0 && this->joint2_id > 0)
      {
        // constraint_inertia_time_A has been computed just before
        I12.const_cast_derived().noalias() = A1.transpose() * constraint_inertia_time_A;
      }
      else
        I12.const_cast_derived().setZero();
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    // Methods for algorithms --------

    /// \copydoc RootBase::calc
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    void calcImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      ConstraintData & cdata) const
    {
      PINOCCHIO_UNUSED_VARIABLE(model);

      if (this->joint1_id > 0)
        cdata.oMc1 = data.oMi[this->joint1_id] * this->joint1_placement;
      else
        cdata.oMc1 = this->joint1_placement;

      if (this->joint2_id > 0)
        cdata.oMc2 = data.oMi[this->joint2_id] * this->joint2_placement;
      else
        cdata.oMc2 = this->joint2_placement;

      // Compute relative placement
      cdata.c1Mc2 = cdata.oMc1.actInv(cdata.oMc2);

      // Compute errors
      auto & position_error = cdata.constraint_position_error;
      position_error = log6(cdata.c1Mc2).toVector();

      const auto vf1 = this->joint1_placement.actInv(data.v[this->joint1_id]);
      const auto vf2 = this->joint2_placement.actInv(data.v[this->joint2_id]);

      auto & velocity_error = cdata.constraint_velocity_error;
      const Motion vf2_in_frame1 = cdata.c1Mc2.act(vf2);
      const Motion motion_velocity_error = vf2_in_frame1 - vf1;
      velocity_error = motion_velocity_error.toVector();

      const auto af1 = this->joint1_placement.actInv(data.a[this->joint1_id]);
      const auto af2 = this->joint2_placement.actInv(data.a[this->joint2_id]);
      const Motion af2_in_frame1 = cdata.c1Mc2.act(af2);
      auto & acceleration_error = cdata.constraint_acceleration_error;

      acceleration_error = af2_in_frame1 - af1 + motion_velocity_error.cross(vf2_in_frame1);

      cdata.A1_world = this->getA1(cdata, WorldFrameTag());
      cdata.A2_world = this->getA2(cdata, WorldFrameTag());
      cdata.A_world = cdata.A1_world + cdata.A2_world;

      cdata.A1_local = this->getA1(cdata, LocalFrameTag());
      cdata.A2_local = this->getA2(cdata, LocalFrameTag());
      cdata.A_local = cdata.A1_local + cdata.A2_local;
    }

    /// \copydoc RootBase::jacobian
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename JacobianMatrix>
    void jacobianImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<JacobianMatrix> & _jacobian_matrix) const
    {
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      JacobianMatrix & jacobian_matrix = _jacobian_matrix.const_cast_derived();

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      //      const FrameConstraintModelBase & cmodel = *this;

      const SE3 & oMc1 = cdata.oMc1;

      for (Eigen::Index j = 0; j < model.nv; ++j)
      {
        if (colwise_joint1_sparsity[j] || colwise_joint2_sparsity[j])
        {
          typedef typename Data::Matrix6x::ConstColXpr ConstColXpr;
          const ConstColXpr Jcol = data.J.col(j);
          const MotionRef<const ConstColXpr> Jcol_motion(Jcol);

          // TODO: simplify computations
          if (colwise_joint1_sparsity[j] == colwise_joint2_sparsity[j])
            jacobian_matrix.col(j).setZero();
          else
          {
            const Motion Jcol_local(oMc1.actInv(Jcol_motion));
            if (colwise_joint1_sparsity[j])
              jacobian_matrix.col(j) = -Jcol_local.toVector();
            else
              jacobian_matrix.col(j) = Jcol_local.toVector();
          }
        }
      }
    }

    /// \copydoc RootBase::jacobianMatrixProduct
    template<
      typename InputMatrix,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl>
    typename traits<Derived>::template JacobianMatrixProductReturnType<InputMatrix>::type
    jacobianMatrixProductImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<InputMatrix> & mat) const
    {
      typedef typename traits<Derived>::template JacobianMatrixProductReturnType<InputMatrix>::type
        ReturnType;
      ReturnType res(6, mat.cols());
      jacobianMatrixProduct(model, data, cdata, mat.derived(), res);
      return res;
    }

    /// \copydoc RootBase::jacobianMatrixProduct
    template<
      typename InputMatrix,
      typename OutputMatrix,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      AssignmentOperatorType op>
    void jacobianMatrixProductImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<InputMatrix> & mat,
      const Eigen::MatrixBase<OutputMatrix> & _res,
      AssignmentOperatorTag<op> aot) const
    {
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef typename Data::Vector6 Vector6;
      OutputMatrix & res = _res.const_cast_derived();

      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.rows(), model.nv);
      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.cols(), res.cols());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.rows(), residualSize());
      PINOCCHIO_UNUSED_VARIABLE(aot);

      if constexpr (std::is_same<AssignmentOperatorTag<op>, SetTo>::value)
        res.setZero();

      //      const Eigen::Index constraint_size = size();
      //
      //      const Eigen::Index
      //      complexity_strategy_1 = 6 * res.cols() * 36 + constraint_size * 36 * res.cols(),
      //      complexity_strategy_2 = 36 * constraint_size * 6 + constraint_size * 36 * res.cols();

      const Matrix6 A = getA2(cdata, WorldFrameTag());

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      for (Eigen::Index jj = 0; jj < model.nv; ++jj)
      {
        if (!(colwise_joint1_sparsity[jj] || colwise_joint2_sparsity[jj]))
          continue;
        if (colwise_joint1_sparsity[jj] == colwise_joint2_sparsity[jj])
          continue;
        Vector6 AxSi;

        typedef typename Data::Matrix6x::ConstColXpr ConstColXpr;
        const ConstColXpr Jcol = data.J.col(jj);

        if (colwise_joint1_sparsity[jj])
          AxSi.noalias() = -A * Jcol;
        else
          AxSi.noalias() = A * Jcol;

        if constexpr (std::is_same<AssignmentOperatorTag<op>, RmTo>::value)
          res.noalias() -= AxSi * mat.row(jj);
        else // AddTo, SetTo
          res.noalias() += AxSi * mat.row(jj);
      }
    }

    /// \copydoc RootBase::jacobianTransposeMatrixProduct
    template<
      typename InputMatrix,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl>
    typename traits<Derived>::template JacobianTransposeMatrixProductReturnType<InputMatrix>::type
    jacobianTransposeMatrixProductImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<InputMatrix> & mat) const
    {
      typedef typename traits<Derived>::template JacobianTransposeMatrixProductReturnType<
        InputMatrix>::type ReturnType;
      ReturnType res(model.nv, mat.cols());
      jacobianTransposeMatrixProduct(model, data, cdata, mat.derived(), res);
      return res;
    }

    /// \copydoc RootBase::jacobianTransposeMatrixProduct
    template<
      typename InputMatrix,
      typename OutputMatrix,
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      AssignmentOperatorType op>
    void jacobianTransposeMatrixProductImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<InputMatrix> & mat,
      const Eigen::MatrixBase<OutputMatrix> & _res,
      AssignmentOperatorTag<op> aot) const
    {
      typedef DataTpl<Scalar, OtherOptions, JointCollectionTpl> Data;
      typedef typename Data::Vector6 Vector6;
      OutputMatrix & res = _res.const_cast_derived();

      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.rows(), residualSize());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.cols(), mat.cols());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.rows(), model.nv);
      PINOCCHIO_UNUSED_VARIABLE(aot);

      if constexpr (std::is_same<AssignmentOperatorTag<op>, SetTo>::value)
        res.setZero();

      const Matrix6 A = getA2(cdata, WorldFrameTag());

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      for (Eigen::Index jj = 0; jj < model.nv; ++jj)
      {
        if (!(colwise_joint1_sparsity[jj] || colwise_joint2_sparsity[jj]))
          continue;
        if (colwise_joint1_sparsity[jj] == colwise_joint2_sparsity[jj])
          continue;
        Vector6 AxSi;

        typedef typename Data::Matrix6x::ConstColXpr ConstColXpr;
        const ConstColXpr Jcol = data.J.col(jj);

        if (colwise_joint1_sparsity[jj])
          AxSi.noalias() = -A * Jcol;
        else
          AxSi.noalias() = A * Jcol;

        if constexpr (std::is_same<AssignmentOperatorTag<op>, RmTo>::value)
          res.row(jj).noalias() -= AxSi.transpose() * mat;
        else
          res.row(jj).noalias() += AxSi.transpose() * mat;
      }
    }

    /// \copydoc RootBase::mapConstraintForceToJointForces
    template<
      int OtherOptions,
      int ForceOptions,
      template<typename, int> class JointCollectionTpl,
      typename ForceLike,
      typename ForceAllocator,
      ReferenceFrame rf>
    void mapConstraintForceToJointForcesImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<ForceLike> & constraint_forces,
      std::vector<ForceTpl<Scalar, ForceOptions>, ForceAllocator> & joint_forces,
      ReferenceFrameTag<rf> reference_frame) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(joint_forces.size(), size_t(model.njoints));
      PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_forces.rows(), residualSize());
      PINOCCHIO_UNUSED_VARIABLE(data);
      PINOCCHIO_UNUSED_VARIABLE(reference_frame);

      // Todo: optimize code
      const auto & A1 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A1_world : cdata.A1_local;
      const auto & A2 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A2_world : cdata.A2_local;

      if (this->joint1_id > 0)
        joint_forces[this->joint1_id].toVector().noalias() += A1.transpose() * constraint_forces;
      if (this->joint2_id > 0)
        joint_forces[this->joint2_id].toVector().noalias() += A2.transpose() * constraint_forces;
    }

    /// \copydoc RootBase::mapJointMotionsToConstraintMotion
    template<
      int OtherOptions,
      int MotionOptions,
      template<typename, int> class JointCollectionTpl,
      typename MotionAllocator,
      typename VectorLike,
      ReferenceFrame rf>
    void mapJointMotionsToConstraintMotionImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const std::vector<MotionTpl<Scalar, MotionOptions>, MotionAllocator> & joint_accelerations,
      const Eigen::MatrixBase<VectorLike> & constraint_motion,
      ReferenceFrameTag<rf> reference_frame) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(joint_accelerations.size(), size_t(model.njoints));
      PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_motion.rows(), residualSize());
      PINOCCHIO_UNUSED_VARIABLE(data);
      PINOCCHIO_UNUSED_VARIABLE(reference_frame);

      const auto & A1 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A1_world : cdata.A1_local;
      const auto & A2 =
        std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value ? cdata.A2_world : cdata.A2_local;

      if (this->joint1_id > 0 && this->joint2_id > 0)
        constraint_motion.const_cast_derived().noalias() =
          A1 * joint_accelerations[this->joint1_id].toVector()
          + A2 * joint_accelerations[this->joint2_id].toVector();
      else if (this->joint1_id > 0)
        constraint_motion.const_cast_derived().noalias() =
          A1 * joint_accelerations[this->joint1_id].toVector();
      else if (this->joint2_id > 0)
        constraint_motion.const_cast_derived().noalias() =
          A2 * joint_accelerations[this->joint2_id].toVector();
      else
        constraint_motion.const_cast_derived().setZero();
    }

    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename Matrix6Like,
      ReferenceFrame rf>
    void appendFrameContactConstraintInertias(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::EigenBase<Matrix6Like> & constraint_inertia,
      const ReferenceFrameTag<rf> reference_frame) const
    {
      PINOCCHIO_UNUSED_VARIABLE(model);
      Matrix6 I11, I12, I22;
      computeConstraintInertias(cdata, constraint_inertia, I11, I12, I22, reference_frame);

      assert(
        (std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value
         || std::is_same<ReferenceFrameTag<rf>, LocalFrameTag>::value)
        && "must never happened");

      Matrix6 & Y1 = std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value
                       ? data.oYaba_augmented[this->joint1_id]
                       : data.oYaba_augmented[this->joint1_id];

      if (this->joint1_id > 0)
        Y1 += I11;

      Matrix6 & Y2 = std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value
                       ? data.oYaba_augmented[this->joint2_id]
                       : data.oYaba_augmented[this->joint2_id];

      if (this->joint2_id > 0)
        Y2 += I22;

      if (this->joint1_id > 0 && this->joint2_id > 0)
      {
        assert(
          data.joint_cross_coupling.exists({this->joint1_id, this->joint2_id})
          || data.joint_cross_coupling.exists({this->joint2_id, this->joint1_id}));
        if (data.joint_cross_coupling.exists({this->joint1_id, this->joint2_id}))
        {
          data.joint_cross_coupling.get({this->joint1_id, this->joint2_id}) += I12;
        }
        else
        {
          data.joint_cross_coupling.get({this->joint2_id, this->joint1_id}) += I12.transpose();
        }
      }
    }

    /// \copydoc RootBase::appendCouplingConstraintInertias
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename Vector6Like,
      ReferenceFrame rf>
    void appendCouplingConstraintInertiasImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<Vector6Like> & diagonal_constraint_inertia,
      const ReferenceFrameTag<rf> reference_frame) const
    {
      EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Vector6Like, Vector6);
      appendFrameContactConstraintInertias(
        model, data, cdata, diagonal_constraint_inertia.asDiagonal(), reference_frame);
    }

    /// \copydoc RootBase::appendCouplingConstraintInertias
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename MatrixOrMap,
      typename MapEnable,
      ReferenceFrame rf>
    void appendCouplingConstraintInertiasImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const internal::MatrixBlockElementTpl<MatrixOrMap, MapEnable> & constraint_inertia,
      const ReferenceFrameTag<rf> reference_frame) const
    {
      assert(constraint_inertia.size() == 6);
      switch (constraint_inertia.type())
      {
      case internal::MatrixBlockType::Zero: {
        break;
      }
      case internal::MatrixBlockType::Identity: {
        appendFrameContactConstraintInertias(
          model, data, cdata, Matrix6::Identity(), reference_frame);
        break;
      }
      case internal::MatrixBlockType::ScalarIdentity: {
        const Scalar inertia_val = constraint_inertia.container()(0, 0);
        const auto cinertia = Vector6::Constant(inertia_val);
        appendFrameContactConstraintInertias(
          model, data, cdata, cinertia.asDiagonal(), reference_frame);
        break;
      }
      case internal::MatrixBlockType::Diagonal: {
        Vector6 cinertia;
        constraint_inertia.diagonal(cinertia);
        appendFrameContactConstraintInertias(
          model, data, cdata, cinertia.asDiagonal(), reference_frame);
        break;
      }
      case internal::MatrixBlockType::Plain: {
        Matrix6 cinertia;
        constraint_inertia.matrix(cinertia);
        appendFrameContactConstraintInertias(model, data, cdata, cinertia, reference_frame);
        break;
      }
      default:
        assert(false && "Invalid MatrixBlockType for FrameConstraintModelBase.");
        PINOCCHIO_THROW_PRETTY(
          std::invalid_argument, "Invalid MatrixBlockType for FrameConstraintModelBase.");
      }
    }

    /// \copydoc Base::getA1
    template<ReferenceFrame rf>
    Matrix6 getA1Impl(const ConstraintData & cdata, ReferenceFrameTag<rf>) const
    {
      Matrix6 res;

      if constexpr (std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value)
      {
        const SE3 & oM1 = cdata.oMc1;
        res = -oM1.toActionMatrixInverse();
      }
      else if constexpr (std::is_same<ReferenceFrameTag<rf>, LocalFrameTag>::value)
      {
        const SE3 & j1Mc1 = this->joint1_placement;
        res = -j1Mc1.toActionMatrixInverse();
      }

      return res;
    }

    /// \copydoc Base::getA2
    template<ReferenceFrame rf>
    Matrix6 getA2Impl(const ConstraintData & cdata, ReferenceFrameTag<rf>) const
    {
      Matrix6 res;

      if constexpr (std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value)
      {
        const SE3 & oM1 = cdata.oMc1;
        res = oM1.toActionMatrixInverse();
      }
      else if constexpr (std::is_same<ReferenceFrameTag<rf>, LocalFrameTag>::value)
      {
        const SE3 & j2Mc2 = this->joint2_placement;
        const SE3 & c1Mc2 = cdata.c1Mc2;
        const SE3 c1Mj2 = c1Mc2.act(j2Mc2.inverse());
        res = c1Mj2.toActionMatrix();
      }

      return res;
    }
  }; // struct FrameConstraintModelBase

} // namespace pinocchio
