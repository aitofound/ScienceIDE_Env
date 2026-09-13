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

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    // -----------------------------------------------------------------
    // Declaration of all inheritance visitor we want to apply
    // i.e. level of CRTP where member or methods are declared
    // -----------------------------------------------------------------
    template<class T, class = void>
    struct BinaryKinematicsConstraintModelBasePythonVisitor;

    template<class T, class = void>
    struct PointConstraintModelBasePythonVisitor;

    // -----------------------------------------------------------------
    // The visitor that apply all the possible inheritance
    // -----------------------------------------------------------------
    template<class T>
    struct ConstraintModelInheritancePythonVisitor
    : public bp::def_visitor<ConstraintModelInheritancePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        // Apply all possible inheritance
        cl.def(BinaryKinematicsConstraintModelBasePythonVisitor<T>())
          .def(PointConstraintModelBasePythonVisitor<T>());
      }
    };

    // -----------------------------------------------------------------
    // Actual implementation of visitors when there is inheritence
    // -----------------------------------------------------------------

    template<class T, class> // Default: do nothing
    struct BinaryKinematicsConstraintModelBasePythonVisitor
    : public bp::def_visitor<BinaryKinematicsConstraintModelBasePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & /*cl*/) const
      {
      }
    };

    template<class T> // SFINAE specialization discard except when inherit
    struct BinaryKinematicsConstraintModelBasePythonVisitor<T, enable_if_binarykinematic_model_t<T>>
    : public bp::def_visitor<BinaryKinematicsConstraintModelBasePythonVisitor<T>>
    {
      typedef typename T::ConstraintData ConstraintData;
      typedef typename T::MatrixSize6 MatrixSize6;
      typedef typename T::SE3 SE3;
      typedef context::Model Model;

    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(
            bp::init<const Model &, JointIndex, const SE3 &, JointIndex, const SE3 &>(
              (bp::arg("self"), bp::arg("model"), bp::arg("joint1_id"), bp::arg("joint1_placement"),
               bp::arg("joint2_id"), bp::arg("joint2_placement")),
              "Constructor from given joint index and placement for the two joints "
              "implied in the constraint."))
          .def(
            bp::init<const Model &, JointIndex>(
              (bp::arg("self"), bp::arg("model"), bp::arg("joint1_id")),
              "Constructor from given joint index of the first joint "
              "implied in the constraint."))
          .def(
            bp::init<const Model &, JointIndex, const SE3 &>(
              (bp::arg("self"), bp::arg("model"), bp::arg("joint1_id"),
               bp::arg("joint1_placement")),
              "Constructor from given joint index and placement of the first joint "
              "implied in the constraint."))
          .def(
            bp::init<const Model &, JointIndex, JointIndex>(
              (bp::arg("self"), bp::arg("model"), bp::arg("joint1_id"), bp::arg("joint2_id")),
              "Constructor from given joint index for the two joints "
              "implied in the constraint."))
          .def(
            bp::init<const Model &>(
              (bp::arg("self"), bp::arg("model")), "Constructor from the model only."))
          .def(
            "getA1", &getA1, bp::args("self", "constraint_data", "reference_frame"),
            "Returns the constraint projector associated with joint 1. "
            "This matrix transforms a spatial velocity expressed in a reference frame "
            "to the first component of the constraint associated with joint 1.")
          .def(
            "getA2", &getA2, bp::args("self", "constraint_data", "reference_frame"),
            "Returns the constraint projector associated with joint 2. "
            "This matrix transforms a spatial velocity expressed in a reference frame "
            "to the first component of the constraint associated with joint 2.")
          .PINOCCHIO_ADD_PROPERTY(T, joint1_id, "Index of the first joint in the model tree.")
          .PINOCCHIO_ADD_PROPERTY(T, joint2_id, "Index of the second joint in the model tree.")
          .PINOCCHIO_ADD_PROPERTY(
            T, joint1_placement, "Position of attached point with respect to the frame of joint1.")
          .PINOCCHIO_ADD_PROPERTY(
            T, joint2_placement, "Position of attached point with respect to the frame of joint2.")
          .PINOCCHIO_ADD_PROPERTY(
            T, desired_constraint_offset, "Desired constraint shift at position level.")
          .PINOCCHIO_ADD_PROPERTY(
            T, desired_constraint_velocity, "Desired constraint velocity at velocity level.")
          .PINOCCHIO_ADD_PROPERTY(
            T, desired_constraint_acceleration,
            "Desired constraint velocity at acceleration level.")
          .PINOCCHIO_ADD_PROPERTY(T, nv, "Dimension of the model velocity.")
          .PINOCCHIO_ADD_PROPERTY(T, depth_joint1, "Depth of the kinematic tree for joint1.")
          .PINOCCHIO_ADD_PROPERTY(T, depth_joint2, "Depth of the kinematic tree for joint2.");
      }

      static MatrixSize6
      getA1(const T & self, const ConstraintData & constraint_data, ReferenceFrame rf)
      {
        MatrixSize6 res;
        switch (rf)
        {
        case WORLD:
          res = self.getA1(constraint_data, WorldFrameTag());
          break;
        case LOCAL:
          res = self.getA1(constraint_data, LocalFrameTag());
          break;
        case LOCAL_WORLD_ALIGNED:
          res = self.getA1(constraint_data, LocalWorldAlignedFrameTag());
          break;
        }
        return res;
      }

      static MatrixSize6
      getA2(const T & self, const ConstraintData & constraint_data, ReferenceFrame rf)
      {
        MatrixSize6 res;
        switch (rf)
        {
        case WORLD:
          res = self.getA2(constraint_data, WorldFrameTag());
          break;
        case LOCAL:
          res = self.getA2(constraint_data, LocalFrameTag());
          break;
        case LOCAL_WORLD_ALIGNED:
          res = self.getA2(constraint_data, LocalWorldAlignedFrameTag());
          break;
        }
        return res;
      }
    }; // BinaryKinematicsConstraintModelBasePythonVisitor

    template<class T, class> // Default: do nothing
    struct PointConstraintModelBasePythonVisitor
    : public bp::def_visitor<PointConstraintModelBasePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & /*cl*/) const
      {
      }
    };

    template<class T> // SFINAE specialization discard except when inherit
    struct PointConstraintModelBasePythonVisitor<T, enable_if_point_model_t<T>>
    : public bp::def_visitor<PointConstraintModelBasePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(
          "computeConstraintSpatialInertia", &computeConstraintSpatialInertia,
          bp::args("self", "placement", "diagonal_constraint_inertia"),
          "This function computes the spatial inertia associated with the constraint.");
        // computeConstraintInertias is not exposed as it is designed for Eigen Blocks
      }

      static context::Matrix6s computeConstraintSpatialInertia(
        const T & self,
        const context::SE3 & placement,
        const context::Vector3s & diagonal_constraint_inertia)
      {
        return self.computeConstraintSpatialInertia(
          placement, diagonal_constraint_inertia.asDiagonal());
      }
    }; // PointConstraintModelBasePythonVisitor

  } // namespace python
} // namespace pinocchio
