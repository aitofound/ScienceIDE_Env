//
// Copyright (c) 2024-2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/contact-inverse-dynamics.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/contact-inverse-dynamics.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  template<
    typename Scalar,
    class PointContactConstraintModelVector,
    class PointContactConstraintDataVector,
    typename VectorLikeC,
    typename VectorLikeResult>
  bool computeInverseDynamicsConstraintForces(
    const PointContactConstraintModelVector & constraint_models,
    const PointContactConstraintDataVector & constraint_datas,
    const Eigen::MatrixBase<VectorLikeC> & c_ref,
    const Eigen::MatrixBase<VectorLikeResult> & _lambda,
    ProximalSettingsTpl<Scalar> & settings,
    bool solve_ncp)
  {
    static_assert(
      internal::helper::is_std_vector_v<PointContactConstraintModelVector>,
      "PointContactConstraintModelVector should be a std::vector<T,Allocator>");
    static_assert(
      internal::helper::is_std_vector_v<PointContactConstraintDataVector>,
      "PointContactConstraintDataVector should be a std::vector<T,Allocator>");

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> VectorXs;
    typedef Eigen::Matrix<Scalar, 3, 1> Vector3;

    const Eigen::Index problem_size = getTotalConstraintResidualSize(constraint_models);
    const std::size_t n_constraints = constraint_models.size();
    VectorXs R(problem_size);
    Eigen::Index constraint_index = 0;
    for (std::size_t i = 0; i < constraint_models.size(); i++)
    {
      const auto & cmodel = internal::helper::get_ref(constraint_models[i]);
      const auto csize = cmodel.residualSize();
      cmodel.retrieveCompliance(R.segment(constraint_index, csize));
      constraint_index += csize;
    }
    const VectorXs R_prox = R + VectorXs::Constant(problem_size, settings.mu);

    auto & lambda = _lambda.const_cast_derived();

    // PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_correction.size(), problem_size);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(constraint_models.size(), n_constraints);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(lambda.size(), problem_size);
    PINOCCHIO_CHECK_INPUT_ARGUMENT(
      check_expression_if_real<Scalar>(settings.mu >= Scalar(0)), "mu has to be strictly positive");

    assert(
      R.size() > 0 && check_expression_if_real<Scalar>(R_prox.minCoeff() >= Scalar(0))
      && "The minimal value of R_prox should strictly positive");

    Scalar lambda_c_prev_norm_inf = lambda.template lpNorm<Eigen::Infinity>();

    bool has_converged = false;
    settings.iter = 1;
    for (; settings.iter <= settings.max_iter; ++settings.iter)
    {
      bool abs_prec_reached = false, rel_prec_reached = false;
      settings.relative_residual = settings.absolute_residual = Scalar(0);

      Eigen::Index row_id = 0;
      for (std::size_t constraint_id = 0; constraint_id < n_constraints; ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const auto constraint_size = cmodel.residualSize();

        const auto cone = cmodel.set(cdata);
        auto lambda_segment = lambda.segment(row_id, constraint_size);
        const Vector3 lambda_c_previous = lambda_segment;

        const auto R_segment = R.segment(row_id, constraint_size);
        const auto R_prox_segment = R_prox.segment(row_id, constraint_size);
        const auto c_ref_segment = c_ref.segment(row_id, constraint_size);

        const Vector3 sigma_segment =
          c_ref_segment + (R_segment.array() * lambda_segment.array()).matrix();
        Vector3 desaxce_correction = Vector3::Zero();
        if (solve_ncp)
          desaxce_correction = cone.computeNormalCorrection(sigma_segment);
        const Vector3 c_cor_segment = c_ref_segment + desaxce_correction;

        // Update segment value
        const Vector3 lambda_ref =
          -(Vector3(c_cor_segment - settings.mu * lambda_c_previous).array()
            / R_prox_segment.array());
        lambda_segment = cone.weightedProject(lambda_ref, R_prox_segment);

        // Compute convergence criteria
        const Scalar contact_complementarity = cone.computeConicComplementarity(
          Vector3(sigma_segment + desaxce_correction), lambda_segment);
        const Scalar dual_feasibility =
          math::fabs(math::min(Scalar(0.), sigma_segment(2))); // proxy of dual feasibility
        settings.absolute_residual = math::max(
          settings.absolute_residual, math::max(contact_complementarity, dual_feasibility));

        const Vector3 dlambda_c = lambda_segment - lambda_c_previous;
        const Scalar proximal_metric = dlambda_c.template lpNorm<Eigen::Infinity>();
        settings.relative_residual = math::max(settings.relative_residual, proximal_metric);

        row_id += constraint_size;
      }

      const Scalar lambda_c_norm_inf = lambda.template lpNorm<Eigen::Infinity>();

      if (
        check_expression_if_real<Scalar, false>(
          settings.absolute_residual <= settings.absolute_accuracy))
        abs_prec_reached = true;

      if (
        check_expression_if_real<Scalar, false>(
          settings.relative_residual
          <= settings.relative_accuracy * math::max(lambda_c_norm_inf, lambda_c_prev_norm_inf)))
        rel_prec_reached = true;

      if (abs_prec_reached || rel_prec_reached)
      {
        has_converged = true;
        break;
      }

      lambda_c_prev_norm_inf = lambda_c_norm_inf;
    }

    return has_converged;
  }

  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    class PointContactConstraintModelVector,
    class PointContactConstraintDataVector,
    typename VectorLikeGamma,
    typename VectorLikeLam>
  bool contactInverseDynamics(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const Scalar dt,
    const PointContactConstraintModelVector & constraint_models,
    PointContactConstraintDataVector & constraint_datas,
    const Eigen::MatrixBase<VectorLikeGamma> & constraint_correction,
    const Eigen::MatrixBase<VectorLikeLam> & _lambda_sol,
    ProximalSettingsTpl<Scalar> & settings,
    bool solve_ncp)
  {
    static_assert(
      internal::helper::is_std_vector_v<PointContactConstraintModelVector>,
      "PointContactConstraintModelVector should be a std::vector<T,Allocator>");
    static_assert(
      internal::helper::is_std_vector_v<PointContactConstraintDataVector>,
      "PointContactConstraintDataVector should be a std::vector<T,Allocator>");

    typedef ModelTpl<Scalar, Options, JointCollectionTpl> Model;
    typedef typename Model::VectorXs VectorXs;

    auto & lambda_sol = _lambda_sol.const_cast_derived();

    const Eigen::Index problem_size = getTotalConstraintResidualSize(constraint_models);
    VectorXs v_ref(model.nv), c_ref(problem_size);
    v_ref = v + dt * a;

    evalConstraintJacobianMatrixProduct(
      model, data, constraint_models, constraint_datas, v_ref, c_ref, SetTo());
    c_ref += constraint_correction;
    c_ref /= dt; // we work with a formulation on forces

    const bool has_converged = computeInverseDynamicsConstraintForces(
      constraint_models, constraint_datas, c_ref, lambda_sol, settings, solve_ncp);

    rnea(model, data, q, v, a);
    auto & tau = data.tau;
    evalConstraintJacobianTransposeMatrixProduct(
      model, data, constraint_models, constraint_datas, lambda_sol, tau, RmTo());

    return has_converged;
  }

} // namespace pinocchio
