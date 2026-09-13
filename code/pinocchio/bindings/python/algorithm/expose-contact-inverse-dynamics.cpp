//
// Copyright (c) 2024-2025 INRIA
//

#include "pinocchio/bindings/python/algorithm/algorithms.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"
#include "pinocchio/bindings/python/utils/model-checker.hpp"

#include "pinocchio/algorithm/contact-inverse-dynamics.hpp"

#include <boost/optional.hpp>

namespace pinocchio
{
  namespace python
  {

#ifndef PINOCCHIO_PYTHON_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS
    typedef context::Scalar Scalar;
    typedef context::VectorXs VectorXs;
    using context::RefConstVectorXs;
    using context::RefVectorXs;

    static bp::tuple computeInverseDynamicsConstraintForces_wrapper(
      const VectorXs & c_ref,
      const context::PointContactConstraintModelVector & contact_models,
      const context::PointContactConstraintDataVector & contact_datas,
      const boost::optional<VectorXs> & lambda_guess,
      ProximalSettingsTpl<Scalar> & settings,
      bool solve_ncp)
    {
      const Eigen::Index problem_size = getTotalConstraintResidualSize(contact_models);
      VectorXs lambda_sol = lambda_guess ? lambda_guess.get() : VectorXs::Zero(problem_size);

      const bool has_converged = computeInverseDynamicsConstraintForces(
        contact_models, contact_datas, c_ref, lambda_sol, settings, solve_ncp);
      return bp::make_tuple(has_converged, bp::object(lambda_sol));
    }

    static bp::tuple contactInverseDynamics_wrapper(
      const context::Model & model,
      context::Data & data,
      RefConstVectorXs q,
      RefConstVectorXs v,
      RefConstVectorXs a,
      Scalar dt,
      const context::PointContactConstraintModelVector & contact_models,
      context::PointContactConstraintDataVector & contact_datas,
      RefVectorXs constraint_correction,
      ProximalSettingsTpl<Scalar> & settings,
      const boost::optional<VectorXs> & lambda_guess = boost::none,
      bool solve_ncp = true)
    {
      const Eigen::Index problem_size = getTotalConstraintResidualSize(contact_models);
      VectorXs lambda_sol = lambda_guess ? lambda_guess.get() : VectorXs::Zero(problem_size);

      const bool has_converged = contactInverseDynamics(
        model, data, q, v, a, dt, contact_models, contact_datas, constraint_correction, lambda_sol,
        settings, solve_ncp);

      return bp::make_tuple(has_converged, data.tau, lambda_sol);
    }
#endif // PINOCCHIO_PYTHON_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS

    void exposeContactInverseDynamics()
    {
#ifndef PINOCCHIO_PYTHON_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS
      bp::def(
        "computeInverseDynamicsConstraintForces", computeInverseDynamicsConstraintForces_wrapper,
        (bp::args("c_ref", "contact_models", "contact_datas"),
         bp::arg("lambda_guess") = boost::none, bp::arg("settings"), bp::arg("solve_ncp") = true),
        "Computes the inverse dynamics with point contacts. Returns a tuple containing "
        "(has_converged, lambda_sol).\n\n"
        "Parameters:\n"
        "\tcontact_models: list of contact models\n"
        "\tc_ref: the reference velocity of contact points\n"
        "\tlambda_guess: optional initial guess for contact forces\n"
        "\tsettings: the settings of the proximal algorithm\n"
        "\tsolve_ncp: whether to solve the NCP (true) or CCP (false)\n",
        mimic_not_supported_function<>(0));

      bp::def(
        "contactInverseDynamics", contactInverseDynamics_wrapper,
        (bp::args(
           "model", "data", "q", "v", "a", "dt", "contact_models", "contact_datas",
           "constraint_correction", "settings"),
         bp::arg("lambda_guess") = boost::none, bp::arg("solve_ncp") = true),
        "Compute the inverse dynamics with point contacts, store the result in Data and "
        "return it.\n\n"
        "Parameters:\n"
        "\tmodel: model of the kinematic tree\n"
        "\tdata: data related to the model\n"
        "\tq: the joint configuration vector (size model.nq)\n"
        "\tv: the joint velocity vector (size model.nv)\n"
        "\ta: the joint acceleration vector (size model.nv)\n"
        "\tdt: the time step\n"
        "\tcontact_models: list of contact models\n"
        "\tcontact_datas: list of contact datas\n"
        "\tconstraint_correction: vector representing the constraint correction\n"
        "\tsettings: the settings of the proximal algorithm\n"
        "\tlambda_guess: initial guess for contact forces\n"
        "\tsolve_ncp: whether to solve the NCP (true) or CCP (false)\n",
        mimic_not_supported_function<>(0));
#endif // PINOCCHIO_PYTHON_SKIP_ALGORITHM_CONSTRAINED_DYNAMICS
    }
  } // namespace python
} // namespace pinocchio
