//
// Copyright (c) 2024-2025 INRIA
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
  /// \brief Init the data according to the contact information contained in constraint_models.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam Allocator Allocator class for the std::vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] constraint_models Vector of contact information related to the problem.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator,
    class ConstraintData,
    class ConstraintDataAllocator>
  inline void computeJointMinimalOrdering(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models);

  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  struct ConstraintCouplingInformationCollectorStep
  : visitors::ConstraintUnaryVisitorBase<
      ConstraintCouplingInformationCollectorStep<Scalar, Options, JointCollectionTpl>>
  {
    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef DataTpl<Scalar, Options, JointCollectionTpl> Data;

    typedef boost::fusion::vector<const Model &, Data &> ArgsType;

    typedef visitors::ConstraintUnaryVisitorBase<
      ConstraintCouplingInformationCollectorStep<Scalar, Options, JointCollectionTpl>>
      Base;

    template<typename ConstraintModel>
    static void
    algo(const ConstraintModelBase<ConstraintModel> & cmodel, const Model & model, Data & data)
    {
      algo_step(cmodel.derived(), model, data);
    }

    template<typename ConstraintModel>
    static void algo_step(
      const BinaryKinematicsConstraintModelBase<ConstraintModel> & cmodel,
      const Model & model,
      Data & data)
    {
      PINOCCHIO_UNUSED_VARIABLE(model);

      const JointIndex joint1_id = cmodel.joint1_id;
      const JointIndex joint2_id = cmodel.joint2_id;
      auto & neighbours = data.joint_neighbours;

      // Here we suppose all BinartyKinematicsConstraintModelBase<ConstraintModel> are constant size
      const auto constraint_size = cmodel.residualSize();
      data.constraints_supported_dim[joint1_id] += constraint_size;
      data.constraints_supported_dim[joint2_id] += constraint_size;

      if (joint1_id > 0 && joint2_id > 0)
      {
        data.joint_coupling_info(Eigen::Index(joint1_id), Eigen::Index(joint2_id)) = true;
        data.joint_coupling_info(Eigen::Index(joint2_id), Eigen::Index(joint1_id)) = true;

        auto & joint1_neighbours = neighbours[joint1_id];
        if (!internal::helper::exists(joint1_neighbours, joint2_id))
          joint1_neighbours.push_back(joint2_id);
        auto & joint2_neighbours = neighbours[joint2_id];
        if (!internal::helper::exists(joint2_neighbours, joint1_id))
          joint2_neighbours.push_back(joint1_id);
      }
    }

    template<typename ConstraintModel>
    static void algo_step(
      const JointWiseConstraintModelBase<ConstraintModel> & cmodel,
      const Model & model,
      Data & data)
    {
      PINOCCHIO_UNUSED_VARIABLE(cmodel);
      PINOCCHIO_UNUSED_VARIABLE(model);
      PINOCCHIO_UNUSED_VARIABLE(data);
    }

    using Base::run;

    template<typename ConstraintModel>
    static void run(
      const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
      const Model & model,
      Data & data)
    {
      algo(cmodel.derived(), model, data);
    }

    template<
      typename _Scalar,
      int _Options,
      template<typename S, int O> class ConstraintCollectionTpl>
    static void run(
      const pinocchio::ConstraintModelTpl<_Scalar, _Options, ConstraintCollectionTpl> & cmodel,
      const Model & model,
      Data & data)
    {
      ArgsType args(model, data);
      run(cmodel.derived(), args);
    }

  }; // struct ConstraintCouplingInformationCollectorStep

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    class ConstraintModel,
    class ConstraintModelAllocator>
  inline void computeJointMinimalOrdering(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models)
  {
    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef DataTpl<Scalar, Options, JointCollectionTpl> Data;

    using JointIndex = typename Model::JointIndex;
    using Matrix6 = typename Data::Matrix6;
    typedef std::pair<JointIndex, JointIndex> JointPair;

    auto & joint_coupling_info = data.joint_coupling_info;
    joint_coupling_info.setZero();

    // First step: for each joint, collect their neighbourds
    auto & neighbours = data.joint_neighbours;
    for (auto & neighbour_elt : neighbours)
      neighbour_elt.clear();

    // Get links supporting constraints
    std::fill(data.constraints_supported_dim.begin(), data.constraints_supported_dim.end(), 0);
    typedef ConstraintCouplingInformationCollectorStep<Scalar, Options, JointCollectionTpl>
      CollectorStep;
    for (std::size_t i = 0; i < constraint_models.size(); ++i)
    {
      const auto & cmodel = internal::helper::get_ref(constraint_models[i]);
      CollectorStep::run(cmodel, model, data);
    }

    // Second step: order the joints according to the minimum degree heuristic
    auto & joint_elimination_order = data.joint_elimination_order;
    joint_elimination_order.clear(); // clearing in case inited once more

    std::vector<size_t> num_children(size_t(model.njoints), 0);
    std::list<JointIndex> leaf_vertices;

    // Collect all the tree leaves
    for (JointIndex joint_id = JointIndex(model.njoints - 1); joint_id > 0; --joint_id)
    {
      num_children[joint_id] = model.children[joint_id].size();
      if (num_children[joint_id] == 0)
        leaf_vertices.push_back(joint_id);
    }

    // While there is still some leaves
    while (leaf_vertices.size() > 0)
    {
      JointIndex joint_id_with_least_neighbors = std::numeric_limits<JointIndex>::max();
      size_t least_neighbours = std::numeric_limits<size_t>::max();

      // Retrieve the joint_id with the minimum number of neighbors
      for (const auto joint_id : leaf_vertices)
      {
        assert(joint_id != 0);
        const auto leaf_num_neighours = neighbours[joint_id].size();
        if (leaf_num_neighours < least_neighbours)
        {
          least_neighbours = leaf_num_neighours;
          joint_id_with_least_neighbors = joint_id;
        }
      }

      const JointIndex joint_id = joint_id_with_least_neighbors;
      assert(joint_id != 0);
      leaf_vertices.remove(joint_id);
      joint_elimination_order.push_back(joint_id);

      const JointIndex parent_id = model.parents[joint_id];
      num_children[parent_id]--;
      // If the number of children joints of parent is reaching zero, this means that parent is now
      // a leaf node.
      if (num_children[parent_id] == 0 && parent_id != 0)
        leaf_vertices.push_front(parent_id);

      data.constraints_supported_dim[parent_id] += data.constraints_supported_dim[joint_id];
      const auto & joint_neighbours = neighbours[joint_id];
      auto & parent_neighbours = neighbours[parent_id];

      for (size_t j = 0; j < joint_neighbours.size(); j++)
      {
        const JointIndex neighbour_j = joint_neighbours[j];
        auto & neighbour_j_neighbours = neighbours[neighbour_j];
        if (neighbour_j != parent_id)
        {
          const JointPair jp_pair = neighbour_j < parent_id ? JointPair(neighbour_j, parent_id)
                                                            : JointPair(parent_id, neighbour_j);
#define EXIST_JOINT_PAIR(pair)                                                                     \
  bool(                                                                                            \
    joint_coupling_info(Eigen::Index(pair.first), Eigen::Index(pair.second))                       \
    && joint_coupling_info(Eigen::Index(pair.second), Eigen::Index(pair.first)))
#define REGISTER_JOINT_PAIR(pair)                                                                  \
  joint_coupling_info(Eigen::Index(pair.first), Eigen::Index(pair.second)) = true;                 \
  joint_coupling_info(Eigen::Index(pair.second), Eigen::Index(pair.first)) = true
          if (!EXIST_JOINT_PAIR(jp_pair))
          {
            REGISTER_JOINT_PAIR(jp_pair);

            if (!internal::helper::exists(parent_neighbours, neighbour_j))
            {
              parent_neighbours.push_back(neighbour_j);
              neighbour_j_neighbours.push_back(parent_id);
            }
          }
        }

        // Remove joint_id from the list of neighbours for neighbour_j_neighbours
        internal::helper::erase(neighbour_j_neighbours, joint_id, internal::helper::erase_first);

        for (size_t k = j + 1; k < joint_neighbours.size(); ++k)
        {
          const JointIndex neighbour_k = joint_neighbours[k];
          auto & neighbour_k_neighbours = neighbours[neighbour_k];
          assert(neighbour_k != neighbour_j && "Must never happen!");
          const JointPair cross_coupling_pair = neighbour_j < neighbour_k
                                                  ? JointPair{neighbour_j, neighbour_k}
                                                  : JointPair{neighbour_k, neighbour_j};

          if (!EXIST_JOINT_PAIR(cross_coupling_pair))
          {
            REGISTER_JOINT_PAIR(cross_coupling_pair);

            neighbour_j_neighbours.push_back(neighbour_k);
            neighbour_k_neighbours.push_back(neighbour_j);
          }
        }
#undef EXIST_JOINT_PAIR
#undef REGISTER_JOINT_PAIR
      }
    } // while (leaf_vertices.size() > 0)

    // We need to reorder neighbours according to elimanation order.
    // This enables to properly allocate cross coupling terms
    {
      std::vector<size_t> sort_ordering(model.joints.size(), model.joints.size());
      size_t val = 0;
      for (const JointIndex joint_i : joint_elimination_order)
      {
        sort_ordering[joint_i] = val++;
      }

      auto sort_function = [&](const JointIndex & joint1_id, const JointIndex & joint2_id) {
        const auto & val1 = sort_ordering[joint1_id];
        const auto & val2 = sort_ordering[joint2_id];

        return !(val1 < val2);
      };

      for (auto & joint_neighbours : neighbours)
      {
        std::sort(joint_neighbours.begin(), joint_neighbours.end(), sort_function);
      }
    }
    // Allocate memory for coupling terms
    auto & joint_cross_coupling = data.joint_cross_coupling;
    joint_cross_coupling.clear();

    auto & projected_joint_cross_coupling = data.projected_joint_cross_coupling;
    projected_joint_cross_coupling.clear();

#define INSERT_JOINT_INERTIA_COUPLING_TERM(pair)                                                   \
  assert(!joint_cross_coupling.exists(pair));                                                      \
  joint_cross_coupling.insert(pair, Matrix6::Zero());

    for (const JointIndex joint_i : joint_elimination_order)
    {
      const auto & joint_neighbours = neighbours[joint_i];

      if (joint_neighbours.size() == 0)
        continue;

      const auto joint_nv = model.nvs[joint_i];
      for (size_t j = 0; j < joint_neighbours.size(); j++)
      {
        const auto joint_j = joint_neighbours[j];
        const auto pair_ji = JointPair(joint_j, joint_i);
        INSERT_JOINT_INERTIA_COUPLING_TERM(pair_ji);

        assert(!projected_joint_cross_coupling.exists(pair_ji));
        projected_joint_cross_coupling.insert(pair_ji, 6, joint_nv);
      }
    }
#undef INSERT_JOINT_INERTIA_COUPLING_TERM
  }

} // namespace pinocchio
