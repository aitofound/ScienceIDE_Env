//
// Copyright (c) 2023-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/constraints.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/constraints.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<typename Derived>
  struct JointWiseConstraintModelBase : ConstraintModelBase<Derived>
  {
    // --------------------------------------------------------------
    // Type defs
    // --------------------------------------------------------------
    // CRTP related types -------------------------------------------
    typedef ConstraintModelBase<Derived> Base;

    // Retrieving traits --------------------------------------------
    typedef typename traits<Derived>::ConstraintModel ConstraintModel;
    typedef typename traits<Derived>::ConstraintData ConstraintData;

    typedef typename traits<Derived>::Scalar Scalar;
    static constexpr int Options = traits<Derived>::Options;

    // Base usage ---------------------------------------------------
    using Base::derived;

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

  protected:
    /// \brief Default constructor
    /// Protected so can't be used
    JointWiseConstraintModelBase()
    {
    }

    /// \brief Default constructor from model
    template<int OtherOptions, template<typename, int> class JointCollectionTpl>
    JointWiseConstraintModelBase(const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model)
    : Base(model)
    {
    }

    // Operators ---------------------

  public:
    /// \brief Comparison operator
    template<typename OtherDerived>
    bool operator==(const JointWiseConstraintModelBase<OtherDerived> & other) const
    {
      return base() == other.base();
    }

    /// \brief Comparison operator
    template<typename OtherDerived>
    bool operator!=(const JointWiseConstraintModelBase<OtherDerived> & other) const
    {
      return !(*this == other);
    }

    // Rigid Body Methods ------------

    /// \brief Map the constraint forces (aka constraint Lagrange multipliers) to the joint torques
    /// associated to each independant constraint. This operation corresponds to the mapping of the
    /// constraint multipliers on the joint torque.
    ///
    /// \param[in] model The model of the rigid body system.
    /// \param[in] data The data associated with model.
    /// \param[in] cdata The constraint data associated with the constraint model.
    /// \param[in] constraint_forces Input constraint forces (Lagrange multipliers) associated with
    /// the constraint.
    /// \param[out] joint_torques Output joint torques associated with the model.
    ///
    /// \note The results will be added to the joint_torques ouput argument.
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename ConstraintForcesLike,
      typename JointTorquesLike>
    void mapConstraintForceToJointTorques(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<ConstraintForcesLike> & constraint_forces,
      const Eigen::MatrixBase<JointTorquesLike> & joint_torques) const
    {
      derived().mapConstraintForceToJointTorquesImpl(
        model, data, cdata, constraint_forces, joint_torques.const_cast_derived());
    }

    /// \brief Map the joint motions to the constraint motions.
    /// This operation corresponds to the dual mapping wrt mapConstraintForcesToJointTorques.
    ///
    /// \param[in] model The model of the rigid body system.
    /// \param[in] data The data associated with model.
    /// \param[in] cdata The constraint data associated with the constraint model.
    /// \param[in] joint_generalized_velocity Input joint motions associated with the model.
    /// \param[out] constraint_motions Output constraint motions.
    ///
    template<
      int OtherOptions,
      template<typename, int> class JointCollectionTpl,
      typename JointMotionsLike,
      typename ConstraintMotionsLike>
    void mapJointMotionsToConstraintMotion(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<JointMotionsLike> & joint_generalized_velocity,
      const Eigen::MatrixBase<ConstraintMotionsLike> & constraint_motions) const
    {
      derived().mapJointMotionsToConstraintMotionImpl(
        model, data, cdata, joint_generalized_velocity, constraint_motions.const_cast_derived());
    }

    // -------------------------------
    // IMPLEMENTATIONS OF BASE METHODS
    // -------------------------------

    // Methods for algorithms --------

    /// \copydoc Base::mapConstraintForceToJointSpace
    template<
      int OtherOptions,
      int ForceOptions,
      template<typename, int> class JointCollectionTpl,
      typename ConstraintForceLike,
      typename ForceAllocator,
      typename JointTorquesLike,
      ReferenceFrame rf>
    void mapConstraintForceToJointSpaceImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const Eigen::MatrixBase<ConstraintForceLike> & constraint_forces,
      std::vector<ForceTpl<Scalar, ForceOptions>, ForceAllocator> & joint_forces,
      const Eigen::MatrixBase<JointTorquesLike> & joint_torques,
      ReferenceFrameTag<rf> reference_frame) const
    {
      PINOCCHIO_UNUSED_VARIABLE(joint_forces);
      PINOCCHIO_UNUSED_VARIABLE(reference_frame);
      mapConstraintForceToJointTorques(model, data, cdata, constraint_forces, joint_torques);
    }

    /// \copydoc Base::mapJointSpaceToConstraintMotion
    template<
      int OtherOptions,
      int MotionOptions,
      template<typename, int> class JointCollectionTpl,
      typename MotionAllocator,
      typename JointMotionsLike,
      typename VectorLike,
      ReferenceFrame rf>
    void mapJointSpaceToConstraintMotionImpl(
      const ModelTpl<Scalar, OtherOptions, JointCollectionTpl> & model,
      const DataTpl<Scalar, OtherOptions, JointCollectionTpl> & data,
      const ConstraintData & cdata,
      const std::vector<MotionTpl<Scalar, MotionOptions>, MotionAllocator> & joint_motions,
      const Eigen::MatrixBase<JointMotionsLike> & joint_generalized_velocity,
      const Eigen::MatrixBase<VectorLike> & constraint_motions,
      ReferenceFrameTag<rf> reference_frame) const
    {
      PINOCCHIO_UNUSED_VARIABLE(joint_motions);
      PINOCCHIO_UNUSED_VARIABLE(reference_frame);
      mapJointMotionsToConstraintMotion(
        model, data, cdata, joint_generalized_velocity, constraint_motions.const_cast_derived());
    }

  }; // struct JointWiseConstraintModelBase

} // namespace pinocchio
