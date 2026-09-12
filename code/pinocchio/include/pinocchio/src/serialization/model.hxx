//
// Copyright (c) 2019-2020 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/serialization.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/serialization.hpp"
#endif // PINOCCHIO_LSP

namespace boost
{
  namespace serialization
  {
    template<
      class Archive,
      typename Scalar,
      int Options,
      template<typename, int> class JointCollectionTpl>
    void serialize(
      Archive & ar,
      pinocchio::ModelTpl<Scalar, Options, JointCollectionTpl> & model,
      const unsigned int /*version*/)
    {
      ar & make_nvp("nq", model.nq);
      ar & make_nvp("nqs", model.nqs);
      ar & make_nvp("idx_qs", model.idx_qs);
      ar & make_nvp("nv", model.nv);
      ar & make_nvp("nvs", model.nvs);
      ar & make_nvp("idx_vs", model.idx_vs);
      ar & make_nvp("nvExtended", model.nvExtended);
      ar & make_nvp("nvExtendeds", model.nvExtendeds);
      ar & make_nvp("idx_vExtendeds", model.idx_vExtendeds);
      ar & make_nvp("njoints", model.njoints);
      ar & make_nvp("nbodies", model.nbodies);
      ar & make_nvp("nframes", model.nframes);
      ar & make_nvp("parents", model.parents);
      ar & make_nvp("children", model.children);
      ar & make_nvp("names", model.names);
      ar & make_nvp("supports", model.supports);
      ar & make_nvp("mimic_joint_supports", model.mimic_joint_supports);
      ar & make_nvp("subtrees", model.subtrees);
      ar & make_nvp("mimicking_joints", model.mimicking_joints);
      ar & make_nvp("mimicked_joints", model.mimicked_joints);
      ar & make_nvp("gravity", model.gravity);
      ar & make_nvp("name", model.name);

      ar & make_nvp("referenceConfigurations", model.referenceConfigurations);
      ar & make_nvp("armature", model.armature);
      ar & make_nvp("rotorInertia", model.rotorInertia);
      ar & make_nvp("rotorGearRatio", model.rotorGearRatio);
      ar & make_nvp("lowerDryFrictionLimit", model.lowerDryFrictionLimit);
      ar & make_nvp("upperDryFrictionLimit", model.upperDryFrictionLimit);
      ar & make_nvp("damping", model.damping);
      ar & make_nvp("lowerEffortLimit", model.lowerEffortLimit);
      ar & make_nvp("upperEffortLimit", model.upperEffortLimit);
      ar & make_nvp("lowerVelocityLimit", model.lowerVelocityLimit);
      ar & make_nvp("upperVelocityLimit", model.upperVelocityLimit);
      ar & make_nvp("lowerPositionLimit", model.lowerPositionLimit);
      ar & make_nvp("upperPositionLimit", model.upperPositionLimit);
      ar & make_nvp("positionLimitMargin", model.positionLimitMargin);
      ar & make_nvp("lowerAccelerationLimit", model.lowerAccelerationLimit);
      ar & make_nvp("upperAccelerationLimit", model.upperAccelerationLimit);
      ar & make_nvp("lowerJerkLimit", model.lowerJerkLimit);
      ar & make_nvp("upperJerkLimit", model.upperJerkLimit);

      ar & make_nvp("inertias", model.inertias);
      ar & make_nvp("jointPlacements", model.jointPlacements);

      ar & make_nvp("joints", model.joints);
      ar & make_nvp("frames", model.frames);

      // sparsity_pattern_vector and span_indexes_vector are derived from the model structure
      // (supports, nvs, idx_vs). They are not serialized but rebuilt after loading to avoid
      // issues with bool array serialization in text/XML archives.
      if (Archive::is_loading::value)
      {
        typedef pinocchio::ModelTpl<Scalar, Options, JointCollectionTpl> Model;
        typedef typename Model::BooleanVector BooleanVector;
        typedef typename Model::EigenIndexVector EigenIndexVector;
        typedef typename Model::JointIndex JointIndex;

        model.sparsity_pattern_vector.clear();
        model.span_indexes_vector.clear();
        model.sparsity_pattern_vector.push_back(BooleanVector::Zero(model.nv));
        model.span_indexes_vector.push_back(EigenIndexVector());

        for (JointIndex jid = 1; jid < (JointIndex)model.njoints; ++jid)
        {
          EigenIndexVector extended_support;
          extended_support.reserve(size_t(model.nv));
          const auto & jsupport = model.supports[jid];
          for (size_t j = 1; j < jsupport.size() - 1; ++j)
          {
            const JointIndex jsupport_id = jsupport[j];
            const int jsupport_nv = model.nvs[jsupport_id];
            const int jsupport_idx_v = model.idx_vs[jsupport_id];
            for (int k = 0; k < jsupport_nv; ++k)
              extended_support.push_back(jsupport_idx_v + k);
          }
          const int jnv = model.nvs[jid];
          const int jidx_v = model.idx_vs[jid];
          for (int k = 0; k < jnv; ++k)
            extended_support.push_back(jidx_v + k);

          BooleanVector sparsity_pattern = BooleanVector::Zero(model.nv);
          for (const auto col_id : extended_support)
            sparsity_pattern[col_id] = true;

          model.sparsity_pattern_vector.push_back(std::move(sparsity_pattern));
          model.span_indexes_vector.push_back(std::move(extended_support));
        }
      }
    }

  } // namespace serialization
} // namespace boost
