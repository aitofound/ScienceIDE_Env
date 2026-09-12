//
// Copyright (c) 2019-2022 INRIA
//

#pragma once

#include <eigenpy/memory.hpp>

#include "pinocchio/constraints.hpp"

#include "pinocchio/bindings/python/utils/cast.hpp"
#include "pinocchio/bindings/python/utils/macros.hpp"
#include "pinocchio/bindings/python/utils/comparable.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/baumgarte-corrector-parameters.hpp"
#include "pinocchio/bindings/python/utils/eigen.hpp"

#include "pinocchio/bindings/python/utils/model-checker.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    template<typename RigidConstraintModel>
    struct RigidConstraintModelPythonVisitor
    : public boost::python::def_visitor<RigidConstraintModelPythonVisitor<RigidConstraintModel>>
    {
      typedef typename RigidConstraintModel::Scalar Scalar;
      typedef typename RigidConstraintModel::SE3 SE3;
      typedef RigidConstraintModel Self;
      typedef typename RigidConstraintModel::ContactData ContactData;
      typedef
        typename RigidConstraintModel::BaumgarteCorrectorParameters BaumgarteCorrectorParameters;

      typedef ModelTpl<Scalar, RigidConstraintModel::Options, JointCollectionDefaultTpl> Model;
      typedef DataTpl<Scalar, RigidConstraintModel::Options, JointCollectionDefaultTpl> Data;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl
          //        .def(bp::init<>(bp::arg("self"),
          //                        "Default constructor."))
          .def(
            bp::init<
              ContactType, const Model &, JointIndex, const SE3 &, JointIndex, const SE3 &,
              bp::optional<ReferenceFrame>>(
              (bp::arg("self"), bp::arg("contact_type"), bp::arg("model"), bp::arg("joint1_id"),
               bp::arg("joint1_placement"), bp::arg("joint2_id"), bp::arg("joint2_placement"),
               bp::arg("reference_frame")),
              "Constructor from a given ContactType, joint index and placement for the two joints "
              "implied in the constraint."))
          .def(
            bp::init<
              ContactType, const Model &, JointIndex, const SE3 &, bp::optional<ReferenceFrame>>(
              (bp::arg("self"), bp::arg("contact_type"), bp::arg("model"), bp::arg("joint1_id"),
               bp::arg("joint1_placement"), bp::arg("reference_frame")),
              "Constructor from a given ContactType, joint index and placement only for the first "
              "joint implied in the constraint."))
          .def(
            bp::init<ContactType, const Model &, JointIndex, bp::optional<ReferenceFrame>>(
              (bp::arg("self"), bp::arg("contact_type"), bp::arg("model"), bp::arg("joint1_id"),
               bp::arg("reference_frame")),
              "Constructor from a given ContactType and joint index. The base joint is taken as 0 "
              "in "
              "the constraint."))
          .PINOCCHIO_ADD_PROPERTY(Self, name, "Name of the contact.")
          .PINOCCHIO_ADD_PROPERTY(Self, type, "Type of the contact.")
          .PINOCCHIO_ADD_PROPERTY(Self, joint1_id, "Index of first parent joint in the model tree.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, joint2_id, "Index of second parent joint in the model tree.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, joint1_placement, "Relative placement with respect to the frame of joint1.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, joint2_placement, "Relative placement with respect to the frame of joint2.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, reference_frame,
            "Reference frame where the constraint is expressed (WORLD, "
            "LOCAL_WORLD_ALIGNED or LOCAL).")
          .PINOCCHIO_ADD_PROPERTY(Self, desired_contact_placement, "Desired contact placement.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, desired_contact_velocity, "Desired contact spatial velocity.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, desired_contact_acceleration, "Desired contact spatial acceleration.")
          .def(
            "createData", &RigidConstraintModelPythonVisitor::createData,
            "Create a Data object for the given model.")
          .def(ComparableVisitor<Self, pinocchio::is_floating_point<Scalar>::value>())
          .def(
            "calc", (void (Self::*)(const Model &, const Data &, ContactData &) const) & Self::calc,
            bp::args("self", "model", "data", "constraint_data"))
          .def("jacobian", &jacobian, bp::args("self", "model", "data", "constraint_data"))
          .def(
            "residualSize", &residualSize,
            (bp::arg("self"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Constraint size for the selection.")
          .def(
            "setCompliance", &setCompliance,
            (bp::arg("self"), bp::arg("vec"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Set the compliance value for the selected constraint.")
          .def(
            "retrieveCompliance", &retrieveCompliance,
            (bp::arg("self"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Retrieve the compliance value for the selected constraint.")
          .def(
            "setBaumgarteCorrectorParameters", &setBaumgarteCorrectorParameters,
            (bp::arg("self"), bp::arg("bp"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Set the Baumgarte parameters.");
        cl.add_property(
          "m_baumgarte_parameters",
          bp::make_function( //
            +[](Self & self) -> BaumgarteCorrectorParameters & {
              return self.baumgarte_corrector_parameters();
            },
            bp::return_internal_reference<>()),
          bp::make_function( //
            +[](Self & self, const BaumgarteCorrectorParameters & copy) {
              self.baumgarte_corrector_parameters() = copy;
            },
            bp::return_internal_reference<>()),
          "Baumgarte parameters associated with the constraint.");
      }

      static context::RigidConstraintModel convertToRigidConstraintModel_point(
        const context::Model & model,
        const context::PointAnchorConstraintModel & constraint,
        const ReferenceFrame reference_frame)
      {
        return convertToRigidConstraintModel(model, constraint, reference_frame);
      }

      static context::RigidConstraintModel convertToRigidConstraintModel_frame(
        const context::Model & model,
        const context::FrameAnchorConstraintModel & constraint,
        const ReferenceFrame reference_frame)
      {
        return convertToRigidConstraintModel(model, constraint, reference_frame);
      }

      static void expose()
      {
        bp::class_<RigidConstraintModel>(
          "RigidConstraintModel", "Rigid contact model for contact dynamic algorithms.",
          bp::no_init)
          .def(RigidConstraintModelPythonVisitor())
          .def(CastVisitor<RigidConstraintModel>())
          .def(
            ExposeConstructorByCastVisitor<
              RigidConstraintModel, ::pinocchio::RigidConstraintModel>());

        bp::def(
          "convertToRigidConstraintModel", convertToRigidConstraintModel_point,
          (bp::arg("model"), bp::arg("constraint"),
           bp::arg("reference_frame") = ReferenceFrame::LOCAL),
          "Convert a PointAnchorConstraintModel to a RigidConstraintModel with contact type "
          "CONTACT_3D.\n\n"
          "The kinematic structure (joint IDs and placements) is preserved. The 3D desired "
          "offset is placed in the translation of desired_contact_placement, and the 3D desired "
          "velocity/acceleration are placed in the linear parts of the corresponding Motion "
          "fields.",
          mimic_not_supported_function<>(0));

        bp::def(
          "convertToRigidConstraintModel", convertToRigidConstraintModel_frame,
          (bp::arg("model"), bp::arg("constraint"),
           bp::arg("reference_frame") = ReferenceFrame::LOCAL),
          "Convert a FrameAnchorConstraintModel to a RigidConstraintModel with contact type "
          "CONTACT_6D.\n\n"
          "The kinematic structure (joint IDs and placements) is preserved. The 6D desired "
          "offset (ordered [linear; angular]) is mapped to desired_contact_placement via exp6, "
          "and the 6D desired velocity/acceleration vectors are mapped directly to Motion fields.",
          mimic_not_supported_function<>(0));
      }

      static ContactData createData(const Self & self)
      {
        return ContactData(self);
      }

      static context::MatrixXs jacobian(
        const Self & self, const Model & model, const Data & data, ContactData & constraint_data)
      {
        context::MatrixXs res(self.residualSize(), model.nv);
        self.jacobian(model, data, constraint_data, res);
        return res;
      }

      static int residualSize(const Self & self, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.residualSize(CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.residualSize(MaximalSelection());
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      static void
      setCompliance(Self & self, context::VectorXs & vector, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.setCompliance(vector, CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.setCompliance(vector, MaximalSelection());
        }
      }

      static context::VectorXs retrieveCompliance(const Self & self, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT: {
          context::VectorXs res(self.residualSize(CurrentSelection()));
          self.retrieveCompliance(res, CurrentSelection());
          return res;
        }
        case ConstraintSelectionType::MAXIMAL: {
          context::VectorXs res(self.residualSize(MaximalSelection()));
          self.retrieveCompliance(res, MaximalSelection());
          return res;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      static void setBaumgarteCorrectorParameters(
        Self & self, const BaumgarteCorrectorParameters & bp, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.setBaumgarteCorrectorParameters(bp, CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.setBaumgarteCorrectorParameters(bp, MaximalSelection());
        }
      }
    };

    template<typename RigidConstraintData>
    struct RigidConstraintDataPythonVisitor
    : public boost::python::def_visitor<RigidConstraintDataPythonVisitor<RigidConstraintData>>
    {
      typedef typename RigidConstraintData::Scalar Scalar;
      typedef RigidConstraintData Self;
      typedef typename RigidConstraintData::ContactModel ContactModel;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(
            bp::init<const ContactModel &>(
              bp::args("self", "contact_model"), "Default constructor."))

          .PINOCCHIO_ADD_PROPERTY(Self, contact_force, "Constraint force.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, oMc1, "Placement of the constraint frame 1 with respect to the WORLD frame.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, oMc2, "Placement of the constraint frame 2 with respect to the WORLD frame.")
          .PINOCCHIO_ADD_PROPERTY(Self, c1Mc2, "Relative displacement between the two frames.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_placement_error,
            "Current contact placement error between the two contact Frames.\n"
            "This corresponds to the relative placement between the two contact Frames seen as a "
            "Motion error.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact1_velocity, "Current contact Spatial velocity of the constraint 1.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact2_velocity, "Current contact Spatial velocity of the constraint 2.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_velocity_error,
            "Current contact Spatial velocity error between the two contact Frames.\n"
            "This corresponds to the relative velocity between the two contact Frames.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_acceleration, "Current contact Spatial acceleration.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_acceleration_desired, "Desired contact acceleration.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_acceleration_error,
            "Current contact spatial error (due to the integration step).")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact1_acceleration_drift,
            "Current contact drift acceleration (acceleration only due to the Coriolis and "
            "centrifugal effects) of the contact frame 1.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact2_acceleration_drift,
            "Current contact drift acceleration (acceleration only due to the Coriolis and "
            "centrifugal effects) of the contact frame 2.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, contact_acceleration_deviation,
            "Contact deviation from the reference acceleration (a.k.a the error).")

          .PINOCCHIO_ADD_PROPERTY(
            Self, extended_motion_propagators_joint1,
            "Extended force/motion propagators for joint 1.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, lambdas_joint1, "Extended force/motion propagators for joint 1.")
          .PINOCCHIO_ADD_PROPERTY(
            Self, extended_motion_propagators_joint2,
            "Extended force/motion propagators for joint 2.")

          .def(ComparableVisitor<Self, pinocchio::is_floating_point<Scalar>::value>());
      }

      static void expose()
      {
        bp::class_<RigidConstraintData>(
          "RigidConstraintData",
          "Rigid constraint data associated to a "
          "RigidConstraintModel for contact dynamic algorithms.",
          bp::no_init)
          .def(RigidConstraintDataPythonVisitor());

        typedef typename RigidConstraintData::VectorOfMatrix6 VectorOfMatrix6;
        StdVectorPythonVisitor<VectorOfMatrix6, true>::expose("StdVec_Matrix6_");
      }
    };

  } // namespace python
} // namespace pinocchio
