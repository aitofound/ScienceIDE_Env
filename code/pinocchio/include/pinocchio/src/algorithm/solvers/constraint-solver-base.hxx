//
// Copyright (c) 2022-2024 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/solvers/constraint-solver-base.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/solvers/constraint-solver-base.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{

  // fwd declaration for below
  template<typename Derived>
  struct ConstraintSolverBase;

  template<typename Derived>
  struct ConstraintSolverResultBase;

  ///
  /// \brief Base struct for settings to pass to the solve method of a constraint solver.
  template<typename Derived>
  struct ConstraintSolverSettingsBase
  {
    typedef typename traits<Derived>::Scalar Scalar;

    /// \brief Cast to Derived.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    /// \brief Const cast to Derived.
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Default constructor
    ConstraintSolverSettingsBase(
      std::size_t max_iterations,
      Scalar absolute_feasibility_tol,
      Scalar relative_feasibility_tol,
      Scalar absolute_complementarity_tol,
      Scalar relative_complementarity_tol,
      bool solve_ncp,
      bool measure_timings,
      bool stat_record)
    : max_iterations(max_iterations)
    , absolute_feasibility_tol(absolute_feasibility_tol)
    , relative_feasibility_tol(relative_feasibility_tol)
    , absolute_complementarity_tol(absolute_complementarity_tol)
    , relative_complementarity_tol(relative_complementarity_tol)
    , solve_ncp(solve_ncp)
    , measure_timings(measure_timings)
    , stat_record(stat_record)
    {
    }

    /// \brief Throws if settings are not valid.
    void checkValidity() const
    {
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        check_expression_if_real<Scalar>(absolute_feasibility_tol >= Scalar(0)),
        "absolute_feasibility_tol should be >= 0.");
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        check_expression_if_real<Scalar>(relative_feasibility_tol >= Scalar(0)),
        "relative_feasibility_tol should be >= 0.");
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        check_expression_if_real<Scalar>(absolute_complementarity_tol >= Scalar(0)),
        "absolute_complementarity_tol should be >= 0.");
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        check_expression_if_real<Scalar>(relative_complementarity_tol >= Scalar(0)),
        "relative_complementarity_tol should be >= 0.");

      derived().checkValidityImpl();
    }

    /// \brief Maximum number of iterations of the solver.
    std::size_t max_iterations;

    /// \brief Tolerance on the primal/dual feasibility.
    Scalar absolute_feasibility_tol;

    /// \brief Relative tolerance on the primal/dual feasibility.
    Scalar relative_feasibility_tol;

    /// \brief Absolute tolerance on the complementarity (duality gap).
    Scalar absolute_complementarity_tol;

    /// \brief Relative tolerance on the complementarity (duality gap).
    Scalar relative_complementarity_tol;

    /// \brief Whether or not to solve the NCP. If set to solve, the equivalent CCP
    /// is solved.
    bool solve_ncp;

    /// \brief Measure solve timings
    bool measure_timings;

    /// \brief Record per iteration stats.
    bool stat_record;
  }; // struct ConstraintSolverSettingsBaseTpl

  /// \brief Unsafe version of ConstraintSolverResultBaseTpl
  template<typename Derived>
  struct Unsafe<ConstraintSolverResultBase<Derived>>
  {
    typedef ConstraintSolverResultBase<Derived> SafeSelf;

    explicit Unsafe(SafeSelf & self)
    : self(self)
    {
    }

    /// \brief Make the solution valid.
    /// This is typically called by solvers when they are done with their computation.
    /// This allows them to mark the result as valid.
    void makeValid()
    {
      self.m_is_valid = true;
    }

  protected:
    SafeSelf & self;
  };

  ///
  /// \brief Base struct for result of a constraint solver.
  template<typename Derived>
  struct ConstraintSolverResultBase
  {
    typedef typename traits<Derived>::Scalar Scalar;
    typedef ConstraintSolverResultBase Self;

    /// \brief Cast to Derived.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    /// \brief Const cast to Derived.
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Cast this class to its unsafe version.
    Unsafe<Self> unsafe()
    {
      return Unsafe<Self>(*this);
    }
    friend struct Unsafe<Self>;

    /// \brief Default constructor.
    ConstraintSolverResultBase()
    : iterations(0)
    , converged(false)
    , primal_feasibility(std::numeric_limits<Scalar>::quiet_NaN())
    , dual_feasibility(std::numeric_limits<Scalar>::quiet_NaN())
    , complementarity(std::numeric_limits<Scalar>::quiet_NaN())
    , m_is_valid(false)
    {
    }

    /// \brief Reset the solution.
    void reset()
    {
      iterations = 0;
      converged = false;
      primal_feasibility = std::numeric_limits<Scalar>::quiet_NaN();
      dual_feasibility = std::numeric_limits<Scalar>::quiet_NaN();
      complementarity = std::numeric_limits<Scalar>::quiet_NaN();

      m_is_valid = false;

      derived().resetImpl();
    }

    /// \brief Returns true if the solution is in a valid state.
    /// If it is, it represents the result of a meaningful computation.
    /// Otherwise, it does not.
    bool isValid() const
    {
      return m_is_valid;
    }

    /// \brief Size of quantities related to constraints contained in result (typically constraint
    /// impulses or velocities).
    int constraintSize() const
    {
      return derived().constraintSizeImpl();
    }

    /// \brief Set the constraint impulse guess given to the solver.
    template<typename VectorLike>
    void setConstraintImpulseGuess(const Eigen::MatrixBase<VectorLike> & impulse_guess)
    {
      derived().setConstraintImpulseGuessImpl(impulse_guess);
    }

    /// \brief Clears the constraint impulse guess - as if no guess were given to the solver.
    void clearConstraintImpulseGuess()
    {
      derived().clearConstraintImpulseGuessImpl();
    }

    /// \brief Set the constraint velocity guess given to the solver.
    template<typename VectorLike>
    void setConstraintVelocityGuess(const Eigen::MatrixBase<VectorLike> & velocity_guess)
    {
      derived().setConstraintVelocityGuessImpl(velocity_guess);
    }

    /// \brief Clears the constraint velocity guess - as if no guess were given to the solver.
    void clearConstraintVelocityGuess()
    {
      derived().clearConstraintVelocityGuessImpl();
    }

    /// \brief Retrieve constraint impulses.
    template<typename VectorLike>
    void retrieveConstraintImpulses(const Eigen::MatrixBase<VectorLike> & constraint_impulses) const
    {
      derived().retrieveConstraintImpulsesImpl(constraint_impulses);
    }

    /// \brief Retrieve constraint velocities.
    template<typename VectorLike>
    void
    retrieveConstraintVelocities(const Eigen::MatrixBase<VectorLike> & constraint_velocities) const
    {
      derived().retrieveConstraintVelocitiesImpl(constraint_velocities);
    }

    /// \brief Number of iterations of the solver
    std::size_t iterations;

    /// \brief Whether or not the solver has converged
    bool converged;

    /// \brief Value of the primal feasibility
    Scalar primal_feasibility;

    /// \brief Value of the dual feasibility
    Scalar dual_feasibility;

    /// \brief Value of the complementarity
    Scalar complementarity;

  protected:
    /// \brief Whether or not the solution is in a valid state.
    /// If it is, it represents the result of a meaningful computation.
    /// Otherwise, it does not.
    bool m_is_valid;

  }; // struct ConstraintSolverResultBaseTpl

  ///
  /// \brief Base struct to track a constraint solver progress per iteration.
  template<typename Derived>
  struct ConstraintSolverStatsBase
  {
    typedef typename traits<Derived>::Scalar Scalar;

    /// \brief Cast to Derived.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    /// \brief Const cast to Derived.
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Default constructor.
    ConstraintSolverStatsBase()
    : iterations(0)
    {
    }

    /// \brief Constructor given a maximum iteration of the solver.
    explicit ConstraintSolverStatsBase(std::size_t max_iterations)
    : iterations(0)
    {
      reserve(max_iterations);
    }

    /// \brief Reserve enough storage for max_it iterations.
    void reserve(std::size_t max_iterations)
    {
      primal_feasibility.reserve(max_iterations);
      dual_feasibility.reserve(max_iterations);
      dual_feasibility_ncp.reserve(max_iterations);
      complementarity.reserve(max_iterations);

      derived().reserveImpl(max_iterations);
    }

    /// \brief Reset stats.
    void reset()
    {
      iterations = 0;
      primal_feasibility.clear();
      dual_feasibility.clear();
      dual_feasibility_ncp.clear();
      complementarity.clear();

      derived().resetImpl();
    }

    /// \brief Returns the size of the stats (number of iterations tracked).
    size_t size() const
    {
      return primal_feasibility.size();
    }

    ///  \brief Total number of iterations.
    std::size_t iterations;

    /// \brief History of primal feasibility values.
    std::vector<Scalar> primal_feasibility;

    /// \brief History of dual feasibility values.
    std::vector<Scalar> dual_feasibility;

    /// \brief History of NCP dual feasibility values.
    std::vector<Scalar> dual_feasibility_ncp;

    /// \brief History of complementarity values.
    std::vector<Scalar> complementarity;
  };

  template<typename Derived>
  struct ConstraintSolverBase
  {
    typedef typename traits<Derived>::Scalar Scalar;
    typedef typename traits<Derived>::SolverSettings SolverSettings;
    typedef typename traits<Derived>::SolverResult SolverResult;

    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::duration<double, std::micro>;

    /// \brief Cast to Derived.
    Derived & derived()
    {
      return static_cast<Derived &>(*this);
    }
    /// \brief Const cast to Derived.
    const Derived & derived() const
    {
      return static_cast<const Derived &>(*this);
    }

    /// \brief Solve the constrained problem composed of problem data (G,g,constraint_models,
    /// constraint_datas).
    ///
    /// \param[in] G Symmetric PSD matrix representing the Delassus of the constraint problem.
    /// \param[in] g Free constraint acceleration or velocity associted with the constraint problem.
    /// \param[in] constraint_models Vector of constraint models.
    /// \param[in] constraint_datas Vector of constraint datas.
    /// \param[in] settings Settings for the solver.
    /// \param[in/out] result Solution to the constraint problem. Also contains the warmstart to
    /// solve the problem.
    ///
    /// \returns True if the problem has converged.
    template<
      typename DelassusDerived,
      typename VectorLike,
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator>
    bool solve(
      DelassusOperatorBase<DelassusDerived> & delassus,
      const Eigen::MatrixBase<VectorLike> & g,
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const SolverSettings & settings,
      SolverResult & result)
    {
      return derived().solveImpl(
        delassus, g, constraint_models, constraint_datas, settings, result);
    }

    /// \brief Reset the solver as if it never ran.
    void reset()
    {
      derived().resetImpl();
    }

    /// \brief Last solve call elapsed time in microseconds.
    double getElapsedTime() const
    {
      return timer_duration.count();
    }

  protected:
    void timerStart()
    {
      timer_start = clock::now();
    }

    void timerStop()
    {
      timer_duration = clock::now() - timer_start;
    }

    time_point timer_start;
    duration timer_duration;

  }; // struct ConstraintSolverBaseTpl

} // namespace pinocchio
