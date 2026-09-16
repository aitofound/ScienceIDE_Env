//
// Copyright (c) 2022-2024 INRIA
//

#pragma once

#include "pinocchio/bindings/python/fwd.hpp"
#include "pinocchio/algorithm/solvers/constraint-solver-base.hpp"

#include <eigenpy/eigen-from-python.hpp>
#include <eigenpy/eigen-to-python.hpp>

namespace pinocchio
{
  namespace python
  {

    namespace bp = boost::python;

    // ============================================================================
    // ConstraintSolverSettingsBasePythonVisitor
    // Injects base settings fields and methods into a Python class.
    // ============================================================================

    template<typename Settings>
    struct ConstraintSolverSettingsBasePythonVisitor
    : public boost::python::def_visitor<ConstraintSolverSettingsBasePythonVisitor<Settings>>
    {
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def_readwrite(
            "max_iterations", &Settings::max_iterations, "Maximum number of solver iterations.")
          .def_readwrite(
            "absolute_feasibility_tol", &Settings::absolute_feasibility_tol,
            "Absolute tolerance on primal/dual feasibility.")
          .def_readwrite(
            "relative_feasibility_tol", &Settings::relative_feasibility_tol,
            "Relative tolerance on primal/dual feasibility.")
          .def_readwrite(
            "absolute_complementarity_tol", &Settings::absolute_complementarity_tol,
            "Absolute tolerance on complementarity (duality gap).")
          .def_readwrite(
            "relative_complementarity_tol", &Settings::relative_complementarity_tol,
            "Relative tolerance on complementarity (duality gap).")
          .def_readwrite(
            "solve_ncp", &Settings::solve_ncp,
            "Whether to solve the NCP. If false, the equivalent CCP is solved.")
          .def_readwrite(
            "measure_timings", &Settings::measure_timings, "Whether to measure solve timings.")
          .def_readwrite(
            "stat_record", &Settings::stat_record, "Whether to record per-iteration stats.")
          .def(
            "checkValidity", &Settings::checkValidity, bp::arg("self"),
            "Throw if the settings are not valid.");
      }
    }; // struct ConstraintSolverSettingsBasePythonVisitor

    // ============================================================================
    // ConstraintSolverResultBasePythonVisitor
    // Injects base result fields and methods into a Python class.
    // ============================================================================

    template<typename Result>
    struct ConstraintSolverResultBasePythonVisitor
    : public boost::python::def_visitor<ConstraintSolverResultBasePythonVisitor<Result>>
    {
      typedef typename Result::Scalar Scalar;
      typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1, Result::Options> VectorXs;

      static VectorXs retrieveConstraintImpulses_wrapper(const Result & self)
      {
        VectorXs out(static_cast<Eigen::Index>(self.constraintSize()));
        self.retrieveConstraintImpulses(out);
        return out;
      }

      static VectorXs retrieveConstraintVelocities_wrapper(const Result & self)
      {
        VectorXs out(static_cast<Eigen::Index>(self.constraintSize()));
        self.retrieveConstraintVelocities(out);
        return out;
      }

      static void setConstraintImpulseGuess_wrapper(Result & self, const VectorXs & impulse_guess)
      {
        self.setConstraintImpulseGuess(impulse_guess);
      }

      static void setConstraintVelocityGuess_wrapper(Result & self, const VectorXs & velocity_guess)
      {
        self.setConstraintVelocityGuess(velocity_guess);
      }

      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def_readwrite("iterations", &Result::iterations, "Number of solver iterations.")
          .def_readwrite("converged", &Result::converged, "Whether the solver has converged.")
          .def_readonly(
            "primal_feasibility", &Result::primal_feasibility, "Value of the primal feasibility.")
          .def_readonly(
            "dual_feasibility", &Result::dual_feasibility, "Value of the dual feasibility.")
          .def_readonly(
            "complementarity", &Result::complementarity, "Value of the complementarity.")
          .def(
            "isValid", &Result::isValid, bp::arg("self"),
            "Return True if the result represents a valid (post-solve) state.")
          .def(
            "constraintSize", &Result::constraintSize, bp::arg("self"),
            "Size of quantities related to constraints contained in result (typically constraint "
            "impulses or velocities).")
          .def(
            "reset", &Result::reset, bp::arg("self"),
            "Reset the result to an invalid (pre-solve) state.")
          .def(
            "retrieveConstraintImpulses", retrieveConstraintImpulses_wrapper, bp::arg("self"),
            "Return the primal solution (constraint impulses) as a new vector.")
          .def(
            "retrieveConstraintVelocities", retrieveConstraintVelocities_wrapper, bp::arg("self"),
            "Return the dual solution (constraint velocities) as a new vector.")
          .def(
            "setConstraintImpulseGuess", setConstraintImpulseGuess_wrapper,
            bp::args("self", "impulse_guess"),
            "Set the constraint impulse warmstart guess for the next solve call.")
          .def(
            "clearConstraintImpulseGuess", &Result::clearConstraintImpulseGuess, bp::arg("self"),
            "Clear the constraint impulse warmstart guess.")
          .def(
            "setConstraintVelocityGuess", setConstraintVelocityGuess_wrapper,
            bp::args("self", "velocity_guess"),
            "Set the constraint velocity warmstart guess for the next solve call.")
          .def(
            "clearConstraintVelocityGuess", &Result::clearConstraintVelocityGuess, bp::arg("self"),
            "Clear the constraint velocity warmstart guess.");
      }
    }; // struct ConstraintSolverResultBasePythonVisitor

    // ============================================================================
    // ConstraintSolverStatsBasePythonVisitor
    // Injects base stats fields and methods into a Python class.
    // ============================================================================

    template<typename Stats>
    struct ConstraintSolverStatsBasePythonVisitor
    : public boost::python::def_visitor<ConstraintSolverStatsBasePythonVisitor<Stats>>
    {
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def_readwrite("iterations", &Stats::iterations, "Total number of iterations tracked.")
          .def_readonly(
            "primal_feasibility", &Stats::primal_feasibility,
            "Per-iteration history of primal feasibility values.")
          .def_readonly(
            "dual_feasibility", &Stats::dual_feasibility,
            "Per-iteration history of dual feasibility values.")
          .def_readonly(
            "dual_feasibility_ncp", &Stats::dual_feasibility_ncp,
            "Per-iteration history of NCP dual feasibility values.")
          .def_readonly(
            "complementarity", &Stats::complementarity,
            "Per-iteration history of complementarity values.")
          .def(
            "size", &Stats::size, bp::arg("self"),
            "Return the number of iterations tracked (size of history vectors).")
          .def(
            "reserve", &Stats::reserve, bp::args("self", "max_iterations"),
            "Reserve storage for up to max_iterations iterations.")
          .def(
            "reset", &Stats::reset, bp::arg("self"),
            "Clear all history and reset the iteration counter to zero.");
      }
    }; // struct ConstraintSolverStatsBasePythonVisitor

    // ============================================================================
    // ConstraintSolverBasePythonVisitor
    // Injects base solver methods into a Python class.
    // ============================================================================

    template<typename Solver>
    struct ConstraintSolverBasePythonVisitor
    : public boost::python::def_visitor<ConstraintSolverBasePythonVisitor<Solver>>
    {
      template<class PyClass>
      void visit(PyClass & cl) const
      {
        cl.def("reset", &Solver::reset, bp::arg("self"), "Reset the solver to its initial state.");

        cl.def(
          "getElapsedTime", &Solver::getElapsedTime, bp::arg("self"),
          "Return last solve call elapsed time in microseconds.");
      }
    }; // struct ConstraintSolverBasePythonVisitor

  } // namespace python
} // namespace pinocchio
