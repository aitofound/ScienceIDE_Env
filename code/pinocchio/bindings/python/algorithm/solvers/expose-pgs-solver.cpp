//
// Copyright (c) 2022-2025 INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/algorithm/solvers/pgs-solver.hpp"
#include "pinocchio/algorithm/constraint-cholesky.hpp"
#include "pinocchio/algorithm/delassus-operator.hpp"

#include "pinocchio/bindings/python/algorithm/constraint-solver-base.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"
#include "pinocchio/bindings/python/utils/macros.hpp"

#include <eigenpy/memory.hpp>
#include <eigenpy/eigen-from-python.hpp>
#include <eigenpy/eigen-to-python.hpp>

namespace pinocchio
{
  namespace python
  {
    namespace bp = boost::python;

    typedef context::Scalar Scalar;
    static constexpr int Options = context::Options;
    typedef context::VectorXs VectorXs;

    typedef PGSConstraintSolverTpl<Scalar, Options> PGSSolver;
    typedef typename PGSSolver::PGSSolverSettings PGSSolverSettings;
    typedef typename PGSSolver::PGSSolverResult PGSSolverResult;
    typedef typename PGSSolver::PGSSolverStats PGSSolverStats;

    typedef ConstraintCholeskyDecompositionTpl<Scalar, Options> ConstraintCholeskyDecomposition;

    // ============================================================================
    // Expose PGSSolverSettings
    // ============================================================================

    void exposePGSSolverSettings()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      bp::class_<PGSSolverSettings>(
        "PGSSolverSettings", "Settings for the PGS constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor with default settings."))

        // Base settings
        .def(ConstraintSolverSettingsBasePythonVisitor<PGSSolverSettings>())

        // PGS specific settings
        .PINOCCHIO_ADD_PROPERTY(
          PGSSolverSettings, over_relaxation,
          "Over-relaxation parameter (should be in ]0,2[, default 1)");
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    }

    // ============================================================================
    // Expose PGSSolverResult
    // ============================================================================

    static void retrievePrimalSolution_pgs_wrapper(
      const PGSSolverResult & solution, Eigen::Ref<VectorXs> primal_solution)
    {
      solution.retrievePrimalSolution(primal_solution);
    }

    static void retrieveDualSolution_pgs_wrapper(
      const PGSSolverResult & solution, Eigen::Ref<VectorXs> dual_solution)
    {
      solution.retrieveDualSolution(dual_solution);
    }

    void exposePGSSolverResult()
    {
      bp::class_<PGSSolverResult>(
        "PGSSolverResult", "Solution of the PGS constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor."))

        // Base result
        .def(ConstraintSolverResultBasePythonVisitor<PGSSolverResult>())

        // PGS specific properties
        .PINOCCHIO_ADD_PROPERTY_READONLY(PGSSolverResult, problem_size, "Problem size")

        .def(
          "resize", &PGSSolverResult::resize, bp::args("self", "problem_size"),
          "Resize solution vectors")

        // Raw retrieve methods (in-place, caller provides output vector)
        .def(
          "retrievePrimalSolution", retrievePrimalSolution_pgs_wrapper,
          bp::args("self", "primal_solution"),
          "Retrieve the primal solution (copy x into primal_solution)")
        .def(
          "retrieveDualSolution", retrieveDualSolution_pgs_wrapper,
          bp::args("self", "dual_solution"),
          "Retrieve the dual solution (copy y into dual_solution)");
    }

    // ============================================================================
    // Expose PGSSolverStats
    // ============================================================================

    void exposePGSSolverStats()
    {
      bp::class_<PGSSolverStats>(
        "PGSSolverStats", "Per-iteration statistics of the PGS constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor."))
        .def(
          bp::init<std::size_t>(
            bp::args("self", "max_iterations"), "Constructor with maximum iterations."))

        // Base stats
        .def(ConstraintSolverStatsBasePythonVisitor<PGSSolverStats>());

      // Note: No PGS-specific stats beyond base class
    }

    // ============================================================================
    // Expose PGSConstraintSolver
    // ============================================================================

#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    template<
      typename DelassusDerived,
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator>
    static bool solve_pgs_wrapper(
      PGSSolver & solver,
      DelassusDerived & delassus,
      const VectorXs & g,
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const PGSSolverSettings & settings,
      PGSSolverResult & result)
    {
      return solver.solve(delassus, g, constraint_models, constraint_datas, settings, result);
    }
#endif

    template<typename Solver>
    struct PGSSolveMethodExposer
    {
      PGSSolveMethodExposer(bp::class_<Solver> & class_)
      : class_(class_)
      {
      }

      template<class T>
      void operator()(T)
      {
        run(static_cast<typename T::type *>(nullptr));
      }

      template<typename ConstraintModel>
      void run(ConstraintModelBase<ConstraintModel> * ptr = 0)
      {
        PINOCCHIO_UNUSED_VARIABLE(ptr);

#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
        using ConstraintData = typename traits<ConstraintModel>::ConstraintData;

        typedef std::allocator<ConstraintModel> ConstraintModelAllocator;
        typedef std::allocator<ConstraintData> ConstraintDataAllocator;

        class_
          .def(
            "solve",
            solve_pgs_wrapper<
              ConstraintCholeskyDecomposition::DelassusOperatorCholeskyExpression, ConstraintModel,
              ConstraintModelAllocator, ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.")
          .def(
            "solve",
            solve_pgs_wrapper<
              context::DelassusOperatorDense, ConstraintModel, ConstraintModelAllocator,
              ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.");
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      }

      void run(boost::blank * ptr = 0)
      {
        PINOCCHIO_UNUSED_VARIABLE(ptr);
      }

      bp::class_<Solver> & class_;
    };

    void exposePGSConstraintSolver()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE

      // Expose Settings, Solution, Stats
      exposePGSSolverSettings();
      exposePGSSolverResult();
      exposePGSSolverStats();

      // Expose the solver itself
      bp::class_<PGSSolver> cl(
        "PGSConstraintSolver", "Projected Gauss-Seidel (PGS) solver for contact dynamics.",
        bp::init<>(bp::arg("self"), "Default constructor."));
      cl.def(
        bp::init<std::size_t>(
          bp::args("self", "problem_size"),
          "Constructor with problem dimension. Allows to pre-allocate data if problem_size is "
          "known in advance. The solver will automatically resize its workspace and the result in "
          "any case."));

      cl
        // Base solver
        .def(ConstraintSolverBasePythonVisitor<PGSSolver>())
        // PGS specifics
        .PINOCCHIO_ADD_PROPERTY_READONLY(PGSSolver, stats, "Access the statistics of the solver")
        .def(
          "isValid", &PGSSolver::isValid, bp::arg("self"),
          "Check if the solver is in a valid state (has solved a constraint problem)");

      // Expose solve methods for different constraint models
      PGSSolveMethodExposer<PGSSolver> solve_exposer(cl);
      solve_exposer.run(static_cast<context::ConstraintModel *>(nullptr));

#endif
    }

  } // namespace python
} // namespace pinocchio
