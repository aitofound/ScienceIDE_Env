//
// Copyright (c) 2016-2023 CNRS INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/parsers/mjcf.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/parsers/mjcf.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace mjcf
  {
    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      return buildModelFromXML(filename, model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      typedef ::pinocchio::parsers::Model Model;

      Model mjcf_model = model;
      ::pinocchio::mjcf::details::MjcfVisitor visitor(mjcf_model);

      typedef ::pinocchio::mjcf::details::MjcfGraph MjcfGraph;

      MjcfGraph graph(visitor, filename);
      if (verbose)
        visitor.log = &std::cout;

      graph.parseGraphFromXML(filename);

      // Use the Mjcf graph to create the model
      graph.parseRootTree();

      model = visitor.model;
      return model;
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      return buildModelFromXML(filename, rootJoint, model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      return buildModelFromXML(filename, rootJoint, "root_joint", model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      const std::string & rootJointName,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      return buildModelFromXML(filename, rootJoint, rootJointName, model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      const std::string & rootJointName,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const bool verbose)
    {
      typedef ::pinocchio::parsers::Model Model;
      typedef ::pinocchio::parsers::JointModel JointModel;
      if (rootJointName.empty())
        throw std::invalid_argument(
          "rootJoint was given without a name. Please fill the argument rootJointName");

      Model mjcf_model = model;
      JointModel root_joint = rootJoint;

      ::pinocchio::mjcf::details::MjcfVisitor visitor(mjcf_model);

      typedef ::pinocchio::mjcf::details::MjcfGraph MjcfGraph;

      MjcfGraph graph(visitor, filename);
      if (verbose)
        visitor.log = &std::cout;

      graph.parseGraphFromXML(filename);

      // Use the Mjcf graph to create the model
      boost::optional<const JointModel &> root_joint_opt(root_joint);
      boost::optional<const std::string &> root_joint_name_opt(rootJointName);
      graph.parseRootTree(root_joint_opt, root_joint_name_opt);

      model = visitor.model;
      return model;
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<PointAnchorConstraintModel> & point_anchor_constraint_models,
      std::vector<FrameAnchorConstraintModel> & frame_anchor_constraint_models,
      const bool verbose)
    {
      typedef ::pinocchio::parsers::Model Model;
      Model mjcf_model = model;

      ::pinocchio::mjcf::details::MjcfVisitor visitor(mjcf_model);

      typedef ::pinocchio::mjcf::details::MjcfGraph MjcfGraph;
      MjcfGraph graph(visitor, filename);

      if (verbose)
        visitor.log = &std::cout;

      graph.parseGraphFromXML(filename);

      // Use the Mjcf graph to create the model
      graph.parseRootTree();

      model = visitor.model;

      // Use the Mjcf graph to create the model
      graph.parseContactInformation(
        model, point_anchor_constraint_models, frame_anchor_constraint_models);

      return model;
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<PointAnchorConstraintModel> & point_anchor_constraint_models,
      std::vector<FrameAnchorConstraintModel> & frame_anchor_constraint_models,
      const bool verbose)
    {
      return buildModel(
        filename, rootJoint, "root_joint", model, point_anchor_constraint_models,
        frame_anchor_constraint_models, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      const std::string & rootJointName,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<PointAnchorConstraintModel> & point_anchor_constraint_models,
      std::vector<FrameAnchorConstraintModel> & frame_anchor_constraint_models,
      const bool verbose)
    {
      typedef ::pinocchio::parsers::Model Model;
      typedef ::pinocchio::parsers::JointModel JointModel;
      if (rootJointName.empty())
        throw std::invalid_argument(
          "rootJoint was given without a name. Please fill the argument rootJointName");

      Model mjcf_model = model;
      JointModel root_joint = rootJoint;

      ::pinocchio::mjcf::details::MjcfVisitor visitor(mjcf_model);

      typedef ::pinocchio::mjcf::details::MjcfGraph MjcfGraph;

      MjcfGraph graph(visitor, filename);
      if (verbose)
        visitor.log = &std::cout;

      graph.parseGraphFromXML(filename);

      // Use the Mjcf graph to create the model
      boost::optional<const JointModel &> root_joint_opt(root_joint);
      boost::optional<const std::string &> root_joint_name_opt(rootJointName);
      graph.parseRootTree(root_joint_opt, root_joint_name_opt);

      model = visitor.model;

      // Use the Mjcf graph to create the model
      graph.parseContactInformation(
        model, point_anchor_constraint_models, frame_anchor_constraint_models);

      return model;
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      return buildModelFromXML(filename, model, rigid_constraint_model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      std::vector<PointAnchorConstraintModel> point_anchor_constraint_models;
      std::vector<FrameAnchorConstraintModel> frame_anchor_constraint_models;
      buildModel(
        filename, model, point_anchor_constraint_models, frame_anchor_constraint_models, verbose);

      for (const auto & pacm : point_anchor_constraint_models)
      {
        rigid_constraint_model.push_back(convertToRigidConstraintModel(model, pacm));
      }
      for (const auto & facm : frame_anchor_constraint_models)
      {
        rigid_constraint_model.push_back(convertToRigidConstraintModel(model, facm));
      }
      return model;
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      return buildModelFromXML(filename, rootJoint, model, rigid_constraint_model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      return buildModelFromXML(
        filename, rootJoint, "root_joint", model, rigid_constraint_model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModel(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      const std::string & rootJointName,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      return buildModelFromXML(
        filename, rootJoint, rootJointName, model, rigid_constraint_model, verbose);
    }

    template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
    PINOCCHIO_UNSUPPORTED ModelTpl<Scalar, Options, JointCollectionTpl> & buildModelFromXML(
      const std::string & filename,
      const typename ModelTpl<Scalar, Options, JointCollectionTpl>::JointModel & rootJoint,
      const std::string & rootJointName,
      ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      std::vector<RigidConstraintModel> & rigid_constraint_model,
      const bool verbose)
    {
      std::vector<PointAnchorConstraintModel> point_anchor_constraint_models;
      std::vector<FrameAnchorConstraintModel> frame_anchor_constraint_models;
      buildModel(
        filename, rootJoint, rootJointName, model, point_anchor_constraint_models,
        frame_anchor_constraint_models, verbose);

      for (const auto & pacm : point_anchor_constraint_models)
      {
        rigid_constraint_model.push_back(convertToRigidConstraintModel(model, pacm));
      }
      for (const auto & facm : frame_anchor_constraint_models)
      {
        rigid_constraint_model.push_back(convertToRigidConstraintModel(model, facm));
      }
      return model;
    }

  } // namespace mjcf
} // namespace pinocchio
