# mudpack: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

MUDPACK 5.0.1 solves separable and nonseparable linear elliptic PDEs in 2D
and 3D by multigrid iteration (real and complex, second- and fourth-order,
hybrid multigrid/direct, cross-derivative, separable and self-adjoint
forms). The module owns the whole vendored tree `code/mudpack` (one build,
one static library, one test suite). All 36 rows of the Step 2 survey
(`tests.json`) are `suitable: true` and each became exactly one check, one
per official test driver in `test/t*.f`: none was left out. Each check's
`ic/{nominal,variant}/driver.f` is a copy of the corresponding official test
with one output block appended right before the program's own final `end`
(never touching the solver call, the PDE coefficients, the boundary
conditions or the grid size); the block writes the solved grid array in a
fixed column-major order and the driver's own final discretization-error
scalar to `sab_solution.dat`, so the check can grade the solution field
pointwise instead of only the printed error norm the original driver prints.
`run.sh` and `validate.py` are the same file, byte-for-byte, in all 36 check
directories (only the check name in the header comment differs): one shared
build/run/compare pattern instantiated once per test, per the packaging
brief. Nothing under `code/mudpack/` was edited.

MUDPACK ships 515 lines of OpenMP directives on its relaxation loops across
the library; every check builds without `-fopenmp` (native `make`,
`environment/Dockerfile`, `tests/Dockerfile` and both build flag sets in
`run.sh`), so those directives stay inactive and the multigrid summation
order is fixed across nominal, variant and altbuild runs. A threaded
(`-fopenmp`) build is out of scope for these checks: it changes the
relaxation-loop reduction order, which is exactly the kind of legitimate
difference a check's floor is supposed to measure, not eliminate by
construction, and no reference/darwin transcript exists for it to calibrate
against.

## Build

Each check's `run.sh` compiles the whole 54-file MUDPACK static library from
`$SOURCE_DIR/src` (`gfortran -fdefault-real-8 <-O2|-O0> -std=legacy`, one
`gfortran -c` invocation over all 54 files, then `ar rcs`), then compiles and
links `ic/<ic>/driver.f` against it. Measured natively: about 44 s wall for
the library build (per-process `gfortran` startup dominates over 54 small
files; matches the ~53 s Step 1.2 measured for the equivalent native `make`)
and well under 1 s for the driver compile, link and run.

The library build is cached and shared across checks and across solves,
keyed by build flags and source identity only, never by the initial
condition (pattern copied from `tasks/swmf/swmf-batsrus`): `tests/test.sh`
computes one `SAB_SOURCE_FINGERPRINT` per `produce` invocation and hands
each check's `run.sh` a `SAB_BUILD_CACHE_ROOT`; `solution/solve.sh` mounts
one host directory at `/app/build-cache` in every container of every solve
of one selfcheck (`dirname($ORACLE)` is the same run root for the nominal,
variant and altbuild solves, so the mounted path is identical across all
three). `run.sh` hashes its own build flags (`-O2` or `-O0`, `-fdefault-real-8
-std=legacy`, the compiler and `ar` versions, the machine architecture) plus
the source fingerprint into one cache key, and reuses a previously published
`libmudpack.a` on a verified digest match instead of rebuilding. Concretely:
the `-O2` library built once (by whichever of the 36 parallel `nominal`
containers finishes first) is reused by every other `nominal` check and by
every `variant` check (same flags); the `-O0` library is its own cache entry,
built once during the `altbuild` solve and reused by the other 35 declaring
checks in that solve. `SAB_BUILD_SECONDS` on each run reports 0 on a verified
cache hit and the measured build time on a miss; the 900 s `suite_budget_s`
counts run time only (builds are excluded by definition either way).

## Tolerances

Every check is deterministic double-precision serial arithmetic: MUDPACK's
test drivers set `tolmax=0.0` (no iterative error-control flag), so each
solve runs a fixed multigrid cycle count and the driver's own printed
discretization error (`errmax`/`errm`/`err2`, from `1e-7` to a few `1e0`
across the 36 tests, always matching `code/mudpack/output/darwin.dp` to the
printed digits — confirmed for all 36 before authoring) is the scale a wrong
algorithm (a dropped coefficient, a coarser grid, fewer cycles, a broken
relaxation sweep) would violate. The two runs a check actually has to agree
on, nominal-vs-variant (a two-ULP perturbation of one active domain-extent
literal, `xb`, `pd` or `rb` depending on the family) and nominal-vs-altbuild
(`-O0` vs `-O2`, same compiler), differ only at the level of floating-point
reassociation: on the worker's final selfcheck (x86_64, reward 1.0, 36/36),
every one of the 35 `pointwise` checks' measured floor sits between
`7.8e-7` and `1.3e-3` of the bound `atol=1e-10, rtol=1e-9` (worst case
`tmuh3`, headroom about 746x); the provisional bound authored before
calibration needed no revision. Of the 36 declared `altbuild` runs, 26
moved by a measured, nonzero amount and 10 were bit-identical to nominal on
this host (an ordinary result at `-O0` vs `-O2`, not a sign that no port
happened — see SPEC's identical-warning rule, which is about nominal vs
candidate, not the altbuild axis). **One check, `tcud3cr`, changed policy
to `invariants`** because its official configuration is a non-convergent,
multi-state outer cross-derivative correction: it grades the termination
code (`ierror`) exactly and the residual (`err2`) as an order-of-magnitude
band, not the solved field pointwise; see its own `rubric.json` and
`README.md` for the full measurement, and "## Blind spots" below — **this
is the one row the human should rule on** (keep as `invariants`, or exclude
it with a `tests.json` reason). Every other check needed no policy change,
no shorter window and no different observable.

## Blind spots

- **No OpenMP build.** See "## Module" above: an `-fopenmp` build is a
  different, unrated axis (relaxation order), not covered by any check here.
- **Single precision is out of scope.** The library's default build is
  single precision; Step 1.2 measured that the shipped `darwin.sp`
  transcript reproduces far fewer digits and that `tcud3cr` fails to
  converge in single precision. Every check here builds
  `-fdefault-real-8`, matching the `darwin.dp` reference; a single-precision
  port is not graded.
- **`tcud3cr` and `tmud3cr` do not converge within their own coded outer
  cross-derivative correction iteration** (`ierror=-10` in `output/darwin.dp`
  and in every build measured here, for the same reason upstream ships it
  that way): this is the official test's own shipped behavior, faithfully
  reproduced, not a defect introduced here. For `tmud3cr` (real-valued) this
  is cosmetic: its measured build floor is round-off-level (`~1.8e-15`,
  consistent with the other 34 pointwise checks), so it keeps the default
  `pointwise` policy. For `tcud3cr` (complex-valued) it is not cosmetic:
  measured across two architectures, two optimization levels and one input
  perturbation (five runs before the worker selfcheck, plus the worker's own
  final run), the driver's own final least-squares error landed on
  different values in a range from `0.0986` to `3.06` while `ierror=-10`
  every time — the same non-scaling, build-dependent-jump signature as the
  pitfalls index's `meep-mpb-eigensolver-two-state` entry, not a smooth
  floor. **`tcud3cr` grades the outer correction's termination code
  (`ierror`) exactly and its residual (`err2`) as a wide, measured
  order-of-magnitude band, because the official configuration this test
  exercises is a non-convergent, multi-state outer correction, not because
  a tighter pointwise bound was inconvenient. This is the one row in this
  leaf the human should rule on: keep it as `invariants` (recommended: it
  is a real check, cheap, and it does reject a crash, a trap, or a
  qualitatively different residual), or exclude it from the check set with
  a reason recorded in `tests.json`.** See `tests/checks/tcud3cr/rubric.json`
  (`warrant`) and `tests/checks/tcud3cr/README.md` for the full five-run
  measurement and the bound's derivation. This is also a discovered pitfall
  candidate (a build- and architecture-sensitive, non-convergent outer
  correction loop landing on one of several discrete residual values) worth
  a Known-pitfall issue on the benchmark repository; flagged to the curator
  rather than filed from this worker.
- **Acceleration workload.** No check is singled out as the timed
  acceleration workload, per the skill's own rule; what is timed on the
  target is decided downstream with the tasks themselves.
