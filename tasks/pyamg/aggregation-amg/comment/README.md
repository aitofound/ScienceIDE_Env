# aggregation-amg: authoring notes

## Module

The leaf owns pyamg/aggregation: aggregate formation, candidate fitting, prolongator smoothing, smoothed aggregation, root-node, adaptive, and pairwise solvers. Gallery generators, strength measures, multilevel cycling, sparse kernels, and relaxation are shared infrastructure. 23 checks cover every one of the 58 official pytest items measured by collection in pyamg/aggregation/tests (`pytest --collect-only`; the revision brief's figure of 61 was not reproduced and is corrected here to the measured count), plus the shipped `pyamg/gallery/demo.py` example and the `docs/paper/example.py` one-million-unknown example.

## Revision (5.11.0, 2026-09-06)

The prior round's 16 checks each ran one immutable upstream pytest class or case as a gate, then graded a byte-identical 18x18-or-so Poisson sentinel regardless of what the gate actually tested — `pairwise-spd`'s probe, for instance, called plain `smoothed_aggregation_solver`, never `pairwise_solver`. This revision keeps every existing gate (no test method dropped) and gives each check a probe on a shipped problem or gallery generator that exercises the same code path its gate tests: BSR `linear_elasticity` for block/candidate-fitting paths, the shipped `recirc_flow` operator for nonsymmetric SA/root-node, the shipped `helmholtz_2D` operator for complex/non-Hermitian paths, `gauge_laplacian` for the inherently-complex cases, and 3-D structured/unstructured meshes for pairwise aggregation. All eight shipped `load_example` matrices (airfoil, bar, helmholtz_2D, knot, local_disc_galerkin_diffusion, recirc_flow, unit_cube, unit_square) are used somewhere in the suite.

Six new checks split a test class's methods where they exercise genuinely different code paths, semi-exhaustively covering the supply per the human's request: `aggregate-pairwise-real` (pairwise matching is a distinct kernel from standard/naive aggregation), `sa-performance-nonsymmetric-real` and `rootnode-performance-nonsymmetric-real` (nonsymmetric strength + energy/GMRES smoothing + pinv coarse solve is a different hierarchy from the rest of `TestSolverPerformance`), `sa-performance-nonhermitian-complex` and `rootnode-performance-nonhermitian-complex` (the same split for the complex class), and `energy-prolongation-bsr` (a distinct low-level BSR kernel test, `test_incomplete_mat_mult_bsr`, split from the prolongator-filtering methods). A seventh new check, `gallery-demo`, adds the shipped `pyamg/gallery/demo.py` example (standalone SA and CG-accelerated SA solves) as its own check, as the revision brief asked. 16 -> 23 checks.

`paper-smoothed-aggregation-example` was RED under 5.10.2: it graded a `tol=1e-10` residual-history array whose length is the iteration count of an adaptive solve — bookkeeping, and one extra cycle on a differently-ordered but correct port is a shape mismatch, not a numeric fault. It now runs a fixed `maxiter=21` (`tol=0`, the cycle count the pinned build takes to first cross `1e-10`, measured with roughly 1.8x headroom: residual 1.406e-10 at cycle 20, 5.532e-11 at cycle 21) and asserts the final residual is below the example's own `1e-10` bound; a failed assertion produces no graded output and fails the check exactly as an unmet official assertion would. Hierarchy sizes, nnz and complexities stay graded (deterministic products of the fixed algorithm on a fixed matrix); a parallel coarsening on a different target is a different algorithm and is expected to need its own cycle count, possibly its own tolerance. `gallery-demo` is built the same way from the start (fixed 18/11-cycle counts reproducing `demo()`'s own `tol=1e-10`), so it never carries the same flaw.

Every check's `run.sh` build line now passes `-Ccompile-args=-j2` to the per-check `pip install` (nominal and altbuild alike): the worker's 88 cores otherwise drive meson-python/ninja to compile roughly 18 pybind11 translation units concurrently inside the container's declared `--memory 2g` limit, and `cc1plus` gets OOM-killed ("c++: fatal error: Killed signal terminated program cc1plus") — measured on the relaxation-smoothing leaf's selfcheck on huangzesen@136.114.2.6 on 2026-09-06, which failed every check with this signature before the cap was added. `memory_gb` in `task.toml` stays at the declared 2.0 GB; only the build's compile parallelism is capped, so each check's independent per-check install now compiles at most two translation units at a time regardless of host core count.

Every check now declares `run.sh altbuild` (the same pinned source built `-Doptimization=0 -Dbuildtype=debug` via meson-python instead of the pinned release build, verified on the arm64 authoring host to reach every C++ compile command; falls back to `CXXFLAGS=-O0` if meson-python drops the setup-arg on the grading host). Per the `altbuild-floors-are-host-specific` pitfall, a floor of exactly zero on the x86 grading worker is reported as "not measured", never "stable" — the worker's own floor from this run supersedes any number measured here. `tests/test.sh` and `solution/solve.sh` were updated from the skill's current templates to accept the third `altbuild` initial condition (they previously only knew `nominal`/`variant`).

Every `validate.py` was replaced with the 5.11.0 pointwise template: it now also reports `bound_fraction` (the worst graded value's `|err| / (atol + rtol|ref|)`; the presentation prints its reciprocal as the margin), and every rubric carries `evidence.self_validation_bound_fraction` alongside `self_validation_spread`, both written by `task selfcheck`.

## Revision round 2 (5.11.0, 2026-09-06): consistent right-hand sides, the solution field graded

The first selfcheck of the round above failed on the x86 worker: `sa-performance-real`
put 6 of its 8 graded values over the bound, `bound_fraction` 1.5e10, max |err| 277.
Measured cause, from the two `observable.npy` files side by side: the shipped
`unit_square` operator is **singular**. Its smallest eigenvalue is 3.04e-16 against a
largest of 6.788 (condition number 2.2e16) and its null vector is the constant vector
to 1.2e-14 relative (`||A @ 1|| = 4.3e-15`), which is also the shipped near-null-space
candidate `B` (all ones). The probe's right-hand side, `b = np.linspace(0.5, 1.5, 191)`,
had 96% of its norm in that null direction, so `A x = b` has no solution: the fixed six
V-cycles diverged (20.34, 122.0, 190.0, 209.3, 180.1, 173.7, 376.0) and the
coarsest-level `pinv` solve amplified the variant's two-ulp perturbation of `b` into an
O(100) swing. The graded array's first entry, the initial residual computed before the
first cycle, moved only in its last bit; the six cycle residuals were the ones that
moved.

The arbitrary right-hand side was not local to that one check. It was in every solve
probe of the leaf, and the graded residual histories of run 1 show what it cost: nine
of the eleven solve checks stalled or diverged inside their fixed window
(`adaptive-sa-complex` 255.8 -> 130.3 -> 127.6 -> 127.8 -> 128.8;
`rootnode-performance-real` 305.1 -> 122.5 -> ... -> 94.3, ratio 0.95 per cycle;
`sa-performance-nonsymmetric-real` 15.62 -> 551.4 -> 194.3 -> ... , the first cycle
multiplying the residual by 35). Upstream never builds a right-hand side that way:
every solver test in these classes uses `b = A @ np.random.rand(n)`, a right-hand side
in the range of the operator (`test_aggregation.py:204`, `:326`, `:493`, `:513`).

What changed:

- **Consistent right-hand sides.** Every solve probe now builds `b = A @ v` with `v` a
  seeded random vector, exactly as its upstream case does. The fixed-cycle iterations
  now converge instead of stalling on a component the operator cannot reach.
- **The solution field is graded.** A residual norm is a scalar diagnostic; the
  production quantity of a linear solve is the discrete field the solve produces, and it
  is the quantity the module's own official test compares directly
  (`TestComplexSolverPerformance::test_precision`, `test_aggregation.py:583`). Each
  solve probe now grades that field, then the residual history, then the hierarchy
  depth. Graded array sizes go from 5-8 values to 465-11535.
- **`sa-performance-real` moves off `unit_square`** onto `poisson((60, 60))`, one of the
  gate class's own `setUp` cases (`test_aggregation.py:176`), 3600 dofs, SPD.
  `unit_square` stays in `energy-prolongation`, where a pure-Neumann Laplacian whose
  exact null vector is the constant near-null-space candidate is the right problem for
  the energy-minimization fit and no solve is made.
- **`gallery-demo`** grades both solution fields of the shipped demo sequence beside its
  two residual histories.
- **The three aggregate checks declare their measured-identical variant**, so the record
  reads "identical, as the rubric declares" instead of a perturbation-never-took-effect
  warning. The finding itself is unchanged: the graded output is a discrete 0/1
  aggregate map and a few-ulp perturbation of one matrix entry moves no row's strength
  threshold.

Calibration on the grading worker before the rerun (one build, every probe on both
initial conditions, each check graded with its own `validate.py`): 23 of 23 pass, worst
`bound_fraction` 0.271, `sa-performance-real` 0.00292 over 3608 graded values.

## Tolerances

The recommended pointwise policy of atol 1e-12 plus rtol 1e-10 is kept as the working
bound for all 23 checks; the human owns the final numbers. The calibration measured on
the grading worker (huangzesen@136.114.2.6, x86_64) after the round-2 fix, one build and
every probe on both initial conditions, worst `bound_fraction` per check:

| check | graded values | spread | bound_fraction | margin |
|---|---|---|---|---|
| adaptive-sa-complex | 5767 | 5.684e-14 | 0.00873 | 115x |
| adaptive-sa-real | 807 | 1.397e-09 | 3.865e-04 | 2588x |
| aggregate-complex | 480 | 0 | 0 (identical, as declared) | - |
| aggregate-pairwise-real | 126 | 0 | 0 (identical, as declared) | - |
| aggregate-real | 522 | 0 | 0 (identical, as declared) | - |
| energy-prolongation | 830 | 3.608e-16 | 9.416e-05 | 10621x |
| energy-prolongation-bsr | 6144 | 1.051e-12 | 0.1429 | 7x |
| fit-candidates | 4032 | 3.886e-16 | 1.103e-05 | 90661x |
| gallery-demo | 20031 | 1.592e-12 | 0.2707 | 3.7x |
| pairwise-spd | 1736 | 7.105e-14 | 6.551e-04 | 1527x |
| paper-smoothed-aggregation-example | 39 | 1.364e-12 | 1.381e-05 | 72405x |
| rootnode-parameters-complex | 5766 | 5.684e-14 | 0.01699 | 59x |
| rootnode-parameters-real | 806 | 1.397e-09 | 7.041e-04 | 1420x |
| rootnode-performance-complex | 1808 | 1.421e-14 | 3.330e-04 | 3003x |
| rootnode-performance-nonhermitian-complex | 11535 | 2.274e-13 | 0.03654 | 27x |
| rootnode-performance-nonsymmetric-real | 465 | 1.665e-14 | 0.005295 | 189x |
| rootnode-performance-real | 974 | 2.274e-13 | 0.008365 | 120x |
| sa-parameters-complex | 5766 | 5.684e-14 | 0.01157 | 86x |
| sa-parameters-real | 806 | 1.397e-09 | 6.090e-04 | 1642x |
| sa-performance-complex | 1808 | 1.421e-14 | 3.907e-04 | 2560x |
| sa-performance-nonhermitian-complex | 11535 | 2.274e-13 | 0.03085 | 32x |
| sa-performance-nonsymmetric-real | 465 | 8.882e-15 | 0.002465 | 406x |
| sa-performance-real | 3608 | 7.105e-14 | 0.002921 | 342x |

Two rows carry a margin under 50 and both are open calls for the human, not decisions
made here:

- **`gallery-demo`, margin 3.7x.** The worst value is not in either solution field --
  those sit at `bound_fraction` 4.07e-05 and 5.06e-05, margin above 19,000x -- but in
  the standalone residual history at cycle 15, where the residual has fallen to
  2.758e-07. There `rtol * |ref|` is 2.8e-17 and the bound is atol 1e-12 alone, while the
  two-ulp variant moves the deeply converged residual by 2.71e-13. The residual tail of
  a converged solve is an atol-dominated diagnostic beside a solution field graded with
  four orders of magnitude more headroom. Options for the human: leave the bound as it
  is (the check still rejects a real fault by a wide factor), or drop the residual tail
  from the graded set and keep the two solution fields.
- **`energy-prolongation-bsr`, margin 7x.** The worst value is prolongator entry 2882,
  whose reference value is 7.939e-14 -- 1.6e-13 of the matrix's largest entry, 0.5. It is
  a numerically-zero entry of the smoothed BSR prolongator; the two-ulp variant moves it
  by 1.43e-13, again against atol 1e-12 alone. The same shape as the
  `residual-below-one-ulp` pitfall: the flagged value carries no physics. Options: leave
  it, or raise atol for this check to cover the near-zero entries, with the number the
  human chooses.

Each run first executes its immutable task-owned upstream gate, so failure of any
official assertion (or, for `gallery-demo` and `paper-smoothed-aggregation-example`, the
probe's own residual-bound assertion) produces no graded output and fails the check. The
selfcheck's measured spread, `bound_fraction` and altbuild floor per check are recorded
in each check's `rubric.json` and in `comment/pipeline/`.

## Blind spots

The official suite is broad but uses serial CPU execution; it does not cover distributed aggregation or GPU-specific sparse formats. Three checks (`aggregate-real`, `aggregate-pairwise-real`, `aggregate-complex`) measure an **identical** nominal-versus-variant pair: aggregate assignment is a discrete threshold decision over exact IEEE754 comparisons, and a single few-ulp perturbation of one matrix entry does not cross any row's relative-strength threshold on the shipped meshes tested (confirmed up to a 0.1% perturbation during authoring, still no structural change) — this is documented discreteness, not a broken variant, per the rubric's `variant` field on each. Those three checks also grade a canonical, order-independent per-dof aggregate identity (each dof mapped to its aggregate's minimum-index member) rather than the raw aggregate-column numbering, which is bookkeeping a correct alternative implementation may assign differently while grouping the same dofs — consistent with the pointwise-grades-physics-not-storage policy. `gallery-demo` and `paper-smoothed-aggregation-example`'s fixed cycle counts (18/11 and 21) were measured on the arm64 authoring host and are pending reconfirmation on the pinned x86_64 grading build.

The `unit_square` operator shipped in `pyamg/gallery/example_data/` is singular
(lambda_min 3.04e-16, lambda_max 6.788) with the constant vector as its null space,
despite the folder's `README.txt` saying Dirichlet boundary conditions have been removed
from the matrices. It is a valid problem for the aggregation and prolongator-smoothing
paths, where the constant null vector is exactly the near-null-space candidate the
method is built around, and `energy-prolongation` uses it for that. It is not a valid
problem for a fixed-step solve against an arbitrary right-hand side, and that is the
mistake round 1 made. This is a candidate for a `Known pitfall` issue on the benchmark
repository: *a shipped example operator can be singular, and a fixed-step iteration on
an inconsistent right-hand side amplifies the two-ulp variant by the operator's
condition number* — measured here as a `bound_fraction` of 1.5e10 against 0.00292 for
the same solver on a nonsingular problem.

Two checks carry a margin under 50 for reasons that have nothing to do with the physics
under test: `gallery-demo` (3.7x, from the atol-dominated tail of a converged residual
history, while the two solution fields in the same check sit above 19,000x) and
`energy-prolongation-bsr` (7x, from a numerically-zero prolongator entry at 7.9e-14).
Both are written up under Tolerances as open calls for the human.
