//
// Copyright (c) 2025 INRIA
//

#pragma once

#include <boost/python.hpp>
#include <eigenpy/exception.hpp>
#include <eigenpy/eigen-to-python.hpp>

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
    struct BinaryKinematicsConstraintDataBasePythonVisitor;

    // -----------------------------------------------------------------
    // The visitor that apply all the possible inheritance
    // -----------------------------------------------------------------
    template<class T>
    struct ConstraintDataInheritancePythonVisitor
    : public bp::def_visitor<ConstraintDataInheritancePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(BinaryKinematicsConstraintDataBasePythonVisitor<T>());
      }
    };

    // -----------------------------------------------------------------
    // Actual implementation of visitors when there is inheritence
    // -----------------------------------------------------------------
    template<class T> // Check inheritence
    inline constexpr bool inherit_binarykinematic_data_v =
      std::is_base_of_v<PointConstraintDataBase<T>, T>
      || std::is_base_of_v<FrameConstraintDataBase<T>, T>;

    template<class T, class> // Default: do nothing
    struct BinaryKinematicsConstraintDataBasePythonVisitor
    : public bp::def_visitor<BinaryKinematicsConstraintDataBasePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & /*cl*/) const
      {
      }
    };

    template<class T> // SFINAE specialization discard except when inherit
    struct BinaryKinematicsConstraintDataBasePythonVisitor<
      T,
      std::enable_if_t<inherit_binarykinematic_data_v<T>>>
    : public bp::def_visitor<BinaryKinematicsConstraintDataBasePythonVisitor<T>>
    {
    public:
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def(bp::init<>(bp::arg("self"), "Default constructor."))
          .def(
            bp::init<const typename T::ConstraintModel &>(
              bp::args("self", "constraint_model"), "From model constructor."))
          .PINOCCHIO_ADD_PROPERTY(T, constraint_force, "Resulting force.")
          .PINOCCHIO_ADD_PROPERTY(T, oMc1, "Placement of the constraint frame 1 wrt WORLD.")
          .PINOCCHIO_ADD_PROPERTY(T, oMc2, "Placement of the constraint frame 2 wrt WORLD.")
          .PINOCCHIO_ADD_PROPERTY(T, c1Mc2, "Placement of the constraint frame 2 wrt frame 1.")
          .PINOCCHIO_ADD_PROPERTY(T, constraint_position_error, "Constraint position error.")
          .PINOCCHIO_ADD_PROPERTY(T, constraint_velocity_error, "Constraint velocity error.")
          .PINOCCHIO_ADD_PROPERTY(
            T, constraint_acceleration_error, "Constraint acceleration error.")
          .PINOCCHIO_ADD_PROPERTY(
            T, constraint_acceleration_biais_term, "Constraint acceleration term.")
          .PINOCCHIO_ADD_PROPERTY(T, A1_world, "Transform for joint1 in world frame.")
          .PINOCCHIO_ADD_PROPERTY(T, A2_world, "Transform for joint2 in world frame.")
          .PINOCCHIO_ADD_PROPERTY(
            T, A_world, "Relative Transform between joint1 and joint2 in world frame.")
          .PINOCCHIO_ADD_PROPERTY(T, A1_local, "Transform for joint1 in local frame.")
          .PINOCCHIO_ADD_PROPERTY(T, A2_local, "Transform for joint2 in local frame.")
          .PINOCCHIO_ADD_PROPERTY(
            T, A_local, "Relative Transform between joint1 and joint2 in local frame.");
      }
    };
  } // namespace python
} // namespace pinocchio
