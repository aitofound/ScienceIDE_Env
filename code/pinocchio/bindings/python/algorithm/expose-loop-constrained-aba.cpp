//
// Copyright (c) 2024-2025 INRIA
//

#include "pinocchio/bindings/python/algorithm/algorithms.hpp"
#include "pinocchio/bindings/python/utils/model-checker.hpp"

#include "pinocchio/algorithm/loop-constrained-aba.hpp"

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    template<typename ConstraintModel, typename ConstraintData>
    static const context::VectorXs lcaba_proxy(
      const context::Model & model,
      context::Data & data,
      const context::VectorXs & q,
      const context::VectorXs & v,
      const context::VectorXs & tau,
      const std::vector<ConstraintModel> & constraint_models,
      std::vector<ConstraintData> & constraint_datas,
      context::ProximalSettings & prox_settings)
    {
      return lcaba(model, data, q, v, tau, constraint_models, constraint_datas, prox_settings);
    }
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE

    template<typename ConstraintModel>
    void exposeLcabaFor()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      typedef typename ConstraintModel::ConstraintData ConstraintData;
      typedef std::allocator<ConstraintModel> ConstraintModelAllocator;

      bp::def(
        "computeJointMinimalOrdering",
        &computeJointMinimalOrdering<
          context::Scalar, context::Options, JointCollectionDefaultTpl, ConstraintModel,
          ConstraintModelAllocator>,
        bp::args("model", "data", "constraint_models"),
        "Computes the joint minimal ordering for closed-loop constrained dynamics.\n"
        "This function must be called before lcaba.\n"
        "Parameters:\n"
        "\t model: Model of the kinematic tree\n"
        "\t data: Data related to the kinematic tree\n"
        "\t constraint_models: vector of constraint models",
        mimic_not_supported_function<>(0));

      bp::def(
        "lcaba", lcaba_proxy<ConstraintModel, ConstraintData>,
        bp::args(
          "model", "data", "q", "v", "tau", "constraint_models", "constraint_datas",
          "prox_settings"),
        "Computes the forward dynamics with closed-loop contact constraints using the\n"
        "Loop-Constrained Articulated Body Algorithm (LCABA).\n"
        "Before calling lcaba for the first time, you should call computeJointMinimalOrdering\n"
        "to set up the joint elimination order.\n\n"
        "Parameters:\n"
        "\t model: Model of the kinematic tree\n"
        "\t data: Data related to the kinematic tree\n"
        "\t q: joint configuration (size model.nq)\n"
        "\t v: joint velocity (size model.nv)\n"
        "\t tau: joint torque (size model.nv)\n"
        "\t constraint_models: vector of constraint models\n"
        "\t constraint_datas: vector of constraint data\n"
        "\t prox_settings: Proximal settings (mu, accuracy and maximal number of iterations)\n\n"
        "Note: A typical value for mu in proximal settings is 1e-6, and it must be positive.\n"
        "This function returns joint acceleration stored in data.ddq.\n"
        "The constraint forces are stored in data.lambdaA[0].",
        mimic_not_supported_function<>(0));
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    }

    void exposeLcaba()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      exposeLcabaFor<context::RigidConstraintModel>();
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    }

  } // namespace python
} // namespace pinocchio
