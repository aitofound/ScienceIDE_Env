//
// Copyright (c) 2025 INRIA
//

#pragma once

#include <boost/python.hpp>
#include <eigenpy/exception.hpp>
#include <eigenpy/eigen-to-python.hpp>

#include "pinocchio/multibody.hpp"
#include "pinocchio/constraints.hpp"

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/bindings/python/utils/macros.hpp"
#include "pinocchio/bindings/python/utils/eigen.hpp"
#include "pinocchio/bindings/python/algorithm/constraints/baumgarte-corrector-parameters.hpp"

namespace pinocchio
{
  namespace python
  {

    namespace bp = boost::python;

    template<class ConstraintModelDerived>
    struct ConstraintModelBasePythonVisitor
    : public bp::def_visitor<ConstraintModelBasePythonVisitor<ConstraintModelDerived>>
    {
      typedef ConstraintModelDerived Self;
      typedef typename ::pinocchio::traits<Self>::ConstraintModel ConstraintModel;
      typedef typename ::pinocchio::traits<Self>::ConstraintData ConstraintData;
      typedef typename ::pinocchio::traits<Self>::Scalar Scalar;

      typedef typename ::pinocchio::traits<Self>::ConstraintSet ConstraintSet;
      typedef typename ::pinocchio::traits<Self>::JordanOperation JordanOperation;

      typedef typename ::pinocchio::traits<Self>::ResidualVectorType ResidualVectorType;
      typedef typename ::pinocchio::traits<Self>::JacobianMatrixType JacobianMatrixType;
      typedef typename ::pinocchio::traits<Self>::ConeVectorType ConeVectorType;
      typedef typename ::pinocchio::traits<Self>::ConeScalingVectorType ConeScalingVectorType;

      static constexpr bool has_baumgarte_corrector =
        ::pinocchio::traits<Self>::has_baumgarte_corrector;
      static constexpr bool has_set = ::pinocchio::traits<Self>::has_set;

      typedef context::Model Model;
      typedef context::Data Data;
      typedef std::vector<context::Force> ForceVector;
      typedef std::vector<context::Motion> MotionVector;

      typedef typename Self::BooleanVector BooleanVector;
      typedef typename Self::EigenIndexVector EigenIndexVector;

      typedef BaumgarteCorrectorParametersTpl<context::Scalar> BaumgarteCorrectorParameters;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.PINOCCHIO_ADD_PROPERTY(Self, name, "Name of the constraint.")
#ifndef PINOCCHIO_PYTHON_SKIP_COMPARISON_OPERATIONS
          .def(bp::self == bp::self)
          .def(bp::self != bp::self)
#endif
          .def("classname", &Self::classname)
          .staticmethod("classname")
          .def("shortname", &Self::shortname, "Short name of the class.")
          .def(
            "createData", &Self::createData, "Create a Data object for the given constraint model.")
          .def(
            "residualSize", &residualSize,
            (bp::arg("self"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Constraint size for the selection.")
          .def(
            "symmetricConeResidualSize", &symmetricConeResidualSize,
            (bp::arg("self"), bp::arg("sel") = ConstraintSelectionType::CURRENT),
            "Symmetric cone residual size for the selection.")
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
        if constexpr (has_baumgarte_corrector)
        {
          cl.add_property(
            "baumgarte_corrector_parameters",
            bp::make_function( //
              +[](const Self & self) -> const BaumgarteCorrectorParameters & {
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
        if constexpr (has_set)
        {
          cl.def(
            "set",
            +[](const Self & self, const ConstraintData & cdata) -> ConstraintSet {
              return self.set(cdata);
            },
            bp::args("self", "constraint_data"), "Constraint set.");
        }
        cl.def(
            "calc", &calc, bp::args("self", "model", "data", "constraint_data"),
            "Evaluate the constraint values at the current state given by data and store the "
            "results.")
          .def(
            "getRowSparsityPattern",
            bp::make_function(
              +[](
                 const Self & self, const Model & model, const Data & data,
                 const ConstraintData & cdata, const Eigen::Index row_id) -> BooleanVector {
                BooleanVector res;
                self.getRowSparsityPattern(model, data, cdata, row_id, res);
                return res;
              }),
            "Active colwise sparsity associated with a given row.")
          .def(
            "getRowIndexes",
            bp::make_function(
              +[](
                 const Self & self, const Model & model, const Data & data,
                 const ConstraintData & cdata, const Eigen::Index row_id) -> EigenIndexVector {
                EigenIndexVector res;
                self.getRowIndexes(model, data, cdata, row_id, res);
                return res;
              }),
            "Vector of the active indexes associated with a given row.")
          .def(
            "jacobian",
            (JacobianMatrixType (Self::*)(const Model &, const Data &, ConstraintData &) const)
              & Self::jacobian,
            bp::args("self", "model", "data", "constraint_data"),
            "Compute the constraint jacobian.")
          .def(
            "jacobianMatrixProduct", &jacobianMatrixProduct,
            bp::args("self", "model", "data", "constraint_data", "matrix"),
            "Forward chain rule: return product between the jacobian and a matrix.")
          .def(
            "jacobianTransposeMatrixProduct", &jacobianTransposeMatrixProduct,
            bp::args("self", "model", "data", "constraint_data", "matrix"),
            "Backward chain rule: return product between the jacobian transpose and a matrix.")
          .def(
            "mapConstraintForceToJointSpace", &mapConstraintForceToJointSpace,
            (bp::arg("self"), bp::arg("model"), bp::arg("data"), bp::arg("constraint_data"),
             bp::arg("constraint_forces"), bp::arg("joint_forces"),
             bp::arg("reference_frame") = LOCAL),
            "Map constraint forces to joint space.")
          .def(
            "mapJointSpaceToConstraintMotion", &mapJointSpaceToConstraintMotion,
            (bp::arg("self"), bp::arg("model"), bp::arg("data"), bp::arg("constraint_data"),
             bp::arg("joint_motions"), bp::arg("joint_generalized_velocity"),
             bp::arg("reference_frame") = LOCAL),
            "Map joint space quantities to constraint motion.")
          .def(
            "appendCouplingConstraintInertias", &appendCouplingConstraintInertias,
            (bp::arg("self"), bp::arg("model"), bp::arg("data"), bp::arg("constraint_data"),
             bp::arg("diagonal_constraint_inertia"), bp::arg("reference_frame") = LOCAL),
            "Append to data the apparent inertia due to the constraint.");
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

      static int symmetricConeResidualSize(const Self & self, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.symmetricConeResidualSize(CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.symmetricConeResidualSize(MaximalSelection());
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      static int symmetricConeResidualScalingSize(const Self & self, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.symmetricConeResidualScalingSize(CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.symmetricConeResidualScalingSize(MaximalSelection());
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      static void
      setCompliance(Self & self, const ResidualVectorType & vector, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.setCompliance(vector, CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.setCompliance(vector, MaximalSelection());
        }
      }

      static ResidualVectorType retrieveCompliance(const Self & self, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT: {
          ResidualVectorType rescs(self.residualSize(CurrentSelection()));
          self.retrieveCompliance(rescs, CurrentSelection());
          return rescs;
        }
        case ConstraintSelectionType::MAXIMAL: {
          ResidualVectorType resms(self.residualSize(MaximalSelection()));
          self.retrieveCompliance(resms, MaximalSelection());
          return resms;
        }
        default:
          PINOCCHIO_UNREACHABLE();
        }
      }

      static void setBaumgarteCorrectorParameters(
        Self & self, const BaumgarteCorrectorParameters & copy, ConstraintSelectionType sel)
      {
        switch (sel)
        {
        case ConstraintSelectionType::CURRENT:
          return self.setBaumgarteCorrectorParameters(copy, CurrentSelection());
        case ConstraintSelectionType::MAXIMAL:
          return self.setBaumgarteCorrectorParameters(copy, MaximalSelection());
        }
      }

      static void calc(
        const Self & self, const Model & model, const Data & data, ConstraintData & constraint_data)
      {
        self.calc(model, data, constraint_data);
      }

      static context::MatrixXs jacobianMatrixProduct(
        const Self & self,
        const Model & model,
        const Data & data,
        const ConstraintData & constraint_data,
        const context::RefConstMatrixXs & matrix)
      {
        context::MatrixXs res = context::MatrixXs::Zero(self.residualSize(), matrix.cols());
        self.jacobianMatrixProduct(model, data, constraint_data, matrix, res);
        return res;
      }

      static context::MatrixXs jacobianTransposeMatrixProduct(
        const Self & self,
        const Model & model,
        const Data & data,
        const ConstraintData & constraint_data,
        const context::RefConstMatrixXs & matrix)
      {
        context::MatrixXs res = context::MatrixXs::Zero(model.nv, matrix.cols());
        self.jacobianTransposeMatrixProduct(model, data, constraint_data, matrix, res);
        return res;
      }

      static context::VectorXs mapConstraintForceToJointSpace(
        const Self & self,
        const Model & model,
        const Data & data,
        const ConstraintData & cdata,
        const context::VectorXs & constraint_forces,
        ForceVector & joint_forces,
        ReferenceFrame rf)
      {
        context::VectorXs joint_torques = context::VectorXs::Zero(model.nv);
        switch (rf)
        {
        case WORLD:
          self.mapConstraintForceToJointSpace(
            model, data, cdata, constraint_forces, joint_forces, joint_torques, WorldFrameTag());
          break;
        case LOCAL:
          self.mapConstraintForceToJointSpace(
            model, data, cdata, constraint_forces, joint_forces, joint_torques, LocalFrameTag());
          break;
        case LOCAL_WORLD_ALIGNED:
          self.mapConstraintForceToJointSpace(
            model, data, cdata, constraint_forces, joint_forces, joint_torques,
            LocalWorldAlignedFrameTag());
          break;
        }
        return joint_torques;
      }

      static context::VectorXs mapJointSpaceToConstraintMotion(
        const Self & self,
        const Model & model,
        const Data & data,
        const ConstraintData & cdata,
        const MotionVector & joint_motions,
        const context::VectorXs & joint_generalized_velocity,
        ReferenceFrame rf)
      {
        context::VectorXs constraint_motions = context::VectorXs::Zero(self.residualSize());
        switch (rf)
        {
        case WORLD:
          self.mapJointSpaceToConstraintMotion(
            model, data, cdata, joint_motions, joint_generalized_velocity, constraint_motions,
            WorldFrameTag());
          break;
        case LOCAL:
          self.mapJointSpaceToConstraintMotion(
            model, data, cdata, joint_motions, joint_generalized_velocity, constraint_motions,
            LocalFrameTag());
          break;
        case LOCAL_WORLD_ALIGNED:
          self.mapJointSpaceToConstraintMotion(
            model, data, cdata, joint_motions, joint_generalized_velocity, constraint_motions,
            LocalWorldAlignedFrameTag());
          break;
        }
        return constraint_motions;
      }

      static void appendCouplingConstraintInertias(
        const Self & self,
        const Model & model,
        Data & data,
        const ConstraintData & constraint_data,
        const context::VectorXs & diagonal_constraint_inertia,
        ReferenceFrame rf)
      {
        switch (rf)
        {
        case WORLD:
          self.appendCouplingConstraintInertias(
            model, data, constraint_data, diagonal_constraint_inertia, WorldFrameTag());
          break;
        case LOCAL:
          self.appendCouplingConstraintInertias(
            model, data, constraint_data, diagonal_constraint_inertia, LocalFrameTag());
          break;
        case LOCAL_WORLD_ALIGNED:
          self.appendCouplingConstraintInertias(
            model, data, constraint_data, diagonal_constraint_inertia, LocalWorldAlignedFrameTag());
          break;
        }
      }
    };
  } // namespace python
} // namespace pinocchio
