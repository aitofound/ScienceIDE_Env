//
// Copyright (c) 2019-2024 INRIA CNRS
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
  // Cast
  // --------------------------------------------------------------
  template<typename NewScalar, typename Scalar, int Options>
  struct CastType<NewScalar, PointContactConstraintModelTpl<Scalar, Options>>
  {
    typedef PointContactConstraintModelTpl<NewScalar, Options> type;
  };

  // --------------------------------------------------------------
  // Traits
  // --------------------------------------------------------------
  template<typename _Scalar, int _Options>
  struct traits<PointContactConstraintModelTpl<_Scalar, _Options>>
  : traits<PointConstraintModelBase<PointContactConstraintModelTpl<_Scalar, _Options>>>
  {
    // --------------------------------------------------------------
    // Traits referencing the constraint and associated types
    // --------------------------------------------------------------
    typedef PointContactConstraintModelTpl<_Scalar, _Options> ConstraintModel;
    typedef PointContactConstraintDataTpl<_Scalar, _Options> ConstraintData;

    typedef ConstraintModel Model;
    typedef ConstraintData Data;

    // --------------------------------------------------------------
    // Traits characterizing the constraints
    // --------------------------------------------------------------
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    // constraint_size_type = ConstraintSizeType::STATIC;

    static constexpr bool has_baumgarte_corrector = true;
    static constexpr bool has_set = true;
    static constexpr bool is_inequality_constraint = true;

    // --------------------------------------------------------------
    // Traits for associated struct and sizes
    // --------------------------------------------------------------
    typedef CoulombFrictionConeTpl<Scalar> ConstraintSet;
    typedef SecondOrderConeJordanOperationTpl<Scalar, Options> JordanOperation;
    typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;

    static constexpr int Size = 3;
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
  };

  template<typename _Scalar, int _Options>
  struct traits<PointContactConstraintDataTpl<_Scalar, _Options>>
  : traits<PointContactConstraintModelTpl<_Scalar, _Options>>
  {
  };

  ///
  /// \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct PointContactConstraintModelTpl
  : PointConstraintModelBase<PointContactConstraintModelTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef PointContactConstraintModelTpl Self;
    typedef PointConstraintModelBase<Self> Base;
    typedef ConstraintModelCommonParameters<Self> BaseCommonParameters;
    typedef ConstraintModelBase<Self> RootBase;

    // Retrieving traits --------------------------------------------
    typedef typename traits<Self>::ConstraintModel ConstraintModel;
    typedef typename traits<Self>::ConstraintData ConstraintData;

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
    friend struct PointContactConstraintModelTpl;

    // Base usage ---------------------------------------------------
    using RootBase::classname;
    using typename Base::SE3;

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

    // Constructors ------------------

    ///
    /// \brief Default constructor
    ///
    PointContactConstraintModelTpl()
    : Base()
    {
    }

    ///
    /// \brief Constructor from model only.
    ///
    /// \param[in] model Kinematic tree.
    ///
    /// \remarks The second joint id (joint2_id) is set to be 0 (corresponding to the index of the
    /// universe).
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    PointContactConstraintModelTpl(const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model)
    : Base(model)
    {
    }

    ///
    /// \brief Constructor from joint indexes and placements.
    ///
    /// \param[in] model Kinematic tree.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// \param[in] joint2_placement Placement of the constraint w.r.t the frame of joint2.
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    PointContactConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement,
      const JointIndex joint2_id,
      const SE3 & joint2_placement)
    : Base(model, joint1_id, joint1_placement, joint2_id, joint2_placement)
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
    PointContactConstraintModelTpl(
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
    PointContactConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const JointIndex joint2_id)
    : Base(model, joint1_id, joint2_id)
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
    PointContactConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model, const JointIndex joint1_id)
    : Base(model, joint1_id)
    {
    }

    // Operators ---------------------

    /// \brief Cast operator
    template<typename NewScalar>
    typename CastType<NewScalar, PointContactConstraintModelTpl>::type cast() const
    {
      typedef typename CastType<NewScalar, PointContactConstraintModelTpl>::type ReturnType;
      ReturnType res;
      Base::template cast<NewScalar>(res);
      res.m_friction = static_cast<NewScalar>(m_friction);
      res.geom1_id = geom1_id;
      res.geom2_id = geom2_id;
      return res;
    }

    ///
    /// \brief Comparison operator
    ///
    /// \param[in] other Other PointContactConstraintModelTpl to compare with.
    ///
    /// \returns true if the two *this is equal to other (type, joint1_id and placement attributes
    /// must be the same).
    ///
    bool operator==(const PointContactConstraintModelTpl & other) const
    {
      return base() == other.base() && m_friction == other.m_friction && geom1_id == other.geom1_id
             && geom2_id == other.geom2_id;
    }

    ///
    /// \brief Opposite of the comparison operator.
    ///
    /// \param[in] other Other PointContactConstraintModelTpl to compare with.
    ///
    /// \returns false if the two *this is not equal to other (at least type, joint1_id or placement
    /// attributes is different).
    ///
    bool operator!=(const PointContactConstraintModelTpl & other) const
    {
      return !(*this == other);
    }

    /// Specialized accessors --------

    /// \brief Get the friction coefficient of this contact constraint.
    Scalar getFriction() const
    {
      return m_friction;
    }

    /// \brief Set the friction coefficient of this contact constraint.
    void setFriction(Scalar friction)
    {
      PINOCCHIO_THROW_IF(
        check_expression_if_real<Scalar>(friction < 0), std::runtime_error,
        "friction must be >= 0 for contact constraints.");
      m_friction = friction;
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    // General -----------------------

    /// \copydoc RootBase::classname
    static std::string classnameImpl()
    {
      return std::string("PointContactConstraintModel");
    }

    /// \copydoc RootBase::shortname
    std::string shortnameImpl() const
    {
      return classname();
    }

    /// \copydoc RootBase::createData
    ConstraintData createDataImpl() const
    {
      return ConstraintData(*this);
    }

    // Methods for algorithms -------------

    /// \copydoc RootBase::set
    ConstraintSet setImpl(const ConstraintData & cdata) const
    {
      PINOCCHIO_UNUSED_VARIABLE(cdata);
      return ConstraintSet(m_friction);
    }

    // ------------------------------
    // MEMBERS
    // ------------------------------
    /// \brief Index of first geometry to which the point contact constraint is attached.
    GeomIndex geom1_id = 0;

    /// \brief Index of second geometry to which the point contact constraint is attached.
    GeomIndex geom2_id = 0;

  protected:
    Scalar m_friction = Scalar(0.5);

  }; // struct PointContactConstraintModelTpl<_Scalar,_Options>

  ///
  /// \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct PointContactConstraintDataTpl
  : PointConstraintDataBase<PointContactConstraintDataTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef PointContactConstraintDataTpl Self;
    typedef PointConstraintDataBase<Self> Base;
    typedef ConstraintDataBase<Self> RootBase;

    // Retrieving traits --------------------------------------------
    typedef typename traits<Self>::ConstraintModel ConstraintModel;
    typedef typename traits<Self>::ConstraintData ConstraintData;

    typedef typename traits<Self>::Scalar Scalar;
    static constexpr int Options = traits<Self>::Options;

    // Base usage ---------------------------------------------------
    using Base::classname;
    using typename Base::SE3;

    // -------------------------------
    // METHODS SPECIFIC TO CLASS
    // -------------------------------

    // CRTP related ------------------

    /// \brief Cast to base class
    Base & base()
    {
      return static_cast<Base &>(*this);
    }

    /// \brief Const cast to base class
    const Base & base() const
    {
      return static_cast<const Base &>(*this);
    }

    // Constructors ------------------

    /// \brief Default constructor
    PointContactConstraintDataTpl()
    : Base()
    {
    }

    /// \brief Constructor from a constraint model
    explicit PointContactConstraintDataTpl(const ConstraintModel & cmodel)
    : Base(cmodel)
    {
    }

    // Operators ---------------------

    /// \brief Comparison operator
    bool operator==(const PointContactConstraintDataTpl & other) const
    {
      return base() == other.base();
    }

    /// \brief Comparison operator
    bool operator!=(const PointContactConstraintDataTpl & other) const
    {
      return !(*this == other);
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc Base::classname
    static std::string classnameImpl()
    {
      return std::string("PointContactConstraintData");
    }

    /// \copydoc Base::shortname
    std::string shortnameImpl() const
    {
      return classname();
    }
  }; // struct PointContactConstraintDataTpl

} // namespace pinocchio
