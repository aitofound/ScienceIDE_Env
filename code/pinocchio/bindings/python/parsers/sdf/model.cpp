//
// Copyright (c) 2021 CNRS INRIA
//

#ifdef PINOCCHIO_WITH_SDFORMAT
  #include "pinocchio/parsers/sdf.hpp"
#endif
#include "pinocchio/bindings/python/parsers/sdf.hpp"
#include "pinocchio/bindings/python/utils/path.hpp"

#include <eigenpy/deprecation-policy.hpp>

#include <boost/python.hpp>
#include <boost/python/tuple.hpp>

namespace pinocchio
{
  namespace python
  {

    namespace bp = boost::python;

#ifdef PINOCCHIO_WITH_SDFORMAT
    bp::tuple buildModelAndConstraintsFromSdf(
      const bp::object & filename,
      const std::string & root_link_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<PointAnchorConstraintModel> constraint_models;
      ::pinocchio::sdf::buildModel(
        path(filename), model, constraint_models, root_link_name, parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }

    bp::tuple buildModelAndConstraintsFromSdfAndParentGuidance(
      const bp::object & filename,
      const JointModel & root_joint,
      const std::string & root_link_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<PointAnchorConstraintModel> constraint_models;
      pinocchio::sdf::buildModel(
        path(filename), root_joint, model, constraint_models, root_link_name, parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }

    bp::tuple buildModelAndConstraintsFromSdfAndRootJointAndParentGuidance(
      const bp::object & filename,
      const JointModel & root_joint,
      const std::string & root_link_name,
      const std::string & root_joint_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<PointAnchorConstraintModel> constraint_models;
      pinocchio::sdf::buildModel(
        path(filename), root_joint, root_joint_name, model, constraint_models, root_link_name,
        parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }

    bp::tuple buildModelAndLegacyConstraintsFromSdf(
      const bp::object & filename,
      const std::string & root_link_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<RigidConstraintModel> constraint_models;
      ::pinocchio::sdf::buildModel(
        path(filename), model, constraint_models, root_link_name, parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }

    bp::tuple buildModelAndLegacyConstraintsFromSdfAndParentGuidance(
      const bp::object & filename,
      const JointModel & root_joint,
      const std::string & root_link_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<RigidConstraintModel> constraint_models;
      pinocchio::sdf::buildModel(
        path(filename), root_joint, model, constraint_models, root_link_name, parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }

    bp::tuple buildModelAndLegacyConstraintsFromSdfAndRootJointAndParentGuidance(
      const bp::object & filename,
      const JointModel & root_joint,
      const std::string & root_link_name,
      const std::string & root_joint_name,
      const std::vector<std::string> & parent_guidance)
    {
      Model model;
      std::vector<RigidConstraintModel> constraint_models;
      pinocchio::sdf::buildModel(
        path(filename), root_joint, root_joint_name, model, constraint_models, root_link_name,
        parent_guidance);
      return bp::make_tuple(model, constraint_models);
    }
#endif

    void exposeSDFModel()
    {
#ifdef PINOCCHIO_WITH_SDFORMAT
      bp::def(
        "buildModelFromSdf", pinocchio::python::buildModelAndLegacyConstraintsFromSdf,
        (bp::arg("sdf_filename"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        eigenpy::deprecated_function<>(
          "Deprecated function. Use buildModelAndLegacyConstraintsFromSdf "
          "instead."),
        "Parse the SDF file given in input and return a pinocchio Model and constraint models.");

      bp::def(
        "buildModelFromSdf",
        pinocchio::python::buildModelAndLegacyConstraintsFromSdfAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        eigenpy::deprecated_function<>(
          "Deprecated function. Use buildModelAndLegacyConstraintsFromSdf "
          "instead."),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint.");

      bp::def(
        "buildModelFromSdf",
        pinocchio::python::buildModelAndLegacyConstraintsFromSdfAndRootJointAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("root_joint_name"), bp::arg("parent_guidance") = bp::list()),
        eigenpy::deprecated_function<>(
          "Deprecated function. Use buildModelAndLegacyConstraintsFromSdf "
          "instead."),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint and its specified name.");

      bp::def(
        "buildModelAndLegacyConstraintsFromSdf",
        pinocchio::python::buildModelAndLegacyConstraintsFromSdf,
        (bp::arg("sdf_filename"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint models.");

      bp::def(
        "buildModelAndLegacyConstraintsFromSdf",
        pinocchio::python::buildModelAndLegacyConstraintsFromSdfAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint.");

      bp::def(
        "buildModelAndLegacyConstraintsFromSdf",
        pinocchio::python::buildModelAndLegacyConstraintsFromSdfAndRootJointAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("root_joint_name"), bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint and its specified name.");

      bp::def(
        "buildModelAndConstraintsFromSdf", pinocchio::python::buildModelAndConstraintsFromSdf,
        (bp::arg("sdf_filename"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint models.");

      bp::def(
        "buildModelAndConstraintsFromSdf",
        pinocchio::python::buildModelAndConstraintsFromSdfAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint.");

      bp::def(
        "buildModelAndConstraintsFromSdf",
        pinocchio::python::buildModelAndConstraintsFromSdfAndRootJointAndParentGuidance,
        (bp::arg("sdf_filename"), bp::arg("root_joint"), bp::arg("root_link_name"),
         bp::arg("root_joint_name"), bp::arg("parent_guidance") = bp::list()),
        "Parse the SDF file given in input and return a pinocchio Model and constraint "
        "models starting with the given root joint and its specified name.");
#endif
    }
  } // namespace python
} // namespace pinocchio
