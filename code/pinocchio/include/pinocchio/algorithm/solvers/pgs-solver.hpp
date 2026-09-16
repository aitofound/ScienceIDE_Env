//
// Copyright (c) 2022-2024 INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <cassert>
#include <cstddef>
#include <optional>
#include <limits>
#include <vector>

#include <Eigen/Core>

#include <boost/fusion/container/vector/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/utils/check.hpp"
#include "pinocchio/utils/reference.hpp"

#include "pinocchio/container/eigen-storage.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/constraints.hpp"

#include "pinocchio/algorithm/solvers/fwd.hpp"
#include "pinocchio/algorithm/solvers/constraint-solver-base.hpp"
#include "pinocchio/algorithm/solvers/constraint-solver-utils.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  // fwd declarations for PGS-internal structs
  // user-api structs are fwd delclared in solvers/fwd.hpp.
  // see below for definitions
  namespace internal
  {
    template<typename Scalar, int Options>
    struct PGSSolverWorkspaceTpl;
  }

  template<typename _Scalar, int _Options>
  struct traits<PGSConstraintSolverTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;

    typedef PGSSolverSettingsTpl<Scalar> SolverSettings;
    typedef PGSSolverResultTpl<Scalar, Options> SolverResult;
  };

  /// \brief Projected Gauss Siedel solver
  template<typename _Scalar, int _Options>
  struct PGSConstraintSolverTpl : ConstraintSolverBase<PGSConstraintSolverTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef PGSConstraintSolverTpl Self;
    typedef ConstraintSolverBase<Self> Base;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Ref<const VectorXs> RefConstVectorXs;

    typedef internal::PGSSolverWorkspaceTpl<Scalar, Options> PGSSolverWorkspace;
    typedef PGSSolverSettingsTpl<Scalar> PGSSolverSettings;
    typedef PGSSolverResultTpl<Scalar, Options> PGSSolverResult;
    typedef PGSSolverStatsTpl<Scalar> PGSSolverStats;

    using Base::reset;
    using Base::timerStart;
    using Base::timerStop;

    /// \brief Default constructor.
    /// \note The user can give `max_problem_size` to preallocate maximum problem sizes data.
    /// This is optional and the solver will automatically resize its workspace to handle
    /// the constraint problem thrown at it.
    /// If the solver encounters a problem which size is bigger than `max_problem_size`,
    /// it will automatically resize its internals as well as the result output of `solve`.
    ///
    /// \param[in] max_problem_size maximum problem size (before automatic reallocation)
    explicit PGSConstraintSolverTpl(std::size_t max_problem_size = 0)
    : Base()
    , stats()
    , m_workspace(max_problem_size)
    , m_is_valid(false)
    {
      // we need to call reset - the solver needs to look as if it never ran
      reset();
    }

    /// \brief Returns true if solver is in a valid state (it has solved a constraint problem).
    /// If so, its stats are valid.
    bool isValid() const
    {
      return m_is_valid;
    }

    template<
      typename DelassusDerived,
      typename VectorLike,
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator>
    bool solveImpl(
      DelassusOperatorBase<DelassusDerived> & delassus,
      const Eigen::MatrixBase<VectorLike> & g,
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const PGSSolverSettings & settings,
      PGSSolverResult & result);

    void resetImpl()
    {
      stats.reset();
      m_workspace.reset();
      m_is_valid = false;
    }

    /// \brief Per-iteration stats of the PGS solver.
    PGSSolverStats stats;

  protected:
    /// \brief Workspace of the PGS solver.
    /// This is an internal of the solver and is not meant to be accessed by
    /// users.
    PGSSolverWorkspace m_workspace;

    /// \brief Flag to check whether or not the solver is in a reset state.
    /// If not, its stats are valid.
    bool m_is_valid;

  }; // struct PGSConstraintSolverTpl

  template<typename _Scalar>
  struct traits<PGSSolverSettingsTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
  };

  ///
  /// \brief Settings for the PGS constraint solver loop.
  template<typename _Scalar>
  struct PGSSolverSettingsTpl : ConstraintSolverSettingsBase<PGSSolverSettingsTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef PGSSolverSettingsTpl Self;
    typedef ConstraintSolverSettingsBase<Self> Base;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> VectorXs;

    /// \brief Default constructor
    PGSSolverSettingsTpl(
      std::size_t max_iterations = 1000,
      Scalar absolute_feasibility_tol = Scalar(1e-6),
      Scalar relative_feasibility_tol = Scalar(1e-6),
      Scalar absolute_complementarity_tol = Scalar(1e-6),
      Scalar relative_complementarity_tol = Scalar(1e-6),
      bool solve_ncp = true,
      bool measure_timings = false,
      bool stat_record = false,
      Scalar over_relaxation = Scalar(1))
    : Base(
        max_iterations,
        absolute_feasibility_tol,
        relative_feasibility_tol,
        absolute_complementarity_tol,
        relative_complementarity_tol,
        solve_ncp,
        measure_timings,
        stat_record)
    , over_relaxation(over_relaxation)
    {
    }

    void checkValidityImpl() const
    {
      PINOCCHIO_CHECK_INPUT_ARGUMENT(
        over_relaxation < Scalar(2) && over_relaxation > Scalar(0),
        "over_relaxation should lie in ]0,2[.");
    }

    // ----------------------
    // General settings

    /// \brief Maximum number of iterations of the solver.
    using Base::max_iterations;

    /// \brief Tolerance on the primal/dual feasibilityibility.
    using Base::absolute_feasibility_tol;

    /// \brief Relative tolerance on the primal/dual feasibilityibility.
    using Base::relative_feasibility_tol;

    /// \brief Absolute tolerance of complementarity (duality complementarity).
    using Base::absolute_complementarity_tol;

    /// \brief Relative tolerance of complementarity (duality complementarity).
    using Base::relative_complementarity_tol;

    /// \brief Whether or not to solve the NCP. If set to solve, the equivalent CCP
    /// is solved.
    using Base::solve_ncp;

    /// \brief Measure solve timings
    using Base::measure_timings;

    /// \brief Record per iteration stats.
    using Base::stat_record;

    // ----------------------
    // PGS specific settings

    /// \brief Over-relaxation of PGS step. Default value is 1.
    Scalar over_relaxation;
  }; // struct PGSSolverSettingsTpl

  template<typename _Scalar, int _Options>
  struct traits<PGSSolverResultTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
  };

  ///
  /// \brief Struct describing the solution of the PGS constraint solver
  /// after calling the `solve` method.
  /// Also contains the warmstart of the solution to the constraint problem.
  template<typename _Scalar, int _Options>
  struct PGSSolverResultTpl : ConstraintSolverResultBase<PGSSolverResultTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef PGSSolverResultTpl Self;
    typedef ConstraintSolverResultBase<Self> Base;

    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
    typedef Eigen::Ref<const VectorXs> RefConstVectorXs;
    typedef internal::EigenStorageTpl<VectorXs> VectorXsStorage;

    using Base::constraintSize;
    using Base::setConstraintImpulseGuess;
    using Base::setConstraintVelocityGuess;

    /// \brief Default constructor.
    PGSSolverResultTpl()
    : Base()
    , problem_size(0)
    , impulse_guess(std::nullopt)
    {
    }

    /// \brief Copy constructor.
    PGSSolverResultTpl(const PGSSolverResultTpl & other)
    {
      *this = other;
    }

    /// \brief Assignment operator.
    PGSSolverResultTpl & operator=(const PGSSolverResultTpl & other)
    {
      if (this != &other)
      {
        Base::operator=(other);
        problem_size = other.problem_size;
        // Since some members are maps reference on EigenStorage, we cannot simply copy them.
        // Thus we need to explicitly say we copy the storage, and the maps will automatically point
        // to the new storage.
        x_storage = other.x_storage;
        y_storage = other.y_storage;

        // special care must be taken for the optional guesses
        if (other.impulse_guess)
        {
          setConstraintImpulseGuess(other.impulse_guess.value());
        }
        else
        {
          clearConstraintImpulseGuessImpl();
        }
      }
      return *this;
    }

    /// \brief \copydoc Base::constraintSize
    int constraintSizeImpl() const
    {
      return static_cast<int>(problem_size);
    }

    void resetImpl()
    {
      impulse_guess.reset();

      // set solution to nan - solver has not run
      x.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
      y.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
    }

    /// \brief Resize the primal/dual vectors of the solution.
    void resize(std::size_t problem_size_)
    {
      problem_size = problem_size_;

      Eigen::Index np = static_cast<Eigen::Index>(problem_size);
      x_storage.resize(np);
      y_storage.resize(np);
    }

    /// \brief Retrieve primal solution.
    template<typename VectorLike>
    void retrievePrimalSolution(const Eigen::MatrixBase<VectorLike> & primal_solution_) const
    {
      auto & primal_solution = primal_solution_.const_cast_derived();
      primal_solution = x;
    }

    /// \brief Retrieve dual solution.
    template<typename VectorLike>
    void retrieveDualSolution(const Eigen::MatrixBase<VectorLike> & dual_solution_) const
    {
      auto & dual_solution = dual_solution_.const_cast_derived();
      dual_solution = y;
    }

    template<typename VectorLike>
    void setConstraintImpulseGuessImpl(const Eigen::MatrixBase<VectorLike> & impulse_guess_in)
    {
      m_impulse_guess_storage.resize(impulse_guess_in.size());
      m_impulse_guess = impulse_guess_in;
      impulse_guess.emplace(m_impulse_guess);
    }

    void clearConstraintImpulseGuessImpl()
    {
      m_impulse_guess_storage.resize(0);
      impulse_guess.reset();
    }

    template<typename VectorLike>
    void setConstraintVelocityGuessImpl(const Eigen::MatrixBase<VectorLike> & /*velocity_guess_*/)
    {
      // do nothing, no velocity guess for PGS
    }

    void clearConstraintVelocityGuessImpl()
    {
      // do nothing, no velocity guess for PGS
    }

    template<typename VectorLike>
    void
    retrieveConstraintImpulsesImpl(const Eigen::MatrixBase<VectorLike> & constraint_impulses_) const
    {
      auto & constraint_impulses = constraint_impulses_.const_cast_derived();
      constraint_impulses = x;
    }

    /// At the optimum we have y = Gx + g.
    /// WARNING: the PGS solver does not take into account desaxce terms for now.
    /// It only solves the CCP (not the NCP).
    template<typename VectorLike>
    void retrieveConstraintVelocitiesImpl(
      const Eigen::MatrixBase<VectorLike> & constraint_velocities_) const
    {
      auto & constraint_velocities = constraint_velocities_.const_cast_derived();
      constraint_velocities = y;
    }

    /// \brief Number of iterations of the solver.
    using Base::iterations;

    /// \brief Whether or not the solver has converged.
    using Base::converged;

    /// \brief Value of the primal feasibility.
    using Base::primal_feasibility;

    /// \brief Value of the dual feasibility.
    using Base::dual_feasibility;

    /// \brief Value of the complementarity.
    using Base::complementarity;

    /// \brief Size of primal/dual variables.
    std::size_t problem_size;

    // ----------------------
    // Solution warmstart

    /// \brief Optional guess for the primal variable (impulses).
    std::optional<RefConstVectorXs> impulse_guess;

    // ----------------------
    // Solution - output of the solver

    /// \brief Primal solution.
    /// \note Order of storage/map declaration is important!
    /// First declare the storage, then the map, otherwise map will point to nothing.
    VectorXsStorage x_storage;
    typename VectorXsStorage::RefMapType x = x_storage.map();

    /// \brief Dual solution.
    VectorXsStorage y_storage;
    typename VectorXsStorage::RefMapType y = y_storage.map();

    /// \brief Storage for the optional impulse guess.
    VectorXsStorage m_impulse_guess_storage;
    typename VectorXsStorage::RefMapType m_impulse_guess = m_impulse_guess_storage.map();
  }; // struct PGSSolverResultTpl

  template<typename _Scalar>
  struct traits<PGSSolverStatsTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
  };

  ///
  /// \brief Struct to track per iteration progress of PGS constraint solver.
  template<typename _Scalar>
  struct PGSSolverStatsTpl : ConstraintSolverStatsBase<PGSSolverStatsTpl<_Scalar>>
  {
    typedef _Scalar Scalar;
    typedef PGSSolverStatsTpl Self;
    typedef ConstraintSolverStatsBase<Self> Base;

    using Base::reserve;

    /// \brief Default constructor.
    PGSSolverStatsTpl()
    : Base()
    {
    }

    /// \brief Constructor given a maximum iteration of the solver.
    explicit PGSSolverStatsTpl(std::size_t max_iterations)
    : Base(max_iterations)
    {
      reserve(max_iterations);
    }

    void reserveImpl(std::size_t /*max_iterations*/)
    {
      // No extra fields to reserve for PGS stats.
    }

    void resetImpl()
    {
      // No extra fields to reset for PGS stats.
    }

    /// \brief How many iterations the solver ran.
    using Base::iterations;

    /// \brief Vector storing per iteration primal feasibility.
    using Base::primal_feasibility;

    /// \brief Vector storing per iteration dual feasibility.
    using Base::dual_feasibility;

    /// \brief Vector storing per iteration NCP feasibility.
    using Base::dual_feasibility_ncp;

    /// \brief Vector storing per iteration complementarity.
    using Base::complementarity;
  }; // struct PGSSolverStatsTpl

  namespace internal
  {
    ///
    /// \brief Workspace for the PGS constraint solver.
    template<typename _Scalar, int _Options>
    struct PGSSolverWorkspaceTpl
    {
      typedef _Scalar Scalar;
      static constexpr int Options = _Options;
      typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Options> VectorXs;
      typedef internal::EigenStorageTpl<VectorXs> VectorXsStorage;
      typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options> MatrixXs;
      typedef internal::EigenStorageTpl<MatrixXs> MatrixXsStorage;

      /// \brief Constructor given problem_size.
      PGSSolverWorkspaceTpl(std::size_t problem_size = 0)
      : problem_size(problem_size)
      {
        resize(problem_size);
        reset();
      }

      /// \brief Copy constructor.
      PGSSolverWorkspaceTpl(const PGSSolverWorkspaceTpl & other)
      : PGSSolverWorkspaceTpl(0)
      {
        *this = other;
      }

      /// \brief Assignment operator.
      PGSSolverWorkspaceTpl & operator=(const PGSSolverWorkspaceTpl & other)
      {
        if (this != &other)
        {
          problem_size = other.problem_size;

          // Since some members are maps reference on EigenStorage, we cannot simply copy them.
          // Thus we need to explicitly say we copy the storage, and the maps will automatically
          // point to the new storage.
          delassus_matrix_storage = other.delassus_matrix_storage;
          x_storage = other.x_storage;
          x_previous_storage = other.x_previous_storage;
          y_storage = other.y_storage;
          rhs_storage = other.rhs_storage;
          tmp_storage = other.tmp_storage;
        }
        return *this;
      }

      /// \brief Reset the workspace.
      void reset()
      {
        resize(problem_size);

#ifndef NDEBUG
        delassus_matrix.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        x.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        x_previous.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        y.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        tmp.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        rhs.setConstant(std::numeric_limits<Scalar>::quiet_NaN());
#endif
      }

      /// \brief Resize workspace vectors to problem size.
      void resize(std::size_t problem_size_)
      {
        problem_size = problem_size_;

        Eigen::Index np = static_cast<Eigen::Index>(problem_size);
        delassus_matrix_storage.resize(np, np);
        x_storage.resize(np);
        x_previous_storage.resize(np);
        y_storage.resize(np);
        tmp_storage.resize(np);
        rhs_storage.resize(np);
      }

      /// \brief Size of problem.
      std::size_t problem_size;

      /// \brief Storage for the delassus matrix.
      MatrixXsStorage delassus_matrix_storage;
      typename MatrixXsStorage::RefMapType delassus_matrix = delassus_matrix_storage.map();

      /// \brief Primal variable (impulses) at current iteration.
      VectorXsStorage x_storage;
      typename VectorXsStorage::RefMapType x = x_storage.map();

      /// \brief Primal variable (impulses) at previous iteration.
      VectorXsStorage x_previous_storage;
      typename VectorXsStorage::RefMapType x_previous = x_previous_storage.map();

      /// \brief Dual variable (constraint velocities) at current iteration.
      VectorXsStorage y_storage;
      typename VectorXsStorage::RefMapType y = y_storage.map();

      /// \brief Temporary vector for computations.
      VectorXsStorage rhs_storage;
      typename VectorXsStorage::RefMapType rhs = rhs_storage.map();

      /// \brief Temporary vector for computations.
      VectorXsStorage tmp_storage;
      typename VectorXsStorage::RefMapType tmp = tmp_storage.map();
    }; // struct PGSSolverWorkspaceTpl
  } // namespace internal

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/solvers/pgs-solver.hxx"
// IWYU pragma: end_exports
