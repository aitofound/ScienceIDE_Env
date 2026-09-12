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
  struct CastType<NewScalar, FrameAnchorConstraintModelTpl<Scalar, Options>>
  {
    typedef FrameAnchorConstraintModelTpl<NewScalar, Options> type;
  };

  // --------------------------------------------------------------
  // Traits
  // --------------------------------------------------------------
  template<typename _Scalar, int _Options>
  struct traits<FrameAnchorConstraintModelTpl<_Scalar, _Options>>
  : traits<FrameConstraintModelBase<FrameAnchorConstraintModelTpl<_Scalar, _Options>>>
  {
    // --------------------------------------------------------------
    // Traits referencing the constraint and associated types
    // --------------------------------------------------------------
    typedef FrameAnchorConstraintModelTpl<_Scalar, _Options> ConstraintModel;
    typedef FrameAnchorConstraintDataTpl<_Scalar, _Options> ConstraintData;

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
    static constexpr bool is_inequality_constraint = false;

    // --------------------------------------------------------------
    // Traits for associated struct and sizes
    // --------------------------------------------------------------
    typedef FullSpaceConeTpl<Scalar, Options> ConstraintSet;
    typedef ZeroConeJordanOperationTpl<Scalar, Options> JordanOperation;
    typedef BaumgarteCorrectorParametersTpl<Scalar> BaumgarteCorrectorParameters;

    static constexpr int Size = 6;
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
  struct traits<FrameAnchorConstraintDataTpl<_Scalar, _Options>>
  : traits<FrameAnchorConstraintModelTpl<_Scalar, _Options>>
  {
  };

  ///
  /// \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct FrameAnchorConstraintModelTpl
  : FrameConstraintModelBase<FrameAnchorConstraintModelTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef FrameAnchorConstraintModelTpl Self;
    typedef FrameConstraintModelBase<Self> Base;
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
    friend struct FrameAnchorConstraintModelTpl;

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
    FrameAnchorConstraintModelTpl()
    : Base()
    {
    }

    ///
    /// \brief Constructor from model only.
    ///
    /// \param[in] model Model associated to the constraint.
    ///
    /// \remarks The second joint id (joint2_id) is set to be 0 (corresponding to the index of the
    /// universe).
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameAnchorConstraintModelTpl(const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model)
    : Base(model)
    {
    }

    ///
    /// \brief Constructor from joint indexes and placements.
    ///
    /// \param[in] model Model associated to the constraint.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// \param[in] joint2_placement Placement of the constraint w.r.t the frame of joint2.
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameAnchorConstraintModelTpl(
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
    /// \param[in] model Model associated to the constraint.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint1_placement Placement of the constraint w.r.t the frame of joint1.
    /// expressed.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameAnchorConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const SE3 & joint1_placement)
    : Base(model, joint1_id, joint1_placement)
    {
    }

    ///
    /// \brief Constructor from joint ids.
    ///
    /// \param[in] model Model associated to the constraint.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    /// \param[in] joint2_id Index of the joint 2 in the model tree.
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameAnchorConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const JointIndex joint1_id,
      const JointIndex joint2_id)
    : Base(model, joint1_id, joint2_id)
    {
    }

    ///
    /// \brief Constructor from joint1_id.
    ///
    /// \param[in] model Model associated to the constraint.
    /// \param[in] joint1_id Index of the joint 1 in the model tree.
    ///
    /// \remarks The second joint id (joint2_id) is set to be 0 (corresponding to the index of the
    /// universe).
    ///
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    FrameAnchorConstraintModelTpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model, const JointIndex joint1_id)
    : Base(model, joint1_id)
    {
    }

    // Operators ---------------------

    /// \brief Cast operator
    template<typename NewScalar>
    typename CastType<NewScalar, FrameAnchorConstraintModelTpl>::type cast() const
    {
      typedef typename CastType<NewScalar, FrameAnchorConstraintModelTpl>::type ReturnType;
      ReturnType res;
      Base::template cast<NewScalar>(res);
      return res;
    }

    ///
    /// \brief Comparison operator
    ///
    /// \param[in] other Other FrameAnchorConstraintModelTpl to compare with.
    ///
    /// \returns true if the two *this is equal to other (type, joint1_id and placement attributes
    /// must be the same).
    ///
    bool operator==(const FrameAnchorConstraintModelTpl & other) const
    {
      return base() == other.base();
    }

    ///
    /// \brief Opposite of the comparison operator.
    ///
    /// \param[in] other Other FrameAnchorConstraintModelTpl to compare with.
    ///
    /// \returns false if the two *this is not equal to other (at least type, joint1_id or placement
    /// attributes is different).
    ///
    bool operator!=(const FrameAnchorConstraintModelTpl & other) const
    {
      return !(*this == other);
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    // General -----------------------

    /// \copydoc RootBase::classname
    static std::string classnameImpl()
    {
      return std::string("FrameAnchorConstraintModel");
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

    // Methods for algorithms --------

    /// \copydoc RootBase::set
    ConstraintSet setImpl(const ConstraintData & cdata) const
    {
      PINOCCHIO_UNUSED_VARIABLE(cdata);
      return ConstraintSet();
    }

    // Other methods for algorithms are in FrameConstraintModelBase

  }; // struct FrameAnchorConstraintModelTpl

  ///
  /// \brief Contact model structure containing all the info describing the rigid contact model
  ///
  template<typename _Scalar, int _Options>
  struct FrameAnchorConstraintDataTpl
  : FrameConstraintDataBase<FrameAnchorConstraintDataTpl<_Scalar, _Options>>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef FrameAnchorConstraintDataTpl Self;
    typedef FrameConstraintDataBase<Self> Base;
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

    /// \brief Default constructor
    FrameAnchorConstraintDataTpl()
    : Base()
    {
    }

    /// \brief Constructor from constraint model
    explicit FrameAnchorConstraintDataTpl(const ConstraintModel & cmodel)
    : Base(cmodel)
    {
    }

    // Operators ---------------------

    /// \brief Comparison operator
    bool operator==(const FrameAnchorConstraintDataTpl & other) const
    {
      return base() == other.base();
    }

    /// \brief Comparison operator
    bool operator!=(const FrameAnchorConstraintDataTpl & other) const
    {
      return !(*this == other);
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    /// \copydoc RootBase::classname
    static std::string classnameImpl()
    {
      return std::string("FrameAnchorConstraintData");
    }

    /// \copydoc RootBase::shortname
    std::string shortnameImpl() const
    {
      return classname();
    }
  }; // struct FrameAnchorConstraintDataTpl

} // namespace pinocchio
