//
// Copyright (c) 2022-2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/solvers/pgs-solver.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/solvers/pgs-solver.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  // -----------------------------------
  // Visitor structure
  // -----------------------------------

  // Implementation of step given a set: Default to Non
  template<typename ConstraintSet>
  struct PGSConstraintProjectionStepOnSet
  {
  };

  // Implementation of step given a constraint, default to using the set of the constraint
  template<typename ConstraintModel, class = void>
  struct PGSConstraintProjectionStepImpl
  {
    template<typename Scalar, typename BlockType, typename ImpulseType, typename VelocityType>
    static void run(
      const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
      const typename ConstraintModel::ConstraintData & cdata,
      const Scalar over_relax_value,
      const Eigen::EigenBase<BlockType> & G_block,
      ImpulseType & impulse,
      VelocityType & velocity,
      Scalar & complementarity,
      Scalar & primal_feasibility,
      Scalar & dual_feasibility)
    {
      typedef typename ConstraintModel::ConstraintSet ConstraintSet;

      auto set = cmodel.set(cdata);
      PGSConstraintProjectionStepOnSet<ConstraintSet> step(over_relax_value, set);
      step.project(G_block.derived(), impulse.const_cast_derived(), velocity.const_cast_derived());
      step.computeFeasibility(impulse, velocity);

      complementarity = step.complementarity;
      dual_feasibility = step.dual_feasibility;
      primal_feasibility = step.primal_feasibility;
    }
  }; // PGSConstraintProjectionStepImpl

  // Storage Base class for PGSConstraintProjectionStepVisitor
  template<typename _Scalar>
  struct PGSConstraintProjectionStepBase
  {
    typedef _Scalar Scalar;

    explicit PGSConstraintProjectionStepBase(const Scalar over_relax_value)
    : over_relax_value(over_relax_value)
    {
    }

    const Scalar over_relax_value;
    Scalar complementarity;
    Scalar dual_feasibility;
    Scalar primal_feasibility;
  }; // PGSConstraintProjectionBase

  // The visitor
  template<typename Scalar, typename BlockType, typename ImpulseType, typename VelocityType>
  struct PGSConstraintProjectionStepVisitor
  : visitors::ConstraintUnaryVisitorBase<
      PGSConstraintProjectionStepVisitor<Scalar, BlockType, ImpulseType, VelocityType>>
  , PGSConstraintProjectionStepBase<Scalar>
  {
    typedef boost::fusion::vector<
      const Scalar,
      const BlockType &,
      ImpulseType &,
      VelocityType &,
      Scalar &,
      Scalar &,
      Scalar &>
      ArgsType;

    typedef PGSConstraintProjectionStepBase<Scalar> Base;
    typedef visitors::ConstraintUnaryVisitorBase<
      PGSConstraintProjectionStepVisitor<Scalar, BlockType, ImpulseType, VelocityType>>
      VisitorBase;

    explicit PGSConstraintProjectionStepVisitor(const Scalar over_relax_value)
    : Base(over_relax_value)
    {
    }

    template<typename ConstraintModel>
    static void algo(
      const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
      const typename ConstraintModel::ConstraintData & cdata,
      const Scalar over_relax_value,
      const Eigen::EigenBase<BlockType> & G_block,
      ImpulseType & impulse,
      VelocityType & velocity,
      Scalar & complementarity,
      Scalar & primal_feasibility,
      Scalar & dual_feasibility)
    {
      typedef PGSConstraintProjectionStepImpl<ConstraintModel> Impl;
      Impl::run(
        cmodel.derived(), cdata.derived(), over_relax_value, G_block.derived(), impulse, velocity,
        complementarity, primal_feasibility, dual_feasibility);
    }

    using VisitorBase::run;
    template<typename ConstraintModel>
    void run(
      const pinocchio::ConstraintModelBase<ConstraintModel> & cmodel,
      const typename ConstraintModel::ConstraintData & cdata,
      const Eigen::EigenBase<BlockType> & G_block,
      ImpulseType & impulse,
      VelocityType & velocity)
    {
      algo(
        cmodel.derived(), cdata.derived(), this->over_relax_value, G_block.derived(), impulse,
        velocity, this->complementarity, this->primal_feasibility, this->dual_feasibility);
    }

    template<int Options, template<typename S, int O> class ConstraintCollectionTpl>
    void run(
      const pinocchio::ConstraintModelTpl<Scalar, Options, ConstraintCollectionTpl> & cmodel,
      const pinocchio::ConstraintDataTpl<Scalar, Options, ConstraintCollectionTpl> & cdata,
      const Eigen::EigenBase<BlockType> & G_block,
      ImpulseType & impulse,
      VelocityType & velocity)
    {
      ArgsType args(
        this->over_relax_value, G_block.derived(), impulse, velocity, this->complementarity,
        this->primal_feasibility, this->dual_feasibility);
      this->run(cmodel.derived(), cdata.derived(), args);
    }
  }; // PGSConstraintProjectionStepVisitor

  // -----------------------------------
  // Spcialization of the Impl for sets
  // -----------------------------------
  template<typename _Scalar>
  struct PGSConstraintProjectionStepOnSet<CoulombFrictionConeTpl<_Scalar>>
  : PGSConstraintProjectionStepBase<_Scalar>
  {
    typedef _Scalar Scalar;
    typedef CoulombFrictionConeTpl<Scalar> ConstraintSet;
    typedef Eigen::Matrix<Scalar, 3, 1> Vector3;
    typedef PGSConstraintProjectionStepBase<Scalar> Base;

    PGSConstraintProjectionStepOnSet(const Scalar over_relax_value, const ConstraintSet & set)
    : Base(over_relax_value)
    , set(set)
    {
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::EigenBase<BlockType> & G_block_,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_) const
    {
      typedef Eigen::Matrix<Scalar, 2, 1> Vector2;

      auto & G_block = G_block_.derived();
      auto & primal_vector = primal_vector_.const_cast_derived();
      auto & dual_vector = dual_vector_.const_cast_derived();

      // Normal update
      Scalar & fz = primal_vector.coeffRef(2);
      const Scalar fz_previous = fz;
      fz -= Scalar(
              this->over_relax_value
              / math::max(Eigen::NumTraits<Scalar>::dummy_precision(), G_block.coeff(2, 2)))
            * dual_vector[2];
      fz = math::max(Scalar(0), fz);

      // Account for the fz updated value
      dual_vector += G_block.col(2) * (fz - fz_previous);

      // Tangential update
      const Scalar min_D_tangent = math::max(
        Eigen::NumTraits<Scalar>::dummy_precision(),
        math::min(G_block.coeff(0, 0), G_block.coeff(1, 1)));
      auto f_tangent = primal_vector.template head<2>();
      const Vector2 f_tangent_previous = f_tangent;

      f_tangent -= this->over_relax_value / min_D_tangent * dual_vector.template head<2>();
      const Scalar f_tangent_norm = f_tangent.norm();

      const Scalar mu_fz = this->set.mu * fz;
      if (f_tangent_norm > mu_fz) // Project in the circle of radius mu_fz
      {
        assert(f_tangent_norm > 0 && "f_tangent_norm is zero");
        f_tangent *= mu_fz / f_tangent_norm;
      }

      // Account for the f_tangent updated value
      dual_vector.noalias() += G_block.template leftCols<2>() * (f_tangent - f_tangent_previous);
    }

    /// \brief Compute the feasibility conditions associated with the optimization problem
    template<typename PrimalVectorType, typename DualVectorType>
    void computeFeasibility(
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector)
    {
      // The name should be inverted.
      this->primal_feasibility =
        Scalar(0); // always zero as the primal variable belongs to the friction cone.

      typedef Eigen::Matrix<Scalar, 3, 1> Vector3;
      const Vector3 dual_vector_corrected =
        dual_vector + this->set.computeNormalCorrection(dual_vector);
      this->complementarity =
        this->set.computeConicComplementarity(dual_vector_corrected, primal_vector);
      assert(this->complementarity >= Scalar(0) && "The complementarity should be positive");
      const Vector3 reprojection_residual =
        this->set.dual().project(dual_vector_corrected) - dual_vector_corrected;
      this->dual_feasibility = reprojection_residual.norm();
    }

    const ConstraintSet & set;

  }; // PGSConstraintProjectionStepOnSet<CoulombFrictionConeTpl<_Scalar>>

  template<typename _Scalar, int _Options>
  struct PGSConstraintProjectionStepOnSet<FullSpaceConeTpl<_Scalar, _Options>>
  : PGSConstraintProjectionStepBase<_Scalar>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef FullSpaceConeTpl<Scalar, Options> ConstraintSet;
    typedef PGSConstraintProjectionStepBase<Scalar> Base;

    PGSConstraintProjectionStepOnSet(const Scalar over_relax_value, const ConstraintSet & set)
    : Base(over_relax_value)
    , set(set)
    {
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::MatrixBase<BlockType> & G_block_,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_) const
    {

      const Eigen::Index size = primal_vector_.size();
      assert(G_block_.rows() == size);
      assert(dual_vector_.size() == size);

      auto & G_block = G_block_.derived();
      auto & primal_vector = primal_vector_.const_cast_derived();
      auto & dual_vector = dual_vector_.const_cast_derived();

      for (Eigen::Index i = 0; i < size; ++i)
      {
        Scalar d_primal_value =
          -this->over_relax_value * dual_vector[i]
          / math::max(Eigen::NumTraits<Scalar>::dummy_precision(), G_block.coeff(i, i));
        primal_vector[i] += d_primal_value;
        dual_vector.noalias() += G_block.col(i) * d_primal_value; // TODO: this could be optimized
      }
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::EigenBase<BlockType> & G_block_,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_) const
    {

      const Eigen::Index size = primal_vector_.size();
      assert(G_block_.rows() == size);
      assert(dual_vector_.size() == size);

      auto & G_block = G_block_.derived();
      auto & primal_vector = primal_vector_.const_cast_derived();
      auto & dual_vector = dual_vector_.const_cast_derived();

      for (Eigen::Index i = 0; i < size; ++i)
      {
        Scalar d_primal_value =
          -this->over_relax_value * dual_vector[i]
          / math::max(Eigen::NumTraits<Scalar>::dummy_precision(), G_block.coeff(i, i));
        primal_vector[i] += d_primal_value;
        dual_vector += G_block.col(i) * d_primal_value; // TODO: this could be optimized using aloca
      }
    }

    /// \brief Compute the feasibility conditions associated with the optimization problem
    template<typename PrimalVectorType, typename DualVectorType>
    void computeFeasibility(
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector)
    {
      this->primal_feasibility = Scalar(0);
      this->complementarity = primal_vector.dot(dual_vector);
      this->dual_feasibility = dual_vector.template lpNorm<Eigen::Infinity>();
    }

    const ConstraintSet & set;

  }; // PGSConstraintProjectionStepOnSet<FullSpaceConeTpl<_Scalar,_Options>>

  template<typename _Scalar>
  struct PGSConstraintProjectionStepOnSet<BoxSetTpl<_Scalar>>
  : PGSConstraintProjectionStepBase<_Scalar>
  {
    typedef _Scalar Scalar;
    typedef BoxSetTpl<Scalar> ConstraintSet;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Vector;
    typedef PGSConstraintProjectionStepBase<Scalar> Base;

    PGSConstraintProjectionStepOnSet(const Scalar over_relax_value, const ConstraintSet & set)
    : Base(over_relax_value)
    , set(set)
    {
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::MatrixBase<BlockType> & G_block,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector) const
    {
      project_impl(
        this->set, this->over_relax_value, G_block.derived(), primal_vector.const_cast_derived(),
        dual_vector.const_cast_derived());
    }

    template<
      typename ConstraintSetType,
      typename BlockType,
      typename PrimalVectorType,
      typename DualVectorType>
    static void project_impl(
      const ConstraintSetType & set,
      const Scalar over_relax_value,
      const Eigen::MatrixBase<BlockType> & G_block_,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_)
    {
      const Eigen::Index size = primal_vector_.size();
      assert(G_block_.rows() == size);
      assert(dual_vector_.size() == size);

      const auto & G_block = G_block_.derived();
      auto & primal_vector = primal_vector_.const_cast_derived();
      auto & dual_vector = dual_vector_.const_cast_derived();

      for (Eigen::Index row_id = 0; row_id < size; ++row_id)
      {
        Scalar & value = primal_vector.coeffRef(row_id);
        const Scalar value_previous = value;
        value -=
          Scalar(
            over_relax_value
            / math::max(Eigen::NumTraits<Scalar>::dummy_precision(), G_block.coeff(row_id, row_id)))
          * dual_vector[row_id];
        value = set.rowiseProject(row_id, value);
        dual_vector.noalias() +=
          G_block.col(row_id)
          * Scalar(value - value_previous); // TODO optimize: we only need dual_vector[row_id] for
                                            // the update and not the full dual vector
      }
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::EigenBase<BlockType> & G_block,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector) const
    {
      project_impl(
        this->set, this->over_relax_value, G_block.derived(), primal_vector.const_cast_derived(),
        dual_vector.const_cast_derived());
    }

    template<
      typename ConstraintSetType,
      typename BlockType,
      typename PrimalVectorType,
      typename DualVectorType>
    static void project_impl(
      const ConstraintSetType & set,
      const Scalar over_relax_value,
      const Eigen::EigenBase<BlockType> & G_block_, // for Sparse matrices
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_)
    {
      const Eigen::Index size = primal_vector_.size();
      assert(G_block_.rows() == size);
      assert(dual_vector_.size() == size);

      const auto & G_block = G_block_.derived();
      auto & primal_vector = primal_vector_.const_cast_derived();
      auto & dual_vector = dual_vector_.const_cast_derived();

      for (Eigen::Index row_id = 0; row_id < size; ++row_id)
      {
        Scalar & value = primal_vector.coeffRef(row_id);
        const Scalar value_previous = value;
        value -=
          Scalar(
            over_relax_value
            / math::max(Eigen::NumTraits<Scalar>::dummy_precision(), G_block.coeff(row_id, row_id)))
          * dual_vector[row_id];
        value = set.rowiseProject(row_id, value);
        dual_vector += G_block.col(row_id) * Scalar(value - value_previous);
      }
    }

    /// \brief Compute the feasibility conditions associated with the optimization problem
    template<typename PrimalVectorType, typename DualVectorType>
    void computeFeasibility(
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector)
    {
      this->primal_feasibility =
        Scalar(0); // always zero as the primal variable belongs to the constraint set.
      this->dual_feasibility =
        Scalar(0); // always zero as the dual variable belongs to the constraint set.

      const Eigen::Index size = primal_vector.size();
      assert(dual_vector.size() == size);
      Scalar complementarity = Scalar(0);

      const auto & lb = set.lb;
      const auto & ub = set.ub;
      for (Eigen::Index row_id = 0; row_id < size; ++row_id)
      {
        const Scalar dual_positive_part = math::max(Scalar(0), dual_vector[row_id]);
        const Scalar dual_negative_part = dual_positive_part - dual_vector[row_id];

        Scalar row_complementarity = dual_positive_part * (primal_vector[row_id] - lb[row_id]);
        row_complementarity =
          math::max(row_complementarity, dual_negative_part * (ub[row_id] - primal_vector[row_id]));
        complementarity = math::max(complementarity, row_complementarity);
      }
      this->complementarity = complementarity;
    }

    const ConstraintSet & set;

  }; // PGSConstraintProjectionStepOnSet<BoxSetTpl<_Scalar>>

  template<typename _Scalar>
  struct PGSConstraintProjectionStepOnSet<NonNegativeOrthantConeTpl<_Scalar>>
  : PGSConstraintProjectionStepBase<_Scalar>
  {
    typedef _Scalar Scalar;
    typedef NonNegativeOrthantConeTpl<Scalar> ConstraintSet;
    typedef BoxSetTpl<Scalar> BoxSet;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Vector;
    typedef PGSConstraintProjectionStepBase<Scalar> Base;

    PGSConstraintProjectionStepOnSet(const Scalar over_relax_value, const ConstraintSet & set)
    : Base(over_relax_value)
    , set(set)
    {
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::MatrixBase<BlockType> & G_block_,
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_) const
    {
      PGSConstraintProjectionStepOnSet<BoxSet>::project_impl(
        this->set, this->over_relax_value, G_block_.derived(), primal_vector_.const_cast_derived(),
        dual_vector_.const_cast_derived());
    }

    ///
    /// \brief Perform a projection step associated with the PGS algorithm
    ///
    /// \param[in] G_block block asscociated with the current
    /// \param[in,out] primal_vector_ primal vector which will be update with the new estimate
    /// \param[in,out] dual_vector_ dual vector which will be update with the new estimate
    ///
    template<typename BlockType, typename PrimalVectorType, typename DualVectorType>
    void project(
      const Eigen::EigenBase<BlockType> & G_block_, // for Sparse matrices
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector_,
      const Eigen::MatrixBase<DualVectorType> & dual_vector_) const
    {
      PGSConstraintProjectionStepOnSet<BoxSet>::project_impl(
        this->set, this->over_relax_value, G_block_.derived(), primal_vector_.const_cast_derived(),
        dual_vector_.const_cast_derived());
    }

    /// \brief Compute the feasibility conditions associated with the optimization problem
    template<typename PrimalVectorType, typename DualVectorType>
    void computeFeasibility(
      const Eigen::MatrixBase<PrimalVectorType> & primal_vector,
      const Eigen::MatrixBase<DualVectorType> & dual_vector)
    {
      this->primal_feasibility =
        Scalar(0); // always zero as the primal variable belongs to the constraint set.

      const Eigen::Index size = primal_vector.size();
      assert(dual_vector.size() == size);
      Scalar complementarity = Scalar(0);
      Scalar dual_feasibility = Scalar(0);

      for (Eigen::Index row_id = 0; row_id < size; ++row_id)
      {
        const Scalar row_complementarity =
          math::fabs(Scalar(primal_vector[row_id] * dual_vector[row_id]));
        complementarity = math::max(complementarity, row_complementarity);

        const Scalar row_dual_feasibility =
          math::fabs(dual_vector[row_id] - set.dual().rowiseProject(row_id, dual_vector[row_id]));
        dual_feasibility = math::max(dual_feasibility, row_dual_feasibility);
      }
      this->complementarity = complementarity;
      this->dual_feasibility = dual_feasibility;
    }

    const ConstraintSet & set;

  }; // PGSConstraintProjectionStepOnSet<NonNegativeOrthantConeTpl<_Scalar>>

  // -----------------------------------
  // Algorithms implementation
  // -----------------------------------

  template<typename _Scalar, int _Options>
  template<
    typename DelassusDerived,
    typename VectorLike,
    typename ConstraintModel,
    typename ConstraintModelAllocator,
    typename ConstraintData,
    typename ConstraintDataAllocator>
  bool PGSConstraintSolverTpl<_Scalar, _Options>::solveImpl(
    DelassusOperatorBase<DelassusDerived> & delassus,
    const Eigen::MatrixBase<VectorLike> & g,
    const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
    const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
    const PGSSolverSettings & settings,
    PGSSolverResult & result)
  {
    // for easier access
    PGSSolverResult & res = result;
    PGSSolverWorkspace & ws = m_workspace;

    // Configure/reset workspace, stats and results.
    // note: the order matters as workspace is initialized using
    // optional warmstarts contained in results.
    const Eigen::Index np = g.size();
    const std::size_t problem_size = static_cast<std::size_t>(np);
    assert(delassus.cols() == np);
    assert(delassus.rows() == np);
    assert(residualSize(constraint_models) == np);

    // -- check if settings are valid
    settings.checkValidity();

    // -- reset workspace
    ws.resize(problem_size);
    ws.reset();
    assert(ws.delassus_matrix.cols() == np);
    assert(ws.delassus_matrix.rows() == np);
    assert(ws.problem_size == problem_size);
    assert(ws.x.size() == np);
    delassus.derived().matrix(ws.delassus_matrix, true /*enforce symmetry*/, true /*with damping*/);
    const auto & G = ws.delassus_matrix;

    // -- reset per-iteration statistics
    stats.reset();
    if (settings.stat_record)
    {
      stats.reserve(settings.max_iterations);
    }

    // -- retrieve warmstart from results, then reset results
    bool has_impulse_guess = res.impulse_guess.has_value();
    PINOCCHIO_THROW_PRETTY_IF(
      has_impulse_guess && (res.impulse_guess.value().size() != ws.x.size()), std::runtime_error,
      "Impulse guess given to PGS is of incorrect size.");
    if (has_impulse_guess)
    {
      if (res.impulse_guess.value().size() != ws.x.size())
      {
        has_impulse_guess = false;
      }
    }

    if (has_impulse_guess)
    {
      ws.x = res.impulse_guess.value();
    }
    else
    {
      ws.x.setZero();
    }
    PINOCCHIO_CHECK_ARGUMENT_SIZE(ws.x.size(), np);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(ws.y.size(), np);
    res.resize(problem_size);
    res.reset();
    assert(res.isValid() == false);
    assert(res.problem_size == problem_size);
    assert(res.iterations == 0);

    // the solver can now be marked as reset
    m_is_valid = false;

    if (settings.measure_timings)
    {
      timerStart();
    }

    bool abs_prec_reached = false;
    bool rel_prec_reached = false;
    Scalar x_previous_norm_inf = ws.x.template lpNorm<Eigen::Infinity>();
    const std::size_t num_constraints = constraint_models.size();

    res.iterations = 0;
    for (; res.iterations <= settings.max_iterations; ++res.iterations)
    {
      ws.x_previous = ws.x;

      res.complementarity = Scalar(0);
      res.dual_feasibility = Scalar(0);
      res.primal_feasibility = Scalar(0);

      // PGS step for each constraint
      Eigen::Index row_id = 0;
      for (size_t constraint_id = 0; constraint_id < num_constraints; ++constraint_id)
      {
        const auto & cmodel = internal::helper::get_ref(constraint_models[constraint_id]);
        const auto & cdata = internal::helper::get_ref(constraint_datas[constraint_id]);
        const Eigen::Index constraint_size = cmodel.residualSize();

        auto G_block = G.block(row_id, row_id, constraint_size, constraint_size);
        auto impulse = ws.x.segment(row_id, constraint_size);
        auto velocity = ws.y.segment(row_id, constraint_size);

        // Update dual variable
        velocity.noalias() = G.middleRows(row_id, constraint_size) * ws.x;
        velocity += g.segment(row_id, constraint_size);

        typedef PGSConstraintProjectionStepVisitor<
          Scalar, decltype(G_block), decltype(impulse), decltype(velocity)>
          Step;
        Step step(settings.over_relaxation);
        step.run(cmodel, cdata, G_block, impulse, velocity);

        res.complementarity = math::max(res.complementarity, step.complementarity);
        res.dual_feasibility = math::max(res.dual_feasibility, step.dual_feasibility);
        res.primal_feasibility = math::max(res.primal_feasibility, step.primal_feasibility);

        row_id += constraint_size;
      }

      // Checking stopping criterion
      // -- absolute
      if (
        check_expression_if_real<Scalar, false>(
          res.primal_feasibility <= settings.absolute_feasibility_tol)
        && check_expression_if_real<Scalar, false>(
          res.dual_feasibility <= settings.absolute_feasibility_tol)
        && check_expression_if_real<Scalar, false>(
          res.complementarity <= settings.absolute_complementarity_tol))
      {
        abs_prec_reached = true;
      }
      else
      {
        abs_prec_reached = false;
      }

      // -- relative
      const Scalar proximal_metric = (ws.x - ws.x_previous).template lpNorm<Eigen::Infinity>();
      const Scalar x_norm_inf = ws.x.template lpNorm<Eigen::Infinity>();
      if (
        check_expression_if_real<Scalar, false>(
          proximal_metric
          <= settings.relative_feasibility_tol * math::max(x_norm_inf, x_previous_norm_inf)))
      {
        rel_prec_reached = true;
      }
      else
      {
        rel_prec_reached = false;
      }

      // Record stats
      if (settings.stat_record)
      {
        ws.tmp.noalias() = G * ws.x;
        ws.tmp += g;
        if (settings.solve_ncp)
        {
          internal::computeDeSaxeCorrection(constraint_models, constraint_datas, ws.tmp, ws.rhs);
          ws.tmp += ws.rhs;
        }
        ws.rhs = ws.tmp;
        internal::computeDualConstraintSetProjection(
          constraint_models, constraint_datas, ws.rhs, ws.rhs);
        ws.tmp -= ws.rhs;
        const Scalar dual_feasibility_ncp = ws.tmp.template lpNorm<Eigen::Infinity>();

        stats.primal_feasibility.push_back(res.primal_feasibility);
        stats.dual_feasibility.push_back(res.dual_feasibility);
        stats.dual_feasibility_ncp.push_back(dual_feasibility_ncp);
        stats.complementarity.push_back(res.complementarity);
      }

      if (abs_prec_reached || rel_prec_reached)
        break;

      x_previous_norm_inf = x_norm_inf;
    }

    if (settings.measure_timings)
    {
      timerStop();
    }

    // Retrieve solution
    res.x = ws.x;
    res.y = ws.y;
    res.converged = abs_prec_reached || rel_prec_reached;
    res.unsafe().makeValid();

    // the solver has run, we mark it as valid
    m_is_valid = true;

    return res.converged;
  }

} // namespace pinocchio

#ifdef PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION

  #ifndef PINOCCHIO_SKIP_ALGORITHM_SOLVERS

namespace pinocchio
{

  // -------------------------------------------------------------------------
  // Struct instantiations
  // -------------------------------------------------------------------------

  extern template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    PGSSolverSettingsTpl<context::Scalar>;

  extern template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    PGSSolverStatsTpl<context::Scalar>;

  extern template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    PGSSolverResultTpl<context::Scalar, context::Options>;

  namespace internal
  {
    extern template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
      PGSSolverWorkspaceTpl<context::Scalar, context::Options>;
  } // namespace internal

  extern template struct PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI
    PGSConstraintSolverTpl<context::Scalar, context::Options>;

  // -------------------------------------------------------------------------
  // solveImpl() with DelassusOperatorCholeskyExpression + default constraint collection
  // -------------------------------------------------------------------------

  extern template PINOCCHIO_EXPLICIT_INSTANTIATION_DECLARATION_DLLAPI bool
  PGSConstraintSolverTpl<context::Scalar, context::Options>::solveImpl<
    DelassusOperatorCholeskyExpressionTpl<
      ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>>,
    context::VectorXs,
    ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
    std::allocator<
      ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>,
    ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
    std::allocator<
      ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>>(
    DelassusOperatorBase<DelassusOperatorCholeskyExpressionTpl<
      ConstraintCholeskyDecompositionTpl<context::Scalar, context::Options>>> &,
    const Eigen::MatrixBase<context::VectorXs> &,
    const std::vector<
      ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
      std::allocator<
        ConstraintModelTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>> &,
    const std::vector<
      ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>,
      std::allocator<
        ConstraintDataTpl<context::Scalar, context::Options, ConstraintCollectionDefaultTpl>>> &,
    const PGSSolverSettingsTpl<context::Scalar> &,
    PGSSolverResultTpl<context::Scalar, context::Options> &);

} // namespace pinocchio

  #endif // ifndef PINOCCHIO_SKIP_ALGORITHM_SOLVERS

#endif // PINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION
