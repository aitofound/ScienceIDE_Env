//
// Copyright (c) 2024-2025 INRIA
//

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/algorithm/solvers/admm-solver.hpp"
#include "pinocchio/algorithm/constraint-cholesky.hpp"
#include "pinocchio/algorithm/delassus-operator.hpp"

#include "pinocchio/bindings/python/algorithm/constraint-solver-base.hpp"
#include "pinocchio/bindings/python/utils/std-vector.hpp"
#include "pinocchio/bindings/python/utils/macros.hpp"

#include <boost/optional.hpp>

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

    typedef ADMMConstraintSolverTpl<Scalar, Options> ADMMSolver;
    typedef typename ADMMSolver::ADMMSolverSettings ADMMSolverSettings;
    typedef typename ADMMSolver::ADMMSolverResult ADMMSolverResult;
    typedef typename ADMMSolver::ADMMSolverStats ADMMSolverStats;

    typedef ConstraintCholeskyDecompositionTpl<Scalar, Options> ConstraintCholeskyDecomposition;

    // ============================================================================
    // Expose ADMM Enums
    // ============================================================================

    void exposeADMMEnums()
    {
      if (!eigenpy::register_symbolic_link_to_registered_type<::pinocchio::ADMMUpdateRule>())
      {
        bp::enum_<::pinocchio::ADMMUpdateRule>("ADMMUpdateRule")
          .value("SPECTRAL", ::pinocchio::ADMMUpdateRule::SPECTRAL)
          .value("OSQP", ::pinocchio::ADMMUpdateRule::OSQP)
          .value("LINEAR", ::pinocchio::ADMMUpdateRule::LINEAR)
          .value("CONSTANT", ::pinocchio::ADMMUpdateRule::CONSTANT)
          .export_values();
      }

      if (!eigenpy::register_symbolic_link_to_registered_type<::pinocchio::ADMMProximalRule>())
      {
        bp::enum_<::pinocchio::ADMMProximalRule>("ADMMProximalRule")
          .value("MANUAL", ::pinocchio::ADMMProximalRule::MANUAL)
          .value("AUTOMATIC", ::pinocchio::ADMMProximalRule::AUTOMATIC)
          .export_values();
      }
    }

    // ============================================================================
    // Expose ADMMSolverSettings
    // ============================================================================

    // Wrapper functions for std::optional<Scalar> <-> boost::optional<Scalar> conversion
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    static boost::optional<Scalar> getRhoInitWrapper(const ADMMSolverSettings & settings)
    {
      if (settings.rho_init.has_value())
        return boost::optional<Scalar>(settings.rho_init.value());
      else
        return boost::none;
    }

    static void
    setRhoInitWrapper(ADMMSolverSettings & settings, const boost::optional<Scalar> & value)
    {
      if (value)
        settings.rho_init = value.get();
      else
        settings.rho_init = std::nullopt;
    }
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE

    void exposeADMMSolverSettings()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      bp::class_<ADMMSolverSettings>(
        "ADMMSolverSettings", "Settings for the ADMM constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor with default settings."))

        // Base settings
        .def(ConstraintSolverSettingsBasePythonVisitor<ADMMSolverSettings>())

        // ADMM specific settings
        .add_property(
          "rho_init", &getRhoInitWrapper, &setRhoInitWrapper,
          "Initial value of rho parameter (optional). If None, will be estimated from Delassus.")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, warmstart_rho_with_previous_result,
          "Whether to warmstart rho with previous result")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, admm_update_rule, "ADMM update rule")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, admm_proximal_rule, "ADMM proximal rule")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, mu_prox, "Proximal penalty parameter")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, tau_prox, "Proximal scaling factor")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, tau, "ADMM penalty scaling factor")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, ratio_primal_dual, "Primal/dual ratio threshold for rho update")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, dual_momentum, "Dual variable momentum")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, rho_update_ratio, "Ratio threshold for rho update")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, rho_min_update_frequency, "Minimum frequency for rho updates")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, rho_momentum, "Momentum on rho updates")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, rho_min, "Minimum value for rho parameter")
        .PINOCCHIO_ADD_PROPERTY(ADMMSolverSettings, rho_max, "Maximum value for rho parameter")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, spectral_rho_power_init, "Initial rho power for SPECTRAL rule")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, spectral_rho_power_factor, "Rho power factor for SPECTRAL rule")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, linear_update_rule_factor, "Factor for LINEAR update rule")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, lanczos_size,
          "Size of Lanczos decomposition for eigenvalue estimation")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, max_delassus_decomposition_updates,
          "Maximum number of Delassus decomposition updates")
        .PINOCCHIO_ADD_PROPERTY(
          ADMMSolverSettings, anderson_capacity, "Anderson acceleration capacity");
#endif // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    }

    // ============================================================================
    // Expose ADMMSolverResult
    // ============================================================================

    static void retrievePrimalSolution_wrapper(
      const ADMMSolverResult & solution, Eigen::Ref<VectorXs> primal_solution)
    {
      solution.retrievePrimalSolution(primal_solution);
    }

    static void retrieveDualSolution_wrapper(
      const ADMMSolverResult & solution, Eigen::Ref<VectorXs> dual_solution)
    {
      solution.retrieveDualSolution(dual_solution);
    }

    static void retrieveDesaxceTerm_wrapper(
      const ADMMSolverResult & solution, Eigen::Ref<VectorXs> desaxce_term)
    {
      solution.retrieveDesaxceTerm(desaxce_term);
    }

    void exposeADMMSolverResult()
    {
      bp::class_<ADMMSolverResult>(
        "ADMMSolverResult", "Solution of the ADMM constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor."))

        // Base result
        .def(ConstraintSolverResultBasePythonVisitor<ADMMSolverResult>())

        // ADMM specific properties
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolverResult, problem_size, "Problem size")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverResult, delassus_decomposition_update_count,
          "Number of Delassus decomposition updates")
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolverResult, rho, "Final rho value")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverResult, spectral_rho_power, "Final spectral rho power")
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolverResult, mu_prox, "Final proximal parameter")

        .def(
          "resize", &ADMMSolverResult::resize, bp::args("self", "problem_size"),
          "Resize solution vectors")

        // Retrieve methods
        .def(
          "retrievePrimalSolution", retrievePrimalSolution_wrapper,
          bp::args("self", "primal_solution"),
          "Retrieve the primal solution (copy y into primal_solution)")
        .def(
          "retrieveDualSolution", retrieveDualSolution_wrapper, bp::args("self", "dual_solution"),
          "Retrieve the dual solution (copy z into dual_solution)")
        .def(
          "retrieveDesaxceTerm", retrieveDesaxceTerm_wrapper, bp::args("self", "desaxce_term"),
          "Retrieve the DeSaxce correction term (copy desaxce into desaxce_term)");
    }

    // ============================================================================
    // Expose ADMMSolverStats
    // ============================================================================

    void exposeADMMSolverStats()
    {
      bp::class_<ADMMSolverStats>(
        "ADMMSolverStats", "Per-iteration statistics of the ADMM constraint solver.",
        bp::init<>(bp::arg("self"), "Default constructor."))
        .def(
          bp::init<std::size_t>(
            bp::args("self", "max_iterations"), "Constructor with maximum iterations."))

        // Base stats
        .def(ConstraintSolverStatsBasePythonVisitor<ADMMSolverStats>())

        // ADMM specific properties
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolverStats, rho, "History of rho values")
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolverStats, mu_prox, "History of mu_prox values")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverStats, anderson_size, "History of Anderson acceleration size")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverStats, linear_system_residual, "History of linear system residuals")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverStats, linear_system_consistency, "History of linear system consistency")
        .PINOCCHIO_ADD_PROPERTY_READONLY(
          ADMMSolverStats, delassus_decomposition_update_count,
          "Number of Delassus decomposition updates");
    }

    // ============================================================================
    // Expose ADMMConstraintSolver
    // ============================================================================

#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
    template<
      typename DelassusDerived,
      typename ConstraintModel,
      typename ConstraintModelAllocator,
      typename ConstraintData,
      typename ConstraintDataAllocator>
    static bool solve_admm_wrapper(
      ADMMSolver & solver,
      DelassusDerived & delassus,
      const VectorXs & g,
      const std::vector<ConstraintModel, ConstraintModelAllocator> & constraint_models,
      const std::vector<ConstraintData, ConstraintDataAllocator> & constraint_datas,
      const ADMMSolverSettings & settings,
      ADMMSolverResult & result)
    {
      return solver.solve(delassus, g, constraint_models, constraint_datas, settings, result);
    }
#endif

    template<typename Solver>
    struct ADMMSolveMethodExposer
    {
      ADMMSolveMethodExposer(bp::class_<Solver> & class_)
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
            solve_admm_wrapper<
              ConstraintCholeskyDecomposition::DelassusOperatorCholeskyExpression, ConstraintModel,
              ConstraintModelAllocator, ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.")
          .def(
            "solve",
            solve_admm_wrapper<
              context::DelassusOperatorDense, ConstraintModel, ConstraintModelAllocator,
              ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.")
          .def(
            "solve",
            solve_admm_wrapper<
              context::DelassusOperatorSparse, ConstraintModel, ConstraintModelAllocator,
              ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.");

  #ifdef PINOCCHIO_WITH_ACCELERATE_SUPPORT
        {
          typedef Eigen::AccelerateLLT<context::SparseMatrix> AccelerateLLT;
          typedef DelassusOperatorSparseTpl<context::Scalar, Options, AccelerateLLT>
            DelassusOperatorSparseAccelerate;
          class_.def(
            "solve",
            solve_admm_wrapper<
              DelassusOperatorSparseAccelerate, ConstraintModel, ConstraintModelAllocator,
              ConstraintData, ConstraintDataAllocator>,
            bp::args(
              "self", "delassus", "g", "constraint_models", "constraint_datas", "settings",
              "result"),
            "Solve the constrained conic problem with given settings and result.");
        }
  #endif // ifdef PINOCCHIO_WITH_ACCELERATE_SUPPORT
#endif   // ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE
      }

      void run(boost::blank * ptr = 0)
      {
        PINOCCHIO_UNUSED_VARIABLE(ptr);
      }

      bp::class_<Solver> & class_;
    };

    void exposeADMMConstraintSolver()
    {
#ifdef PINOCCHIO_PYTHON_PLAIN_SCALAR_TYPE

      // Expose enums first
      exposeADMMEnums();

      // Expose Settings, Solution, Stats
      exposeADMMSolverSettings();
      exposeADMMSolverResult();
      exposeADMMSolverStats();

      // Expose the solver itself
      bp::class_<ADMMSolver> cl(
        "ADMMConstraintSolver",
        "Alternating Direction Method of Multipliers (ADMM) solver for contact dynamics.",
        bp::init<>(bp::arg("self"), "Default constructor."));
      cl.def(
        bp::init<std::size_t>(
          bp::args("self", "problem_size"),
          "Constructor with problem dimension. Allows to pre-allocate data if problem_size is "
          "known in advance. The solver will automatically resize its workspace and the result in "
          "any case."));

      cl
        // Base solver
        .def(ConstraintSolverBasePythonVisitor<ADMMSolver>())
        // ADMM solver specific
        .PINOCCHIO_ADD_PROPERTY_READONLY(ADMMSolver, stats, "Access the statistics of the solver")
        .def(
          "isValid", &ADMMSolver::isValid, bp::arg("self"),
          "Check if the solver is in a valid state (has solved a constraint problem)");

      // Expose solve methods for different constraint models
      ADMMSolveMethodExposer<ADMMSolver> solve_exposer(cl);
      solve_exposer.run(static_cast<context::ConstraintModel *>(nullptr));

#endif
    }

  } // namespace python
} // namespace pinocchio
