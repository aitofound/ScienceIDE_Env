# paper-smoothed-aggregation-example

Upstream test: `code/pyamg/docs/paper/example.py`. Policy: `pointwise`.

## The test

The check executes the shipped paper example at its exact `n=1000` default: one million Poisson unknowns, `smoothed_aggregation_solver(max_coarse=10)`, seed 2022 and a solve from random `x0`. The upstream example runs to `tol=1e-10`; this check instead runs a **fixed** `maxiter=21` (`tol=0`) -- the cycle count the pinned build takes to cross `1e-10` on this exact problem -- and asserts the final residual is below `1e-10`, so the check is exactly as strict a gate as running the example unmodified, without grading the iteration count of an adaptive solve as if it were physics (an unmet assertion produces no graded output and fails the check). It records the fixed-window residual history, hierarchy dimensions/nonzeros and complexity diagnostics. `SAB_GRID_SIZE=1000` restores the upstream configuration.

## The two initial conditions

Nominal preserves the upstream seed and initial guess (`variant_scale=1.0`). Variant multiplies the complete seeded `x0` field by `1.000000000000001`; this one scalar input applies the same few-ULP relative perturbation to every initial-guess value, leaving the one-million-unknown problem, the fixed iteration count and the assertion unchanged.

## The pass policy

The graded observable is PyAMG's shipped one-million-unknown paper example run for a fixed, assertion-gated 21-cycle window: final binary64 residual history, hierarchy dimensions/nonzeros and complexity diagnostics, under atol 1e-12 plus rtol 1e-10. A wrong aggregate, candidate fit, prolongator smoothing, Galerkin product or cycle either fails the assertion or changes the graded values far beyond the bound.

## Evidence

The pinned source's own residual history (measured on the authoring host): 1.406e-10 at cycle 20, 5.532e-11 at cycle 21 -- comfortably below the example's own 1e-10 target, with roughly 1.8x headroom on the boundary, so the fixed cycle count is not a razor's-edge choice. `task selfcheck` records the measured nominal-versus-variant spread and bound_fraction into this rubric's evidence, and the altbuild floor when the alternative build is run. This replaces the 5.10.2-era design, which graded a tol=1e-10 residual-history array whose length was the adaptive solve's own iteration count -- bookkeeping that a differently-ordered but correct port could legitimately shift by one cycle, producing a shape mismatch rather than a numeric verdict (finding C of the 5.11.0 revision brief). Parallel coarsening on a different target is a different algorithm from this serial reference and is expected to need its own cycle count and possibly its own tolerance; this check only defends the pinned build's serial reference path.
