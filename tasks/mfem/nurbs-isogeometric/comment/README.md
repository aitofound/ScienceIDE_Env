# nurbs-isogeometric: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

MFEM's NURBS isogeometric analysis. The finite element space is spanned by the same rational
B-spline basis that defines the geometry, so the domain stays exact under refinement rather
than being approximated by polynomial elements; knot insertion adds degrees of freedom without
moving the geometry at all, which is the property that distinguishes the method from ordinary
finite elements. The module owns `mesh/nurbs.cpp` and `mesh/spacing.cpp` in the library and the
whole of `miniapps/nurbs/`.

Unlike the other seven modules of this cut, the official tests here are taken from what
upstream's own `make test` registers in `miniapps/nurbs/` rather than from the documented
sample-run headers: that directory registers the largest single block of invocations in the
serial build, and the registered set is already the coverage-driven inventory.

Deliberately excluded: the parallel miniapps (`nurbs_ex1p`, `nurbs_ex11p`) and everything else
behind `[Parallel]`, because this leaf is calibrated on a serial build — MPI would need hypre
and METIS. `nurbs_surface` is registered by the makefile but is not in `SEQ_MINIAPPS`, so the
serial `make test` does not run it. The `-patcha` run of `nurbs_patch_ex1` without `-pa` is
gated on `MFEM_USE_LAPACK` and self-skips here.

## Build

Yes: every check compiles the pinned MFEM source at solve time. There is nothing prebuilt in
either image.

The recipe is one line, identical for every check —
`make serial -j$SAB_MAKE_JOBS MFEM_USE_METIS=NO CXXFLAGS="-O3 -std=c++17"` for the library, then
the check's own miniapp (or `unit_tests`) — where `-O3 -std=c++17` is MFEM's own `OPTIM_FLAGS`
(`config/defaults.mk:29`, taken by `CXXFLAGS ?= $(OPTIM_FLAGS)` at `makefile:227`). The altbuild
appends `-mfma -ffp-contract=fast` and nothing else.

**Reuse within a run.** Each `run.sh` copies `SOURCE_DIR`, takes a SHA-256 over the sorted
relative paths and bytes of that copy plus the exact recipe string, and keys a cache under
`/tmp/sab-build-mfem-nurbs-isogeometric/<hash>`. `tests/test.sh produce` runs checks
sequentially, so no cross-check race protocol is needed: on a miss the arriving check copies the
tree into its own cache directory, builds there, and writes `BUILD_OK` only after `make`
succeeds; every later check of the same run finds `BUILD_OK` and reports
`SAB_BUILD_SECONDS=0`. If the shared root cannot be created, the same build function runs
privately under the check's `$WORK`, so each `run.sh` stays self-contained.

**The initial condition is overlaid after the build, not before.** An initial condition here is
a set of mesh files, which no compilation reads. Keying the build on it would give nominal,
variant and altbuild three different keys and force three full library builds where two would
do, so the key covers the pinned source and the recipe only, and the meshes are copied over the
built tree afterwards. The runs read their meshes from that overlaid tree, so nominal and
variant genuinely solve on different geometry while sharing one build.

**Build jobs come from the cgroup, not from `nproc`.** `cpus_allowed()` reads cgroup v2
`cpu.max` and only falls back to `nproc`. This is the failure recorded in
`tasks/pyamg/classical-amg/comment/README.md`: ninja detected the worker's 88 cores under a
`--cpus 1` limit and the OOM killer took out `cc1plus` mid-compile against a declared
`memory_gb: 2.0`. Related: `references/pitfalls/blas-threads-follow-the-host-core-count.md`.

Build and run seconds: PENDING the calibration selfcheck, recorded in
`comment/pipeline/runtime-metadata.json`. Measured natively on the packaging host
(x86_64, AL2023, GCC 11.5, 4 cores) the library build is 12 min 53 s at `-j4`, and the entire
graded run time of all twenty checks is **8.3 s** — this module is almost pure build cost.

## Tolerances

Finalized with the human at STOP 4 on 2026-09-11, from the first `selfcheck` (the
calibration run: two solves, nominal 1450 s and variant 1444 s, on 4 cpus / 4.0 GB).
Every pointwise check carries **atol 1e-7, rtol 1e-6**; the three verdict-graded unit
checks carry exact equality.

**What the calibration measured.** The nominal-versus-variant spread of all twenty checks
spans 1e-9 to 1e-6, with no check losing an order of magnitude anywhere:

| check | spread | bound used |
| --- | ---: | ---: |
| solenoidal-fields | 1.0e-6 | 0.25 |
| hyperelasticity, poisson-multipatch | 1.0e-7 | 0.59, 0.10 |
| derham-interpolators | 2.17e-8 | **1.82** |
| maxwell-definite | 2.00e-8 | **1.51** |
| single-patch, weak-boundary, two-patch, darcy | 1.0e-8 | 0.05-0.09 |
| patch-partial-assembly, patch-full-integration | 4-5e-9 | ~0.05 |
| no-integration-by-parts, periodic | 1.0e-9 | 0.02-0.04 |
| seven checks with a declared identical variant | 0 | - |

**Why atol moved from 1e-8 to 1e-7.** Two checks exceeded the provisional bound, both
because their reference values are small: atol governs the bound only where
|ref| < atol/rtol, and for derham and maxwell that is where the graded values sit. Raising
atol one decade brings derham from 1.82 of the bound to about 0.22.

**What that cost, measured rather than asserted.** Across the 106,983 nonzero graded
values of the suite, the share whose bound is decided by atol rather than by rtol|ref|
rises from **9% to 41%**, and for those the threshold rises tenfold. Two checks are
entirely inside that band (patch-partial-assembly and patch-full-integration, max |ref|
0.097), as are most of single-patch, two-patch and periodic. The discrimination that
remains is still four orders of magnitude: a real port fault in this module -- a wrong
rational basis, a dropped weight, a mis-scaled weak-boundary penalty, geometry drifting
under refinement -- moves these fields by 1e-3 or more.

**The alternative was rejected on measurement.** Shrinking the variant to keep atol at 1e-8
would make the perturbation vacuous: 1e-10 moves 5 of 4225 values and 1e-12 moves none,
which is exactly `references/pitfalls/output-precision-floors-the-bound.md`. Per-check
tolerances were considered and rejected for consistency: the two failing checks are not
special, they are simply the first to reach a boundary that patch-partial-assembly would
reach next.

**Floors.** The altbuild solve did not run on the calibration pass -- the third solve is
skipped when the verify step fails -- so `evidence.floor` is filled by the confirming
`selfcheck`, not by this one.

## Blind spots

* **`refined.mesh` is written by most runs and is not graded.** Section 9 of the packaging
  direction forbids grading element numbering after refinement, and every `-r N` run refines, so
  the numbering is a legitimate implementation choice rather than physics. A genuinely different
  discrete space is still rejected, because the solution stream then has a different length and
  `validate.py` fails on shape before it compares a value. The consequence accepted here is that
  a port which produced the *same* field on a *differently numbered* mesh passes, which is the
  intended behaviour.
* **The VisIt collections are not graded**: they are written at precision 6 rather than 8, so
  they are strictly coarser than the `.gf` streams that are graded. Grading them would risk
  `references/pitfalls/ungraded-sidecars-mask-identical-graded-output.md` in reverse.
* **`|| div u_h - div u_ex ||` is not graded by value.** It is a residual whose exact value is
  zero — the solenoidal construction is pointwise divergence-free by design — and it measures
  4.04e-13 here, pure round-off. A different build swaps one remainder for another, so the line
  is graded by its verdict, not its value
  (`references/pitfalls/residual-below-one-ulp.md`). The same applies to the de Rham complex
  identity in `nurbs-derham-interpolators`.
* **Iteration counts, residual histories and wall clocks never reach `OUT_DIR`.** `nurbs_ex10`
  prints 75 lines of Newton residual history in the same `||...=` notation as the graded L2
  errors, and `nurbs_ex5` prints `MINRES solver took 0.000441939s.` one line away from them, so
  the printed errors are extracted by curated per-binary patterns and never by a generic grep.
* **The three unit checks grade verdicts, not values.** A Catch2 case exposes no numeric
  stream: its XML carries the verdict and assertion counts but also the absolute source path,
  which differs between reference and candidate, and its `-s` expansion prints values in an
  unstable format that are often zero-valued residuals. So these checks cannot detect a port
  that stays inside the upstream test's own tolerance while being subtly wrong.
* **Thirteen of the twenty altbuild floors are exactly zero, and that must be read as "not
  measured", not as "stable".** The altbuild rebuilds the pinned source with
  `CXXFLAGS="-O3 -std=c++17 -mfma -ffp-contract=fast"`, chosen over `-O0` precisely because
  `-O0` is a no-op for floating-point evaluation on baseline x86_64 gcc. It did produce a
  real floor on seven checks -- `darcy-mixed` 1.0e-8, `derham-interpolators` 1.0e-11,
  `maxwell-definite` 1.0e-10, `poisson-weak-boundary` 1.0e-13, `solenoidal-fields` 1.2e-15,
  `hyperelasticity` 1.5e-16, `curve-interpolation` 3.6e-18 -- but on the other thirteen the
  two builds agreed bit for bit, which means the code paths those checks exercise contain no
  multiply-add pair the compiler was contracting differently. Per
  `references/pitfalls/altbuild-floors-are-host-specific.md`, a zero floor is evidence that
  the alternative build computed the same thing, not evidence that the check is insensitive
  to a legitimate build difference. **So thirteen checks have no measured floor.** Their
  tolerance rests on the variant spread and on the precision argument alone.
* **Six checks have neither a variant spread nor an altbuild floor**:
  `basis-function-output`, `interpolation-point-families`, `knot-operations-invariance`,
  `mesh-io-and-patch-loading`, `mesh-topology-report`, `naca-cmesh-generation`. They pass,
  but passing here means only that two runs of the same build agree; nothing in this leaf
  measures how far a legitimate build difference could move them. The reviewer should treat
  their bounds as argued from the eight-digit output precision, not as calibrated. Making
  them real would need a stronger alternative build -- a second compiler in the image is the
  candidate the pitfalls index recommends and that the s4 leaf used -- which was considered
  and deferred rather than done.
* **Seven of the twenty checks have an explicitly identical variant**, so their only
  calibration evidence is the altbuild floor. Three take no input file at all
  (basis-function-output, curve-interpolation, naca-cmesh-generation): they generate their
  answer from command-line parameters, and perturbing one of those would be a different
  configuration, not a variant. Three are the Catch2 unit checks, which build their meshes
  in code. The seventh, mesh-topology-report, emits the Greville, Chebyshev and Botella
  point families, which are pure functions of the knot vector and degree -- measured: a
  control-point perturbation leaves every graded `.dat` byte-identical, and the mesh's knot
  vectors are `0 0 1 1` with no interior knot to move. All seven say so in their rubric's
  `variant` field rather than claiming a spread they do not have.
* **Six upstream cases in this module's files are unavailable in a serial build** and are
  recorded in the survey as `suitable: false` rather than dropped; none of them is in
  `test_nurbs.cpp`, so this module loses nothing to that. The module-wide picture is in
  `~/direction.md` 7.6.
