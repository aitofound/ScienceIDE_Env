//
// Copyright (c) 2019-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  ///  \brief Type of contact
  enum ContactType
  {
    CONTACT_3D = 0,   /// \brief Point contact model
    CONTACT_6D,       ///  \brief Frame contact model
    CONTACT_UNDEFINED ///  \brief The default contact is undefined
  };

  template<ContactType contact_type>
  struct contact_dim
  {
    static constexpr int value = 0;
  };

  template<>
  struct contact_dim<CONTACT_3D>
  {
    static constexpr int value = 3;
  };

  template<>
  struct contact_dim<CONTACT_6D>
  {
    static constexpr int value = 6;
  };

  template<typename NewScalar, typename Scalar, int Options>
  struct CastType<NewScalar, RigidConstraintModelTpl<Scalar, Options>>
  {
    typedef RigidConstraintModelTpl<NewScalar, Options> type;
  };

  // --------------------------------------------------------------
  // Traits
  // --------------------------------------------------------------
  template<typename _Scalar, int _Options>
  struct traits<RigidConstraintModelTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Traits referencing the constraint and associated types
    // --------------------------------------------------------------
    typedef RigidConstraintModelTpl<_Scalar, _Options> ConstraintModel;
    typedef RigidConstraintDataTpl<_Scalar, _Options> ConstraintData;

    typedef ConstraintModel Model;
    typedef ConstraintData Data;

    // --------------------------------------------------------------
    // Traits characterizing the constraints
    // --------------------------------------------------------------
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    static constexpr ConstraintSizeType constraint_size_type = ConstraintSizeType::CONSTANT;

    static constexpr bool has_baumgarte_corrector = true;
    static constexpr bool has_set = true;
    static constexpr bool is_inequality_constraint = false;

    // --------------------------------------------------------------
    // Traits for associated struct and sizes
    // --------------------------------------------------------------
    typedef FullSpaceConeTpl<Scalar, Options> ConstraintSet;
    typedef ZeroConeJordanOperationTpl<Scalar, Options> JordanOperation;
    typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;

    static constexpr int Size = Eigen::Dynamic;
    static constexpr int SymmetricConeSize = JordanOperation::ConeSize;
    static constexpr int SymmetricConeScalingSize = JordanOperation::ConeScalingSize;

    // --------------------------------------------------------------
    // Traits that are helper for Eigen types
    // --------------------------------------------------------------
    typedef Eigen::Matrix<Scalar, Size, 1, Options> ResidualVectorType;
    typedef Eigen::Matrix<Scalar, Size, Eigen::Dynamic, Options> JacobianMatrixType;
    typedef Eigen::Matrix<Scalar, SymmetricConeSize, 1, Options> ConeVectorType;
    typedef Eigen::Matrix<Scalar, SymmetricConeScalingSize, 1, Options> ConeScalingVectorType;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Matrix<Scalar, 1, Eigen::Dynamic, Eigen::RowMajor> RowVectorXs;

    // Template to generate type
    template<typename InputMatrix>
    struct JacobianMatrixProductReturnType
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

  template<typename _Scalar, int _Options>
  struct traits<RigidConstraintDataTpl<_Scalar, _Options>>
  : traits<RigidConstraintModelTpl<_Scalar, _Options>>
  {
  };

  // Helper to initialize the type before Binary kinematic
  template<typename Derived>
  struct RigidConstraintModelTyper
  {
    ContactType type;

    RigidConstraintModelTyper()
    : type(ContactType::CONTACT_UNDEFINED)
    {
    }

    RigidConstraintModelTyper(ContactType t)
    : type(t)
    {
    }
  };

  ///
  ///  \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct PINOCCHIO_UNSUPPORTED_MESSAGE("The API will change towards more flexibility")
    RigidConstraintModelTpl
  : RigidConstraintModelTyper<RigidConstraintModelTpl<_Scalar, _Options>>
  , BinaryKinematicsConstraintModelBase<RigidConstraintModelTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef RigidConstraintModelTpl Self;
    typedef RigidConstraintModelTyper<Self> Typer;
    typedef BinaryKinematicsConstraintModelBase<Self> Base;
    typedef ConstraintModelCommonParameters<Self> BaseCommonParameters;
    typedef ConstraintModelBase<Self> RootBase;

    // Retrieving traits --------------------------------------------
    typedef typename traits<Self>::ConstraintModel ConstraintModel;
    typedef typename traits<Self>::ConstraintData ConstraintData;
    typedef typename traits<Self>::ConstraintData ContactData;

    typedef typename traits<Self>::Scalar Scalar;
    static constexpr int Options = traits<Self>::Options;

    static constexpr ConstraintSizeType constraint_size_type = traits<Self>::constraint_size_type;

    static constexpr bool has_baumgarte_corrector = traits<Self>::has_baumgarte_corrector;

    typedef typename traits<Self>::ConstraintSet ConstraintSet;
    typedef typename traits<Self>::JordanOperation JordanOperation;
    typedef typename traits<Self>::BaumgarteCorrectorParameters BaumgarteCorrectorParameters;

    static constexpr int Size = traits<Self>::Size;
    static constexpr int SymmetricConeSize = traits<Self>::SymmetricConeSize;
    static constexpr int SymmetricConeScalingSize = traits<Self>::SymmetricConeScalingSize;

    typedef typename traits<Self>::ResidualVectorType ResidualVectorType;
    typedef typename traits<Self>::JacobianMatrixType JacobianMatrixType;
    typedef typename traits<Self>::ConeVectorType ConeVectorType;
    typedef typename traits<Self>::ConeScalingVectorType ConeScalingVectorType;

    // Friendship ---------------------------------------------------
    template<typename NewScalar, int NewOptions>
    friend struct RigidConstraintModelTpl;

    // Base usage ---------------------------------------------------
    // Usefull types ------------------------------------------------
    typedef SE3Tpl<Scalar, Options> SE3;
    typedef MotionTpl<Scalar, Options> Motion;
    typedef ForceTpl<Scalar, Options> Force;
    typedef Eigen::Matrix<Scalar, 3, 6, Options> Matrix36;
    typedef Eigen::Matrix<Scalar, 6, 6, Options> Matrix6;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 6, Options> MatrixSize6;
    typedef Eigen::Matrix<Scalar, 3, 1, Options> Vector3;
    typedef Eigen::Matrix<Scalar, 6, 1, Options> Vector6;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;

    // Base usage ---------------------------------------------------
    using Base::base;
    using Base::base_common_parameters;
    using Base::getA1;
    using Base::getA2;
    using RootBase::classname;
    using RootBase::derived;
    using RootBase::residualSize;
    using typename RootBase::BooleanVector;
    using typename RootBase::EigenIndexVector;

    // -------------------------------
    // MEMBERS
    // -------------------------------

    /// \brief Reference frame where the constraint is expressed (LOCAL_WORLD_ALIGNED or LOCAL)
    ReferenceFrame reference_frame;

    /// \brief Desired contact placement
    SE3 desired_contact_placement; // -> desired_constraint_offset

    /// \brief Desired contact spatial velocity
    Motion desired_contact_velocity; // -> desired_constraint_velocity

    /// \brief Desired contact spatial acceleration
    Motion desired_contact_acceleration; // -> desired_constraint_acceleration

    EigenIndexVector loop_span_indexes;

    /// Members from base
    using Base::depth_joint1;
    using Base::depth_joint2;
    using Base::joint1_id;
    using Base::joint1_placement;
    using Base::joint2_id;
    using Base::joint2_placement;
    using Base::m_baumgarte_parameters;
    using Base::m_compliance;
    using Base::nv;
    using Typer::type;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    // Constructors ------------------

    ///
    ///  \brief Default constructor
    ///
    RigidConstraintModelTpl()
    : Typer()
    , Base()
    {
    }

    ///
    ///  \brief Constructor with from a given type, joint indexes and placements.
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
    RigidConstraintModelTpl(
      const ContactType type,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement,
      const JointIndex joint2_id,
      const SE3 & joint2_placement,
      const ReferenceFrame & reference_frame = LOCAL)
    : Typer(type)
    , Base(model, joint1_id, joint1_placement, joint2_id, joint2_placement)
    , reference_frame(reference_frame)
    , desired_contact_placement(SE3::Identity())
    , desired_contact_velocity(Motion::Zero())
    , desired_contact_acceleration(Motion::Zero())
    , loop_span_indexes((size_t)model.nv)
    {
      init(model);
    }

    ///
    ///  \brief Constructor with from a given type, joint1_id and placement.
    ///
    /// \param[in] type Type of the contact.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// \param[in] reference_frame Reference frame in which the constraints quantities are
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    RigidConstraintModelTpl(
      const ContactType type,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement,
      const ReferenceFrame & reference_frame = LOCAL)
    : RigidConstraintModelTpl(
        type, model, joint1_id, joint1_placement, 0, SE3::Identity(), reference_frame)
    {
    }

    ///
    ///  \brief Constructor with from a given type and the joint ids.
    ///
    /// \param[in] type Type of the contact.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    RigidConstraintModelTpl(
      const ContactType type,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const JointIndex joint2_id,
      const ReferenceFrame & reference_frame = LOCAL)
    : RigidConstraintModelTpl(
        type, model, joint1_id, SE3::Identity(), joint2_id, SE3::Identity(), reference_frame)
    {
    }

    ///
    ///  \brief Constructor with from a given type and .
    ///
    /// \param[in] type Type of the contact.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    ///
    /// \remarks The second joint id (joint2_id) is set to be 0 (corresponding to the index of the
    /// universe).
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    RigidConstraintModelTpl(
      const ContactType type,
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const ReferenceFrame & reference_frame = LOCAL)
    : RigidConstraintModelTpl(
        type, model, joint1_id, SE3::Identity(), 0, SE3::Identity(), reference_frame)
    {
    }

    // Operators ---------------------

    /// \returns An expression of *this with the Scalar type casted to NewScalar.
    template<typename NewScalar>
    RigidConstraintModelTpl<NewScalar, Options> cast() const
    {
      typedef RigidConstraintModelTpl<NewScalar, Options> ReturnType;
      ReturnType res;
      res.type = type;
      Base::template cast<NewScalar>(res);
      res.reference_frame = reference_frame;
      res.desired_contact_placement = desired_contact_placement.template cast<NewScalar>();
      res.desired_contact_velocity = desired_contact_velocity.template cast<NewScalar>();
      res.desired_contact_acceleration = desired_contact_acceleration.template cast<NewScalar>();
      res.loop_span_indexes = loop_span_indexes;

      return res;
    }

    ///
    ///  \brief Comparison operator
    ///
    /// \param[in] other Other RigidConstraintModelTpl to compare with.
    ///
    /// \returns true if the two *this is equal to other (type, joint1_id and placement attributes
    /// must be the same).
    ///
    template<int OtherOptions>
    bool operator==(const RigidConstraintModelTpl<Scalar, OtherOptions> & other) const
    {
      return type == other.type && base() == other.base()
             && reference_frame == other.reference_frame
             && loop_span_indexes == other.loop_span_indexes;
    }

    ///
    ///  \brief Opposite of the comparison operator.
    ///
    /// \param[in] other Other RigidConstraintModelTpl to compare with.
    ///
    /// \returns false if the two *this is not equal to other (at least type, joint1_id or placement
    /// attributes is different).
    ///
    template<int OtherOptions>
    bool operator!=(const RigidConstraintModelTpl<Scalar, OtherOptions> & other) const
    {
      return !(*this == other);
    }

    // Specific methods --------------

    ///
    /// @brief This function computes the spatial inertia associated with the constraint.
    /// This function is useful to express the constraint inertia associated with the constraint for
    /// AL settings.
    ///
    template<typename Vector3Like>
    Matrix6 computeConstraintSpatialInertia(
      const SE3Tpl<Scalar, Options> & placement,
      const Eigen::MatrixBase<Vector3Like> & diagonal_constraint_inertia) const
    {
      EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Vector3Like, Vector3);
      Matrix6 res;

      const auto & R = placement.rotation();
      const auto & t = placement.translation();

      typedef Eigen::Matrix<Scalar, 3, 3, Options> Matrix3;
      const Matrix3 R_Sigma = R * diagonal_constraint_inertia.asDiagonal();
      const Matrix3 t_skew = skew(t);

      auto block_LL = res.template block<3, 3>(SE3::LINEAR, SE3::LINEAR);
      auto block_LA = res.template block<3, 3>(SE3::LINEAR, SE3::ANGULAR);
      auto block_AL = res.template block<3, 3>(SE3::ANGULAR, SE3::LINEAR);
      auto block_AA = res.template block<3, 3>(SE3::ANGULAR, SE3::ANGULAR);

      block_LL.noalias() = R_Sigma * R.transpose();
      block_LA.noalias() = -block_LL * t_skew;
      block_AL.noalias() = block_LA.transpose();
      block_AA.noalias() = t_skew * block_LA;

      return res;
    }

    template<
      typename InputMatrix,
      typename OutputMatrix,
      template<typename, int> class JointCollectionTpl>
    void jacobian_matrix_product(
      const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const DataTpl<Scalar, Options, JointCollectionTpl> & data,
      RigidConstraintDataTpl<Scalar, Options> & cdata,
      const Eigen::MatrixBase<InputMatrix> & mat,
      const Eigen::MatrixBase<OutputMatrix> & _res) const
    {
      typedef DataTpl<Scalar, Options, JointCollectionTpl> Data;
      typedef typename Data::Vector3 Vector3;
      OutputMatrix & res = _res.const_cast_derived();

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.rows(), model.nv);
      PINOCCHIO_CHECK_ARGUMENT_SIZE(mat.cols(), res.cols());
      PINOCCHIO_CHECK_ARGUMENT_SIZE(res.rows(), residualSize()); // We know it is constant
      res.setZero();

      //      const Eigen::Index constraint_size = size();
      //
      //      const Eigen::Index
      //      complexity_strategy_1 = 6 * res.cols() * 36 + constraint_size * 36 * res.cols(),
      //      complexity_strategy_2 = 36 * constraint_size * 6 + constraint_size * 36 * res.cols();

      const Matrix36 A1 = getA1(cdata, WorldFrameTag());
      const Matrix36 A2 = getA2(cdata, WorldFrameTag());
      for (Eigen::Index jj = 0; jj < model.nv; ++jj)
      {
        if (!(colwise_joint1_sparsity[jj] || colwise_joint2_sparsity[jj]))
          continue;
        Matrix36 A;
        Vector3 AxSi;

        typedef typename Data::Matrix6x::ConstColXpr ConstColXpr;
        const ConstColXpr Jcol = data.J.col(jj);

        if (colwise_joint1_sparsity[jj] && colwise_joint2_sparsity[jj])
        {
          A = A1 + A2;
          AxSi.noalias() = A * Jcol;
        }
        else if (colwise_joint1_sparsity[jj])
          AxSi.noalias() = A1 * Jcol;
        else
          AxSi.noalias() = A2 * Jcol;

        res.noalias() += AxSi * mat.row(jj);
      }
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    // General -----------------------

    /// \copydoc RootBase::classname
    static std::string classnameImpl()
    {
      return std::string("DEPRECATED:ConstraintModel");
    }

    /// \copydoc RootBase::shortname
    std::string shortnameImpl() const
    {
      return classname();
    }

    ///
    /// \brief Create data storage associated to the constraint
    ///
    ConstraintData createDataImpl() const
    {
      return ConstraintData(*this);
    }

    // Sizes ------------------------------

    /// \copydoc RootBase::residualSize
    template<ConstraintSelectionType Sel>
    int residualSizeImpl(ConstraintSelectionTag<Sel> sel) const
    {
      PINOCCHIO_UNUSED_VARIABLE(sel);
      switch (type)
      {
      case CONTACT_3D:
        return contact_dim<CONTACT_3D>::value;
      case CONTACT_6D:
        return contact_dim<CONTACT_6D>::value;
      default:
        return contact_dim<CONTACT_UNDEFINED>::value;
      }
      return -1;
    }

    // Methods for algorithms --------

    /// \copydoc RootBase::set
    ConstraintSet setImpl(const ConstraintData & cdata) const
    {
      PINOCCHIO_UNUSED_VARIABLE(cdata);
      return ConstraintSet();
    }

    /// \brief Evaluate the constraint values at the current state given by data and store the
    /// results in cdata.
    template<template<typename, int> class JointCollectionTpl>
    void calcImpl(
      const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const DataTpl<Scalar, Options, JointCollectionTpl> & data,
      RigidConstraintDataTpl<Scalar, Options> & cdata) const
    {
      PINOCCHIO_UNUSED_VARIABLE(model);

      if (joint1_id > 0)
        cdata.oMc1 = data.oMi[joint1_id] * joint1_placement;
      else
        cdata.oMc1 = joint1_placement;

      if (joint2_id > 0)
        cdata.oMc2 = data.oMi[joint2_id] * joint2_placement;
      else
        cdata.oMc2 = joint2_placement;

      // Compute relative placement
      cdata.c1Mc2 = cdata.oMc1.actInv(cdata.oMc2);
    }

    ///  \brief Evaluate the Jacobian associated to the constraint at the given state stored in data
    /// and cdata.  The results Jacobian is evaluated in the jacobian input/output matrix.
    /// \remarks This method assumes `calc` has been called on this contraint model.
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename JacobianMatrix>
    void jacobianImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const RigidConstraintDataTpl<Scalar, Options> & cdata,
      const Eigen::MatrixBase<JacobianMatrix> & _jacobian_matrix) const
    {
      typedef DataTpl<Scalar, Options, JointCollectionTpl> Data;
      JacobianMatrix & jacobian_matrix = _jacobian_matrix.const_cast_derived();

      const RigidConstraintModelTpl & cmodel = *this;

      const SE3 & oMc1 = cdata.oMc1;
      const SE3 & oMc2 = cdata.oMc2;
      const SE3 & c1Mc2 = cdata.c1Mc2;

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      for (Eigen::Index jj = 0; jj < model.nv; ++jj)
      {

        if (colwise_joint1_sparsity[jj] || colwise_joint2_sparsity[jj])
        {
          const int sign = colwise_joint1_sparsity[jj] != colwise_joint2_sparsity[jj]
                             ? colwise_joint1_sparsity[jj] ? +1 : -1
                             : 0; // specific case for CONTACT_3D

          typedef typename Data::Matrix6x::ConstColXpr ConstColXpr;
          const ConstColXpr Jcol = data.J.col(jj);
          const MotionRef<const ConstColXpr> Jcol_motion(Jcol);

          switch (cmodel.type)
          {
          case CONTACT_3D: {
            switch (cmodel.reference_frame)
            {
            case LOCAL: {
              if (sign == 0)
              {
                const Motion Jcol_local1(oMc1.actInv(Jcol_motion)); // TODO: simplify computations
                const Motion Jcol_local2(oMc2.actInv(Jcol_motion)); // TODO: simplify computations
                const typename Motion::Vector3 Jdiff_linear =
                  Jcol_local1.linear() - c1Mc2.rotation() * Jcol_local2.linear();
                jacobian_matrix.col(jj) = Jdiff_linear;
                break;
              }
              else if (sign == 1)
              {
                const Motion Jcol_local(oMc1.actInv(Jcol_motion));
                jacobian_matrix.col(jj) = Jcol_local.linear();
                break;
              }
              else // sign == -1
              {
                Motion Jcol_local(oMc2.actInv(Jcol_motion)); // TODO: simplify computations
                Jcol_local.linear() =
                  c1Mc2.rotation() * Jcol_local.linear(); // TODO: simplify computations
                jacobian_matrix.col(jj) = -Jcol_local.linear();
                break;
              }
            }
            case LOCAL_WORLD_ALIGNED: {
              if (sign == 0)
              {
                const typename Motion::Vector3 Jdiff_linear =
                  (oMc2.translation() - oMc1.translation()).cross(Jcol_motion.angular());
                jacobian_matrix.col(jj) = Jdiff_linear;
                break;
              }
              else
              {
                typename Motion::Vector3 Jcol_local_world_aligned_linear(Jcol_motion.linear());
                if (sign == 1)
                  Jcol_local_world_aligned_linear -=
                    oMc1.translation().cross(Jcol_motion.angular());
                else
                  Jcol_local_world_aligned_linear -=
                    oMc2.translation().cross(Jcol_motion.angular());
                jacobian_matrix.col(jj) = Jcol_local_world_aligned_linear * Scalar(sign);
                break;
              }
            }
            case WORLD: {
              PINOCCHIO_THROW_PRETTY(
                std::invalid_argument, "Contact3D in world frame is not managed");
            }
            }
            break;
          }

          case CONTACT_6D: {
            switch (cmodel.reference_frame)
            {
            case LOCAL: {
              const Motion Jcol_local(oMc1.actInv(Jcol_motion));
              jacobian_matrix.col(jj) = Jcol_local.toVector() * Scalar(sign);
              break;
            }
            case LOCAL_WORLD_ALIGNED: {
              Motion Jcol_local_world_aligned(Jcol_motion);
              Jcol_local_world_aligned.linear() -=
                oMc1.translation().cross(Jcol_local_world_aligned.angular());
              jacobian_matrix.col(jj) = Jcol_local_world_aligned.toVector() * Scalar(sign);
              break;
            }
            case WORLD: {
              PINOCCHIO_THROW_PRETTY(
                std::invalid_argument, "Contact6D in world frame is not managed");
            }
            }
            break;
          }

          default:
            PINOCCHIO_UNREACHABLE();
          }
        }
      }
    }

    /// NO jacobianMatrixProductImpl
    /// NO jacobianTransposeMatrixProductImpl

    /// \brief Map the constraint forces (aka constraint Lagrange multipliers) to the forces
    /// supported by the joints.
    template<
      int OtherOptions,
      int ForceOptions,
      template<typename, int> class JointCollectionTpl,
      typename ForceLike,
      typename ForceAllocator>
    void mapConstraintForceToJointForcesImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const RigidConstraintDataTpl<Scalar, Options> & cdata,
      const Eigen::MatrixBase<ForceLike> & constraint_forces,
      std::vector<ForceTpl<Scalar, ForceOptions>, ForceAllocator> & joint_forces) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(joint_forces.size(), size_t(model.njoints));
      PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_forces.rows(), residualSize());
      PINOCCHIO_UNUSED_VARIABLE(data);

      assert(this->type == CONTACT_3D);

      // Todo: optimize code
      const Matrix36 A1 = getA1(cdata, LocalFrameTag()), A2 = getA2(cdata, LocalFrameTag());
      joint_forces[this->joint1_id].toVector().noalias() += A1.transpose() * constraint_forces;
      joint_forces[this->joint2_id].toVector().noalias() += A2.transpose() * constraint_forces;
    }

    /// \brief Map the joint accelerations to constraint value
    template<
      int OtherOptions,
      int MotionOptions,
      template<typename, int> class JointCollectionTpl,
      typename MotionAllocator,
      typename VectorLike>
    void mapJointMotionsToConstraintMotionImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const RigidConstraintDataTpl<Scalar, Options> & cdata,
      const std::vector<MotionTpl<Scalar, MotionOptions>, MotionAllocator> & joint_accelerations,
      const Eigen::MatrixBase<VectorLike> & constraint_value) const
    {
      PINOCCHIO_CHECK_ARGUMENT_SIZE(joint_accelerations.size(), size_t(model.njoints));
      PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_value.rows(), residualSize());
      PINOCCHIO_UNUSED_VARIABLE(data);

      assert(this->type == CONTACT_3D);

      // Todo: optimize code

      if (this->joint1_id != 0 && this->joint2_id != 0)
      {
        const Matrix36 A1 = getA1(cdata, LocalFrameTag()), A2 = getA2(cdata, LocalFrameTag());
        constraint_value.const_cast_derived().noalias() =
          A1 * joint_accelerations[this->joint1_id].toVector()
          + A2 * joint_accelerations[this->joint2_id].toVector();
      }
      else if (this->joint1_id != 0)
      {
        const Matrix36 A1 = getA1(cdata, LocalFrameTag());
        constraint_value.const_cast_derived().noalias() =
          A1 * joint_accelerations[this->joint1_id].toVector();
      }
      else if (this->joint2_id != 0)
      {
        const Matrix36 A2 = getA2(cdata, LocalFrameTag());
        constraint_value.const_cast_derived().noalias() =
          A2 * joint_accelerations[this->joint2_id].toVector();
      }
      else
        constraint_value.const_cast_derived().setZero();
    }

    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename Vector3Like,
      typename Matrix6Like,
      typename Matrix6LikeAllocator>
    void appendCouplingConstraintInertiasImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const RigidConstraintDataTpl<Scalar, Options> & cdata,
      const Eigen::MatrixBase<Vector3Like> & diagonal_constraint_inertia,
      std::vector<Matrix6Like, Matrix6LikeAllocator> & inertias) const
    {
      EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Vector3Like, Vector3);
      PINOCCHIO_UNUSED_VARIABLE(data);
      PINOCCHIO_UNUSED_VARIABLE(cdata);
      PINOCCHIO_CHECK_ARGUMENT_SIZE(inertias.size(), size_t(model.njoints));
      assert(
        ((joint1_id > 0 && joint2_id == 0) || (joint1_id == 0 && joint2_id > 0))
        && "The behavior is only defined for this context");

      if (this->joint1_id != 0)
      {
        const SE3 & placement = this->joint1_placement;
        inertias[this->joint1_id] +=
          computeConstraintSpatialInertia(placement, diagonal_constraint_inertia);
      }

      if (this->joint2_id != 0)
      {
        const SE3 & placement = this->joint2_placement;
        inertias[this->joint2_id] +=
          computeConstraintSpatialInertia(placement, diagonal_constraint_inertia);
      }
    }

    /// \brief Returns the constraint projector associated with joint 1.
    /// This matrix transforms a spatial velocity expressed at the origin to the first component of
    /// the constraint associated with joint 1.
    template<ReferenceFrame rf>
    Matrix36
    getA1Impl(const RigidConstraintDataTpl<Scalar, Options> & cdata, ReferenceFrameTag<rf>) const
    {
      Matrix36 res;
      typedef typename SE3::Vector3 Vector3;

      if constexpr (std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value)
      {
#define INTERNAL_LOOP(axis_id, v3_in, res)                                                         \
  CartesianAxis<axis_id>::cross(v3_in, v_tmp);                                                     \
  res.col(axis_id).noalias() = oM1.rotation().transpose() * v_tmp;

        const SE3 & oM1 = cdata.oMc1;
        Vector3 v_tmp;
        res.template leftCols<3>() = oM1.rotation().transpose();
        INTERNAL_LOOP(0, oM1.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(1, oM1.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(2, oM1.translation(), res.template rightCols<3>());

#undef INTERNAL_LOOP
      }
      else if constexpr (std::is_same<ReferenceFrameTag<rf>, LocalFrameTag>::value)
      {
#define INTERNAL_LOOP(axis_id, v3_in, res)                                                         \
  CartesianAxis<axis_id>::cross(v3_in, v_tmp);                                                     \
  res.col(axis_id).noalias() = M1.rotation().transpose() * v_tmp;

        const SE3 & M1 = this->joint1_placement;
        Vector3 v_tmp;
        res.template leftCols<3>() = M1.rotation().transpose();
        INTERNAL_LOOP(0, M1.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(1, M1.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(2, M1.translation(), res.template rightCols<3>());

#undef INTERNAL_LOOP
      }

      return res;
    }

    /// \brief Returns the constraint projector associated with joint 2.
    /// This matrix transforms a spatial velocity expressed at the origin to the first component of
    /// the constraint associated with joint 2.
    template<ReferenceFrame rf>
    Matrix36
    getA2Impl(const RigidConstraintDataTpl<Scalar, Options> & cdata, ReferenceFrameTag<rf>) const
    {
      Matrix36 res;
      typedef typename SE3::Vector3 Vector3;

      if constexpr (std::is_same<ReferenceFrameTag<rf>, WorldFrameTag>::value)
      {
#define INTERNAL_LOOP(axis_id, v3_in, res)                                                         \
  CartesianAxis<axis_id>::cross(v3_in, v_tmp);                                                     \
  res.col(axis_id).noalias() = oM1.rotation().transpose() * v_tmp;

        const SE3 & oM1 = cdata.oMc1;
        const SE3 & oM2 = cdata.oMc2;
        res.template leftCols<3>() = -oM1.rotation().transpose();
        Vector3 v_tmp;
        INTERNAL_LOOP(0, -oM2.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(1, -oM2.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(2, -oM2.translation(), res.template rightCols<3>());

#undef INTERNAL_LOOP
      }
      else if constexpr (std::is_same<ReferenceFrameTag<rf>, LocalFrameTag>::value)
      {
        const SE3 & j2Mc2 = this->joint2_placement;
        const SE3 & c1Mc2 = cdata.c1Mc2;
        const typename SE3::Matrix3 c1Rj2 = c1Mc2.rotation() * j2Mc2.rotation().transpose();
        res.template leftCols<3>() = -c1Rj2;
        Vector3 v_tmp;
#define INTERNAL_LOOP(axis_id, v3_in, res)                                                         \
  CartesianAxis<axis_id>::cross(v3_in, v_tmp);                                                     \
  res.col(axis_id).noalias() = -c1Rj2 * v_tmp;

        INTERNAL_LOOP(0, j2Mc2.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(1, j2Mc2.translation(), res.template rightCols<3>());
        INTERNAL_LOOP(2, j2Mc2.translation(), res.template rightCols<3>());

#undef INTERNAL_LOOP
      }

      return res;
    }

  protected:
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    void init(const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model)
    {
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        reference_frame == LOCAL || reference_frame == LOCAL_WORLD_ALIGNED,
        "reference_frame should be LOCAL or LOCAL_WORLD_ALIGNED");

      auto & colwise_joint1_sparsity = model.sparsity_pattern_vector[joint1_id];
      auto & colwise_joint2_sparsity = model.sparsity_pattern_vector[joint2_id];

      loop_span_indexes.reserve((size_t)model.nv);
      for (Eigen::Index col_id = 0; col_id < model.nv; ++col_id)
      {
        if (colwise_joint1_sparsity[col_id] != colwise_joint2_sparsity[col_id])
        {
          loop_span_indexes.push_back(col_id);
        }
      }
    }
  };

  ///
  /// \brief Computes the sum of the sizes of the constraints contained in the input
  /// `constraint_models` vector in the state given by `constraint_datas` vector.
  template<
    class ConstraintModel,
    class ConstraintModelAllocator,
    ConstraintSelectionType Sel = ConstraintSelectionType::CURRENT>
  Eigen::Index getTotalConstraintResidualSize(
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    ConstraintSelectionTag<Sel> sel = CurrentSelection())
  {
    Eigen::Index total_size = 0;
    for (size_t k = 0; k < constraint_models.size(); ++k)
    {
      const auto & constraint_model = internal::helper::get_ref(constraint_models[k]);
      total_size += constraint_model.residualSize(sel);
    }

    return total_size;
  }

  ///
  ///  \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct RigidConstraintDataTpl : ConstraintDataBase<RigidConstraintDataTpl<_Scalar, _Options>>
  {

    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    typedef RigidConstraintModelTpl<Scalar, Options> ContactModel;
    typedef RigidConstraintDataTpl ContactData;

    typedef SE3Tpl<Scalar, Options> SE3;
    typedef MotionTpl<Scalar, Options> Motion;
    typedef ForceTpl<Scalar, Options> Force;
    typedef Eigen::Matrix<Scalar, 6, 6, Options> Matrix6;
    typedef std::vector<Matrix6> VectorOfMatrix6;
    typedef Eigen::Matrix<Scalar, 6, Eigen::Dynamic, Options> Matrix6x;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixX;

    // data

    /// \brief Resulting contact forces
    Force contact_force;

    /// \brief Placement of the constraint frame 1 with respect to the WORLD frame
    SE3 oMc1;

    /// \brief Placement of the constraint frame 2 with respect to the WORLD frame
    SE3 oMc2;

    /// \brief Relative displacement between the two frames
    SE3 c1Mc2;

    /// \brief Current contact placement error
    Motion contact_placement_error;

    /// \brief Current contact spatial velocity of the constraint 1
    Motion contact1_velocity;

    /// \brief Current contact spatial velocity of the constraint 2
    Motion contact2_velocity;

    /// \brief Current contact velocity error
    Motion contact_velocity_error;

    /// \brief Current contact spatial acceleration
    Motion contact_acceleration;

    /// \brief Contact spatial acceleration desired
    Motion contact_acceleration_desired;

    /// \brief Current contact spatial error (due to the integration step).
    Motion contact_acceleration_error;

    /// \brief Current contact drift acceleration (acceleration only due to the Coriolis and
    /// centrifugal effects) for the constraint frame 1.
    Motion contact1_acceleration_drift;

    /// \brief Current contact drift acceleration (acceleration only due to the Coriolis and
    /// centrifugal effects) for the constraint frame 2.
    Motion contact2_acceleration_drift;

    /// \brief Contact deviation from the reference acceleration (a.k.a the error)
    Motion contact_acceleration_deviation;

    VectorOfMatrix6 extended_motion_propagators_joint1;
    VectorOfMatrix6 lambdas_joint1;
    VectorOfMatrix6 extended_motion_propagators_joint2;

    Matrix6x dv1_dq, da1_dq, da1_dv, da1_da;
    Matrix6x dv2_dq, da2_dq, da2_dv, da2_da;
    MatrixX dvc_dq, dac_dq, dac_dv, dac_da;

    /// \brief Default constructor
    RigidConstraintDataTpl()
    : contact_force(Force::Zero())
    , oMc1(SE3::Identity())
    , oMc2(SE3::Identity())
    , c1Mc2(SE3::Identity())
    , contact_placement_error(Motion::Zero())
    , contact1_velocity(Motion::Zero())
    , contact2_velocity(Motion::Zero())
    , contact_velocity_error(Motion::Zero())
    , contact_acceleration(Motion::Zero())
    , contact_acceleration_desired(Motion::Zero())
    , contact_acceleration_error(Motion::Zero())
    , contact1_acceleration_drift(Motion::Zero())
    , contact2_acceleration_drift(Motion::Zero())
    , contact_acceleration_deviation(Motion::Zero())
    , extended_motion_propagators_joint1()
    , lambdas_joint1()
    , extended_motion_propagators_joint2()
    , dv1_dq(6, 0)
    , da1_dq(6, 0)
    , da1_dv(6, 0)
    , da1_da(6, 0)
    , dv2_dq(6, 0)
    , da2_dq(6, 0)
    , da2_dv(6, 0)
    , da2_da(6, 0)
    , dvc_dq()
    , dac_dq()
    , dac_dv()
    , dac_da()
    {
    }

    explicit RigidConstraintDataTpl(const ContactModel & constraint_model)
    : contact_force(Force::Zero())
    , oMc1(SE3::Identity())
    , oMc2(SE3::Identity())
    , c1Mc2(SE3::Identity())
    , contact_placement_error(Motion::Zero())
    , contact1_velocity(Motion::Zero())
    , contact2_velocity(Motion::Zero())
    , contact_velocity_error(Motion::Zero())
    , contact_acceleration(Motion::Zero())
    , contact_acceleration_desired(Motion::Zero())
    , contact_acceleration_error(Motion::Zero())
    , contact1_acceleration_drift(Motion::Zero())
    , contact2_acceleration_drift(Motion::Zero())
    , contact_acceleration_deviation(Motion::Zero())
    , extended_motion_propagators_joint1(constraint_model.depth_joint1, Matrix6::Zero())
    , lambdas_joint1(constraint_model.depth_joint1, Matrix6::Zero())
    , extended_motion_propagators_joint2(constraint_model.depth_joint2, Matrix6::Zero())
    , dv1_dq(Matrix6x::Zero(6, constraint_model.nv))
    , da1_dq(Matrix6x::Zero(6, constraint_model.nv))
    , da1_dv(Matrix6x::Zero(6, constraint_model.nv))
    , da1_da(Matrix6x::Zero(6, constraint_model.nv))
    , dv2_dq(Matrix6x::Zero(6, constraint_model.nv))
    , da2_dq(Matrix6x::Zero(6, constraint_model.nv))
    , da2_dv(Matrix6x::Zero(6, constraint_model.nv))
    , da2_da(Matrix6x::Zero(6, constraint_model.nv))
    , dvc_dq(MatrixX::Zero(constraint_model.residualSize(), constraint_model.nv))
    , dac_dq(MatrixX::Zero(constraint_model.residualSize(), constraint_model.nv))
    , dac_dv(MatrixX::Zero(constraint_model.residualSize(), constraint_model.nv))
    , dac_da(MatrixX::Zero(constraint_model.residualSize(), constraint_model.nv))
    {
    }

    bool operator==(const RigidConstraintDataTpl & other) const
    {
      return contact_force == other.contact_force && oMc1 == other.oMc1 && oMc2 == other.oMc2
             && c1Mc2 == other.c1Mc2 && contact_placement_error == other.contact_placement_error
             && contact1_velocity == other.contact1_velocity
             && contact2_velocity == other.contact2_velocity
             && contact_velocity_error == other.contact_velocity_error
             && contact_acceleration == other.contact_acceleration
             && contact_acceleration_desired == other.contact_acceleration_desired
             && contact_acceleration_error == other.contact_acceleration_error
             && contact1_acceleration_drift == other.contact1_acceleration_drift
             && contact2_acceleration_drift == other.contact2_acceleration_drift
             && contact_acceleration_deviation == other.contact_acceleration_deviation
             && extended_motion_propagators_joint1 == other.extended_motion_propagators_joint1
             && lambdas_joint1 == other.lambdas_joint1
             && extended_motion_propagators_joint2 == other.extended_motion_propagators_joint2
             //
             && dv1_dq == other.dv1_dq && da1_dq == other.da1_dq && da1_dv == other.da1_dv
             && da1_da == other.da1_da
             //
             && dv2_dq == other.dv2_dq && da2_dq == other.da2_dq && da2_dv == other.da2_dv
             && da2_da == other.da2_da
             //
             && dvc_dq == other.dvc_dq && dac_dq == other.dac_dq && dac_dv == other.dac_dv
             && dac_da == other.dac_da;
    }

    bool operator!=(const RigidConstraintDataTpl & other) const
    {
      return !(*this == other);
    }
  };

} // namespace pinocchio
