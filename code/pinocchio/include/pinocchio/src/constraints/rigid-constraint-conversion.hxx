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

  ///
  /// \brief Converts a PointAnchorConstraintModelTpl to a RigidConstraintModelTpl with contact
  ///        type CONTACT_3D.
  ///
  /// The kinematic structure (joint IDs and placements) is preserved. The desired fields are
  /// mapped as follows: the 3D desired_constraint_offset is placed in the translation part of
  /// desired_contact_placement (rotation is kept at identity), and the 3D desired velocity and
  /// acceleration vectors are placed in the linear parts of the corresponding Motion fields
  /// (angular parts are set to zero).
  ///
  /// \param[in] model           The kinematic model, required to initialise the sparsity
  ///                            pattern of the returned RigidConstraintModelTpl.
  /// \param[in] constraint      The PointAnchorConstraintModelTpl to convert.
  /// \param[in] reference_frame Reference frame in which constraint quantities are expressed
  ///                            (LOCAL or LOCAL_WORLD_ALIGNED). Defaults to LOCAL.
  ///
  /// \returns A RigidConstraintModelTpl of contact type CONTACT_3D equivalent to \p constraint.
  ///
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  RigidConstraintModelTpl<Scalar, Options> convertToRigidConstraintModel(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const PointAnchorConstraintModelTpl<Scalar, Options> & constraint,
    const ReferenceFrame reference_frame = LOCAL)
  {
    typedef RigidConstraintModelTpl<Scalar, Options> RigidConstraintModel;

    RigidConstraintModel res(
      CONTACT_3D, model, constraint.joint1_id, constraint.joint1_placement, constraint.joint2_id,
      constraint.joint2_placement, reference_frame);

    // Map the 3D desired offset to the translation part of the SE3 desired placement.
    // The rotation is already Identity from the constructor.
    res.desired_contact_placement.translation() = constraint.desired_constraint_offset;

    // Map the 3D desired velocity/acceleration to the linear parts of the spatial Motion.
    // The angular parts are already zero from the constructor.
    res.desired_contact_velocity.linear() = constraint.desired_constraint_velocity;
    res.desired_contact_acceleration.linear() = constraint.desired_constraint_acceleration;

    return res;
  }

  ///
  /// \brief Converts a FrameAnchorConstraintModelTpl to a RigidConstraintModelTpl with contact
  ///        type CONTACT_6D.
  ///
  /// The kinematic structure (joint IDs and placements) is preserved. The desired fields are
  /// mapped as follows: the 6D desired_constraint_velocity and desired_constraint_acceleration
  /// vectors (ordered [linear; angular]) are directly used to construct the corresponding Motion
  /// fields. The 6D desired_constraint_offset is mapped to desired_contact_placement via the
  /// spatial exponential exp6, treating the offset as the coordinate vector of a spatial motion
  /// (ordered [linear; angular]).
  ///
  /// \param[in] model           The kinematic model, required to initialise the sparsity
  ///                            pattern of the returned RigidConstraintModelTpl.
  /// \param[in] constraint      The FrameAnchorConstraintModelTpl to convert.
  /// \param[in] reference_frame Reference frame in which constraint quantities are expressed
  ///                            (LOCAL or LOCAL_WORLD_ALIGNED). Defaults to LOCAL.
  ///
  /// \returns A RigidConstraintModelTpl of contact type CONTACT_6D equivalent to \p constraint.
  ///
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  RigidConstraintModelTpl<Scalar, Options> convertToRigidConstraintModel(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    const FrameAnchorConstraintModelTpl<Scalar, Options> & constraint,
    const ReferenceFrame reference_frame = LOCAL)
  {
    typedef RigidConstraintModelTpl<Scalar, Options> RigidConstraintModel;
    typedef typename RigidConstraintModel::Motion Motion;

    RigidConstraintModel res(
      CONTACT_6D, model, constraint.joint1_id, constraint.joint1_placement, constraint.joint2_id,
      constraint.joint2_placement, reference_frame);

    // Map the 6D desired offset (interpreted as a spatial motion vector [linear; angular])
    // to an SE3 via the spatial exponential.
    res.desired_contact_placement = exp6(Motion(constraint.desired_constraint_offset));

    // Map the 6D desired velocity/acceleration vectors directly to Motion objects.
    res.desired_contact_velocity = Motion(constraint.desired_constraint_velocity);
    res.desired_contact_acceleration = Motion(constraint.desired_constraint_acceleration);

    return res;
  }

} // namespace pinocchio
